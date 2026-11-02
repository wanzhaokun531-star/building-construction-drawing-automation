// Include inside NativeAccessProbe.cpp's anonymous namespace, after its helpers.
// Add these SDK headers at FILE scope (not inside the anonymous namespace):
// AcPlPlotFactory.h, AcPlPlotEngine.h, AcPlPlotInfo.h, AcPlPlotInfoValidator.h,
// dbplotsettings.h, dbplotsetval.h, dblayout.h.
// AutoCAD 2020: the AcPl symbols are already supplied by linked accore.lib.
// This probe uses an unregistered temporary settings override. It does not save
// the DWG or change its layout/PC3/PMP. The explicitly authorized foreground
// publishing scope temporarily sets BACKGROUNDPLOT=0 and verifies its restoration.

const char* plotBool(bool value) { return value ? "true" : "false"; }
std::string plotError(const std::exception& failure) {
    const std::string message(failure.what());
    return quote(std::wstring(message.begin(), message.end()));
}
std::string plotText(const ACHAR* value) { return value ? quote(value) : "null"; }

struct NativePlotStatus {
    bool called{};
    int value{};
    void set(Acad::ErrorStatus status) { called = true; value = static_cast<int>(status); }
    std::string json() const { return called ? std::to_string(value) : "null"; }
    bool ok() const { return called && value == Acad::eOk; }
};

std::string plotSettingsJson(const AcDbPlotSettings& settings) {
    const ACHAR* device{}; const auto deviceStatus = settings.getPlotCfgName(device);
    // The borrowed media-name pointer may be invalidated by later getter calls.
    AcString media; const auto mediaStatus = settings.getCanonicalMediaName(media);
    const ACHAR* sheet{}; const auto sheetStatus = settings.getCurrentStyleSheet(sheet);
    const std::string sheetText = sheetStatus == Acad::eOk ? plotText(sheet) : "null";
    double width{}, height{}, left{}, bottom{}, right{}, top{}, xmin{}, ymin{}, xmax{}, ymax{}, numerator{}, denominator{}, ox{}, oy{};
    const auto sizeStatus = settings.getPlotPaperSize(width, height);
    const auto marginsStatus = settings.getPlotPaperMargins(left, bottom, right, top);
    const auto windowStatus = settings.getPlotWindowArea(xmin, ymin, xmax, ymax);
    const auto scaleStatus = settings.getCustomPrintScale(numerator, denominator);
    const auto originStatus = settings.getPlotOrigin(ox, oy);
    std::ostringstream out;
    out << "{\"deviceStatus\":" << deviceStatus << ",\"device\":" << (deviceStatus == Acad::eOk ? plotText(device) : "null")
        << ",\"mediaStatus\":" << mediaStatus << ",\"canonicalMedia\":" << (mediaStatus == Acad::eOk ? quote(media.kACharPtr()) : "null")
        << ",\"paperSizeStatus\":" << sizeStatus << ",\"paperMM\":" << (sizeStatus == Acad::eOk ? "[" + number(width) + "," + number(height) + "]" : "null")
        << ",\"marginsStatus\":" << marginsStatus << ",\"marginsMM\":" << (marginsStatus == Acad::eOk ? "[" + number(left) + "," + number(bottom) + "," + number(right) + "," + number(top) + "]" : "null")
        << ",\"paperUnits\":" << settings.plotPaperUnits() << ",\"rotation\":" << settings.plotRotation()
        << ",\"plotType\":" << settings.plotType() << ",\"centered\":" << plotBool(settings.plotCentered())
        << ",\"useStandardScale\":" << plotBool(settings.useStandardScale()) << ",\"stdScaleType\":" << settings.stdScaleType()
        << ",\"scaleStatus\":" << scaleStatus << ",\"scale\":" << (scaleStatus == Acad::eOk ? "[" + number(numerator) + "," + number(denominator) + "]" : "null")
        << ",\"windowStatus\":" << windowStatus << ",\"windowDCS\":" << (windowStatus == Acad::eOk ? "[" + number(xmin) + "," + number(ymin) + "," + number(xmax) + "," + number(ymax) + "]" : "null")
        << ",\"originStatus\":" << originStatus << ",\"originMM\":" << (originStatus == Acad::eOk ? "[" + number(ox) + "," + number(oy) + "]" : "null")
        << ",\"styleSheetStatus\":" << sheetStatus << ",\"styleSheet\":" << sheetText
        << ",\"plotPlotStyles\":" << plotBool(settings.plotPlotStyles()) << ",\"printLineweights\":" << plotBool(settings.printLineweights())
        << ",\"scaleLineweights\":" << plotBool(settings.scaleLineweights()) << ",\"modelType\":" << plotBool(settings.modelType()) << '}';
    return out.str();
}

