// Read-only discovery of the installed public symbol-command panels and prompts.
// Own private desktop only. No field setters, drawing append, or guessed IDs.
bool symbolDiscoveryRetained{};
struct SymbolDiscoveryState {
    Paths paths;std::wstring command,kind;DWORD pid{},thread{};UINT_PTR timer{};
    bool inside{},commandInProgress{},cancelPosted{},timedOut{},timerStopped{},observerRemoved{},captured{};
    int sweeps{},promptAdd{-9999},promptRemove{-9999},commandStatus{-9999};
    ULONGLONG started{};std::vector<std::string> snapshots,events;std::string lastSnapshot,error{"null"};
    std::shared_ptr<AxisNativeTraceData> trace{std::make_shared<AxisNativeTraceData>()};
};
SymbolDiscoveryState* activeSymbolDiscovery{};
bool symbolPanelSemantics(const SymbolDiscoveryState& s,const ElevationDialogSnapshot& panel){
    std::wstring all=panel.title;for(const auto& c:panel.controls)if(c.visible)all+=L"\n"+c.text;
    if(s.kind==L"slope")return elevationDialogContains(all,L"箭头")||elevationDialogContains(all,L"引注");
    if(s.kind==L"section")return elevationDialogContains(all,L"剖切");
    if(s.kind==L"pass-text")return elevationDialogContains(all,L"递增");
    return elevationDialogContains(all,L"索引");
}
std::string symbolPanelExtra(const ElevationDialogSnapshot& panel){
    std::vector<std::string> values;
    for(const auto& c:panel.controls){
        if(!c.visible||c.processId!=GetCurrentProcessId())continue;
        if(!_wcsicmp(c.className.c_str(),L"Button")){
            const DWORD kind=static_cast<DWORD>(c.style)&BS_TYPEMASK;
            if(kind==BS_CHECKBOX||kind==BS_AUTOCHECKBOX||kind==BS_3STATE||kind==BS_AUTO3STATE||kind==BS_RADIOBUTTON||kind==BS_AUTORADIOBUTTON){
                DWORD_PTR value{};const bool ok=SendMessageTimeoutW(c.hwnd,BM_GETCHECK,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&value)!=0;
                values.push_back("{\"hwnd\":"+elevationDialogHwnd(c.hwnd)+",\"role\":\"standard-check-state\",\"read\":"+plotBool(ok)+",\"value\":"+std::to_string(value)+"}");
            }
        }else if(!_wcsicmp(c.className.c_str(),L"ComboBox")){
            DWORD_PTR selected{},count{};const bool a=SendMessageTimeoutW(c.hwnd,CB_GETCURSEL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&selected)!=0;
            const bool b=SendMessageTimeoutW(c.hwnd,CB_GETCOUNT,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&count)!=0;
            std::vector<std::string> items;
            if(b&&static_cast<LRESULT>(count)>=0&&count<=32){for(DWORD_PTR i=0;i<count;++i){wchar_t value[2048]{};DWORD_PTR size{};
                if(!SendMessageTimeoutW(c.hwnd,CB_GETLBTEXTLEN,i,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&size)||size>=2048)break;
                DWORD_PTR ignored{};if(!SendMessageTimeoutW(c.hwnd,CB_GETLBTEXT,i,reinterpret_cast<LPARAM>(value),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&ignored))break;
                items.push_back(quote(value));}}
            values.push_back("{\"hwnd\":"+elevationDialogHwnd(c.hwnd)+",\"role\":\"standard-combo-state\",\"read\":"+plotBool(a&&b)+",\"selected\":"+std::to_string(static_cast<LRESULT>(selected))+",\"count\":"+std::to_string(static_cast<LRESULT>(count))+",\"items\":"+axisTraceArray(items)+"}");
        }
    }return axisTraceArray(values);
}
void symbolDiscoveryProgress(SymbolDiscoveryState& s,const wchar_t* stage){
    const auto data="{\"contract\":\"stage1.symbol-discovery-progress.v1\",\"stage\":"+quote(stage)+",\"command\":"+quote(s.command)+",\"kind\":"+quote(s.kind)
        +",\"processId\":"+std::to_string(s.pid)+",\"threadId\":"+std::to_string(s.thread)+",\"desktop\":"+quote(quietCurrentDesktop())
        +",\"snapshots\":"+axisTraceArray(s.snapshots)+",\"events\":"+axisTraceArray(s.events)+",\"inputTrace\":"+axisTraceArray(s.trace->inputs)+",\"commandTrace\":"+axisTraceArray(s.trace->commands)+",\"error\":"+s.error+"}";
    quietAtomic(s.paths.output+L"\\symbol-discovery-progress.json",data,s.paths.output);
}
void symbolDiscoveryCapture(SymbolDiscoveryState& s,const wchar_t* source){
    if(s.inside||!s.commandInProgress)return;s.inside=true;
    try{quietRequireIsolatedWorker();if(GetCurrentProcessId()!=s.pid||GetCurrentThreadId()!=s.thread)throw std::runtime_error("symbol discovery changed native UI thread");
        ++s.sweeps;const auto panels=elevationDialogOwnedSnapshots();
        for(const auto& panel:panels){
            if(panel.isMainFrame||!panel.enumerationComplete||panel.processId!=s.pid||panel.threadId!=s.thread||!symbolPanelSemantics(s,panel))continue;
            const auto record="{\"source\":"+quote(source)+",\"panel\":"+elevationDialogSnapshotJson(panel)+",\"standardValues\":"+symbolPanelExtra(panel)+"}";
            if(s.snapshots.size()<64&&record!=s.lastSnapshot){s.snapshots.push_back(record);s.lastSnapshot=record;}s.captured=true;
            // A genuinely modal command dialog cannot consume our empty token.
            // Cancel only its actual unique standard Cancel button, on own PID/thread.
            if(!s.cancelPosted&&panel.className==L"#32770"&&elevationDialogKnownMainFrame&&!IsWindowEnabled(elevationDialogKnownMainFrame)){
                HWND cancel{};int count=0;for(const auto& c:panel.controls)if(c.visible&&c.enabled&&!_wcsicmp(c.className.c_str(),L"Button")&&elevationDialogCaption(c.text,L"取消",L"Cancel")){cancel=c.hwnd;++count;}
                if(count==1&&IsWindow(cancel)&&GetParent(cancel)&&elevationDialogOnOwnDesktop(panel.hwnd)){
                    s.cancelPosted=PostMessageW(cancel,BM_CLICK,0,0)!=FALSE;
                    s.events.push_back("{\"event\":\"cancel-actual-owned-modal\",\"target\":"+elevationDialogHwnd(cancel)+",\"actualId\":"+std::to_string(GetDlgCtrlID(cancel))+",\"posted\":"+plotBool(s.cancelPosted)+"}");
                }
            }
        }
        if(GetTickCount64()-s.started>15000)s.timedOut=true;
        symbolDiscoveryProgress(s,source);
    }catch(const std::exception& e){s.error=familyError(e);}catch(...){s.error=quote(L"unknown symbol discovery callback failure");}
    s.inside=false;
}
void CALLBACK symbolDiscoveryTimer(HWND,UINT,UINT_PTR timer,DWORD)noexcept{auto* s=activeSymbolDiscovery;if(s&&s->timer==timer&&!s->timedOut)symbolDiscoveryCapture(*s,L"timer-readonly");}
class SymbolDiscoveryObserver final:public AcEdInputContextReactor{
public:SymbolDiscoveryState* state{};explicit SymbolDiscoveryObserver(SymbolDiscoveryState& s):state(&s){}
    void beginSSGet(const ACHAR*,int,const ACHAR*,const ACHAR*,const AcArray<AcGePoint3d>&,const resbuf*)override{if(state)symbolDiscoveryCapture(*state,L"beginSSGet");}
    void beginGetPoint(const AcGePoint3d*,const ACHAR*,int,const ACHAR*)override{if(state)symbolDiscoveryCapture(*state,L"beginGetPoint");}
};
void nativeSymbolDiscover(const wchar_t* kind,const wchar_t* command,bool objectEdit=false){
    const auto p=paths();auto state=std::make_unique<SymbolDiscoveryState>();state->paths=p;state->kind=kind;state->command=command;state->pid=GetCurrentProcessId();state->thread=GetCurrentThreadId();state->started=GetTickCount64();
    auto observer=std::make_unique<SymbolDiscoveryObserver>(*state);AcEdInputPointManager* manager{};AcApDocument* doc{};AcDbDatabase* db{};
    std::string originalBefore="[]",originalAfter="[]",error="null";AcDbObjectIdArray originals;int initial=-1,final=-1,dbmodBefore=-1,dbmodAfter=-1;bool completed=false;
    try{quietRequireIsolatedWorker();doc=acDocManager->curDocument();db=doc?doc->database():nullptr;fourAxisGuard(p,doc,db);if(activeSymbolDiscovery||symbolDiscoveryRetained)throw std::runtime_error("symbol discovery callback remains active");
        originals=modelIds(*db);initial=originals.length();if(initial!=(objectEdit?215:213))throw std::runtime_error("symbol discovery source cohort differs");originalBefore=familyCompactArray(originals);dbmodBefore=axisExplodeDbmod();
        elevationDialogKnownMainFrame=adsw_acadMainWnd();manager=doc->inputPointManager();if(!manager)throw std::runtime_error("symbol discovery input manager unavailable");
        state->promptAdd=manager->addInputContextReactor(observer.get());check(static_cast<Acad::ErrorStatus>(state->promptAdd),"attach symbol discovery observer");
        activeSymbolDiscovery=state.get();state->timer=SetTimer(nullptr,0,150,symbolDiscoveryTimer);if(!state->timer)throw std::runtime_error("symbol discovery timer unavailable");
        {AxisNativeTraceOwner trace(doc,state->trace);state->commandInProgress=true;symbolDiscoveryProgress(*state,L"before-public-command");
            if(objectEdit){
                AcDbDatabase frozen(false,true);check(frozen.readDwgFile(p.scheme.c_str(),AcDbDatabase::kForReadAndAllShare,false),"object-edit discovery frozen membership");check(frozen.closeInput(true),"object-edit discovery frozen close");std::vector<std::wstring> oldHandles;{WorkingDatabaseScope work(frozen);const auto ids=modelIds(frozen);for(int i=0;i<ids.length();++i)oldHandles.push_back(handleOf(ids[i]));}
                AcDbObjectId target;for(int i=0;i<originals.length();++i){if(std::find(oldHandles.begin(),oldHandles.end(),handleOf(originals[i]))!=oldHandles.end())continue;auto e=open<AcDbEntity>(originals[i]);if(!e->isAProxy()&&!_wcsicmp(e->isA()->name(),L"TDbSymbArrow")){target=originals[i];break;}}
                if(target.isNull())throw std::runtime_error("object-edit discovery has no owned new arrow target");ads_name name{};check(acdbGetAdsName(name,target),"object-edit native entity name");state->events.push_back("{\"event\":\"exact-object-edit-target\",\"handle\":"+quote(handleOf(target))+",\"inputType\":\"RTENAME\"}");
                state->commandStatus=acedCommandS(RTSTR,command,RTENAME,name,RTNONE);
            }else state->commandStatus=acedCommandS(RTSTR,command,RTSTR,L"",RTNONE);
            state->commandInProgress=false;symbolDiscoveryProgress(*state,L"public-command-returned");}
        fourAxisGuard(p,doc,db);originalAfter=familyCompactArray(originals);final=modelCount(*db);dbmodAfter=axisExplodeDbmod();
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown symbol discovery failure");}
    if(state->timer){state->timerStopped=KillTimer(nullptr,state->timer)!=FALSE;if(state->timerStopped)state->timer=0;}else state->timerStopped=true;
    observer->state=nullptr;if(manager&&state->promptAdd==Acad::eOk){state->promptRemove=manager->removeInputContextReactor(observer.get());state->observerRemoved=state->promptRemove==Acad::eOk||state->promptRemove==Acad::eInvalidKey;}
    else state->observerRemoved=true;
    completed=error=="null"&&state->error=="null"&&state->captured&&state->timerStopped&&state->observerRemoved&&!state->trace->retained&&!textBridgeHasUnbalancedCommandTrace(*state->trace)&&initial==(objectEdit?215:213)&&final==initial&&originalBefore==originalAfter&&dbmodBefore==dbmodAfter;
    const auto report=p.output+L"\\symbol-discovery-native.json";
    const auto payload="{\"contract\":\"stage1.symbol-discovery-native.v1\",\"kind\":"+quote(kind)+",\"command\":"+quote(command)+",\"commandStatus\":"+std::to_string(state->commandStatus)+",\"processId\":"+std::to_string(state->pid)+",\"threadId\":"+std::to_string(state->thread)+",\"desktop\":"+quote(quietCurrentDesktop())+",\"snapshots\":"+axisTraceArray(state->snapshots)+",\"events\":"+axisTraceArray(state->events)+",\"commandTrace\":"+axisTraceArray(state->trace->commands)+",\"inputTrace\":"+axisTraceArray(state->trace->inputs)+",\"originalBefore\":"+originalBefore+",\"originalAfter\":"+originalAfter+",\"initialCount\":"+std::to_string(initial)+",\"finalCount\":"+std::to_string(final)+",\"dbmodBefore\":"+std::to_string(dbmodBefore)+",\"dbmodAfter\":"+std::to_string(dbmodAfter)+",\"timerStopped\":"+plotBool(state->timerStopped)+",\"observerRemoved\":"+plotBool(state->observerRemoved)+",\"fieldSettersAttempted\":false,\"businessValuesProven\":false,\"completed\":"+plotBool(completed)+",\"error\":"+error+",\"callbackError\":"+state->error+"}";
    if(!state->timerStopped||!state->observerRemoved){symbolDiscoveryRetained=true;observer.release();state.release();acrxDynamicLinker->lockApplication(nativeApplicationId);}else activeSymbolDiscovery=nullptr;
    writeNewReport(report,payload);
}
void nativeSlopeDiscover(){nativeSymbolDiscover(L"slope",L"TArrow");}
void nativeSectionDiscover(){nativeSymbolDiscover(L"section",L"TSection");}
void nativePointerIndexDiscover(){nativeSymbolDiscover(L"pointer-index",L"TPOINTINDEX");}
void nativeSectionIndexDiscover(){nativeSymbolDiscover(L"section-index",L"TSECTINDEX");}
void nativeArrowEditDiscover(){nativeSymbolDiscover(L"slope",L"TObjEdit",true);}
