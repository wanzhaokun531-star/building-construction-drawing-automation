// Publish sequence reused from the existing NativePlotProbe; temporary settings only.
struct PlotStatus { bool called{};int value{};void set(Acad::ErrorStatus v){called=true;value=v;}bool ok()const{return called&&value==Acad::eOk;}std::string json()const{return called?std::to_string(value):"null";} };
class NativePlotPipeline {
public:
    enum Stage { Create, BeginPlot, BeginDocument, BeginPage, BeginGraphics, EndGraphics, EndPage, EndDocument, EndPlot, Count };
    PlotStatus statuses[Count];
    bool destroyed{}, cleanupThrew{};
    NativePlotPipeline() = default;
    NativePlotPipeline(const NativePlotPipeline&) = delete;
    NativePlotPipeline& operator=(const NativePlotPipeline&) = delete;
    ~NativePlotPipeline() { finish(); }
    void begin(AcPlPlotInfo& info, AcPlPlotPageInfo& page, const std::wstring& document, const std::wstring& output) {
        const auto factoryStatus = AcPlPlotFactory::createPublishEngine(engine_); statuses[Create].set(factoryStatus);
        ok(factoryStatus, "create native publish engine");
        if (!engine_) throw std::runtime_error("native publish engine returned null");
        auto status = engine_->beginPlot(nullptr); statuses[BeginPlot].set(status); plot_ = status == Acad::eOk; ok(status, "native beginPlot");
        status = engine_->beginDocument(info, document.c_str(), nullptr, 1, true, output.c_str()); statuses[BeginDocument].set(status); document_ = status == Acad::eOk; ok(status, "native beginDocument");
        status = engine_->beginPage(page, info, true); statuses[BeginPage].set(status); page_ = status == Acad::eOk; ok(status, "native beginPage");
        status = engine_->beginGenerateGraphics(); statuses[BeginGraphics].set(status); graphics_ = status == Acad::eOk; ok(status, "native beginGenerateGraphics");
    }
    void finish() noexcept {
        if (!engine_) return;
        // Every successful begin gets one matching end attempt, in reverse nesting.
        // Engine release uses destroy(), never delete, including failed pipelines.
        if (graphics_) { graphics_ = false; try { statuses[EndGraphics].set(engine_->endGenerateGraphics()); } catch (...) { cleanupThrew = true; } }
        if (page_) { page_ = false; try { statuses[EndPage].set(engine_->endPage()); } catch (...) { cleanupThrew = true; } }
        if (document_) { document_ = false; try { statuses[EndDocument].set(engine_->endDocument()); } catch (...) { cleanupThrew = true; } }
        if (plot_) { plot_ = false; try { statuses[EndPlot].set(engine_->endPlot()); } catch (...) { cleanupThrew = true; } }
        auto* engine = engine_; engine_ = nullptr;
        try { engine->destroy(); destroyed = true; } catch (...) { cleanupThrew = true; }
    }
    bool complete() const {
        if (!destroyed || cleanupThrew) return false;
        for (const auto& status : statuses) if (!status.ok()) return false;
        return true;
    }
    std::string json() const {
        const char* names[]{ "create", "beginPlot", "beginDocument", "beginPage", "beginGraphics", "endGraphics", "endPage", "endDocument", "endPlot" };
        std::ostringstream out; out << '{';
        for (int i = 0; i < Count; ++i) { if (i) out << ','; out << '"' << names[i] << "\":" << statuses[i].json(); }
        out << '}'; return out.str();
    }
private:
    AcPlPlotEngine* engine_{};
    bool plot_{}, document_{}, page_{}, graphics_{};
};

