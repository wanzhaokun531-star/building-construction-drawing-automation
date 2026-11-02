// Capability fixtures only: explicit2%, test numbers/sheet, opposite directions.
// They are not project parameters and do not enter phase2 generation logic.
std::string symbolResourceDefinitions(const AcDbObjectIdArray& ids){auto result=nsrResourceArray(ids);for(const std::string value:{",\"isInUse\":true",",\"isInUse\":false"}){std::size_t at{};while((at=result.find(value,at))!=std::string::npos)result.erase(at,value.size());}return result;}
const wchar_t* symbolCommandFor(const std::wstring& kind){return kind==L"slope"?L"TArrow":kind==L"section"?L"TSection":kind==L"pointer-index"?L"TPOINTINDEX":L"TSECTINDEX";}
const wchar_t* symbolExpectedDxf(const std::wstring& kind){return kind==L"slope"?L"TCH_ARROW":kind==L"section"?L"TCH_SYMB_SECTION":L"TCH_INDEXPOINTER";}
std::vector<AcGePoint3d> symbolFixturePoints(const std::wstring& kind,int n){
    if(kind==L"slope")return n==0?std::vector<AcGePoint3d>{{50000,20000,0},{52000,20000,0}}:std::vector<AcGePoint3d>{{52000,24000,0},{50000,24000,0}};
    if(kind==L"section")return n==0?std::vector<AcGePoint3d>{{48000,26000,0},{52000,26000,0},{50000,27000,0}}:std::vector<AcGePoint3d>{{48000,31000,0},{52000,31000,0},{50000,30000,0}};
    if(kind==L"pointer-index")return n==0?std::vector<AcGePoint3d>{{48000,36000,0},{49500,36500,0},{51000,36500,0}}:std::vector<AcGePoint3d>{{51000,40000,0},{49500,40500,0},{48000,40500,0}};
    return n==0?std::vector<AcGePoint3d>{{48000,45000,0},{49500,45500,0},{51000,45500,0},{48500,46000,0}}:std::vector<AcGePoint3d>{{51000,49000,0},{49500,49500,0},{48000,49500,0},{50500,48000,0}};
}
SymbolUiValues symbolFixtureValues(const std::wstring& kind,const SymbolUiValues& original,int n){
    auto values=original;const auto text=[&](const wchar_t* key,const std::wstring& value){auto it=values.find(key);if(it==values.end())throw std::runtime_error("actual symbol fixture role absent");it->second.text=value;if(it->second.kind==L"combo")it->second.selected=-1;};
    if(kind==L"slope"){text(L"upper",L"2%");text(L"lower",L"");}
    else{text(L"number",std::to_wstring(kind==L"section"?31+n:7+n));text(L"sheet",L"S1-01");if(kind==L"section")values.at(L"showSheet").checked=BST_CHECKED;else{text(L"upper",L"");text(L"lower",L"");}}
    return values;
}
struct SymbolStageSnapshot {std::string json{"null"};std::vector<ElevationReadback> live,saved;bool completed{},safe{true};};
SymbolStageSnapshot symbolSaveStage(const Paths& p,AcApDocument* doc,AcDbDatabase& db,const AcDbObjectIdArray& originals,const std::string& originalBefore,const AcDbObjectIdArray& elevations,const ElevationProtectedValues& protectedBefore,const AcDbObjectIdArray& symbols){
    SymbolStageSnapshot result;const auto output=fullPath(p.output+L"\\symbol-generated.dwg");requireNewFile(output);fourAxisGuard(p,doc,&db);
    for(int i=0;i<symbols.length();++i){result.live.push_back(symbolPublicReadback(symbols[i]));result.safe=result.safe&&result.live.back().ownershipSafe;}
    if(!result.safe)throw std::runtime_error("symbol live public transient ownership unsafe");
    const auto save=db.saveAs(output.c_str(),false,AcDb::kDHL_CURRENT);check(save,"save whole scheme and native symbols");fourAxisGuard(p,doc,&db);bool firstDestroyed=false;
    {auto first=std::make_unique<AcDbDatabase>(false,true);check(first->readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false),"symbol first saved reader");check(first->closeInput(true),"symbol first input close");first.reset();firstDestroyed=true;}
    AcDbDatabase reader(false,true);const auto read=reader.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(read,"symbol fresh saved reader");const auto closed=reader.closeInput(true);check(closed,"symbol fresh close");
    int count=-1;std::string savedOriginal="[]";ElevationProtectedValues savedElevations;
    {WorkingDatabaseScope work(reader);count=modelCount(reader);AcDbObjectIdArray old,oldElevations;
        for(int i=0;i<originals.length();++i){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(handleOf(originals[i]).c_str())),"saved original symbol identity");old.append(id);}savedOriginal=familyCompactArray(old);
        for(int i=0;i<elevations.length();++i){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(handleOf(elevations[i]).c_str())),"saved original elevation identity");oldElevations.append(id);}savedElevations=elevationProtectedValues(oldElevations);
        for(int i=0;i<symbols.length();++i){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(handleOf(symbols[i]).c_str())),"saved native symbol identity");result.saved.push_back(symbolPublicReadback(id));result.safe=result.safe&&result.saved.back().ownershipSafe;}}
    bool same=result.live.size()==result.saved.size();for(std::size_t i=0;i<result.live.size()&&i<result.saved.size();++i)same=same&&result.live[i].completed&&result.saved[i].completed&&result.live[i].texts==result.saved[i].texts&&result.live[i].emptyPublic==result.saved[i].emptyPublic;
    result.safe=result.safe&&savedElevations.ownershipSafe;result.completed=result.safe&&firstDestroyed&&count==215&&savedOriginal==originalBefore&&same&&savedElevations.values==protectedBefore.values&&savedElevations.emptyStructures==protectedBefore.emptyStructures;
    result.json="{\"outputDWG\":"+quote(output)+",\"saveStatus\":"+std::to_string(save)+",\"reopenStatus\":"+std::to_string(read)+",\"closeInputStatus\":"+std::to_string(closed)+",\"firstReaderDestroyed\":"+plotBool(firstDestroyed)+",\"modelCount\":"+std::to_string(count)+",\"originalSaved\":"+savedOriginal+",\"protectedElevationSaved\":"+savedElevations.json+",\"completed\":"+plotBool(result.completed)+"}";return result;
}
void nativeSymbolBusiness(const wchar_t* requestedKind){
    const auto p=paths();const std::wstring kind=requestedKind;const auto report=p.output+L"\\symbol-business-native.json";
    AcApDocument* doc{};AcDbDatabase* db{};AcDbObjectIdArray originals,elevations,symbols,resources;std::string originalBefore="[]",originalAfter="[]",resourceBefore="[]",resourceAfter="[]",resourceRawBefore="[]",resourceRawAfter="[]",newResources="[]",temporaryDefaultRestore="null",error="null",restoreError="null";
    ElevationProtectedValues protectedBefore,protectedAfter;SymbolStageSnapshot snapshot;SymbolUiValues originalDefaults,observedDefaults;std::vector<std::string> operations,symbolRecords;bool safe=true,captured=false,defaultsRestored=false,completed=false;int model=-1;
    const auto operation=[&](const wchar_t* id,const wchar_t* mode,const SymbolUiValues& desired,const std::vector<AcGePoint3d>& points){
        auto s=std::make_unique<SymbolUiOperation>();s->paths=p;s->doc=doc;s->db=db;s->kind=kind;s->command=symbolCommandFor(kind);s->id=id;s->mode=mode;s->desired=desired;s->inputPoints=points;symbolUiRun(*s);operations.push_back(symbolUiOperationJson(*s));safe=s->safe;return s;};
    try{quietRequireIsolatedWorker();doc=acDocManager->curDocument();db=doc?doc->database():nullptr;fourAxisGuard(p,doc,db);requireNewFile(report);originals=modelIds(*db);if(originals.length()!=213)throw std::runtime_error("symbol fixture requires frozen scheme213");originalBefore=familyCompactArray(originals);resources=symbolResourceIds(*db);resourceBefore=symbolResourceDefinitions(resources);resourceRawBefore=nsrResourceArray(resources);
        for(const auto* h:{L"CE8F",L"CE90",L"CE91",L"CE92",L"CE93",L"CFC4"}){AcDbObjectId id;check(db->getAcDbObjectId(id,false,AcDbHandle(h)),"original elevation for symbol protection");elevations.append(id);}protectedBefore=elevationProtectedValues(elevations);if(!protectedBefore.completed||!protectedBefore.ownershipSafe)throw std::runtime_error("original6 elevation source unreadable");
        auto initial=operation(L"initial-defaults",L"observe",{},{});originalDefaults=initial->before;captured=!originalDefaults.empty();if(!safe||!initial->completed||!captured)throw std::runtime_error("initial actual symbol defaults not safely observed");
        for(int n=0;n<2;++n){const auto desired=symbolFixtureValues(kind,originalDefaults,n);const auto prefix=std::to_wstring(n+1);if(kind==L"section"){auto setup=operation((L"setup-"+prefix).c_str(),L"setup",desired,{});if(!safe||!setup->completed)throw std::runtime_error("symbol test field setup failed");
            auto observe=operation((L"preflight-"+prefix).c_str(),L"observe",desired,{});if(!safe||!observe->completed)throw std::runtime_error("independent symbol preflight did not retain requested fields");}
            auto create=operation((L"create-"+prefix).c_str(),L"create",kind==L"section"?desired:SymbolUiValues{},symbolFixturePoints(kind,n));if(create->newEntities.length()==1)symbols.append(create->newEntities[0]);if(!safe||!create->completed)throw std::runtime_error("native symbol creation protocol failed");
            {auto entity=open<AcDbEntity>(symbols[symbols.length()-1]);if(entity->isAProxy()||!entity->isA()->dxfName()||_wcsicmp(entity->isA()->dxfName(),symbolExpectedDxf(kind)))throw std::runtime_error("created symbol is not the corresponding actual native TCH type");}
            if(kind!=L"section"){
                auto edit=symbolEditObject(p,doc,db,kind,symbols[symbols.length()-1],desired,L"edit-"+prefix);operations.push_back(symbolUiOperationJson(*edit));safe=edit->safe;if(!safe||!edit->completed)throw std::runtime_error("direct native TObjEdit field commit failed");
            }
            if(familyCompactArray(originals)!=originalBefore)throw std::runtime_error("symbol command altered original scheme geometry");
        }
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown symbol business failure");safe=false;}
    if(captured&&safe&&!symbolUiBridgeRetained&&!symbolDiscoveryRetained){
        AcDbObjectId temporary;int beforeCount=-1,afterClone=-1,afterErase=-1;std::string cloneHandle="null";bool erased=false,keptTextsSame=true;
        try{
            if(kind==L"section"||symbols.isEmpty()){
                auto reset=operation(L"restore-defaults",L"setup",originalDefaults,{});if(!safe||!reset->completed)throw std::runtime_error("symbol original defaults restore failed");
            }else{
                fourAxisGuard(p,doc,db);beforeCount=modelCount(*db);std::vector<std::vector<std::wstring>> keptTexts;for(int i=0;i<symbols.length();++i){const auto read=symbolPublicReadback(symbols[i]);if(!read.ownershipSafe||!read.completed)throw std::runtime_error("kept symbol unreadable before restoring defaults");keptTexts.push_back(read.texts);}
                std::unique_ptr<AcRxObject> object;{auto source=open<AcDbEntity>(symbols[0]);object.reset(source->clone());}auto* entity=object?AcDbEntity::cast(object.get()):nullptr;
                if(!entity||entity->isAProxy()||!axisExplodeTransient(*entity))throw std::runtime_error("default restore clone is not an owned transient native symbol");
                {auto modelRecord=open<AcDbBlockTableRecord>(modelId(*db),AcDb::kForWrite);check(modelRecord->appendAcDbEntity(temporary,entity),"append own temporary default restore clone");object.release();check(entity->close(),"close database-owned default restore clone");}
                cloneHandle=quote(handleOf(temporary));afterClone=modelCount(*db);auto reset=symbolEditObject(p,doc,db,kind,temporary,originalDefaults,L"restore-defaults-via-owned-clone");operations.push_back(symbolUiOperationJson(*reset));safe=reset->safe;
                if(!safe||!reset->completed)throw std::runtime_error("native editor could not restore actual original defaults on own clone");
                for(int i=0;i<symbols.length();++i){const auto read=symbolPublicReadback(symbols[i]);keptTextsSame=keptTextsSame&&read.completed&&read.ownershipSafe&&read.texts==keptTexts[i];}
                if(!keptTextsSame)throw std::runtime_error("editing temporary clone changed a kept symbol public value");
            }
        }catch(const std::exception& e){restoreError=familyError(e);}catch(...){restoreError=quote(L"unknown native default restore clone failure");safe=false;}
        if(!temporary.isNull()&&!symbolUiBridgeRetained){try{fourAxisGuard(p,doc,db);{auto own=open<AcDbEntity>(temporary,AcDb::kForWrite);check(own->erase(),"erase only own temporary default restore clone");}erased=temporary.isErased();afterErase=modelCount(*db);}catch(const std::exception& e){restoreError=familyError(e);safe=false;}}
        if(!temporary.isNull())temporaryDefaultRestore="{\"cloneHandle\":"+cloneHandle+",\"sourceHandle\":"+quote(handleOf(symbols[0]))+",\"beforeCount\":"+std::to_string(beforeCount)+",\"afterCloneCount\":"+std::to_string(afterClone)+",\"afterEraseCount\":"+std::to_string(afterErase)+",\"erased\":"+plotBool(erased)+",\"keptActualTextsUnchanged\":"+plotBool(keptTextsSame)+",\"originalEntitiesErased\":false}";
        if(safe&&restoreError=="null"){try{auto observe=operation(L"verify-defaults",L"observe",originalDefaults,{});observedDefaults=observe->before;defaultsRestored=safe&&observe->completed&&symbolUiValuesEqual(originalDefaults,observedDefaults);if(!defaultsRestored)throw std::runtime_error("independently reopened symbol defaults do not match original");}catch(const std::exception& e){restoreError=familyError(e);}catch(...){restoreError=quote(L"unknown restored default verification failure");safe=false;}}
    }
    if(safe&&db){try{fourAxisGuard(p,doc,db);originalAfter=familyCompactArray(originals);protectedAfter=elevationProtectedValues(elevations);safe=protectedAfter.ownershipSafe;resourceAfter=symbolResourceDefinitions(resources);resourceRawAfter=nsrResourceArray(resources);model=modelCount(*db);
        const auto allResources=symbolResourceIds(*db);AcDbObjectIdArray added;for(int i=0;i<allResources.length();++i){bool old=false;for(int j=0;j<resources.length();++j)if(allResources[i]==resources[j])old=true;if(!old)added.append(allResources[i]);}newResources=nsrResourceArray(added);
        if(symbols.length()==2&&safe)snapshot=symbolSaveStage(p,doc,*db,originals,originalBefore,elevations,protectedBefore,symbols);
        for(int i=0;i<symbols.length();++i){auto entity=open<AcDbEntity>(symbols[i]);const auto rx=std::wstring(entity->isA()->name()),dxf=std::wstring(entity->isA()->dxfName()?entity->isA()->dxfName():L"");const bool proxy=entity->isAProxy()!=0;entity.reset();const auto live=i<static_cast<int>(snapshot.live.size())?snapshot.live[i]:symbolPublicReadback(symbols[i]);const std::string saved=i<static_cast<int>(snapshot.saved.size())?snapshot.saved[i].json:"null";
            symbolRecords.push_back("{\"handle\":"+quote(handleOf(symbols[i]))+",\"rxName\":"+quote(rx)+",\"dxfName\":"+quote(dxf)+",\"proxy\":"+plotBool(proxy)+",\"geometry\":"+familyCompactEntity(symbols[i])+",\"liveReadback\":"+live.json+",\"savedReadback\":"+saved+"}");}
    }catch(const std::exception& e){if(error=="null")error=familyError(e);}catch(...){if(error=="null")error=quote(L"unknown saved symbol validation failure");safe=false;}}
    completed=safe&&error=="null"&&restoreError=="null"&&defaultsRestored&&symbols.length()==2&&model==215&&originalBefore==originalAfter&&resourceBefore==resourceAfter&&protectedBefore.values==protectedAfter.values&&protectedBefore.emptyStructures==protectedAfter.emptyStructures&&snapshot.completed;
    writeNewReport(report,"{\"contract\":\"stage1.symbol-business-native.v4\",\"kind\":"+quote(kind)+",\"api\":\"ObjectARX C++ + public Tianzheng commands + isolated standard Win32 controls; no COM/CUA\",\"originalBefore\":"+originalBefore+",\"originalAfter\":"+originalAfter+",\"originalResourceBefore\":"+resourceBefore+",\"originalResourceAfter\":"+resourceAfter+",\"originalResourceRawBefore\":"+resourceRawBefore+",\"originalResourceRawAfter\":"+resourceRawAfter+",\"resourceDefinitionExclusion\":[\"isInUse bookkeeping only\"],\"temporaryDefaultRestore\":"+temporaryDefaultRestore+",\"addedResources\":"+newResources+",\"protectedElevationBefore\":"+protectedBefore.json+",\"protectedElevationAfter\":"+protectedAfter.json+",\"originalDefaults\":"+symbolUiValuesJson(originalDefaults)+",\"observedRestoredDefaults\":"+symbolUiValuesJson(observedDefaults)+",\"defaultsRestored\":"+plotBool(defaultsRestored)+",\"operations\":"+axisTraceArray(operations)+",\"symbols\":"+axisTraceArray(symbolRecords)+",\"snapshot\":"+snapshot.json+",\"modelCount\":"+std::to_string(model)+",\"safeToContinue\":"+plotBool(safe)+",\"completed\":"+plotBool(completed)+",\"error\":"+error+",\"restoreError\":"+restoreError+",\"phase2Started\":false}");
}
void nativeSymbolBusinessEntry(const wchar_t* kind){try{nativeSymbolBusiness(kind);}catch(const std::exception& e){acutPrintf(L"\nSYMBOL_PROBE_FAILED %hs\n",e.what());}catch(...){acutPrintf(L"\nSYMBOL_PROBE_FAILED unknown\n");}}
void nativeSlopeBusiness(){nativeSymbolBusinessEntry(L"slope");}
void nativeSectionBusiness(){nativeSymbolBusinessEntry(L"section");}
void nativePointerBusiness(){nativeSymbolBusinessEntry(L"pointer-index");}
void nativeSectIndexBusiness(){nativeSymbolBusinessEntry(L"section-index");}
