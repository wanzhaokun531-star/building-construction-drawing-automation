// Ordinary AutoCAD facts. No TCH commands/panels, no source drawing writes.
short hostGetShort(const wchar_t* name){resbuf v{};if(acedGetVar(name,&v)!=RTNORM||v.restype!=RTSHORT)throw std::runtime_error("host variable not a readable short");return v.resval.rint;}
std::wstring hostGetText(const wchar_t* name){resbuf v{};if(acedGetVar(name,&v)!=RTNORM||v.restype!=RTSTR||!v.resval.rstring)throw std::runtime_error("host text variable unreadable");std::wstring result;try{result=v.resval.rstring;}catch(...){acutDelString(v.resval.rstring);throw;}acutDelString(v.resval.rstring);return result;}
class HostDemandLoadScope {
    const short before_;bool changed_{},restored_{};
public:
    HostDemandLoadScope():before_(hostGetShort(L"DEMANDLOAD")){}
    HostDemandLoadScope(const HostDemandLoadScope&)=delete;HostDemandLoadScope& operator=(const HostDemandLoadScope&)=delete;
    short before()const noexcept{return before_;}
    void apply(){resbuf v{};v.restype=RTSHORT;v.resval.rint=0;changed_=true;if(acedSetVar(L"DEMANDLOAD",&v)!=RTNORM||hostGetShort(L"DEMANDLOAD")!=0)throw std::runtime_error("cannot apply demand-load scope");}
    bool restore()noexcept{if(!changed_||restored_)return true;resbuf v{};v.restype=RTSHORT;v.resval.rint=before_;if(acedSetVar(L"DEMANDLOAD",&v)!=RTNORM)return false;try{restored_=hostGetShort(L"DEMANDLOAD")==before_;}catch(...){return false;}return restored_;}
    ~HostDemandLoadScope(){restore();}
};
struct HostModuleSnapshot {
    HANDLE value{INVALID_HANDLE_VALUE};
    HostModuleSnapshot():value(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId())){if(value==INVALID_HANDLE_VALUE)throw std::runtime_error("own module snapshot failed");}
    HostModuleSnapshot(const HostModuleSnapshot&)=delete;HostModuleSnapshot& operator=(const HostModuleSnapshot&)=delete;
    ~HostModuleSnapshot(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
std::string hostModuleInventory(){HostModuleSnapshot snapshot;MODULEENTRY32W row{};row.dwSize=sizeof(row);std::vector<std::string> entries;if(!Module32FirstW(snapshot.value,&row))throw std::runtime_error("own module enumeration failed");do{entries.push_back("{\"name\":"+quote(row.szModule)+",\"path\":"+quote(row.szExePath)+"}");}while(Module32NextW(snapshot.value,&row));if(GetLastError()!=ERROR_NO_MORE_FILES)throw std::runtime_error("own module enumeration incomplete");return axisTraceArray(entries);}
std::string hostInspectSource(const std::wstring& path,const std::vector<const wchar_t*>& handles){
    AcDbDatabase db(false,true);const auto read=db.readDwgFile(path.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(read,"ordinary AutoCAD source read");const auto closed=db.closeInput(true);check(closed,"ordinary source close");WorkingDatabaseScope work(db);std::vector<std::string> rows;int proxies=0;
    const auto ids=modelIds(db);for(int i=0;i<ids.length();++i){auto entity=open<AcDbEntity>(ids[i]);if(entity->isAProxy())++proxies;}
    for(const auto* handle:handles){AcDbObjectId id;check(db.getAcDbObjectId(id,false,AcDbHandle(handle)),"host sample identity");auto entity=open<AcDbEntity>(id);const auto* proxy=AcDbProxyEntity::cast(entity.get());const wchar_t* original=proxy?proxy->originalClassName():nullptr;const wchar_t* dxf=proxy?proxy->originalDxfName():nullptr;
        rows.push_back("{\"handle\":"+quote(handle)+",\"rxName\":"+quote(entity->isA()->name())+",\"proxy\":"+plotBool(entity->isAProxy()!=0)+",\"originalClassName\":"+(original?quote(original):std::string("null"))+",\"originalDxfName\":"+(dxf?quote(dxf):std::string("null"))+"}");}
    return "{\"path\":"+quote(path)+",\"readStatus\":"+std::to_string(read)+",\"closeInputStatus\":"+std::to_string(closed)+",\"modelCount\":"+std::to_string(ids.length())+",\"proxyCount\":"+std::to_string(proxies)+",\"samples\":"+axisTraceArray(rows)+",\"drawingWritten\":false}";
}
std::string hostLineGeometry(const AcDbObjectId& id){auto line=open<AcDbLine>(id);return "{\"start\":"+point(line->startPoint())+",\"end\":"+point(line->endPoint())+",\"length\":"+number(line->startPoint().distanceTo(line->endPoint()))+"}";}
std::string hostNativeRoundTrip(const Paths& p){
    const auto output=p.output+L"\\native-host-control.dwg";requireNewFile(output);AcDbDatabase db(true,true);AcDbObjectId id;std::wstring handle;std::string live;
    {WorkingDatabaseScope work(db);auto model=open<AcDbBlockTableRecord>(modelId(db),AcDb::kForWrite);auto line=std::make_unique<AcDbLine>(AcGePoint3d(0,0,0),AcGePoint3d(1000,0,0));line->setDatabaseDefaults(&db);check(model->appendAcDbEntity(id,line.get()),"host append native control");auto* resident=line.release();check(resident->close(),"host close native control");model.reset();{auto value=open<AcDbLine>(id,AcDb::kForWrite);check(value->setEndPoint(AcGePoint3d(0,1000,0)),"host control modification");}handle=handleOf(id);live=hostLineGeometry(id);}
    const auto saved=db.saveAs(output.c_str(),false,AcDb::kDHL_CURRENT);check(saved,"host control save");
    {auto first=std::make_unique<AcDbDatabase>(false,true);check(first->readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false),"host first reader");check(first->closeInput(true),"host first close");first.reset();}
    AcDbDatabase reader(false,true);const auto read=reader.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(read,"host fresh read");const auto closed=reader.closeInput(true);check(closed,"host fresh close");std::string actual;{WorkingDatabaseScope work(reader);AcDbObjectId copied;check(reader.getAcDbObjectId(copied,false,AcDbHandle(handle.c_str())),"host saved control identity");actual=hostLineGeometry(copied);}
    return "{\"rxName\":\"AcDbLine\",\"proxy\":false,\"outputDWG\":"+quote(output)+",\"saveStatus\":"+std::to_string(saved)+",\"readStatus\":"+std::to_string(read)+",\"closeInputStatus\":"+std::to_string(closed)+",\"readerDestroyedBeforeReopen\":true,\"live\":"+live+",\"saved\":"+actual+"}";
}
void nativeAutoCadHostBaselineImpl(bool supported){
    const auto p=paths();std::string error="null",modulesBefore="[]",modulesAfter="[]",control="null";std::wstring profile,version;std::vector<std::string> sources;short demandBefore=-1,demandAfter=-1,secureBefore=-1,secureAfter=-1;bool restored=false,complete=false;
    try{quietRequireIsolatedWorker();requireNewFile(p.output+L"\\autocad-host-native.json");HostDemandLoadScope mode;demandBefore=mode.before();secureBefore=hostGetShort(L"SECURELOAD");profile=hostGetText(L"CPROFILE");version=hostGetText(L"ACADVER");modulesBefore=hostModuleInventory();
        try{mode.apply();sources.push_back(hostInspectSource(p.scheme,{L"CD83",L"CD84",L"CD89",L"CDC0"}));sources.push_back(hostInspectSource(p.reference,{L"19A0",L"199F",L"19F3",L"17DF"}));control=hostNativeRoundTrip(p);modulesAfter=hostModuleInventory();}catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown host read failure");}
        restored=mode.restore();demandAfter=hostGetShort(L"DEMANDLOAD");secureAfter=hostGetShort(L"SECURELOAD");complete=error=="null"&&restored&&demandBefore==demandAfter&&secureBefore==secureAfter&&sources.size()==2;
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown host baseline failure");}
    writeNewReport(p.output+L"\\autocad-host-native.json",std::string("{\"contract\":\"stage1.autocad-host-native.v1\",\"supportedMode\":")+plotBool(supported)+",\"profile\":"+quote(profile)+",\"acadVersion\":"+quote(version)+",\"api\":\"ObjectARX C++ only; no TCH commands or panels\",\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengCommandsUsed\":false,\"environment\":{\"demandloadBefore\":"+std::to_string(demandBefore)+",\"demandloadApplied\":0,\"demandloadAfter\":"+std::to_string(demandAfter)+",\"secureloadBefore\":"+std::to_string(secureBefore)+",\"secureloadAfter\":"+std::to_string(secureAfter)+",\"restored\":"+plotBool(restored)+"},\"modulesBefore\":"+modulesBefore+",\"modulesAfter\":"+modulesAfter+",\"sources\":"+axisTraceArray(sources)+",\"nativeControl\":"+control+",\"sourceDrawingWritten\":false,\"phase2Started\":false,\"completed\":"+plotBool(complete)+",\"error\":"+error+"}");
}
void nativeAutoCadHostBaseline(){nativeAutoCadHostBaselineImpl(false);}
void nativeAutoCadSupportedBaseline(){nativeAutoCadHostBaselineImpl(true);}
void nativeAutoCadSupportLoad(){
    const auto p=paths();const auto ini=quietModuleIni();const auto expected=quietIni(ini,L"worker",L"expectedProfile"),component=quietIni(ini,L"worker",L"supportComponent");
    std::string error="null",before="[]",after="[]";std::wstring profile,menuBefore,menuAfter,trusted;bool loaded=false,wall=false,dimension=false;int trustedStatus=-9999;short secure=-1;
    try{quietRequireIsolatedWorker();requireNewFile(p.output+L"\\autocad-support-load-native.json");profile=hostGetText(L"CPROFILE");secure=hostGetShort(L"SECURELOAD");
        if(profile!=expected||profile.rfind(L"CodexS1Native_",0)!=0||secure!=1||_wcsicmp(fullPath(component).c_str(),L"C:\\Tangent\\TArchT20V8\\SYS23x64\\tch_kernal.arx"))throw std::runtime_error("native support load requires authorized dedicated profile and exact installed component");
        wchar_t resolved[32768]{};trustedStatus=acedFindTrustedFile(component.c_str(),resolved,32768);trusted=resolved;if(trustedStatus!=RTNORM||_wcsicmp(fullPath(trusted).c_str(),fullPath(component).c_str()))throw std::runtime_error("component was not confirmed trusted by current AutoCAD profile");
        before=hostModuleInventory();menuBefore=hostGetText(L"MENUNAME");quietAtomic(p.output+L"\\support-load-progress.json","{\"stage\":\"before-native-load\",\"profile\":"+quote(profile)+",\"trustedComponent\":"+quote(trusted)+",\"secureload\":1}",p.output);
        loaded=acrxDynamicLinker->loadModule(component.c_str(),false,false);after=hostModuleInventory();menuAfter=hostGetText(L"MENUNAME");
        wall=AcRxClass::cast(acrxClassDictionary->at(L"TDbWall"))!=nullptr;dimension=AcRxClass::cast(acrxClassDictionary->at(L"TDbDimension2"))!=nullptr;
        if(!loaded||!wall||!dimension||hostGetShort(L"SECURELOAD")!=1||hostGetText(L"CPROFILE")!=profile)throw std::runtime_error("native component load/class/security verification failed");
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown native component load failure");}
    writeNewReport(p.output+L"\\autocad-support-load-native.json","{\"contract\":\"stage1.autocad-support-load-native.v1\",\"profile\":"+quote(profile)+",\"component\":"+quote(component)+",\"trustedPath\":"+quote(trusted)+",\"trustedStatus\":"+std::to_string(trustedStatus)+",\"secureload\":"+std::to_string(secure)+",\"loadModuleResult\":"+plotBool(loaded)+",\"wallClassRegistered\":"+plotBool(wall)+",\"dimensionClassRegistered\":"+plotBool(dimension)+",\"menuBefore\":"+quote(menuBefore)+",\"menuAfter\":"+quote(menuAfter)+",\"modulesBefore\":"+before+",\"modulesAfter\":"+after+",\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengCommandsUsed\":false,\"completed\":"+plotBool(error=="null"&&loaded&&wall&&dimension)+",\"error\":"+error+"}");
}