class BackgroundScope final{
public:
    short before{};bool restored{};
    void activate(){require(acplProcessPlotState()==kNotPlotting,"plot already busy");resbuf old{};require(acedGetVar(L"BACKGROUNDPLOT",&old)==RTNORM&&old.restype==RTSHORT,"read BACKGROUNDPLOT");before=old.resval.rint;active=true;resbuf v{};v.restype=RTSHORT;v.resval.rint=0;require(acedSetVar(L"BACKGROUNDPLOT",&v)==RTNORM,"set foreground plot");}
    void restore()noexcept{if(!active)return;resbuf v{};v.restype=RTSHORT;v.resval.rint=before;const auto set=acedSetVar(L"BACKGROUNDPLOT",&v);resbuf r{};restored=set==RTNORM&&acedGetVar(L"BACKGROUNDPLOT",&r)==RTNORM&&r.restype==RTSHORT&&r.resval.rint==before;if(restored)active=false;}
    ~BackgroundScope(){restore();}
private:bool active{};
};
std::string plotSettings(const AcDbPlotSettings& p){
    double w{},h{},l{},b{},r{},t{},ox{},oy{},n{},d{},x0{},y0{},x1{},y1{};
    ok(p.getPlotPaperSize(w,h),"paper size");ok(p.getPlotPaperMargins(l,b,r,t),"paper margins");ok(p.getPlotOrigin(ox,oy),"plot origin");ok(p.getCustomPrintScale(n,d),"plot scale");ok(p.getPlotWindowArea(x0,y0,x1,y1),"plot window");
    AcString media;ok(p.getCanonicalMediaName(media),"media name");
    return object({{"device",q("DWG To PDF.pc3")},{"media",quote(ws(media.kACharPtr()))},{"paperMM",array({num(w),num(h)})},{"marginsMM",array({num(l),num(b),num(r),num(t)})},
        {"originMM",array({num(ox),num(oy)})},{"scale",array({num(n),num(d)})},{"windowDCS",array({num(x0),num(y0),num(x1),num(y1)})},
        {"rotation",std::to_string(p.plotRotation())},{"paperUnits",std::to_string(p.plotPaperUnits())},{"plotType",std::to_string(p.plotType())},
        {"centered",p.plotCentered()?"true":"false"},{"useStandardScale",p.useStandardScale()?"true":"false"},{"printLineweights",p.printLineweights()?"true":"false"},{"plotStyles",p.plotPlotStyles()?"true":"false"}});
}
std::string plotPage(Session& s,const wchar_t* id){
    require(s.ownsCurrent()&&s.target==acdbHostApplicationServices()->workingDatabase(),"plot control mismatch");
    require(acplProcessPlotState()==kNotPlotting,"plot busy");
    const auto output=s.config.output+L"\\"+id+L".pdf";require(GetFileAttributesW(output.c_str())==INVALID_FILE_ATTRIBUTES,"PDF output exists");
    {auto marker=opened<AcDbPoint>(s.marker,AcDb::kForWrite);marker->recordGraphicsModified(true);}
    auto ms=opened<AcDbBlockTableRecord>(acdbSymUtil()->blockModelSpaceId(s.target));auto layoutId=ms->getLayoutId();ms.reset();
    AcDbPlotSettings settings(true);{auto layout=opened<AcDbLayout>(layoutId);ok(settings.copyFrom(layout.get()),"copy temporary plot settings");}
    auto* v=acdbHostApplicationServices()->plotSettingsValidator();require(v!=nullptr,"plot validator unavailable");v->refreshLists(&settings);
    ok(v->setPlotCfgName(&settings,L"DWG To PDF.pc3"),"PDF device");v->refreshLists(&settings);
    AcArray<const ACHAR*> borrowed;ok(v->canonicalMediaNameList(&settings,borrowed),"media list");std::vector<std::wstring> media;for(int i=0;i<borrowed.length();++i)if(borrowed[i])media.emplace_back(borrowed[i]);
    std::wstring chosen;for(const auto& name:media){if(name.find(L"full_bleed")==std::wstring::npos)continue;ok(v->setCanonicalMediaName(&settings,name.c_str()),"candidate media");double w{},h{};ok(settings.getPlotPaperSize(w,h),"candidate paper");if(std::abs(w-210)<0.1&&std::abs(h-297)<0.1){chosen=name;break;}}
    require(!chosen.empty(),"full bleed portrait A4 absent; no custom media created");ok(v->setCanonicalMediaName(&settings,chosen.c_str()),"selected media");
    ok(v->setPlotPaperUnits(&settings,AcDbPlotSettings::kMillimeters),"mm units");ok(v->setPlotRotation(&settings,AcDbPlotSettings::k0degrees),"zero rotation");
    ok(v->setPlotWindowArea(&settings,0,0,150,100),"window");ok(v->setPlotType(&settings,AcDbPlotSettings::kWindow),"window type");
    ok(v->setUseStandardScale(&settings,false),"custom scale type");ok(v->setCustomPrintScale(&settings,1,1),"1:1 mm scale");ok(v->setPlotCentered(&settings,false),"uncentered");ok(v->setPlotOrigin(&settings,20,20),"20mm origin");
    settings.setPrintLineweights(false);settings.setScaleLineweights(false);settings.setPlotPlotStyles(false);
    const auto requested=plotSettings(settings);AcPlPlotInfo info;info.setLayout(layoutId);info.setOverrideSettings(&settings);AcPlPlotInfoValidator validator;validator.setMediaMatchingPolicy(AcPlPlotInfoValidator::kMatchEnabled);ok(validator.validate(info),"validate PDF info");
    require(info.isValidated()&&info.validatedSettings(),"no validated PDF settings");const auto* actual=info.validatedSettings();
    double width{},height{},left{},bottom{},right{},top{},numerator{},denominator{},ox{},oy{};ok(actual->getPlotPaperSize(width,height),"actual paper");ok(actual->getPlotPaperMargins(left,bottom,right,top),"actual margins");ok(actual->getCustomPrintScale(numerator,denominator),"actual scale");ok(actual->getPlotOrigin(ox,oy),"actual origin");
    require(actual->plotRotation()==AcDbPlotSettings::k0degrees&&actual->plotPaperUnits()==AcDbPlotSettings::kMillimeters&&!actual->useStandardScale()&&!actual->plotCentered()&&std::abs(numerator-1)<1e-12&&std::abs(denominator-1)<1e-12&&std::abs(width-210)<0.1&&std::abs(height-297)<0.1,"validated settings changed scale/orientation");
    // DCS == WCS XY for the prepared orthographic top view. This candidate map is
    // independently checked against all five calibration lines in the native PDF.
    const double k=72.0/25.4;const auto mapping=array({num(k),"0",num(k*(left+ox)),"0",num(-k),num(k*(height-bottom-oy))});
    AcPlPlotPageInfo page;NativePlotPipeline pipeline;pipeline.begin(info,page,s.controlPath(),output);pipeline.finish();require(pipeline.complete()&&acplProcessPlotState()==kNotPlotting,"native publish cleanup failed");
    WIN32_FILE_ATTRIBUTE_DATA file{};require(GetFileAttributesExW(output.c_str(),GetFileExInfoStandard,&file)&&file.nFileSizeLow>0,"native PDF missing/empty");
    return object({{"id",quote(std::wstring(id))},{"pdf",quote(output)},{"pdfSHA256",q(sha256(output).c_str())},{"requestedSettings",requested},{"validatedSettings",plotSettings(*actual)},
        {"worldToPagePoints",mapping},{"mappingBasis",q("validated plot origin+margins+1:1 mm, prepared top WCS XY, y-down page; requires independent five-line calibration")},
        {"engineStatuses",pipeline.json()},{"pipelineComplete","true"},{"lineWeight",q("0mm public traits; printing lineweights false; PDF/raster footprint measured without added margin")}});
}

