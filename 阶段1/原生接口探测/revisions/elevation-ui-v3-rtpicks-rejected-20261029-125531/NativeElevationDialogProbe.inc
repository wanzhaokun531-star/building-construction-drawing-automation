// Include after NativeQuietWorker.inc and AxisNativeTraceOwner helpers.
// Read-only standard Win32 discovery; the sole action closes an exactly
// identified, own-PID/private-desktop TChangeValElev parameter panel.
struct ElevationDialogControl {
    HWND hwnd{},parent{};DWORD processId{},threadId{};int controlId{};
    std::wstring className,text;LONG_PTR style{},exStyle{};RECT rect{};
    bool visible{},enabled{},rectRead{},checkRead{};LRESULT checkState{-1};
};
struct ElevationDialogSnapshot {
    HWND hwnd{};DWORD processId{},threadId{};std::wstring desktop,title,className;
    RECT rect{};std::vector<ElevationDialogControl> controls;
    HWND changeEdit{},increaseRadio{},decreaseRadio{},okButton{},cancelButton{};
    bool recognized{},semanticRolesComplete{},isMainFrame{},enumerationComplete{true};std::wstring recognitionReason;
};
HWND elevationDialogKnownMainFrame{};
std::wstring elevationDialogClass(HWND hwnd){wchar_t value[256]{};GetClassNameW(hwnd,value,256);return value;}
std::wstring elevationDialogText(HWND hwnd){wchar_t value[2048]{};DWORD_PTR result{};if(!SendMessageTimeoutW(hwnd,WM_GETTEXT,2048,reinterpret_cast<LPARAM>(value),SMTO_ABORTIFHUNG|SMTO_BLOCK,80,&result))return L"";return value;}
std::string elevationDialogHwnd(HWND hwnd){return std::to_string(reinterpret_cast<UINT_PTR>(hwnd));}
std::string elevationDialogRect(const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+std::to_string(r.right)+","+std::to_string(r.bottom)+"]";}
bool elevationDialogContains(const std::wstring& text,const wchar_t* token){return text.find(token)!=std::wstring::npos;}
bool elevationDialogCaption(const std::wstring& text,const wchar_t* chinese,const wchar_t* english){
    std::wstring value;for(auto c:text)if(c!=L'&'&&c!=L' '&&c!=L'\t'&&c!=L'\r'&&c!=L'\n')value.push_back(c);
    const auto accelerator=value.find(L'(');if(accelerator!=std::wstring::npos)value=value.substr(0,accelerator);
    if(value==chinese)return true;if(!english)return false;return !_wcsicmp(value.c_str(),english);
}
bool elevationDialogOnOwnDesktop(HWND wanted){struct Find{HWND wanted;bool found{};};Find find{wanted};const auto cb=[](HWND hwnd,LPARAM data)->BOOL{auto& f=*reinterpret_cast<Find*>(data);if(hwnd==f.wanted){DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);f.found=pid==GetCurrentProcessId();return FALSE;}return TRUE;};EnumDesktopWindows(GetThreadDesktop(GetCurrentThreadId()),cb,reinterpret_cast<LPARAM>(&find));return find.found;}
ElevationDialogSnapshot elevationDialogSnapshot(HWND hwnd,ULONGLONG totalDeadline=0){
    if(!totalDeadline)totalDeadline=GetTickCount64()+1200;
    ElevationDialogSnapshot s;s.hwnd=hwnd;s.isMainFrame=hwnd==elevationDialogKnownMainFrame;s.threadId=GetWindowThreadProcessId(hwnd,&s.processId);s.desktop=quietCurrentDesktop();s.title=elevationDialogText(hwnd);s.className=elevationDialogClass(hwnd);GetWindowRect(hwnd,&s.rect);
    struct Capture {ElevationDialogSnapshot* snapshot;ULONGLONG deadline;bool failed{};};Capture capture{&s,totalDeadline};
    const auto cb=[](HWND child,LPARAM parameter)->BOOL{
        auto& c=*reinterpret_cast<Capture*>(parameter);try{
            if(c.snapshot->controls.size()>=(c.snapshot->isMainFrame?24:128)||GetTickCount64()>=c.deadline){c.failed=true;return FALSE;}
            ElevationDialogControl v;v.hwnd=child;v.parent=GetParent(child);v.threadId=GetWindowThreadProcessId(child,&v.processId);
            if(v.processId!=GetCurrentProcessId())return TRUE;
            v.controlId=GetDlgCtrlID(child);v.className=elevationDialogClass(child);v.text=elevationDialogText(child);v.style=GetWindowLongPtrW(child,GWL_STYLE);v.exStyle=GetWindowLongPtrW(child,GWL_EXSTYLE);v.visible=IsWindowVisible(child)!=FALSE;v.enabled=IsWindowEnabled(child)!=FALSE;v.rectRead=GetWindowRect(child,&v.rect)!=FALSE;
            if(!_wcsicmp(v.className.c_str(),L"Button")){const auto kind=static_cast<DWORD>(v.style)&BS_TYPEMASK;if(kind==BS_RADIOBUTTON||kind==BS_AUTORADIOBUTTON){DWORD_PTR value{};v.checkRead=SendMessageTimeoutW(child,BM_GETCHECK,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,80,&value)!=0;if(v.checkRead)v.checkState=static_cast<LRESULT>(value);}}
            c.snapshot->controls.push_back(std::move(v));return TRUE;
        }catch(...){c.failed=true;return FALSE;}
    };
    EnumChildWindows(hwnd,cb,reinterpret_cast<LPARAM>(&capture));s.enumerationComplete=!capture.failed;
    std::wstring semantic=s.title;int edits=0,increase=0,decrease=0,oks=0,cancels=0;bool changePrompt=false;int checked=0;
    for(const auto& c:s.controls){if(c.visible){semantic+=L"\n"+c.text;if(elevationDialogContains(c.text,L"变化值"))changePrompt=true;}
        if(!_wcsicmp(c.className.c_str(),L"Edit")&&c.visible&&c.enabled){++edits;s.changeEdit=c.hwnd;}
        if(_wcsicmp(c.className.c_str(),L"Button")||!c.visible||!c.enabled)continue;const auto kind=static_cast<DWORD>(c.style)&BS_TYPEMASK;
        if(kind==BS_RADIOBUTTON||kind==BS_AUTORADIOBUTTON){
            if(elevationDialogContains(c.text,L"增加")){++increase;s.increaseRadio=c.hwnd;if(c.checkRead&&c.checkState==BST_CHECKED)++checked;}
            if(elevationDialogContains(c.text,L"减少")){++decrease;s.decreaseRadio=c.hwnd;if(c.checkRead&&c.checkState==BST_CHECKED)++checked;}
        }else if(kind==BS_PUSHBUTTON||kind==BS_DEFPUSHBUTTON){
            if(elevationDialogCaption(c.text,L"确定",L"OK")){++oks;s.okButton=c.hwnd;}
            if(elevationDialogCaption(c.text,L"取消",L"Cancel")){++cancels;s.cancelButton=c.hwnd;}
        }
    }
    const auto input=quietInputDesktop();bool readChecks=true;
    for(const auto& c:s.controls)if(c.hwnd==s.increaseRadio||c.hwnd==s.decreaseRadio)readChecks=readChecks&&c.checkRead&&(c.checkState==BST_CHECKED||c.checkState==BST_UNCHECKED);
    s.semanticRolesComplete=!s.isMainFrame&&s.enumerationComplete&&GetTickCount64()<totalDeadline&&s.processId==GetCurrentProcessId()&&s.threadId!=0&&elevationDialogOnOwnDesktop(hwnd)&&s.desktop.rfind(L"CodexS1_",0)==0&&!input.empty()&&input!=s.desktop&&elevationDialogContains(semantic,L"标高")&&elevationDialogContains(semantic,L"改值")&&changePrompt&&edits==1&&increase==1&&decrease==1&&readChecks&&checked==1&&oks<=1&&cancels<=1;
    s.recognized=s.semanticRolesComplete&&s.className==L"#32770";
    s.recognitionReason=s.recognized?L"own private #32770; actual title/descendant elevation-change semantics; unique visible enabled Edit; actual increase/decrease radio pair and value prompt; optional real OK/Cancel":L"incomplete or unknown panel; snapshot only, no action permitted";
    return s;
}
std::vector<ElevationDialogSnapshot> elevationDialogOwnedSnapshots(){
    std::vector<ElevationDialogSnapshot> result;const auto desktop=quietCurrentDesktop(),input=quietInputDesktop();if(desktop.rfind(L"CodexS1_",0)!=0||input.empty()||desktop==input)return result;
    struct Capture{std::vector<HWND> windows;bool failed{};};Capture capture;
    const auto cb=[](HWND hwnd,LPARAM parameter)->BOOL{auto& c=*reinterpret_cast<Capture*>(parameter);try{DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);if(pid==GetCurrentProcessId()&&IsWindowVisible(hwnd)){if(c.windows.size()>=16){c.failed=true;return FALSE;}c.windows.push_back(hwnd);}return TRUE;}catch(...){c.failed=true;return FALSE;}};
    EnumDesktopWindows(GetThreadDesktop(GetCurrentThreadId()),cb,reinterpret_cast<LPARAM>(&capture));
    const auto deadline=GetTickCount64()+1500;
    for(const auto hwnd:capture.windows){if(GetTickCount64()>=deadline)break;auto snapshot=elevationDialogSnapshot(hwnd,deadline);if(capture.failed){snapshot.enumerationComplete=false;snapshot.recognized=false;snapshot.semanticRolesComplete=false;}result.push_back(std::move(snapshot));}return result;
}
std::string elevationDialogSnapshotJson(const ElevationDialogSnapshot& s){
    std::ostringstream out;out<<"{\"hwnd\":"<<elevationDialogHwnd(s.hwnd)<<",\"processId\":"<<s.processId<<",\"threadId\":"<<s.threadId<<",\"desktop\":"<<quote(s.desktop)<<",\"title\":"<<quote(s.title)<<",\"className\":"<<quote(s.className)<<",\"rect\":"<<elevationDialogRect(s.rect)<<",\"recognized\":"<<(s.recognized?"true":"false")<<",\"semanticRolesComplete\":"<<(s.semanticRolesComplete?"true":"false")<<",\"isMainFrame\":"<<(s.isMainFrame?"true":"false")<<",\"enumerationComplete\":"<<(s.enumerationComplete?"true":"false")<<",\"recognitionReason\":"<<quote(s.recognitionReason)<<",\"roles\":{\"changeEdit\":"<<elevationDialogHwnd(s.changeEdit)<<",\"increaseRadio\":"<<elevationDialogHwnd(s.increaseRadio)<<",\"decreaseRadio\":"<<elevationDialogHwnd(s.decreaseRadio)<<",\"okButton\":"<<elevationDialogHwnd(s.okButton)<<",\"cancelButton\":"<<elevationDialogHwnd(s.cancelButton)<<"},\"controls\":[";
    for(std::size_t i=0;i<s.controls.size();++i){const auto& c=s.controls[i];if(i)out<<',';out<<"{\"hwnd\":"<<elevationDialogHwnd(c.hwnd)<<",\"parent\":"<<elevationDialogHwnd(c.parent)<<",\"processId\":"<<c.processId<<",\"threadId\":"<<c.threadId<<",\"controlId\":"<<c.controlId<<",\"className\":"<<quote(c.className)<<",\"text\":"<<quote(c.text)<<",\"style\":"<<static_cast<unsigned long long>(static_cast<DWORD>(c.style))<<",\"exStyle\":"<<static_cast<unsigned long long>(static_cast<DWORD>(c.exStyle))<<",\"visible\":"<<(c.visible?"true":"false")<<",\"enabled\":"<<(c.enabled?"true":"false")<<",\"rectRead\":"<<(c.rectRead?"true":"false")<<",\"rect\":"<<elevationDialogRect(c.rect)<<",\"checkRead\":"<<(c.checkRead?"true":"false")<<",\"checkState\":"<<c.checkState<<'}';}
    out<<"]}";return out.str();
}
struct ElevationDialogDiscoveryState {
    std::wstring output,desktop;DWORD processId{},threadId{};UINT_PTR timer{};ULONGLONG started{};bool inside{},insidePrompt{},commandInProgress{},recognized{},snapshotCaptured{},promptCaptured{},autoClosed{},cancelAttempted{},cancelDelivered{},closeObserved{},timedOut{},timerStopped{},timerStopReturned{},promptAdded{},promptRemoved{},promptRetained{};int promptAddStatus{-9999},promptRemoveStatus{-9999};std::vector<std::string> promptEvents;
    std::string snapshot="null",latest="[]",captureSource="null",cancelMethod="null",error="null";DWORD cancelWin32Error{};int sweeps{};HWND recognizedHwnd{};const AxisNativeTraceData* trace{};std::vector<HWND> preexisting;
};
ElevationDialogDiscoveryState* elevationDialogDiscovery{};
bool elevationDialogTimerRetained{}; // Root must include this in unload/pending guards.
std::string elevationDialogDiscoveryJson(const ElevationDialogDiscoveryState& s){return "{\"processId\":"+std::to_string(s.processId)+",\"threadId\":"+std::to_string(s.threadId)+",\"desktop\":"+quote(s.desktop)+",\"sweeps\":"+std::to_string(s.sweeps)+",\"recognized\":"+(s.recognized?"true":"false")+",\"snapshotCaptured\":"+(s.snapshotCaptured?"true":"false")+",\"promptCaptured\":"+(s.promptCaptured?"true":"false")+",\"autoClosed\":"+(s.autoClosed?"true":"false")+",\"promptObserverAdded\":"+(s.promptAdded?"true":"false")+",\"promptObserverRemoved\":"+(s.promptRemoved?"true":"false")+",\"promptObserverRetained\":"+(s.promptRetained?"true":"false")+",\"promptAddStatus\":"+std::to_string(s.promptAddStatus)+",\"promptRemoveStatus\":"+std::to_string(s.promptRemoveStatus)+",\"promptCaptureEvents\":"+axisTraceArray(s.promptEvents)+",\"captureSource\":"+s.captureSource+",\"snapshot\":"+s.snapshot+",\"latestSnapshots\":"+s.latest+",\"cancelAttempted\":"+(s.cancelAttempted?"true":"false")+",\"cancelDelivered\":"+(s.cancelDelivered?"true":"false")+",\"cancelMethod\":"+s.cancelMethod+",\"cancelWin32Error\":"+std::to_string(s.cancelWin32Error)+",\"closeObserved\":"+(s.closeObserved?"true":"false")+",\"timedOut\":"+(s.timedOut?"true":"false")+",\"timerStopReturned\":"+(s.timerStopReturned?"true":"false")+",\"timerStopped\":"+(s.timerStopped?"true":"false")+",\"error\":"+s.error+"}";}
void elevationDialogDiscoverSweep(ElevationDialogDiscoveryState& s,const wchar_t* source,bool allowClose=false){
    if(GetCurrentProcessId()!=s.processId||GetCurrentThreadId()!=s.threadId||quietCurrentDesktop()!=s.desktop||quietInputDesktop()==s.desktop)throw std::runtime_error("elevation panel sweep identity changed");
    ++s.sweeps;auto snapshots=elevationDialogOwnedSnapshots();std::vector<std::string> json;int matches=0;ElevationDialogSnapshot match;
    bool targetStarted=false;if(s.trace)for(auto event:s.trace->commands){for(auto& c:event)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');if(event.find("TCHANGEVALELEV")!=std::string::npos&&event.find("COMMANDWILLSTART")!=std::string::npos)targetStarted=true;}
    for(const auto& candidate:snapshots){json.push_back(elevationDialogSnapshotJson(candidate));const bool old=std::find(s.preexisting.begin(),s.preexisting.end(),candidate.hwnd)!=s.preexisting.end();if(candidate.semanticRolesComplete&&targetStarted&&!old&&!candidate.isMainFrame){++matches;match=candidate;}}
    s.latest=axisTraceArray(json);if(matches==1&&(!s.snapshotCaptured||s.insidePrompt)){
        s.snapshotCaptured=true;s.recognized=match.recognized;s.recognizedHwnd=match.hwnd;s.snapshot=elevationDialogSnapshotJson(match);s.captureSource=quote(source);if(s.insidePrompt)s.promptCaptured=true;
    }
    if(allowClose&&!s.commandInProgress&&!s.insidePrompt&&s.snapshotCaptured&&matches==1&&match.hwnd==s.recognizedHwnd&&!s.cancelAttempted){
        // No input-reactor or timer callback ever enters this action branch.
        const auto fresh=elevationDialogSnapshot(match.hwnd);
        if(!fresh.recognized||fresh.isMainFrame||fresh.hwnd==elevationDialogKnownMainFrame)return;
        if(fresh.changeEdit!=match.changeEdit||fresh.increaseRadio!=match.increaseRadio||fresh.decreaseRadio!=match.decreaseRadio)throw std::runtime_error("elevation panel changed before exact cancellation");
        s.recognized=true;s.cancelAttempted=true;
        DWORD_PTR returned{};SetLastError(0);
        if(fresh.cancelButton){const int actualId=GetDlgCtrlID(fresh.cancelButton);if(actualId<=0||actualId>65535)throw std::runtime_error("actual cancel control ID cannot be represented safely");s.cancelMethod=quote(L"WM_COMMAND actual observed unique Cancel button");s.cancelDelivered=SendMessageTimeoutW(fresh.hwnd,WM_COMMAND,MAKEWPARAM(actualId,BN_CLICKED),reinterpret_cast<LPARAM>(fresh.cancelButton),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&returned)!=0;}
        else{s.cancelMethod=quote(L"WM_CLOSE exact semantically identified own parameter panel; no OK/Cancel assumed");s.cancelDelivered=SendMessageTimeoutW(fresh.hwnd,WM_CLOSE,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&returned)!=0;}
        s.cancelWin32Error=s.cancelDelivered?0:GetLastError();s.closeObserved=!IsWindow(fresh.hwnd)||!IsWindowVisible(fresh.hwnd);
    }
    if(GetTickCount64()-s.started>20000&&!s.snapshotCaptured)s.timedOut=true;
    quietAtomic(s.output+L"\\elevation-dialog-progress.json",elevationDialogDiscoveryJson(s),s.output);
}
void CALLBACK elevationDialogDiscoveryTimer(HWND,UINT,UINT_PTR timer,DWORD)noexcept{
    auto* state=elevationDialogDiscovery;if(!state||state->timer!=timer||state->inside||state->insidePrompt)return;state->inside=true;
    try{if(!state->snapshotCaptured&&!state->timedOut)elevationDialogDiscoverSweep(*state,L"UI-thread timer read-only snapshot",false);}
    catch(const std::exception& failure){state->error=familyError(failure);try{quietAtomic(state->output+L"\\elevation-dialog-progress.json",elevationDialogDiscoveryJson(*state),state->output);}catch(...){}}
    catch(...){state->error=quote(L"unknown read-only elevation timer exception");}state->inside=false;
}
class ElevationDialogTimerScope {
    ElevationDialogDiscoveryState& state_;bool stopped_{};
public:
    explicit ElevationDialogTimerScope(ElevationDialogDiscoveryState& state):state_(state){if(elevationDialogDiscovery||elevationDialogTimerRetained)throw std::runtime_error("another elevation discovery timer remains");elevationDialogDiscovery=&state_;state_.timer=SetTimer(nullptr,0,120,elevationDialogDiscoveryTimer);if(!state_.timer){elevationDialogDiscovery=nullptr;throw std::runtime_error("elevation discovery timer creation failed");}}
    ElevationDialogTimerScope(const ElevationDialogTimerScope&)=delete;ElevationDialogTimerScope& operator=(const ElevationDialogTimerScope&)=delete;
    void stop()noexcept{if(stopped_)return;stopped_=true;state_.timerStopReturned=true;state_.timerStopped=KillTimer(nullptr,state_.timer)!=FALSE;elevationDialogTimerRetained=elevationDialogTimerRetained||!state_.timerStopped;elevationDialogDiscovery=nullptr;state_.timer=0;}
    ~ElevationDialogTimerScope(){stop();}
};
class ElevationDialogPromptObserver final:public AcEdInputContextReactor {
public:
    ElevationDialogDiscoveryState* state{};
    explicit ElevationDialogPromptObserver(ElevationDialogDiscoveryState& value):state(&value){}
    void capture(const wchar_t* event,const ACHAR* prompt)noexcept{
        auto* s=state;if(!s||!s->commandInProgress||s->insidePrompt||s->inside)return;s->insidePrompt=true;
        try{
            if(s->promptEvents.size()>=8)throw std::runtime_error("elevation prompt snapshot budget exhausted");
            elevationDialogDiscoverSweep(*s,event,false); // Pure snapshot: NEVER closes or changes controls.
            s->promptEvents.push_back("{\"event\":"+quote(event)+",\"prompt\":"+(prompt?quote(prompt):"null")+",\"processId\":"+std::to_string(GetCurrentProcessId())+",\"threadId\":"+std::to_string(GetCurrentThreadId())+",\"desktop\":"+quote(quietCurrentDesktop())+",\"snapshotCaptured\":"+(s->snapshotCaptured?"true":"false")+",\"windowSnapshots\":"+s->latest+",\"controlsWritten\":false,\"closeAttempted\":false}");
            quietAtomic(s->output+L"\\elevation-dialog-progress.json",elevationDialogDiscoveryJson(*s),s->output);
        }catch(const std::exception& failure){s->error=familyError(failure);}catch(...){s->error=quote(L"unknown public prompt snapshot exception");}s->insidePrompt=false;
    }
    void beginEntsel(const ACHAR* prompt,int,const ACHAR*)override{capture(L"beginEntsel",prompt);}
    void beginSSGet(const ACHAR* prompt,int,const ACHAR*,const ACHAR*,const AcArray<AcGePoint3d>&,const resbuf*)override{capture(L"beginSSGet",prompt);}
    void beginGetPoint(const AcGePoint3d*,const ACHAR* prompt,int,const ACHAR*)override{capture(L"beginGetPoint",prompt);}
};
ElevationDialogPromptObserver* elevationDialogRetainedPromptObserver{};
class ElevationDialogPromptScope {
    ElevationDialogDiscoveryState& state_;AcEdInputPointManager* manager_{};std::unique_ptr<ElevationDialogPromptObserver> observer_;bool attached_{};
public:
    ElevationDialogPromptScope(AcApDocument* document,ElevationDialogDiscoveryState& state):state_(state){
        if(elevationDialogRetainedPromptObserver||elevationDialogTimerRetained)throw std::runtime_error("previous elevation callback remains retained");
        manager_=document?document->inputPointManager():nullptr;if(!manager_)throw std::runtime_error("elevation prompt input manager unavailable");
        observer_.reset(new ElevationDialogPromptObserver(state_));
        try{attached_=true;state_.promptAddStatus=manager_->addInputContextReactor(observer_.get());state_.promptAdded=state_.promptAddStatus==Acad::eOk;if(!state_.promptAdded)throw std::runtime_error("elevation public prompt reactor registration failed");}
        catch(...){stop();throw;}
    }
    ElevationDialogPromptScope(const ElevationDialogPromptScope&)=delete;ElevationDialogPromptScope& operator=(const ElevationDialogPromptScope&)=delete;
    void stop()noexcept{
        if(!attached_)return;observer_->state=nullptr;
        try{state_.promptRemoveStatus=manager_->removeInputContextReactor(observer_.get());state_.promptRemoved=state_.promptRemoveStatus==Acad::eOk||state_.promptRemoveStatus==Acad::eInvalidKey;}
        catch(...){state_.promptRemoved=false;}
        attached_=false;if(state_.promptRemoved){observer_.reset();return;}
        state_.promptRetained=true;elevationDialogTimerRetained=true;elevationDialogRetainedPromptObserver=observer_.release();
        try{if(nativeApplicationId)acrxDynamicLinker->lockApplication(nativeApplicationId);}catch(...){}
    }
    ~ElevationDialogPromptScope(){stop();}
};
std::pair<int,std::wstring> elevationDialogCommandState(){resbuf a{},n{};int active=-1;std::wstring names;if(acedGetVar(L"CMDACTIVE",&a)==RTNORM&&a.restype==RTSHORT)active=a.resval.rint;if(acedGetVar(L"CMDNAMES",&n)==RTNORM&&n.restype==RTSTR&&n.resval.rstring){names=n.resval.rstring;acutDelString(n.resval.rstring);}return {active,names};}
void nativeElevationDialogDiscover(){
    try{
        quietRequireIsolatedWorker();const auto p=paths();const auto control=fullPath(p.output+L"\\control.dwg"),report=fullPath(p.output+L"\\elevation-dialog-native.json");requireNewFile(report);
        std::string before="[]",after="[]",error="null";int initial=-1,final=-1,dbmodBefore=-1,dbmodAfter=-1,returned=-9999;bool attempted=false,completed=false;std::pair<int,std::wstring> commandBefore{-1,L""},commandAfter{-1,L""};auto trace=std::make_shared<AxisNativeTraceData>();ElevationDialogDiscoveryState state;state.output=p.output;state.desktop=quietCurrentDesktop();state.processId=GetCurrentProcessId();state.threadId=GetCurrentThreadId();state.started=GetTickCount64();
        try{
            auto* doc=acDocManager?acDocManager->curDocument():nullptr;auto* db=doc?doc->database():nullptr;if(!db||acDocManager->isApplicationContext()||acDocManager->mdiActiveDocument()!=doc||acdbHostApplicationServices()->workingDatabase()!=db||!doc->fileName()||_wcsicmp(fullPath(doc->fileName()).c_str(),control.c_str())||db->transactionManager()->numActiveTransactions()!=0)throw std::runtime_error("elevation discovery requires owned current/MDI/working control and zero transactions");
            const auto ids=modelIds(*db);initial=ids.length();if(initial!=213)throw std::runtime_error("elevation discovery requires complete frozen213 work copy");before=familyCompactArray(ids);dbmodBefore=axisExplodeDbmod();commandBefore=elevationDialogCommandState();
            elevationDialogKnownMainFrame=adsw_acadMainWnd();for(const auto& existing:elevationDialogOwnedSnapshots())state.preexisting.push_back(existing.hwnd);state.trace=trace.get();
            {AxisNativeTraceOwner owner(doc,trace);if(!trace->editorAdded||!trace->inputAddReturned||trace->inputAddStatus!=Acad::eOk)throw std::runtime_error("elevation command trace unavailable");
                ElevationDialogPromptScope prompts(doc,state);ElevationDialogTimerScope timer(state);state.commandInProgress=true;attempted=true;
                returned=acedCommandS(RTSTR,L"TChangeValElev",RTSTR,L"",RTNONE);state.commandInProgress=false;
                prompts.stop();timer.stop();
                if(state.snapshotCaptured&&state.promptCaptured&&state.recognizedHwnd&&(!IsWindow(state.recognizedHwnd)||!IsWindowVisible(state.recognizedHwnd))){state.autoClosed=true;state.closeObserved=true;}
                else elevationDialogDiscoverSweep(state,L"read-only sweep and fresh known-panel close after zero-selection returned",true);
                if(state.recognizedHwnd)state.closeObserved=!IsWindow(state.recognizedHwnd)||!IsWindowVisible(state.recognizedHwnd);
            }
            commandAfter=elevationDialogCommandState();dbmodAfter=axisExplodeDbmod();final=modelCount(*db);after=familyCompactArray(modelIds(*db));
            const bool endedPanel=(state.autoClosed&&state.promptCaptured&&!state.cancelAttempted&&!state.cancelDelivered)||(state.recognized&&state.cancelDelivered);
            completed=state.error=="null"&&state.snapshotCaptured&&endedPanel&&state.closeObserved&&state.promptAdded&&state.promptRemoved&&!state.promptRetained&&state.timerStopped&&!elevationDialogTimerRetained&&trace->editorRemoved&&trace->inputRemoveReturned&&(trace->inputRemoveStatus==Acad::eOk||trace->inputRemoveStatus==Acad::eInvalidKey)&&!trace->retained&&!textBridgeHasUnbalancedCommandTrace(*trace)&&initial==final&&before==after&&dbmodBefore>=0&&dbmodBefore==dbmodAfter&&commandBefore==commandAfter&&(returned==RTNORM||returned==RTCAN);
            if(!completed)error=quote(L"elevation dialog discovery/cancellation or no-change contract not fully verified; no edit/radio/object selection was performed");
        }catch(const std::exception& failure){error=familyError(failure);}
        const auto out="{\"contract\":\"stage1.elevation-dialog-discover-native.v2\",\"api\":\"ObjectARX native + read-only standard Win32 control discovery; no COM\",\"controlDWG\":"+quote(control)+",\"initialModelCount\":"+std::to_string(initial)+",\"finalModelCount\":"+std::to_string(final)+",\"allModelBefore\":"+before+",\"allModelAfter\":"+after+",\"dbmodBefore\":"+std::to_string(dbmodBefore)+",\"dbmodAfter\":"+std::to_string(dbmodAfter)+",\"commandAttempted\":"+(attempted?"true":"false")+",\"commandName\":\"TChangeValElev\",\"commandStatus\":"+std::to_string(returned)+",\"cmdaBefore\":"+std::to_string(commandBefore.first)+",\"cmdaAfter\":"+std::to_string(commandAfter.first)+",\"cmdnamesBefore\":"+quote(commandBefore.second)+",\"cmdnamesAfter\":"+quote(commandAfter.second)+",\"discovery\":"+elevationDialogDiscoveryJson(state)+",\"commandTrace\":"+axisTraceArray(trace->commands)+",\"inputTrace\":"+axisTraceArray(trace->inputs)+",\"trace\":"+axisContinuousTraceMeta(*trace)+",\"callbackRetained\":"+(elevationDialogTimerRetained?"true":"false")+",\"dwgSaved\":false,\"fieldsChanged\":false,\"radioChanged\":false,\"objectsSelectedByProbe\":false,\"comUsed\":false,\"computerUseUsed\":false,\"completed\":"+(completed?"true":"false")+",\"error\":"+error+"}";writeNewReport(report,out);
    }catch(const std::exception& failure){acutPrintf(L"\nELEVATION_DIALOG_DISCOVERY_FAILED %hs\n",failure.what());}catch(...){acutPrintf(L"\nELEVATION_DIALOG_DISCOVERY_FAILED unknown exception\n");}
}