std::string plotGlobalsJson() {
    const wchar_t* names[]{ L"BACKGROUNDPLOT", L"SECURELOAD", L"PICKFIRST", L"FILEDIA", L"TILEMODE" };
    std::ostringstream out; out << '{';
    for (int i = 0; i < 5; ++i) {
        if (i) out << ',';
        resbuf result{}; const int status = acedGetVar(names[i], &result);
        out << quote(names[i]) << ":{\"getStatus\":" << status << ",\"restype\":" << result.restype << ",\"value\":";
        if (status == RTNORM && result.restype == RTSHORT) out << result.resval.rint;
        else if (status == RTNORM && result.restype == RTLONG) out << result.resval.rlong;
        else out << "null";
        out << '}';
    }
    out << '}'; return out.str();
}

class NativePlotBackgroundScope {
public:
    NativePlotBackgroundScope() = default;
    NativePlotBackgroundScope(const NativePlotBackgroundScope&) = delete;
    NativePlotBackgroundScope& operator=(const NativePlotBackgroundScope&) = delete;
    ~NativePlotBackgroundScope() noexcept { restore(); }
    void activate() {
        processState_ = static_cast<int>(acplProcessPlotState());
        if (processState_ != kNotPlotting) throw std::runtime_error("foreground plot override refused: existing plot job is busy; no setting changed or job cancelled");
        resbuf old{}; getStatus_ = acedGetVar(L"BACKGROUNDPLOT", &old); getReturned_ = true; oldType_ = old.restype;
        if (getStatus_ != RTNORM || oldType_ != RTSHORT) throw std::runtime_error("cannot capture original BACKGROUNDPLOT; no override applied");
        oldValue_ = old.resval.rint; oldKnown_ = true;
        // Check immediately before the sole temporary setting mutation as well.
        processState_ = static_cast<int>(acplProcessPlotState());
        if (processState_ != kNotPlotting) throw std::runtime_error("foreground plot override refused: plot system became busy; no setting changed");
        resbuf foreground{}; foreground.restype = RTSHORT; foreground.resval.rint = 0;
        setAttempted_ = true; // An exceptional/partial setter still requires restoration.
        setStatus_ = acedSetVar(L"BACKGROUNDPLOT", &foreground); setReturned_ = true;
        if (setStatus_ != RTNORM) throw std::runtime_error("temporary BACKGROUNDPLOT setter failed");
        resbuf actual{}; readStatus_ = acedGetVar(L"BACKGROUNDPLOT", &actual); readReturned_ = true; newType_ = actual.restype;
        if (readStatus_ == RTNORM && newType_ == RTSHORT) { newValue_ = actual.resval.rint; newKnown_ = true; }
        if (!newKnown_ || newValue_ != 0) throw std::runtime_error("temporary foreground publishing mode did not read back as zero");
    }
    void restore() noexcept {
        if (!setAttempted_ || !oldKnown_ || restoreAttempted_) return;
        restoreAttempted_ = true;
        try {
            resbuf original{}; original.restype = RTSHORT; original.resval.rint = static_cast<short>(oldValue_);
            restoreStatus_ = acedSetVar(L"BACKGROUNDPLOT", &original); restoreReturned_ = true;
            resbuf actual{}; restoreReadStatus_ = acedGetVar(L"BACKGROUNDPLOT", &actual); restoreReadReturned_ = true; restoredType_ = actual.restype;
            if (restoreReadStatus_ == RTNORM && restoredType_ == RTSHORT) { restoredValue_ = actual.resval.rint; restoredKnown_ = true; }
            restored_ = restoreStatus_ == RTNORM && restoredKnown_ && restoredValue_ == oldValue_;
        } catch (...) { restoreThrew_ = true; }
    }
    bool restorationFailed() const { return setAttempted_ && !restored_; }
    std::string json() const {
        const auto actual = [](bool returned, int value) { return returned ? std::to_string(value) : std::string("null"); };
        std::ostringstream out;
        out << "{\"processPlotStateBefore\":" << actual(processState_ >= 0, processState_) << ",\"getStatus\":" << actual(getReturned_, getStatus_)
            << ",\"oldRestype\":" << actual(getReturned_, oldType_) << ",\"oldValue\":" << actual(oldKnown_, oldValue_)
            << ",\"requestedValue\":0,\"setAttempted\":" << plotBool(setAttempted_) << ",\"setStatus\":" << actual(setReturned_, setStatus_)
            << ",\"readbackStatus\":" << actual(readReturned_, readStatus_) << ",\"newRestype\":" << actual(readReturned_, newType_) << ",\"newValue\":" << actual(newKnown_, newValue_)
            << ",\"restoreAttempted\":" << plotBool(restoreAttempted_) << ",\"restoreStatus\":" << actual(restoreReturned_, restoreStatus_)
            << ",\"restoreReadbackStatus\":" << actual(restoreReadReturned_, restoreReadStatus_) << ",\"restoredRestype\":" << actual(restoreReadReturned_, restoredType_)
            << ",\"restoredValue\":" << actual(restoredKnown_, restoredValue_) << ",\"restored\":" << plotBool(restored_) << ",\"restoreThrew\":" << plotBool(restoreThrew_) << '}';
        return out.str();
    }
private:
    int processState_ = -1, getStatus_{}, oldType_{}, oldValue_{}, setStatus_{}, readStatus_{}, newType_{}, newValue_{};
    int restoreStatus_{}, restoreReadStatus_{}, restoredType_{}, restoredValue_{};
    bool getReturned_{}, oldKnown_{}, setAttempted_{}, setReturned_{}, readReturned_{}, newKnown_{};
    bool restoreAttempted_{}, restoreReturned_{}, restoreReadReturned_{}, restoredKnown_{}, restored_{}, restoreThrew_{};
};

