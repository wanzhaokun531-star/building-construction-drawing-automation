// Minimal stage1 public-SDK probes; no TCH commands, private filing edits or business UI.
class PureCommandTrace final : public AcEditorReactor {
    bool attached_{};
public:
    std::vector<std::wstring> commands;
    bool lost{};
    PureCommandTrace(){if(!acedEditor)throw std::runtime_error("native editor unavailable");acedEditor->addReactor(this);attached_=true;}
    PureCommandTrace(const PureCommandTrace&)=delete;
    PureCommandTrace& operator=(const PureCommandTrace&)=delete;
    ~PureCommandTrace() override {if(attached_)acedEditor->removeReactor(this);}
    void commandWillStart(const ACHAR* name) override {try{commands.emplace_back(name?name:L"");}catch(...){lost=true;}}
    std::string json()const{
        std::vector<std::string> values;for(const auto& value:commands)values.push_back(quote(value));
        return "{\"lost\":"+std::string(plotBool(lost))+",\"commands\":"+axisTraceArray(values)+"}";
    }
};

void nativeSemanticCatalog(bool reference){
    const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;
    const auto file=p.output+L"\\native-semantic-catalog.json";requireNewFile(file);
    std::string error="null",before="[]",after="[]",logging="null",log="null";std::vector<std::string> samples;
    const int expected=reference?618:213;int initial=-1,final=-1;
    AcApDocument* doc{};AcDbDatabase* db{};AcDbDatabase* inspected{};std::unique_ptr<AcDbDatabase> side;
    std::unique_ptr<NativeBusinessLoggingScope> logger;
    const auto guard=[&]{fourAxisGuard(p,doc,db);};
    try{
        doc=acDocManager?acDocManager->curDocument():nullptr;db=doc?doc->database():nullptr;guard();
        if(modelCount(*db)!=213)throw std::runtime_error("catalog requires an owned scheme control document");
        if(reference)side=lifecycleRead(p.reference);inspected=reference?side.get():db;
        const auto ids=modelIds(*inspected);initial=ids.length();if(initial!=expected)throw std::runtime_error("catalog input count differs");
        {WorkingDatabaseScope work(*inspected);before=businessGeometryJson(ids);}
        logger=std::make_unique<NativeBusinessLoggingScope>(p,doc,db);logger->activate();
        for(int i=0;i<ids.length();++i){
            guard();WorkingDatabaseScope work(*inspected);auto entity=open<AcDbEntity>(ids[i]);const std::wstring type=entity->isA()->name();
            if(type.rfind(L"TDb",0)!=0)continue;
            const auto handle=handleOf(ids[i]);entity.reset();bool returned{};
            const auto capture=businessListSample(*inspected,handle.c_str(),type.c_str(),returned);
            samples.push_back("{\"handle\":"+quote(handle)+",\"actualType\":"+quote(type)+",\"returned\":"+plotBool(returned)+",\"error\":"+(returned?std::string("null"):quote(L"native list invocation did not return"))+",\"capture\":"+capture+"}");
        }
        guard();{WorkingDatabaseScope work(*inspected);final=modelCount(*inspected);after=businessGeometryJson(ids);}
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown native catalog failure");}
    if(logger){logger->restore();logging=logger->json();if(!logger->restored()||!logger->closeSucceeded())error=quote(L"catalog logging restoration failed");const auto observed=businessLogFile(logger->logPath());log=observed.json;if(!observed.read)error=quote(L"catalog log not readable");}
    writeNewReport(file,"{\"contract\":\"stage1.native-semantic-catalog.v1\",\"role\":"+quote(reference?L"reference":L"scheme")+",\"inspectedPath\":"+quote(reference?p.reference:p.scheme)+",\"inspectedFromSideDatabase\":"+plotBool(reference)+",\"expectedCount\":"+std::to_string(expected)+",\"initialModelCount\":"+std::to_string(initial)+",\"finalModelCount\":"+std::to_string(final)+",\"originalBefore\":"+before+",\"originalAfter\":"+after+",\"samples\":"+axisTraceArray(samples)+",\"logging\":"+logging+",\"logFile\":"+log+",\"commandTrace\":"+trace.json()+",\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"drawingSaved\":false,\"error\":"+error+"}");
}
void pureEntry(void(*operation)())noexcept{stage1::commandBoundary(operation,[](const char* error){acutPrintf(L"\nSTAGE1_PURE_NATIVE_ENTRY_REJECTED: %hs\n",error);});}
void nativeSchemeCatalog()noexcept{pureEntry([]{nativeSemanticCatalog(false);});}
void nativeReferenceCatalog()noexcept{pureEntry([]{nativeSemanticCatalog(true);});}

std::string pureFontLookup(AcDbDatabase& db,const std::wstring& requested){
    std::vector<std::string> attempts;std::wstring selected;int selectedStatus=-9999;
    for(const auto hint:{AcDbHostApplicationServices::kFontFile,AcDbHostApplicationServices::kCompiledShapeFile,AcDbHostApplicationServices::kTrueTypeFontFile}){
        AcString resolved;const auto status=acdbHostApplicationServices()->findFile(resolved,requested.c_str(),&db,hint);
        const auto attrs=status==Acad::eOk?GetFileAttributesW(resolved.kACharPtr()):INVALID_FILE_ATTRIBUTES;
        const bool exists=attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY);
        attempts.push_back("{\"hint\":"+std::to_string(hint)+",\"status\":"+std::to_string(status)+",\"path\":"+(status==Acad::eOk?quote(resolved.kACharPtr()):std::string("null"))+",\"fileExists\":"+plotBool(exists)+"}");
        if(selected.empty()&&exists){selected=resolved.kACharPtr();selectedStatus=status;}
    }
    std::wstring resolution=L"AutoCAD findFile";
    const auto suffix=requested.find_last_of(L'.');const auto ext=suffix==std::wstring::npos?std::wstring():requested.substr(suffix);
    if(selected.empty()&&requested.find_first_of(L"\\/")==std::wstring::npos&&(!_wcsicmp(ext.c_str(),L".ttf")||!_wcsicmp(ext.c_str(),L".ttc")||!_wcsicmp(ext.c_str(),L".otf"))){
        wchar_t windows[32768]{};const auto n=GetWindowsDirectoryW(windows,32768);
        if(n&&n<32768){const auto candidate=std::wstring(windows)+L"\\Fonts\\"+requested;const auto attrs=GetFileAttributesW(candidate.c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY)){selected=candidate;selectedStatus=0;resolution=L"Windows Fonts literal filename; not AutoCAD findFile success";}}
    }
    std::string identity="null";
    if(!selected.empty()){
        LifecycleFile handle(CreateFileW(selected.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));BY_HANDLE_FILE_INFORMATION info{};
        if(handle.value!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(handle.value,&info))identity="{\"volume\":"+std::to_string(info.dwVolumeSerialNumber)+",\"indexHigh\":"+std::to_string(info.nFileIndexHigh)+",\"indexLow\":"+std::to_string(info.nFileIndexLow)+",\"bytes\":"+std::to_string((static_cast<unsigned long long>(info.nFileSizeHigh)<<32)|info.nFileSizeLow)+",\"writeHigh\":"+std::to_string(info.ftLastWriteTime.dwHighDateTime)+",\"writeLow\":"+std::to_string(info.ftLastWriteTime.dwLowDateTime)+"}";
    }
    return "{\"requested\":"+quote(requested)+",\"status\":"+std::to_string(selectedStatus)+",\"path\":"+(selected.empty()?std::string("null"):quote(selected))+",\"fileExists\":"+plotBool(!selected.empty())+",\"resolutionApi\":"+quote(resolution)+",\"fileIdentity\":"+identity+",\"attempts\":"+axisTraceArray(attempts)+"}";
}
void nativeActualFontsImpl(){
    const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;std::vector<std::string> sources;std::string error="null";
    try{
        for(const auto& source:{p.scheme,p.reference}){
            auto db=lifecycleRead(source);WorkingDatabaseScope work(*db);auto table=open<AcDbTextStyleTable>(db->textStyleTableId());
            AcDbTextStyleTableIterator* raw{};check(table->newIterator(raw),"actual font style iterator");std::unique_ptr<AcDbTextStyleTableIterator> it(raw);std::vector<std::string> styles;
            for(;!it->done();it->step()){
                AcDbObjectId id;check(it->getRecordId(id),"actual font style id");auto record=open<AcDbTextStyleTableRecord>(id);AcString name,primary,big;
                check(record->getName(name),"actual font style name");check(record->fileName(primary),"actual primary declaration");check(record->bigFontFileName(big),"actual bigfont declaration");
                const auto before=styleAuditTextStyle(id);std::vector<std::string> beforeFiles;
                if(primary.length())beforeFiles.push_back(pureFontLookup(*db,primary.kACharPtr()));if(big.length())beforeFiles.push_back(pureFontLookup(*db,big.kACharPtr()));
                AcGiTextStyle gs(db.get());const auto converted=fromAcDbTextStyle(gs,id);
                AcString gsBeforeFace,gsAfterFace;bool bold{},italic{};Charset charset{};Autodesk::AutoCAD::PAL::FontUtils::FontPitch pitch{};Autodesk::AutoCAD::PAL::FontUtils::FontFamily family{};
                const auto gsBeforeStatus=gs.font(gsBeforeFace,bold,italic,charset,pitch,family);
                const auto loaded=converted==Acad::eOk?gs.loadStyleRec(db.get()):-9999;
                const auto gsAfterStatus=gs.font(gsAfterFace,bold,italic,charset,pitch,family);
                const std::wstring loadedPrimary=gs.fileName()?gs.fileName():L"",loadedBig=gs.bigFontFileName()?gs.bigFontFileName():L"";
                std::vector<std::string> files;if(!loadedPrimary.empty())files.push_back(pureFontLookup(*db,loadedPrimary));if(!loadedBig.empty())files.push_back(pureFontLookup(*db,loadedBig));
                styles.push_back("{\"handle\":"+quote(handleOf(id))+",\"name\":"+quote(name.kACharPtr())+",\"declaredPrimary\":"+quote(primary.kACharPtr())+",\"declaredBig\":"+quote(big.kACharPtr())+",\"loadedPrimary\":"+quote(loadedPrimary)+",\"loadedBig\":"+quote(loadedBig)+",\"conversionStatus\":"+std::to_string(converted)+",\"loadBits\":"+std::to_string(loaded)+",\"gsBeforeFontStatus\":"+std::to_string(gsBeforeStatus)+",\"gsAfterFontStatus\":"+std::to_string(gsAfterStatus)+",\"gsBeforeTypeface\":"+quote(gsBeforeFace.kACharPtr())+",\"gsAfterTypeface\":"+quote(gsAfterFace.kACharPtr())+",\"beforeResolvedFiles\":"+axisTraceArray(beforeFiles)+",\"resolvedFiles\":"+axisTraceArray(files)+",\"before\":"+before+",\"after\":"+styleAuditTextStyle(id)+"}");
            }
            sources.push_back("{\"path\":"+quote(source)+",\"styles\":"+axisTraceArray(styles)+"}");
        }
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown actual font loading failure");}
    writeNewReport(p.output+L"\\native-actual-fonts.json","{\"contract\":\"stage1.native-actual-fonts.v1\",\"sources\":"+axisTraceArray(sources)+",\"commandTrace\":"+trace.json()+",\"sourceDrawingWritten\":false,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"error\":"+error+"}");
}

void nativeGenericTextCopyImpl(){
    const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;
    std::string error="null",before="null",after="null",saved="null",originalBefore="[]",originalAfter="[]",originalSaved="[]";
    std::string protocolText="null",protocolMText="null";int copied=-9999,controlStatus=-9999,saveStatus=-9999,savedCount=-1;bool changed=false;std::wstring rootType,targetHandle,savedRootType;
    try{
        auto db=lifecycleRead(p.scheme);WorkingDatabaseScope work(*db);const auto originals=modelIds(*db);originalBefore=businessGeometryJson(originals);
        const auto source=nsrFind(*db,L"CDC0");const auto sourceTextBefore=symbolPublicReadback(source);
        AcDbObjectIdArray ids;ids.append(source);AcDbIdMapping map;check(db->deepCloneObjects(ids,modelId(*db),map),"native clone text for generic API trial");
        AcDbIdPair pair(source,AcDbObjectId::kNull,false);if(!map.compute(pair)||!pair.isCloned()||pair.value().isNull()||pair.value()==source)throw std::runtime_error("native text clone mapping invalid");
        const auto target=pair.value();targetHandle=handleOf(target);before=symbolPublicReadback(target).json;
        AcDbText donor;donor.setDatabaseDefaults(db.get());check(donor.setTextString(L"C09235"),"native donor content");AcDbText control;controlStatus=control.copyFrom(&donor);AcString controlText;check(control.textString(controlText),"native text control getter");if(controlStatus!=Acad::eOk||std::wstring(controlText.kACharPtr())!=L"C09235")throw std::runtime_error("standard native copyFrom positive control failed");
        {
            auto entity=open<AcDbEntity>(target,AcDb::kForWrite);rootType=entity->isA()->name();
            const auto* text=entity->queryX(AcDbText::desc());const auto* mtext=entity->queryX(AcDbMText::desc());
            if(text)protocolText=quote(text->isA()->name());if(mtext)protocolMText=quote(mtext->isA()->name());
            copied=entity->copyFrom(&donor);
        }
        const auto result=symbolPublicReadback(target);after=result.json;
        changed=copied==Acad::eOk&&result.completed&&result.ownershipSafe&&std::find(result.texts.begin(),result.texts.end(),L"C09235")!=result.texts.end();
        originalAfter=businessGeometryJson(originals);if(originalBefore!=originalAfter||symbolPublicReadback(source).texts!=sourceTextBefore.texts)throw std::runtime_error("generic trial altered original objects");
        {auto root=open<AcDbEntity>(target);if(std::wstring(root->isA()->name())!=rootType||rootType!=L"TDbText")throw std::runtime_error("generic text trial replaced native TCH root");}
        if(changed){const auto path=p.output+L"\\native-generic-text.dwg";requireNewFile(path);saveStatus=db->saveAs(path.c_str(),false,AcDb::kDHL_CURRENT);check(static_cast<Acad::ErrorStatus>(saveStatus),"save generic text trial");auto reader=lifecycleRead(path);WorkingDatabaseScope fresh(*reader);const auto savedId=nsrFind(*reader,targetHandle.c_str());{auto root=open<AcDbEntity>(savedId);savedRootType=root->isA()->name();}savedCount=modelCount(*reader);originalSaved=businessGeometryJson(nsrResolve(*reader,nsrHandles(originals)));const auto value=symbolPublicReadback(savedId);saved=value.json;changed=value.completed&&value.ownershipSafe&&value.texts==result.texts&&savedRootType==L"TDbText"&&savedCount==214&&originalSaved==originalBefore&&symbolPublicReadback(nsrFind(*reader,L"CDC0")).texts==sourceTextBefore.texts;}
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown generic native text API failure");}
    writeNewReport(p.output+L"\\native-generic-text-copy.json","{\"contract\":\"stage1.native-generic-text-copy.v1\",\"sourceCount\":213,\"originalBefore\":"+originalBefore+",\"originalAfter\":"+originalAfter+",\"originalSaved\":"+originalSaved+",\"savedRootType\":"+quote(savedRootType)+",\"savedModelCount\":"+std::to_string(savedCount)+",\"targetHandle\":"+quote(targetHandle)+",\"rootType\":"+quote(rootType)+",\"standardControlStatus\":"+std::to_string(controlStatus)+",\"copyFromStatus\":"+std::to_string(copied)+",\"textProtocol\":"+protocolText+",\"mtextProtocol\":"+protocolMText+",\"before\":"+before+",\"after\":"+after+",\"saved\":"+saved+",\"saveStatus\":"+std::to_string(saveStatus)+",\"businessContentWritePassed\":"+plotBool(changed&&error=="null")+",\"commandTrace\":"+trace.json()+",\"sourceDrawingWritten\":false,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"error\":"+error+"}");
}
void nativeActualFonts()noexcept{pureEntry(nativeActualFontsImpl);}
void nativeGenericTextCopy()noexcept{pureEntry(nativeGenericTextCopyImpl);}
