// Include after NativeElevationDialogProbe.inc; leave passed Discovery/Core intact.
// Public command + dynamically discovered standard controls on own private UI thread.
enum class ElevationUiMode { ObserveOnly, SetupOnly, Apply };
struct ElevationUiDefaults {std::wstring rawValue;bool increaseChecked{},decreaseChecked{},valid{};};
struct ElevationUiRequest {std::wstring id,rawValue;bool increase{};ElevationUiMode mode{ElevationUiMode::ObserveOnly};AcDbObjectId target;AcGePoint3d pickWcs;};
struct ElevationUiResult {std::string json;bool completed{},safeToContinue{};ElevationUiDefaults originalDefaults;};
bool elevationUiBridgeRetained{};
const wchar_t* elevationUiModeName(ElevationUiMode mode){return mode==ElevationUiMode::Apply?L"Apply":mode==ElevationUiMode::SetupOnly?L"SetupOnly":L"ObserveOnly";}
bool elevationUiNumericRaw(const std::wstring& value){if(value.empty()||value.size()>64)return false;bool digit=false,dot=false;for(std::size_t i=0;i<value.size();++i){const auto c=value[i];if(c>=L'0'&&c<=L'9'){digit=true;continue;}if(c==L'.'&&!dot){dot=true;continue;}if(i==0&&(c==L'+'||c==L'-'))continue;return false;}return digit;}
const ElevationDialogControl* elevationUiControl(const ElevationDialogSnapshot& s,HWND hwnd){for(const auto& c:s.controls)if(c.hwnd==hwnd)return &c;return nullptr;}
ElevationUiDefaults elevationUiDefaults(const ElevationDialogSnapshot& s){ElevationUiDefaults d;const auto* edit=elevationUiControl(s,s.changeEdit);const auto* plus=elevationUiControl(s,s.increaseRadio);const auto* minus=elevationUiControl(s,s.decreaseRadio);if(edit&&plus&&minus){d.rawValue=edit->text;d.increaseChecked=plus->checkRead&&plus->checkState==BST_CHECKED;d.decreaseChecked=minus->checkRead&&minus->checkState==BST_CHECKED;d.valid=s.recognized&&elevationUiNumericRaw(d.rawValue)&&d.increaseChecked!=d.decreaseChecked;}return d;}
bool elevationUiDesired(const ElevationUiDefaults& d,const ElevationUiRequest& r){return d.valid&&d.rawValue==r.rawValue&&d.increaseChecked==r.increase&&d.decreaseChecked!=r.increase;}
bool elevationUiCommandVerified(const AxisNativeTraceData& trace,ElevationUiMode mode){int starts=0,ends=0,cancels=0;for(auto e:trace.commands){for(auto& c:e)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');if(e.find("COMMANDFAILED")!=std::string::npos||e.find("UNKNOWNCOMMAND")!=std::string::npos)return false;if(e.find("TCHANGEVALELEV")==std::string::npos)continue;if(e.find("COMMANDWILLSTART")!=std::string::npos)++starts;if(e.find("COMMANDENDED")!=std::string::npos)++ends;if(e.find("COMMANDCANCELLED")!=std::string::npos)++cancels;}return starts==1&&(mode==ElevationUiMode::Apply?ends==1&&cancels==0:ends+cancels==1);}
std::string elevationUiDefaultsJson(const ElevationUiDefaults& d){return "{\"rawValue\":"+quote(d.rawValue)+",\"increaseChecked\":"+(d.increaseChecked?"true":"false")+",\"decreaseChecked\":"+(d.decreaseChecked?"true":"false")+",\"valid\":"+(d.valid?"true":"false")+"}";}
struct ElevationUiAttempt {
    Paths paths;AcApDocument* document{};AcDbDatabase* database{};ElevationUiRequest request;
    DWORD pid{},thread{};std::wstring desktop,inputDesktop;bool contextGuardPassed{},inside{},commandInProgress{},commandDispatched{},captured{},configured{},configurationAttempted{},configurationVerified{},panelClosed{},autoClosed{},closeDelivered{},observerAdded{},observerRemoved{},observerRetained{},completed{},safe{},lifecycleSafe{},modelProtectionVerified{},modelMembershipUnchanged{},selectionVerified{},pickMatchesActualPublicGrip{};
    int observerAdd{-9999},observerRemove{-9999},commandStatus{-9999},txBefore{-1},txAfter{-1},initialCount{-1},finalCount{-1},protectedCount{-1},transformStatus{-9999},cmdaBefore{-1},cmdaAfter{-1},nonEmptySSGetSelections{},nonEmptyEntselSelections{};HWND panel{};AcGePoint3d pickUcs;std::wstring expectedPickedHandle,targetClass,cmdnamesBefore,cmdnamesAfter;std::vector<std::wstring> actualSelected;std::vector<std::string> unitLabels;
    ElevationUiDefaults original,after;std::string beforeSnapshot="null",afterSnapshot="null",error="null",protectedBefore="[]",protectedAfter="[]",targetBefore="null",targetAfter="null";std::vector<std::string> events,messages;
    std::shared_ptr<AxisNativeTraceData> trace{std::make_shared<AxisNativeTraceData>()};
    unsigned long long attemptSerial{},progressSerial{};DWORD_PTR lastMessageReply{};bool defaultsReceiptPersisted{},progressFailed{};std::wstring defaultsReceipt;
};
unsigned long long elevationUiNextAttemptSerial{},elevationUiNextFileSerial{};
// These are diagnostics, never a completion/default-restoration receipt.  In
// particular a same-queue SendMessageTimeout invokes WndProc directly and its
// timeout is ignored; the durable "before" record must precede that call.
void elevationUiWriteFile(const std::wstring& file,const std::string& data,bool append,bool createNew){
    HANDLE h=CreateFileW(file.c_str(),append?FILE_APPEND_DATA:GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,createNew?CREATE_NEW:append?OPEN_ALWAYS:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("elevation UI diagnostic file create failed");
    DWORD count{};const bool ok=data.size()<=0x7fffffff&&WriteFile(h,data.data(),static_cast<DWORD>(data.size()),&count,nullptr)&&count==data.size()&&FlushFileBuffers(h);const bool closed=CloseHandle(h)!=FALSE;
    if(!ok||!closed)throw std::runtime_error("elevation UI diagnostic file write/flush failed");
}
void elevationUiAtomic(const Paths& p,const std::wstring& file,const std::string& data){
    if(!below(file,p.output))throw std::runtime_error("elevation UI progress path escaped run");
    const auto temporary=file+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++elevationUiNextFileSerial)+L".tmp";
    elevationUiWriteFile(temporary,data,false,true);bool moved=false;
    // Bounded retries, no UI pumping/sleep and no use of worker-state.json.
    for(int retry=0;retry<4&&!moved;++retry)moved=MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!moved){DeleteFileW(temporary.c_str());throw std::runtime_error("elevation UI progress atomic publish failed");}
}
void elevationUiProgress(ElevationUiAttempt& s,const wchar_t* stage,const std::string& pending="null"){
    try{FILETIME utc{};GetSystemTimeAsFileTime(&utc);ULARGE_INTEGER ticks{};ticks.LowPart=utc.dwLowDateTime;ticks.HighPart=utc.dwHighDateTime;
        const auto data="{\"contract\":\"stage1.elevation-ui-progress.v2\",\"attemptSerial\":"+std::to_string(s.attemptSerial)+",\"serial\":"+std::to_string(++s.progressSerial)+",\"utcFileTime\":"+std::to_string(ticks.QuadPart)+",\"tickMs\":"+std::to_string(GetTickCount64())+",\"nonce\":"+(quietWorker?quote(quietWorker->nonce):"null")+",\"session\":"+(quietWorker?quote(quietWorker->session):"null")+",\"run\":"+quote(fullPath(s.paths.output))+",\"processId\":"+std::to_string(s.pid)+",\"threadId\":"+std::to_string(s.thread)+",\"desktop\":"+quote(s.desktop)+",\"inputDesktop\":"+quote(s.inputDesktop)+",\"id\":"+quote(s.request.id)+",\"mode\":"+quote(elevationUiModeName(s.request.mode))+",\"stage\":"+quote(stage)+",\"pendingMessage\":"+pending+",\"requestedRawValue\":"+quote(s.request.rawValue)+",\"requestedIncrease\":"+(s.request.increase?"true":"false")+",\"originalDefaults\":"+elevationUiDefaultsJson(s.original)+",\"afterDefaults\":"+elevationUiDefaultsJson(s.after)+",\"defaultsReceiptPersisted\":"+(s.defaultsReceiptPersisted?"true":"false")+",\"defaultsReceipt\":"+(s.defaultsReceipt.empty()?"null":quote(s.defaultsReceipt))+",\"configurationAttempted\":"+(s.configurationAttempted?"true":"false")+",\"configurationVerified\":"+(s.configurationVerified?"true":"false")+",\"commandDispatched\":"+(s.commandDispatched?"true":"false")+",\"commandStatus\":"+std::to_string(s.commandStatus)+",\"commandInProgress\":"+(s.commandInProgress?"true":"false")+",\"completedMessages\":"+axisTraceArray(s.messages)+",\"error\":"+s.error+",\"scope\":\"diagnostic progress only; no business completion or defaults restoration assertion\"}";
        elevationUiWriteFile(s.paths.output+L"\\elevation-ui-bridge-progress.jsonl",data+"\n",true,false);
        elevationUiAtomic(s.paths,s.paths.output+L"\\elevation-ui-bridge-progress.json",data);
    }catch(...){s.progressFailed=true;throw;}
}
void elevationUiPersistDefaults(ElevationUiAttempt& s){
    if(!s.original.valid)throw std::runtime_error("cannot persist invalid elevation defaults");
    s.defaultsReceipt=s.paths.output+L"\\elevation-ui-defaults-attempt-"+std::to_wstring(s.attemptSerial)+L".json";
    const auto data="{\"contract\":\"stage1.elevation-ui-defaults-before-setter.v2\",\"attemptSerial\":"+std::to_string(s.attemptSerial)+",\"nonce\":"+(quietWorker?quote(quietWorker->nonce):"null")+",\"session\":"+(quietWorker?quote(quietWorker->session):"null")+",\"run\":"+quote(fullPath(s.paths.output))+",\"processId\":"+std::to_string(s.pid)+",\"threadId\":"+std::to_string(s.thread)+",\"desktop\":"+quote(s.desktop)+",\"id\":"+quote(s.request.id)+",\"mode\":"+quote(elevationUiModeName(s.request.mode))+",\"originalDefaults\":"+elevationUiDefaultsJson(s.original)+",\"beforeControls\":"+s.beforeSnapshot+",\"capturedBeforeAnySetter\":true,\"defaultsRestorationProven\":false}";
    if(!below(s.defaultsReceipt,s.paths.output))throw std::runtime_error("elevation defaults receipt path escaped run");elevationUiWriteFile(s.defaultsReceipt,data,false,true);s.defaultsReceiptPersisted=true;
}
void elevationUiGuard(const ElevationUiAttempt& s){
    quietRequireIsolatedWorker();
    if(elevationUiBridgeRetained||elevationDialogTimerRetained||GetCurrentProcessId()!=s.pid||GetCurrentThreadId()!=s.thread||quietCurrentDesktop()!=s.desktop||quietInputDesktop()==s.desktop||!acDocManager||acDocManager->isApplicationContext()||acDocManager->curDocument()!=s.document||acDocManager->mdiActiveDocument()!=s.document||s.document->database()!=s.database||acdbHostApplicationServices()->workingDatabase()!=s.database||!s.document->fileName()||_wcsicmp(fullPath(s.document->fileName()).c_str(),fullPath(s.paths.output+L"\\control.dwg").c_str())||s.database->transactionManager()->numActiveTransactions()!=0)throw std::runtime_error("elevation UI bridge own private/current/MDI/working/zero-TX guard failed");
}
bool elevationUiMessage(ElevationUiAttempt& s,HWND panel,HWND target,const wchar_t* name,UINT message,WPARAM w,LPARAM l,bool nonzeroReply=false);
std::wstring elevationUiReadText(ElevationUiAttempt& s,HWND panel,HWND hwnd){wchar_t value[2048]{};if(!elevationUiMessage(s,panel,hwnd,L"WM_GETTEXT fresh owned snapshot",WM_GETTEXT,2048,reinterpret_cast<LPARAM>(value)))throw std::runtime_error("elevation UI fresh control text read failed");return value;}
// Bridge-local readback logs every explicit WM_GETTEXT/BM_GETCHECK as well as
// setters.  Passed read-only Discovery is left byte-identical.
ElevationDialogSnapshot elevationUiReadSnapshot(ElevationUiAttempt& state,HWND hwnd){
    ElevationDialogSnapshot s;s.hwnd=hwnd;s.isMainFrame=hwnd==elevationDialogKnownMainFrame;s.threadId=GetWindowThreadProcessId(hwnd,&s.processId);s.desktop=state.desktop;s.title=elevationUiReadText(state,hwnd,hwnd);s.className=elevationDialogClass(hwnd);GetWindowRect(hwnd,&s.rect);
    struct Capture{ElevationUiAttempt* state;ElevationDialogSnapshot* snapshot;ULONGLONG deadline;bool failed{};};Capture capture{&state,&s,GetTickCount64()+8000};
    const auto cb=[](HWND child,LPARAM data)->BOOL{auto& c=*reinterpret_cast<Capture*>(data);try{
        if(c.snapshot->controls.size()>=64||GetTickCount64()>=c.deadline){c.failed=true;return FALSE;}
        ElevationDialogControl v;v.hwnd=child;v.parent=GetParent(child);v.threadId=GetWindowThreadProcessId(child,&v.processId);if(v.processId!=c.state->pid||v.threadId!=c.state->thread){c.failed=true;return FALSE;}
        v.controlId=GetDlgCtrlID(child);v.className=elevationDialogClass(child);v.text=elevationUiReadText(*c.state,c.snapshot->hwnd,child);v.style=GetWindowLongPtrW(child,GWL_STYLE);v.exStyle=GetWindowLongPtrW(child,GWL_EXSTYLE);v.visible=IsWindowVisible(child)!=FALSE;v.enabled=IsWindowEnabled(child)!=FALSE;v.rectRead=GetWindowRect(child,&v.rect)!=FALSE;
        if(!_wcsicmp(v.className.c_str(),L"Button")){const auto kind=static_cast<DWORD>(v.style)&BS_TYPEMASK;if(kind==BS_RADIOBUTTON||kind==BS_AUTORADIOBUTTON){
            const auto first=c.state->messages.size();v.checkRead=elevationUiMessage(*c.state,c.snapshot->hwnd,child,L"BM_GETCHECK fresh actual radio",BM_GETCHECK,0,0);if(v.checkRead&&c.state->messages.size()>first){
                // Re-read is unnecessary: the synchronous result is captured by
                // elevationUiMessage in the attempt's lastMessageReply field.
                v.checkState=static_cast<LRESULT>(c.state->lastMessageReply);
            }}}
        c.snapshot->controls.push_back(std::move(v));return TRUE;
    }catch(...){c.failed=true;return FALSE;}};
    EnumChildWindows(hwnd,cb,reinterpret_cast<LPARAM>(&capture));s.enumerationComplete=!capture.failed;
    std::wstring semantic=s.title;int edits=0,increase=0,decrease=0,oks=0,cancels=0,checked=0;bool changePrompt=false,readChecks=true;
    for(const auto& c:s.controls){if(c.visible){semantic+=L"\n"+c.text;if(elevationDialogContains(c.text,L"变化值"))changePrompt=true;}if(!_wcsicmp(c.className.c_str(),L"Edit")&&c.visible&&c.enabled){++edits;s.changeEdit=c.hwnd;}
        if(_wcsicmp(c.className.c_str(),L"Button")||!c.visible||!c.enabled)continue;const auto kind=static_cast<DWORD>(c.style)&BS_TYPEMASK;
        if(kind==BS_RADIOBUTTON||kind==BS_AUTORADIOBUTTON){if(elevationDialogContains(c.text,L"增加")){++increase;s.increaseRadio=c.hwnd;if(c.checkRead&&c.checkState==BST_CHECKED)++checked;}if(elevationDialogContains(c.text,L"减少")){++decrease;s.decreaseRadio=c.hwnd;if(c.checkRead&&c.checkState==BST_CHECKED)++checked;}}
        else if(kind==BS_PUSHBUTTON||kind==BS_DEFPUSHBUTTON){if(elevationDialogCaption(c.text,L"确定",L"OK")){++oks;s.okButton=c.hwnd;}if(elevationDialogCaption(c.text,L"取消",L"Cancel")){++cancels;s.cancelButton=c.hwnd;}}}
    for(const auto& c:s.controls)if(c.hwnd==s.increaseRadio||c.hwnd==s.decreaseRadio)readChecks=readChecks&&c.checkRead&&(c.checkState==BST_CHECKED||c.checkState==BST_UNCHECKED);
    s.semanticRolesComplete=!s.isMainFrame&&s.enumerationComplete&&s.processId==state.pid&&s.threadId==state.thread&&elevationDialogOnOwnDesktop(hwnd)&&elevationDialogContains(semantic,L"标高")&&elevationDialogContains(semantic,L"改值")&&changePrompt&&edits==1&&increase==1&&decrease==1&&checked==1&&readChecks&&oks<=1&&cancels<=1;
    s.recognized=s.semanticRolesComplete&&s.className==L"#32770";s.recognitionReason=s.recognized?L"fresh own private standard elevation roles; every synchronous text/check message durably bracketed":L"unknown/incomplete panel; read only, no action";return s;
}
ElevationDialogSnapshot elevationUiFresh(ElevationUiAttempt& s){
    elevationUiGuard(s);elevationUiProgress(s,L"before fresh snapshot enumeration");
    struct Tops{DWORD pid,thread;std::vector<HWND> windows;bool overflow{};};Tops tops{s.pid,s.thread};const auto cb=[](HWND hwnd,LPARAM data)->BOOL{auto& t=*reinterpret_cast<Tops*>(data);DWORD pid{};const auto thread=GetWindowThreadProcessId(hwnd,&pid);if(pid==t.pid&&thread==t.thread&&IsWindowVisible(hwnd)&&elevationDialogClass(hwnd)==L"#32770"&&hwnd!=elevationDialogKnownMainFrame){if(t.windows.size()>=8){t.overflow=true;return FALSE;}t.windows.push_back(hwnd);}return TRUE;};
    EnumDesktopWindows(GetThreadDesktop(s.thread),cb,reinterpret_cast<LPARAM>(&tops));if(tops.overflow)throw std::runtime_error("elevation fresh standard dialog enumeration budget exceeded");int count=0;ElevationDialogSnapshot found;
    for(auto hwnd:tops.windows){const auto candidate=elevationUiReadSnapshot(s,hwnd);if(candidate.recognized&&!candidate.isMainFrame){++count;found=candidate;}}
    if(count!=1)throw std::runtime_error("elevation UI bridge requires one fresh recognized panel; no HWND or ID fallback");
    found=elevationUiReadSnapshot(s,found.hwnd);
    if(!found.recognized||found.isMainFrame||found.threadId!=s.thread||found.processId!=s.pid||found.desktop!=s.desktop)throw std::runtime_error("elevation UI bridge fresh panel identity differs");
    for(const auto hwnd:{found.changeEdit,found.increaseRadio,found.decreaseRadio}){const auto* c=elevationUiControl(found,hwnd);if(!c||c->threadId!=s.thread||c->processId!=s.pid||!c->visible||!c->enabled)throw std::runtime_error("elevation UI bridge standard role thread/owner changed");}
    elevationUiProgress(s,L"after fresh snapshot enumeration");return found;
}
bool elevationUiMessage(ElevationUiAttempt& s,HWND panel,HWND target,const wchar_t* name,UINT message,WPARAM w,LPARAM l,bool nonzeroReply){
    elevationUiGuard(s);DWORD pid{};const auto thread=GetWindowThreadProcessId(target,&pid);
    if(pid!=s.pid||thread!=s.thread||!IsWindow(panel)||!elevationDialogOnOwnDesktop(panel)||!(target==panel||IsChild(panel,target)))throw std::runtime_error("elevation standard message target is not the exact own panel/child");
    const auto pending="{\"name\":"+quote(name)+",\"panel\":"+elevationDialogHwnd(panel)+",\"target\":"+elevationDialogHwnd(target)+",\"processId\":"+std::to_string(pid)+",\"threadId\":"+std::to_string(thread)+",\"message\":"+std::to_string(message)+",\"wParam\":"+std::to_string(static_cast<unsigned long long>(w))+",\"actualControlId\":"+std::to_string(GetDlgCtrlID(target))+",\"sameQueueTimeoutIsNotBounded\":true}";
    elevationUiProgress(s,L"before synchronous Win32 message",pending);
    DWORD_PTR reply{};SetLastError(0);const bool returned=SendMessageTimeoutW(target,message,w,l,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&reply)!=0;const auto error=returned?0:GetLastError();s.lastMessageReply=reply;
    s.messages.push_back("{\"name\":"+quote(name)+",\"panel\":"+elevationDialogHwnd(panel)+",\"target\":"+elevationDialogHwnd(target)+",\"processId\":"+std::to_string(pid)+",\"threadId\":"+std::to_string(thread)+",\"message\":"+std::to_string(message)+",\"wParam\":"+std::to_string(static_cast<unsigned long long>(w))+",\"actualControlId\":"+std::to_string(GetDlgCtrlID(target))+",\"notificationControlId\":"+std::to_string(message==WM_COMMAND?LOWORD(w):-1)+",\"notificationCode\":"+std::to_string(message==WM_COMMAND?HIWORD(w):-1)+",\"returned\":"+(returned?"true":"false")+",\"reply\":"+std::to_string(static_cast<unsigned long long>(reply))+",\"win32Error\":"+std::to_string(error)+"}");
    elevationUiProgress(s,L"after synchronous Win32 message");return returned&&(!nonzeroReply||reply!=0);
}
void elevationUiConfigure(ElevationUiAttempt& s,const wchar_t* source){
    // Discovery proved this panel is complete at beginGetPoint, not necessarily
    // at the earlier beginSSGet notification. Never consume the one-write budget early.
    if(s.inside)return;
    try{
        elevationUiProgress(s,(std::wstring(L"prompt received: ")+source).c_str());
        if(_wcsicmp(source,L"beginGetPoint")||!s.commandInProgress||s.configured)return;s.inside=true;
        elevationUiProgress(s,L"before prompt fresh defaults read");
        auto before=elevationUiFresh(s);const auto original=elevationUiDefaults(before);
        if(!original.valid)throw std::runtime_error("actual elevation defaults are not mutually exclusive numeric standard controls");
        s.captured=true;s.panel=before.hwnd;s.original=original;s.beforeSnapshot=elevationDialogSnapshotJson(before);
        elevationUiPersistDefaults(s);elevationUiProgress(s,L"actual original defaults durably recorded before any setter");
        for(const auto& c:before.controls)if(c.visible){std::wstring text;for(auto v:c.text)if(v!=L' '&&v!=L'\t')text.push_back(v);if(!_wcsicmp(text.c_str(),L"(M)")||!_wcsicmp(text.c_str(),L"（M）"))s.unitLabels.push_back(quote(c.text));}
        if(s.unitLabels.empty())throw std::runtime_error("actual visible parameter panel metre unit was not observed");
        if(s.request.mode==ElevationUiMode::Apply&&!elevationUiDesired(original,s.request))throw std::runtime_error("Apply prompt differs from independently observed persistent setup; point tokens may still be consumed");
        s.configured=true; // One field-setting attempt maximum, including failures.
        if(s.request.mode!=ElevationUiMode::ObserveOnly){
            s.configurationAttempted=true;
            const auto desiredRadio=s.request.increase?before.increaseRadio:before.decreaseRadio;
            const auto* radio=elevationUiControl(before,desiredRadio);
            if(!radio||radio->controlId!=GetDlgCtrlID(desiredRadio))throw std::runtime_error("fresh radio ID changed");
            if((s.request.increase&&!original.increaseChecked)||(!s.request.increase&&!original.decreaseChecked))
                if(!elevationUiMessage(s,before.hwnd,desiredRadio,L"BM_CLICK actual desired radio",BM_CLICK,0,0))throw std::runtime_error("actual radio BM_CLICK was not delivered");
            auto selected=elevationUiFresh(s);const auto selectedDefaults=elevationUiDefaults(selected);
            if(!selectedDefaults.valid||selectedDefaults.increaseChecked!=s.request.increase)throw std::runtime_error("actual radio mutual-exclusive readback did not change as requested");
            const auto* edit=elevationUiControl(selected,selected.changeEdit);if(!edit||edit->controlId!=GetDlgCtrlID(edit->hwnd)||edit->controlId<=0||edit->controlId>65535||(static_cast<DWORD>(edit->style)&ES_MULTILINE))throw std::runtime_error("actual single-line Edit control ID unavailable");
            if(!elevationUiMessage(s,selected.hwnd,edit->hwnd,L"WM_SETTEXT actual change-value Edit",WM_SETTEXT,0,reinterpret_cast<LPARAM>(s.request.rawValue.c_str()),true))throw std::runtime_error("actual Edit WM_SETTEXT failed");
            // The actual single-line Edit supplies its standard WM_SETTEXT
            // notifications. Do not synthesize duplicate EN_CHANGE or a false
            // EN_KILLFOCUS notification while inside an AutoCAD input callback.
        }
        const auto after=elevationUiFresh(s);s.after=elevationUiDefaults(after);s.afterSnapshot=elevationDialogSnapshotJson(after);
        s.configurationVerified=s.request.mode==ElevationUiMode::ObserveOnly?s.after.valid:elevationUiDesired(s.after,s.request);
        if(!s.configurationVerified)throw std::runtime_error("actual elevation raw/radio readback differs; GUI value is not business success");
        s.events.push_back("{\"event\":"+quote(source)+",\"captured\":true,\"configurationVerified\":true,\"callbackCommandIssued\":false}");
    }catch(const std::exception& failure){s.error=familyError(failure);s.configured=true;s.events.push_back("{\"event\":"+quote(source)+",\"configurationVerified\":false,\"callbackCommandIssued\":false}");}
    catch(...){s.error=quote(L"unknown elevation standard-control callback exception");s.configured=true;}s.inside=false;
    try{elevationUiProgress(s,L"prompt configuration returned");}catch(...){s.error=quote(L"elevation UI prompt diagnostic persistence failed");}
}
class ElevationUiObserver final:public AcEdInputContextReactor {
public:ElevationUiAttempt* state{};explicit ElevationUiObserver(ElevationUiAttempt& s):state(&s){}
    void beginEntsel(const ACHAR*,int,const ACHAR*)override{if(state)elevationUiConfigure(*state,L"beginEntsel");}
    void beginSSGet(const ACHAR*,int,const ACHAR*,const ACHAR*,const AcArray<AcGePoint3d>&,const resbuf*)override{if(state)elevationUiConfigure(*state,L"beginSSGet");}
    void beginGetPoint(const AcGePoint3d*,const ACHAR*,int,const ACHAR*)override{if(state)elevationUiConfigure(*state,L"beginGetPoint");}
};
ElevationUiObserver* elevationUiRetainedObserver{};
class ElevationUiObserverScope {
    ElevationUiAttempt& state_;AcEdInputPointManager* manager_{};std::unique_ptr<ElevationUiObserver> observer_;bool attached_{};
public:
    explicit ElevationUiObserverScope(ElevationUiAttempt& s):state_(s){if(elevationUiBridgeRetained||elevationUiRetainedObserver)throw std::runtime_error("previous elevation UI callback retained");manager_=s.document->inputPointManager();if(!manager_)throw std::runtime_error("elevation UI input manager unavailable");observer_.reset(new ElevationUiObserver(s));try{attached_=true;s.observerAdd=manager_->addInputContextReactor(observer_.get());s.observerAdded=s.observerAdd==Acad::eOk;if(!s.observerAdded)throw std::runtime_error("elevation UI observer add failed");}catch(...){stop();throw;}}
    ElevationUiObserverScope(const ElevationUiObserverScope&)=delete;ElevationUiObserverScope& operator=(const ElevationUiObserverScope&)=delete;
    void stop()noexcept{if(!attached_)return;observer_->state=nullptr;try{state_.observerRemove=manager_->removeInputContextReactor(observer_.get());state_.observerRemoved=state_.observerRemove==Acad::eOk||state_.observerRemove==Acad::eInvalidKey;}catch(...){state_.observerRemoved=false;}attached_=false;if(state_.observerRemoved){observer_.reset();return;}state_.observerRetained=true;elevationUiBridgeRetained=true;elevationUiRetainedObserver=observer_.release();try{if(nativeApplicationId)acrxDynamicLinker->lockApplication(nativeApplicationId);}catch(...){}}
    ~ElevationUiObserverScope(){stop();}
};
std::string elevationUiAttemptFacts(const ElevationUiAttempt& s){
    std::vector<std::string> selected;for(const auto& handle:s.actualSelected)selected.push_back(quote(handle));
    return "\"implementationVersion\":2,\"attemptSerial\":"+std::to_string(s.attemptSerial)+",\"progressFailed\":"+(s.progressFailed?"true":"false")+",\"defaultsReceiptPersisted\":"+(s.defaultsReceiptPersisted?"true":"false")+",\"defaultsReceipt\":"+(s.defaultsReceipt.empty()?"null":quote(s.defaultsReceipt))+",\"syntheticEditNotifications\":false,\"controlDWG\":"+quote(fullPath(s.paths.output+L"\\control.dwg"))+",\"inputDesktop\":"+quote(s.inputDesktop)+",\"contextGuardPassed\":"+(s.contextGuardPassed?"true":"false")+",\"lifecycleSafe\":"+(s.lifecycleSafe?"true":"false")+",\"modelProtectionVerified\":"+(s.modelProtectionVerified?"true":"false")+",\"modelMembershipUnchanged\":"+(s.modelMembershipUnchanged?"true":"false")+",\"selectionVerified\":"+(s.selectionVerified?"true":"false")+",\"actualSelectedHandles\":"+axisTraceArray(selected)+",\"initialModelCount\":"+std::to_string(s.initialCount)+",\"finalModelCount\":"+std::to_string(s.finalCount)+",\"protectedCount\":"+std::to_string(s.protectedCount)+",\"expectedPickedHandle\":"+(s.expectedPickedHandle.empty()?"null":quote(s.expectedPickedHandle))+",\"targetClass\":"+(s.targetClass.empty()?"null":quote(s.targetClass))+",\"pickWcs\":"+point(s.request.pickWcs)+",\"pickUcs\":"+point(s.pickUcs)+",\"coordinateTransformStatus\":"+std::to_string(s.transformStatus)+",\"pickMatchesActualPublicGrip\":"+(s.pickMatchesActualPublicGrip?"true":"false")+",\"cmdaBefore\":"+std::to_string(s.cmdaBefore)+",\"cmdaAfter\":"+std::to_string(s.cmdaAfter)+",\"cmdnamesBefore\":"+quote(s.cmdnamesBefore)+",\"cmdnamesAfter\":"+quote(s.cmdnamesAfter)+",\"beforeDefaults\":"+elevationUiDefaultsJson(s.original)+",\"unitLabels\":"+axisTraceArray(s.unitLabels)+",\"preconfiguredAtPrompt\":"+(s.request.mode==ElevationUiMode::Apply&&elevationUiDesired(s.original,s.request)?"true":"false")+",\"commandDispatched\":"+(s.commandDispatched?"true":"false")+",\"nonEmptySSGetSelections\":"+std::to_string(s.nonEmptySSGetSelections)+",\"nonEmptyEntselSelections\":"+std::to_string(s.nonEmptyEntselSelections)+",\"safeScope\":\"UI lifecycle only; if business/protection failed the coordinator may only restore original defaults and independently ObserveOnly, never blind reverse\"";
}
std::string elevationUiAttemptJson(const ElevationUiAttempt& s){
    return "{\"contract\":\"stage1.elevation-ui-attempt.v1\","+elevationUiAttemptFacts(s)+",\"mode\":"+quote(elevationUiModeName(s.request.mode))+",\"processId\":"+std::to_string(s.pid)+",\"threadId\":"+std::to_string(s.thread)+",\"desktop\":"+quote(s.desktop)+",\"requestedRawValue\":"+quote(s.request.rawValue)+",\"requestedIncrease\":"+(s.request.increase?"true":"false")+",\"commandName\":\"TChangeValElev\",\"commandStatus\":"+std::to_string(s.commandStatus)+",\"originalDefaults\":"+elevationUiDefaultsJson(s.original)+",\"afterDefaults\":"+elevationUiDefaultsJson(s.after)+",\"beforeControls\":"+s.beforeSnapshot+",\"afterControls\":"+s.afterSnapshot+",\"configurationAttempted\":"+(s.configurationAttempted?"true":"false")+",\"configurationVerified\":"+(s.configurationVerified?"true":"false")+",\"messages\":"+axisTraceArray(s.messages)+",\"promptEvents\":"+axisTraceArray(s.events)+",\"callbackCommandIssued\":false,\"observerAddStatus\":"+std::to_string(s.observerAdd)+",\"observerRemoveStatus\":"+std::to_string(s.observerRemove)+",\"observerAdded\":"+(s.observerAdded?"true":"false")+",\"observerRemoved\":"+(s.observerRemoved?"true":"false")+",\"observerRetained\":"+(s.observerRetained?"true":"false")+",\"timerUsed\":false,\"autoClosed\":"+(s.autoClosed?"true":"false")+",\"panelClosed\":"+(s.panelClosed?"true":"false")+",\"closeDelivered\":"+(s.closeDelivered?"true":"false")+",\"activeTransactionsBefore\":"+std::to_string(s.txBefore)+",\"activeTransactionsAfter\":"+std::to_string(s.txAfter)+",\"protectedBefore\":"+s.protectedBefore+",\"protectedAfter\":"+s.protectedAfter+",\"targetBefore\":"+s.targetBefore+",\"targetAfter\":"+s.targetAfter+",\"commandTrace\":"+axisTraceArray(s.trace->commands)+",\"inputTrace\":"+axisTraceArray(s.trace->inputs)+",\"trace\":"+axisContinuousTraceMeta(*s.trace)+",\"completed\":"+(s.completed?"true":"false")+",\"safeToContinue\":"+(s.safe?"true":"false")+",\"error\":"+s.error+"}";
}
ElevationUiAttempt elevationUiAttemptRun(const Paths& p,AcApDocument* doc,AcDbDatabase& db,const ElevationUiRequest& request){
    ElevationUiAttempt s;s.paths=p;s.document=doc;s.database=&db;s.request=request;s.pid=GetCurrentProcessId();s.thread=GetCurrentThreadId();s.desktop=quietCurrentDesktop();s.inputDesktop=quietInputDesktop();s.attemptSerial=++elevationUiNextAttemptSerial;
    try{
        elevationUiGuard(s);s.contextGuardPassed=true;elevationUiProgress(s,L"attempt context guard passed");if(request.mode!=ElevationUiMode::ObserveOnly&&!elevationUiNumericRaw(request.rawValue))throw std::runtime_error("requested elevation raw value is not a finite decimal control token");
        const auto all=modelIds(db);s.initialCount=all.length();if(all.length()!=214)throw std::runtime_error("elevation bridge requires213 originals plus one owned elevation clone");AcDbObjectIdArray protectedIds;
        for(int i=0;i<all.length();++i)if(request.mode!=ElevationUiMode::Apply||all[i]!=request.target)protectedIds.append(all[i]);
        std::wstring targetHandle;AcGePoint3d ucs;int transform=-9999;
        if(request.mode==ElevationUiMode::Apply){
            if(request.target.isNull()||request.target.database()!=&db||protectedIds.length()!=213)throw std::runtime_error("Apply target is not the single actual owned model entity");
            auto target=open<AcDbEntity>(request.target);s.targetClass=target->isA()->name();if(target->isAProxy()||_wcsicmp(target->isA()->name(),L"TDbSymbElevation"))throw std::runtime_error("Apply target is not native TDbSymbElevation");
            AcGePoint3dArray grips;AcDbIntArray snaps,geometry;check(target->getGripPoints(grips,snaps,geometry),"elevation actual public target grips");bool matches=false;for(int i=0;i<grips.length();++i)if(grips[i].distanceTo(request.pickWcs)<1e-6)matches=true;if(!matches)throw std::runtime_error("elevation target pick is not an actual public grip");target.reset();
            s.pickMatchesActualPublicGrip=matches;quietPrepareOwnedView();transform=textBridgeToUcs(request.pickWcs,ucs);s.transformStatus=transform;s.pickUcs=ucs;if(transform!=RTNORM)throw std::runtime_error("elevation WCS/UCS pick conversion failed");targetHandle=handleOf(request.target);s.expectedPickedHandle=targetHandle;s.targetBefore=familyCompactEntity(request.target);
        }
        s.protectedCount=protectedIds.length();s.protectedBefore=familyCompactArray(protectedIds);s.txBefore=db.transactionManager()->numActiveTransactions();const auto commandBefore=elevationDialogCommandState();s.cmdaBefore=commandBefore.first;s.cmdnamesBefore=commandBefore.second;elevationDialogKnownMainFrame=adsw_acadMainWnd();
        {AxisNativeTraceOwner trace(doc,s.trace);if(!s.trace->editorAdded||!s.trace->inputAddReturned||s.trace->inputAddStatus!=Acad::eOk)throw std::runtime_error("elevation bridge command trace unavailable");elevationUiProgress(s,L"before input observer add");ElevationUiObserverScope observer(s);s.commandInProgress=true;s.commandDispatched=true;elevationUiProgress(s,L"before public TChangeValElev command");
            if(request.mode==ElevationUiMode::Apply){ads_point pointIn{ucs.x,ucs.y,ucs.z};s.commandStatus=acedCommandS(RTSTR,L"TChangeValElev",RT3DPOINT,pointIn,RTSTR,L"",RTNONE);}
            else s.commandStatus=acedCommandS(RTSTR,L"TChangeValElev",RTSTR,L"",RTNONE);
            s.commandInProgress=false;elevationUiProgress(s,L"public TChangeValElev command returned; before input observer remove");observer.stop();elevationUiProgress(s,L"after input observer remove");
            if(s.panel){s.panelClosed=!IsWindow(s.panel)||!IsWindowVisible(s.panel);s.autoClosed=s.panelClosed;
                if(!s.panelClosed&&!s.observerRetained){const auto fresh=elevationUiFresh(s);if(fresh.hwnd!=s.panel)throw std::runtime_error("elevation close panel differs");s.closeDelivered=elevationUiMessage(s,fresh.hwnd,fresh.hwnd,L"WM_CLOSE exact known parameter panel after command",WM_CLOSE,0,0);s.panelClosed=!IsWindow(s.panel)||!IsWindowVisible(s.panel);}}
        }
        elevationUiGuard(s);s.txAfter=db.transactionManager()->numActiveTransactions();const auto commandAfter=elevationDialogCommandState();s.cmdaAfter=commandAfter.first;s.cmdnamesAfter=commandAfter.second;
        const bool detached=s.observerAdded&&s.observerRemoved&&!s.observerRetained&&s.trace->editorRemoved&&s.trace->inputRemoveReturned&&(s.trace->inputRemoveStatus==Acad::eOk||s.trace->inputRemoveStatus==Acad::eInvalidKey)&&!s.trace->retained&&!s.trace->traceDropped;
        s.lifecycleSafe=detached&&!elevationUiBridgeRetained&&s.panelClosed&&commandBefore==commandAfter&&!textBridgeHasUnbalancedCommandTrace(*s.trace)&&s.txBefore==0&&s.txAfter==0;s.safe=s.lifecycleSafe;
        s.protectedAfter=familyCompactArray(protectedIds);const auto now=modelIds(db);s.finalCount=now.length();s.modelMembershipUnchanged=all.length()==now.length();if(s.modelMembershipUnchanged)for(int i=0;i<all.length();++i)if(all[i]!=now[i])s.modelMembershipUnchanged=false;s.modelProtectionVerified=s.initialCount==s.finalCount&&s.modelMembershipUnchanged&&s.protectedBefore==s.protectedAfter;if(request.mode==ElevationUiMode::Apply)s.targetAfter=familyCompactEntity(request.target);
        const auto selections=axisContinuousObservedSelections(*s.trace);s.actualSelected=selections;int ssSelections=0,entSelections=0;
        for(const auto& e:s.trace->inputs){if(e.find("\"event\":\"endSSGet\"")!=std::string::npos&&e.find("\"selectedHandles\":[\"")!=std::string::npos)++ssSelections;if(e.find("\"event\":\"endEntsel\"")!=std::string::npos&&e.find("\"selectedHandle\":\"")!=std::string::npos)++entSelections;}
        s.nonEmptySSGetSelections=ssSelections;s.nonEmptyEntselSelections=entSelections;s.selectionVerified=request.mode==ElevationUiMode::Apply?selections.size()==1&&selections[0]==targetHandle&&(ssSelections==1||(ssSelections==0&&entSelections==1)):selections.empty();
        s.completed=s.safe&&s.modelProtectionVerified&&elevationUiCommandVerified(*s.trace,request.mode)&&s.error=="null"&&s.captured&&s.defaultsReceiptPersisted&&!s.progressFailed&&s.configurationVerified&&s.selectionVerified&&(s.commandStatus==RTNORM||s.commandStatus==RTCAN);
        if(!s.completed&&s.error=="null")s.error=quote(L"elevation bridge field/selection/cleanup contract failed; GUI strings are not business success");
    }catch(const std::exception& failure){s.error=familyError(failure);}catch(...){s.error=quote(L"unknown elevation UI attempt failure");}
    if(!s.commandDispatched&&s.contextGuardPassed&&!s.observerRetained&&!elevationUiBridgeRetained&&!s.trace->retained&&(!s.trace->editorAdded||s.trace->editorRemoved)&&(!s.trace->inputAddReturned||(s.trace->inputRemoveReturned&&(s.trace->inputRemoveStatus==Acad::eOk||s.trace->inputRemoveStatus==Acad::eInvalidKey)))){try{elevationUiGuard(s);s.lifecycleSafe=true;s.safe=true;s.panelClosed=true;}catch(...){}}
    try{elevationUiProgress(s,L"attempt returned with lifecycle and business facts");}catch(...){s.completed=false;if(s.error=="null")s.error=quote(L"elevation UI final progress persistence failed");}return s;
}
ElevationUiResult elevationUiRunChange(const Paths& p,AcApDocument* doc,AcDbDatabase& db,const ElevationUiRequest& request){
    ElevationUiResult result;std::string preflight="null",attempt="null",error="null";bool dispatched=false;
    if(request.mode==ElevationUiMode::Apply){auto observe=request;observe.mode=ElevationUiMode::ObserveOnly;const auto checked=elevationUiAttemptRun(p,doc,db,observe);preflight=elevationUiAttemptJson(checked);result.originalDefaults=checked.original;result.safeToContinue=checked.safe;
        if(!checked.completed||!checked.safe||!elevationUiDesired(checked.original,request)){error=quote(L"Apply withheld before point tokens: independent empty-selection reopen does not confirm persistent desired defaults");}
        else{const auto applied=elevationUiAttemptRun(p,doc,db,request);attempt=elevationUiAttemptJson(applied);dispatched=applied.commandDispatched;result.originalDefaults=applied.original;result.safeToContinue=applied.safe;result.completed=applied.completed;if(!result.completed)error=applied.error;}}
    else{const auto observed=elevationUiAttemptRun(p,doc,db,request);attempt=elevationUiAttemptJson(observed);dispatched=observed.commandDispatched;result.originalDefaults=observed.original;result.safeToContinue=observed.safe;result.completed=observed.completed;if(!result.completed)error=observed.error;}
    result.json="{\"contract\":\"stage1.elevation-ui-bridge-operation.v1\",\"id\":"+quote(request.id)+",\"mode\":"+quote(elevationUiModeName(request.mode))+",\"requestedRawValue\":"+quote(request.rawValue)+",\"requestedIncrease\":"+(request.increase?"true":"false")+",\"targetHandle\":"+(request.target.isNull()?"null":quote(handleOf(request.target)))+",\"expectedPickedHandle\":"+(request.mode==ElevationUiMode::Apply&&!request.target.isNull()?quote(handleOf(request.target)):"null")+",\"pickWcs\":"+point(request.pickWcs)+",\"preflight\":"+preflight+",\"attempt\":"+attempt+",\"commandDispatched\":"+(dispatched?"true":"false")+",\"selectionTokensMayHaveBeenConsumed\":"+(dispatched&&request.mode==ElevationUiMode::Apply?"true":"false")+",\"originalDefaults\":"+elevationUiDefaultsJson(result.originalDefaults)+",\"completed\":"+(result.completed?"true":"false")+",\"safeToContinue\":"+(result.safeToContinue?"true":"false")+",\"error\":"+error+",\"scope\":\"dynamic standard controls only; native elevation text and persistence must independently validate business result; restore defaults then reopen ObserveOnly\"}";return result;
}