std::string plotGeometryJson(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) {
        if (i) out << ',';
        auto entity = open<AcDbEntity>(ids[i]); AcString layer; check(entity->layer(layer), "plot geometry layer");
        AcDbExtents bounds; const auto extentStatus = entity->getGeomExtents(bounds);
        AcGePoint3dArray grips, stretch; AcDbIntArray snaps, geometry;
        const auto gripStatus = entity->getGripPoints(grips, snaps, geometry);
        const auto stretchStatus = entity->getStretchPoints(stretch);
        out << "{\"handle\":" << quote(handleOf(ids[i])) << ",\"type\":" << quote(entity->isA()->name())
            << ",\"layer\":" << quote(layer.kACharPtr()) << ",\"proxy\":" << plotBool(entity->isAProxy())
            << ",\"extentStatus\":" << extentStatus << ",\"min\":" << (extentStatus == Acad::eOk ? point(bounds.minPoint()) : "null")
            << ",\"max\":" << (extentStatus == Acad::eOk ? point(bounds.maxPoint()) : "null") << ",\"curve\":" << curveJson(*entity)
            << ",\"grip\":{\"status\":" << gripStatus << ",\"points\":" << points(grips) << ",\"osnapModes\":" << integers(snaps)
            << ",\"geomIds\":" << integers(geometry) << "},\"stretch\":{\"status\":" << stretchStatus << ",\"points\":" << points(stretch) << "}}";
    }
    out << ']'; return out.str();
}

struct NativePlotContext {
    Paths paths;
    std::wstring control;
    AcApDocument* document{};
    AcDbDatabase* database{};
    AcDbObjectId layout;
    double window[4]{};
    std::string windowDCS{"null"}, windowTransforms{"[]"}, viewEvidence{"null"};
    explicit NativePlotContext(const Paths& p) : paths(p), control(fullPath(p.output + L"\\control.dwg")) {}
    void guard() const {
        if (!acDocManager || acDocManager->isApplicationContext() || !document || !database
            || acDocManager->curDocument() != document || acDocManager->mdiActiveDocument() != document
            || document->database() != database || acdbHostApplicationServices()->workingDatabase() != database
            || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("native plot requires owned control document/current/MDI/working database");
        const ACHAR* name{}; check(database->getFilename(name), "native plot database filename");
        if (!name || _wcsicmp(fullPath(name).c_str(), control.c_str()) || !below(control, paths.output))
            throw std::runtime_error("native plot database path guard failed");
        if (!database->tilemode()) throw std::runtime_error("native plot requires the existing current Model layout; no layout switch allowed");
    }
    void prepare() {
        if (!acDocManager) throw std::runtime_error("native plot document manager unavailable");
        document = acDocManager->curDocument(); database = document ? document->database() : nullptr; guard();
        { auto model = open<AcDbBlockTableRecord>(modelId(*database)); layout = model->getLayoutId(); }
        if (layout.isNull()) throw std::runtime_error("native plot Model layout not found");
        { auto object = open<AcDbLayout>(layout); if (!object->modelType()) throw std::runtime_error("native plot requires Model layout"); }
        resbuf direction{}, twist{}, perspective{};
        const int directionStatus = acedGetVar(L"VIEWDIR", &direction), twistStatus = acedGetVar(L"VIEWTWIST", &twist), perspectiveStatus = acedGetVar(L"PERSPECTIVE", &perspective);
        const bool directionValid = directionStatus == RTNORM && direction.restype == RT3DPOINT;
        const bool twistValid = twistStatus == RTNORM && twist.restype == RTREAL;
        const bool perspectiveValid = perspectiveStatus == RTNORM && perspective.restype == RTSHORT;
        viewEvidence = "{\"directionStatus\":" + std::to_string(directionStatus) + ",\"direction\":"
            + (directionValid ? point(AcGePoint3d(direction.resval.rpoint[0], direction.resval.rpoint[1], direction.resval.rpoint[2])) : "null") + ",\"twistStatus\":" + std::to_string(twistStatus)
            + ",\"twist\":" + (twistValid ? number(twist.resval.rreal) : "null") + ",\"perspectiveStatus\":" + std::to_string(perspectiveStatus)
            + ",\"perspective\":" + (perspectiveValid ? std::to_string(perspective.resval.rint) : "null") + '}';
        if (!directionValid || !twistValid || !perspectiveValid || perspective.resval.rint != 0 || std::fabs(twist.resval.rreal) > 1e-9
            || std::fabs(direction.resval.rpoint[0]) > 1e-9 || std::fabs(direction.resval.rpoint[1]) > 1e-9 || direction.resval.rpoint[2] <= 0)
            throw std::runtime_error("native plot fixture requires an existing untwisted orthographic top view; view not changed");
        const double corners[4][3]{ {12000,11000,0}, {36000,11000,0}, {36000,35000,0}, {12000,35000,0} };
        resbuf from{}, to{}; from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 2;
        window[0] = window[1] = std::numeric_limits<double>::infinity(); window[2] = window[3] = -window[0];
        std::ostringstream statuses; statuses << '[';
        for (int i = 0; i < 4; ++i) {
            ads_point projected{}; const int status = acedTrans(corners[i], &from, &to, 0, projected);
            if (i) statuses << ','; statuses << status;
            // Preserve the actual partial status sequence if a conversion fails.
            windowTransforms = statuses.str() + ']';
            if (status != RTNORM) throw std::runtime_error("native plot WCS-to-DCS window conversion failed");
            window[0] = (std::min)(window[0], projected[0]); window[1] = (std::min)(window[1], projected[1]);
            window[2] = (std::max)(window[2], projected[0]); window[3] = (std::max)(window[3], projected[1]);
        }
        statuses << ']'; windowTransforms = statuses.str();
        windowDCS = "[" + number(window[0]) + "," + number(window[1]) + "," + number(window[2]) + "," + number(window[3]) + "]";
        if (std::fabs(window[2]-window[0]-24000) > 1e-6 || std::fabs(window[3]-window[1]-24000) > 1e-6)
            throw std::runtime_error("native plot DCS window does not preserve the exact plan fixture dimensions");
    }
};

class NativePlotPipeline {
public:
    enum Stage { Create, BeginPlot, BeginDocument, BeginPage, BeginGraphics, EndGraphics, EndPage, EndDocument, EndPlot, Count };
    NativePlotStatus statuses[Count];
    bool destroyed{}, cleanupThrew{};
    NativePlotPipeline() = default;
    NativePlotPipeline(const NativePlotPipeline&) = delete;
    NativePlotPipeline& operator=(const NativePlotPipeline&) = delete;
    ~NativePlotPipeline() { finish(); }
    void begin(AcPlPlotInfo& info, AcPlPlotPageInfo& page, const std::wstring& document, const std::wstring& output) {
        const auto factoryStatus = AcPlPlotFactory::createPublishEngine(engine_); statuses[Create].set(factoryStatus);
        check(factoryStatus, "create native publish engine");
        if (!engine_) throw std::runtime_error("native publish engine returned null");
        auto status = engine_->beginPlot(nullptr); statuses[BeginPlot].set(status); plot_ = status == Acad::eOk; check(status, "native beginPlot");
        status = engine_->beginDocument(info, document.c_str(), nullptr, 1, true, output.c_str()); statuses[BeginDocument].set(status); document_ = status == Acad::eOk; check(status, "native beginDocument");
        status = engine_->beginPage(page, info, true); statuses[BeginPage].set(status); page_ = status == Acad::eOk; check(status, "native beginPage");
        status = engine_->beginGenerateGraphics(); statuses[BeginGraphics].set(status); graphics_ = status == Acad::eOk; check(status, "native beginGenerateGraphics");
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

std::string nativePlotPage(NativePlotContext& context, const wchar_t* id, double requestedWidth, double requestedHeight) {
    const auto output = fullPath(context.paths.output + L"\\" + id + L".pdf");
    NativePlotStatus copyStatus, devicesStatus, validateStatus;
    std::map<std::string, NativePlotStatus> configuration;
    std::string deviceList{"[]"}, candidates{"[]"}, settingsJson{"null"}, validatedJson{"null"}, error{"null"}, selectedMedia{"null"};
    int stateBefore = -1, stateAfter = -1, matchingPolicy = -1; bool validated{}, fileObserved{}; unsigned long long fileBytes{};
    // Settings and plot info outlive all begin/endDocument calls and error cleanup.
    AcDbPlotSettings settings(true); AcPlPlotInfo info; AcPlPlotPageInfo page; NativePlotPipeline pipeline;
    const auto configure = [&configuration](const char* key, Acad::ErrorStatus status) { configuration[key].set(status); check(status, key); };
    try {
        context.guard(); if (!below(output, context.paths.output)) throw std::runtime_error("native PDF output guard failed"); requireNewFile(output);
        stateBefore = static_cast<int>(acplProcessPlotState());
        if (stateBefore != kNotPlotting) throw std::runtime_error("native plot subsystem busy; existing plot was not cancelled");
        auto* validator = acdbHostApplicationServices()->plotSettingsValidator(); if (!validator) throw std::runtime_error("native plot settings validator unavailable");
        { auto layout = open<AcDbLayout>(context.layout); const auto status = settings.copyFrom(static_cast<const AcRxObject*>(layout.get())); copyStatus.set(status); check(status, "copy Model settings into temporary override"); }
        validator->refreshLists(&settings);
        AcArray<const ACHAR*> devices; const auto listStatus = validator->plotDeviceList(devices); devicesStatus.set(listStatus); check(listStatus, "native plot device enumeration");
        std::ostringstream names; names << '['; bool pdfAvailable{};
        for (int i = 0; i < devices.length(); ++i) { if (i) names << ','; names << plotText(devices[i]); if (devices[i] && !_wcsicmp(devices[i], L"DWG To PDF.pc3")) pdfAvailable = true; }
        names << ']'; deviceList = names.str();
        if (!pdfAvailable) throw std::runtime_error("enumerated devices do not contain DWG To PDF.pc3");
        configure("setDevice", validator->setPlotCfgName(&settings, L"DWG To PDF.pc3")); validator->refreshLists(&settings);
        configure("setPaperUnits", validator->setPlotPaperUnits(&settings, AcDbPlotSettings::kMillimeters));
        AcArray<const ACHAR*> media; check(validator->canonicalMediaNameList(&settings, media), "native canonical media enumeration");
        // Copy borrowed media strings before any setter can refresh internal lists.
        std::vector<std::wstring> mediaNames; for (int i = 0; i < media.length(); ++i) if (media[i]) mediaNames.emplace_back(media[i]);
        std::wstring chosen; double mediaWidth{}, mediaHeight{}; std::ostringstream mediaEvidence; mediaEvidence << '[';
        for (size_t i = 0; i < mediaNames.size(); ++i) {
            if (i) mediaEvidence << ',';
            const auto setStatus = validator->setCanonicalMediaName(&settings, mediaNames[i].c_str());
            double width{}, height{}; NativePlotStatus sizeStatus;
            if (setStatus == Acad::eOk) sizeStatus.set(settings.getPlotPaperSize(width, height));
            mediaEvidence << "{\"name\":" << quote(mediaNames[i]) << ",\"setStatus\":" << setStatus << ",\"paperSizeStatus\":" << sizeStatus.json()
                << ",\"paperMM\":" << (sizeStatus.ok() ? "[" + number(width) + "," + number(height) + "]" : "null") << '}';
            const bool exact = sizeStatus.ok() && ((std::fabs(width-requestedWidth) < 0.1 && std::fabs(height-requestedHeight) < 0.1)
                || (std::fabs(width-requestedHeight) < 0.1 && std::fabs(height-requestedWidth) < 0.1));
            if (chosen.empty() && exact) { chosen = mediaNames[i]; mediaWidth = width; mediaHeight = height; }
        }
        mediaEvidence << ']'; candidates = mediaEvidence.str();
        if (chosen.empty()) throw std::runtime_error("no exact ISO paper media in the enumerated PDF driver; no custom media created");
        selectedMedia = quote(chosen); configure("setMedia", validator->setCanonicalMediaName(&settings, chosen.c_str()));
        configure("setPaperUnits", validator->setPlotPaperUnits(&settings, AcDbPlotSettings::kMillimeters));
        configure("setRotation", validator->setPlotRotation(&settings, mediaWidth >= mediaHeight ? AcDbPlotSettings::k0degrees : AcDbPlotSettings::k90degrees));
        // Seed valid bounds before selecting kWindow: this fixture's original
        // Model settings can have a degenerate [0,0,0,0] last-window rectangle.
        configure("setWindow", validator->setPlotWindowArea(&settings, context.window[0], context.window[1], context.window[2], context.window[3]));
        configure("setPlotType", validator->setPlotType(&settings, AcDbPlotSettings::kWindow));
        configure("setStandardScale", validator->setUseStandardScale(&settings, false));
        configure("setScale", validator->setCustomPrintScale(&settings, 1.0, 100.0));
        configure("setCentered", validator->setPlotCentered(&settings, true)); settingsJson = plotSettingsJson(settings);
        info.setLayout(context.layout); info.setOverrideSettings(&settings);
        // Enabled searches existing media; only Custom/TmpCustom policies can
        // create a paper definition. Actual validated sizes/scales remain guarded.
        AcPlPlotInfoValidator infoValidator; infoValidator.setMediaMatchingPolicy(AcPlPlotInfoValidator::kMatchEnabled);
        matchingPolicy = static_cast<int>(infoValidator.matchingPolicy());
        const auto status = infoValidator.validate(info); validateStatus.set(status); validated = info.isValidated(); check(status, "validate native PDF plot info");
        if (!validated || !info.validatedSettings()) throw std::runtime_error("native PDF validation produced no actual settings");
        const auto* actual = info.validatedSettings(); validatedJson = plotSettingsJson(*actual);
        double numerator{}, denominator{}, actualWidth{}, actualHeight{}, left{}, bottom{}, right{}, top{};
        check(actual->getCustomPrintScale(numerator, denominator), "read validated native plot scale");
        check(actual->getPlotPaperSize(actualWidth, actualHeight), "read validated native PDF paper");
        check(actual->getPlotPaperMargins(left, bottom, right, top), "read validated native PDF margins");
        if (actual->useStandardScale() || std::fabs(numerator-1.0) > 1e-9 || std::fabs(denominator-100.0) > 1e-9
            || actual->plotPaperUnits() != AcDbPlotSettings::kMillimeters || actual->plotType() != AcDbPlotSettings::kWindow)
            throw std::runtime_error("validated PDF settings do not retain strict 1:100 millimetre window plotting");
        if (!((std::fabs(actualWidth-requestedWidth) < 0.1 && std::fabs(actualHeight-requestedHeight) < 0.1)
            || (std::fabs(actualWidth-requestedHeight) < 0.1 && std::fabs(actualHeight-requestedWidth) < 0.1)))
            throw std::runtime_error("validated native PDF media differs from the requested ISO sheet");
        double printableWidth = actualWidth-left-right, printableHeight = actualHeight-bottom-top;
        if (actual->plotRotation() == AcDbPlotSettings::k90degrees) std::swap(printableWidth, printableHeight);
        if (printableWidth < 240.0-0.1 || printableHeight < 240.0-0.1) throw std::runtime_error("exact-scale fixture window exceeds printable paper; Fit was not enabled");
        context.guard(); pipeline.begin(info, page, context.control, output);
    } catch (const std::exception& failure) { error = plotError(failure); }
      catch (...) { error = quote(L"unknown native plot exception"); }
    pipeline.finish();
    if (error == "null" && !pipeline.complete()) error = quote(L"native plot end/destroy pipeline did not complete successfully");
    stateAfter = static_cast<int>(acplProcessPlotState());
    if (error == "null" && stateAfter != kNotPlotting) error = quote(L"native plot subsystem did not return to idle");
    WIN32_FILE_ATTRIBUTE_DATA file{};
    if (GetFileAttributesExW(output.c_str(), GetFileExInfoStandard, &file) && !(file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        fileObserved = true; fileBytes = (static_cast<unsigned long long>(file.nFileSizeHigh) << 32) | file.nFileSizeLow;
    }
    if (error == "null" && (!fileObserved || fileBytes == 0)) error = quote(L"native plot completed without an observable nonempty PDF file");
    std::ostringstream config; config << '{'; int count{};
    for (const auto& item : configuration) { if (count++) config << ','; config << '"' << item.first << "\":" << item.second.json(); } config << '}';
    std::ostringstream out;
    out << "{\"id\":" << quote(id) << ",\"requestedPaperMM\":[" << number(requestedWidth) << ',' << number(requestedHeight) << "]"
        << ",\"outputPDF\":" << quote(output) << ",\"device\":\"DWG To PDF.pc3\",\"devices\":" << deviceList << ",\"deviceListStatus\":" << devicesStatus.json()
        << ",\"mediaCandidates\":" << candidates << ",\"selectedMedia\":" << selectedMedia << ",\"copySettingsStatus\":" << copyStatus.json()
        << ",\"configurationStatuses\":" << config.str() << ",\"settings\":" << settingsJson << ",\"matchingPolicy\":" << (matchingPolicy < 0 ? "null" : std::to_string(matchingPolicy))
        << ",\"validateStatus\":" << validateStatus.json()
        << ",\"isValidated\":" << plotBool(validated) << ",\"validatedSettings\":" << validatedJson
        << ",\"processPlotStateBefore\":" << stateBefore << ",\"processPlotStateAfter\":" << stateAfter
        << ",\"engineStatuses\":" << pipeline.json() << ",\"engineDestroyed\":" << plotBool(pipeline.destroyed)
        << ",\"cleanupThrew\":" << plotBool(pipeline.cleanupThrew) << ",\"pipelineComplete\":" << plotBool(pipeline.complete())
        << ",\"fileObserved\":" << plotBool(fileObserved) << ",\"fileBytes\":" << fileBytes << ",\"error\":" << error << '}';
    return out.str();
}

void nativePlotProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\native-plot.json"); requireNewFile(report);
        NativePlotContext context(p); NativePlotBackgroundScope background;
        std::string error{"null"}, before{"[]"}, after{"[]"}, globalsBefore{"null"}, globalsAfter{"null"}, layoutBefore{"null"}, layoutAfter{"null"};
        std::string current{"null"}, active{"null"}, working{"null"}, cases;
        int initialCount = -1, finalCount = -1; bool appContext = true, modelCurrent{};
        try {
            context.prepare(); appContext = acDocManager->isApplicationContext(); current = quote(fullPath(context.document->fileName()));
            active = quote(fullPath(acDocManager->mdiActiveDocument()->fileName()));
            const ACHAR* dbName{}; check(context.database->getFilename(dbName), "native plot working filename"); working = dbName ? quote(fullPath(dbName)) : "null";
            modelCurrent = context.database->tilemode(); const auto ids = modelIds(*context.database); initialCount = ids.length();
            before = plotGeometryJson(ids); globalsBefore = plotGlobalsJson();
            { auto layout = open<AcDbLayout>(context.layout); layoutBefore = plotSettingsJson(*layout); }
            if (initialCount != 213) throw std::runtime_error("native PDF requires the full frozen scheme213 control fixture");
            background.activate();
            const wchar_t* names[]{ L"A3-1-100", L"A2-1-100", L"A1-1-100" }; const double sizes[3][2]{ {420,297}, {594,420}, {841,594} };
            for (int i = 0; i < 3; ++i) { if (i) cases += ','; cases += nativePlotPage(context, names[i], sizes[i][0], sizes[i][1]); }
            context.guard(); const auto afterIds = modelIds(*context.database); finalCount = afterIds.length(); after = plotGeometryJson(afterIds);
            { auto layout = open<AcDbLayout>(context.layout); layoutAfter = plotSettingsJson(*layout); }
        } catch (const std::exception& failure) { error = plotError(failure); }
          catch (...) { error = quote(L"unknown native plot batch exception"); }
        // Restore before capturing final globals/report, on both success and failure.
        // The destructor also covers an exception while assembling the report.
        background.restore();
        if (background.restorationFailed()) error = quote(L"BACKGROUNDPLOT restoration failed; inspect actual restore statuses and current value");
        globalsAfter = plotGlobalsJson();
        std::ostringstream out;
        out << "{\"contract\":\"stage1.native-plot.v1\",\"api\":\"ObjectARX native plot; no COM\",\"controlDWG\":" << quote(context.control)
            << ",\"currentDWG\":" << current << ",\"mdiActiveDWG\":" << active << ",\"workingDWG\":" << working
            << ",\"isApplicationContext\":" << plotBool(appContext) << ",\"modelLayoutCurrent\":" << plotBool(modelCurrent)
            << ",\"initialModelCount\":" << initialCount << ",\"finalModelCount\":" << finalCount
            << ",\"globalsBefore\":" << globalsBefore << ",\"globalsAfter\":" << globalsAfter << ",\"backgroundOverride\":" << background.json()
            << ",\"layoutBefore\":" << layoutBefore << ",\"layoutAfter\":" << layoutAfter
            << ",\"windowWCS\":[12000,11000,36000,35000],\"windowDCS\":" << context.windowDCS << ",\"windowTransformStatuses\":" << context.windowTransforms
            << ",\"viewCheck\":" << context.viewEvidence << ",\"fullBuildingCoverage\":false,\"originalBefore\":" << before << ",\"originalAfter\":" << after
            << ",\"cases\":[" << cases << "],\"multiPage\":{\"status\":\"not attempted\",\"reason\":\"this batch validates independent single-page ISO sizes; mixed-media job compatibility is not established\"}"
            << ",\"visualReview\":\"pending: PDF page size, clipping, fonts, lineweights and Tianzheng graphics\",\"error\":" << error << '}';
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_NATIVE_PLOT_COMPLETED; ASSERTIONS_AND_PDF_REVIEW_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_NATIVE_PLOT_FAILED: %hs\n", failure.what()); }
}
