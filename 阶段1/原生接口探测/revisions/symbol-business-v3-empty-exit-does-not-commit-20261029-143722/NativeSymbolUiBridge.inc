// Bind only the standard controls observed in this installed TCH discovery.
// IDs are revalidated against actual panel title/class/labels and current handles.
struct SymbolUiValue {std::wstring kind,text;LRESULT selected{-9999},checked{-9999};};
using SymbolUiValues=std::map<std::wstring,SymbolUiValue>;
std::string symbolUiValuesJson(const SymbolUiValues& values){std::vector<std::string> items;for(const auto& item:values)items.push_back(quote(item.first)+":{\"kind\":"+quote(item.second.kind)+",\"text\":"+quote(item.second.text)+",\"selected\":"+std::to_string(item.second.selected)+",\"checked\":"+std::to_string(item.second.checked)+"}");std::string result="{";for(std::size_t i=0;i<items.size();++i){if(i)result+=",";result+=items[i];}return result+"}";}
bool symbolUiValuesEqual(const SymbolUiValues& a,const SymbolUiValues& b){return symbolUiValuesJson(a)==symbolUiValuesJson(b);}
struct SymbolUiOperation {
    Paths paths;AcApDocument* doc{};AcDbDatabase* db{};std::wstring kind,command,id,mode;
    SymbolUiValues desired,before,after;std::vector<AcGePoint3d> inputPoints;AcDbObjectIdArray newEntities;
    DWORD pid{},thread{};HWND panel{};bool inside{},captured{},configured{},configurationVerified{},fieldSettersAttempted{},observerRemoved{},panelClosed{},completed{},safe{};
    int commandStatus{-9999},addStatus{-9999},removeStatus{-9999};std::string error{"null"},beforeSnapshot{"null"},afterSnapshot{"null"};std::vector<std::string> messages,events;
    std::shared_ptr<AxisNativeTraceData> trace{std::make_shared<AxisNativeTraceData>()};
};
bool symbolUiBridgeRetained{};
void symbolUiProgress(SymbolUiOperation& s,const wchar_t* stage,const std::string& pending="null"){
    quietAtomic(s.paths.output+L"\\symbol-ui-progress.json","{\"contract\":\"stage1.symbol-ui-progress.v1\",\"id\":"+quote(s.id)+",\"kind\":"+quote(s.kind)+",\"mode\":"+quote(s.mode)+",\"stage\":"+quote(stage)+",\"pendingMessage\":"+pending+",\"processId\":"+std::to_string(s.pid)+",\"threadId\":"+std::to_string(s.thread)+",\"beforeFields\":"+symbolUiValuesJson(s.before)+",\"afterFields\":"+symbolUiValuesJson(s.after)+",\"messages\":"+axisTraceArray(s.messages)+",\"events\":"+axisTraceArray(s.events)+",\"inputTrace\":"+axisTraceArray(s.trace->inputs)+",\"commandTrace\":"+axisTraceArray(s.trace->commands)+",\"error\":"+s.error+"}",s.paths.output);
}
void symbolUiGuard(const SymbolUiOperation& s){quietRequireIsolatedWorker();fourAxisGuard(s.paths,s.doc,s.db);if(GetCurrentProcessId()!=s.pid||GetCurrentThreadId()!=s.thread||s.db->transactionManager()->numActiveTransactions()!=0||symbolUiBridgeRetained)throw std::runtime_error("symbol bridge own UI thread/zero transaction guard failed");}
ElevationDialogSnapshot symbolUiFresh(SymbolUiOperation& s){
    symbolUiGuard(s);std::vector<ElevationDialogSnapshot> found;
    const std::wstring title=s.kind==L"slope"?L"箭头引注":s.kind==L"section"?L"剖切符号":s.kind==L"pointer-index"?L"指向索引":L"剖切索引";
    for(auto& panel:elevationDialogOwnedSnapshots())if(panel.className==L"#32770"&&panel.title==title&&panel.enumerationComplete&&panel.processId==s.pid&&panel.threadId==s.thread&&elevationDialogOnOwnDesktop(panel.hwnd))found.push_back(std::move(panel));
    if(found.size()!=1)throw std::runtime_error("symbol bridge requires exactly one actual observed standard panel");
    std::wstring labels;for(const auto& c:found[0].controls)if(c.visible)labels+=c.text+L"\n";
    if(s.kind==L"section"){if(!elevationDialogContains(labels,L"剖切编号")||!elevationDialogContains(labels,L"剖面图号"))throw std::runtime_error("actual section panel semantic labels differ");}
    else if(!elevationDialogContains(labels,L"上标文字")||!elevationDialogContains(labels,L"下标文字"))throw std::runtime_error("actual annotation panel semantic labels differ");
    s.panel=found[0].hwnd;return found[0];
}
HWND symbolUiControl(const ElevationDialogSnapshot& p,int id,const wchar_t* expectedClass){HWND result{};int count=0;for(const auto& c:p.controls)if(c.controlId==id&&!_wcsicmp(c.className.c_str(),expectedClass)&&c.visible){result=c.hwnd;++count;}if(count!=1||!IsWindow(result)||GetDlgCtrlID(result)!=id)throw std::runtime_error("actual symbol standard-control role is not unique");return result;}
std::map<std::wstring,HWND> symbolUiBindings(SymbolUiOperation& s,const ElevationDialogSnapshot& panel){
    std::map<std::wstring,HWND> result;
    if(s.kind==L"slope"){result[L"upper"]=symbolUiControl(panel,1048,L"ComboBox");result[L"lower"]=symbolUiControl(panel,1103,L"ComboBox");}
    else if(s.kind==L"section"){result[L"number"]=symbolUiControl(panel,1200,L"ComboBox");result[L"sheet"]=symbolUiControl(panel,1255,L"Edit");result[L"showSheet"]=symbolUiControl(panel,1213,L"Button");}
    else{result[L"number"]=symbolUiControl(panel,1040,L"ComboBox");result[L"sheet"]=symbolUiControl(panel,1039,L"Edit");result[L"upper"]=symbolUiControl(panel,1032,L"ComboBox");result[L"lower"]=symbolUiControl(panel,1035,L"ComboBox");}
    return result;
}
LRESULT symbolUiMessage(SymbolUiOperation& s,HWND target,UINT msg,WPARAM w,LPARAM l,const wchar_t* role){
    symbolUiGuard(s);DWORD pid{};const auto thread=GetWindowThreadProcessId(target,&pid);
    if(pid!=s.pid||thread!=s.thread||!IsWindow(target)||!s.panel||!(target==s.panel||IsChild(s.panel,target))||!elevationDialogOnOwnDesktop(s.panel))throw std::runtime_error("symbol message does not target current owned panel/child");
    const auto pending="{\"role\":"+quote(role)+",\"target\":"+elevationDialogHwnd(target)+",\"actualId\":"+std::to_string(GetDlgCtrlID(target))+",\"message\":"+std::to_string(msg)+",\"wParam\":"+std::to_string(static_cast<unsigned long long>(w))+",\"notificationId\":"+std::to_string(msg==WM_COMMAND?LOWORD(w):0)+",\"notificationCode\":"+std::to_string(msg==WM_COMMAND?HIWORD(w):0)+",\"sameQueueTimeoutNotBounded\":true}";
    symbolUiProgress(s,L"before-standard-message",pending);DWORD_PTR reply{};SetLastError(0);const bool returned=SendMessageTimeoutW(target,msg,w,l,SMTO_BLOCK|SMTO_ABORTIFHUNG,200,&reply)!=0;
    s.messages.push_back(pending.substr(0,pending.size()-1)+",\"returned\":"+plotBool(returned)+",\"reply\":"+std::to_string(static_cast<LRESULT>(reply))+",\"win32Error\":"+std::to_string(returned?0:GetLastError())+"}");symbolUiProgress(s,L"after-standard-message");if(!returned)throw std::runtime_error("symbol standard-control message failed");return static_cast<LRESULT>(reply);
}
SymbolUiValues symbolUiReadFields(SymbolUiOperation& s,const ElevationDialogSnapshot& panel){
    SymbolUiValues values;for(const auto& binding:symbolUiBindings(s,panel)){SymbolUiValue value;const auto className=elevationDialogClass(binding.second);
        if(className==L"Button"){const auto style=static_cast<DWORD>(GetWindowLongPtrW(binding.second,GWL_STYLE))&BS_TYPEMASK;if(style!=BS_AUTOCHECKBOX&&style!=BS_CHECKBOX)throw std::runtime_error("symbol flag is not a standard checkbox");value.kind=L"check";value.checked=symbolUiMessage(s,binding.second,BM_GETCHECK,0,0,L"actual-checkbox-read");}
        else{value.kind=className==L"ComboBox"?L"combo":L"edit";wchar_t text[2048]{};symbolUiMessage(s,binding.second,WM_GETTEXT,2048,reinterpret_cast<LPARAM>(text),L"actual-field-text-read");value.text=text;if(value.kind==L"combo")value.selected=symbolUiMessage(s,binding.second,CB_GETCURSEL,0,0,L"actual-combo-selection-read");}
        values.emplace(binding.first,std::move(value));}return values;
}
void symbolUiSetOne(SymbolUiOperation& s,const std::wstring& key,const SymbolUiValue& value){
    const auto fresh=symbolUiFresh(s);const auto bindings=symbolUiBindings(s,fresh);const auto found=bindings.find(key);if(found==bindings.end())throw std::runtime_error("requested symbol field has no actual current role");const auto control=found->second;
    if(value.kind==L"check"){const auto old=symbolUiMessage(s,control,BM_GETCHECK,0,0,L"checkbox-before-set");if(old!=value.checked)symbolUiMessage(s,control,BM_CLICK,0,0,L"actual-checkbox-click");}
    else if(value.kind==L"combo"){
        if(value.selected>=0){symbolUiMessage(s,control,CB_SETCURSEL,static_cast<WPARAM>(value.selected),0,L"actual-combo-select");symbolUiMessage(s,GetParent(control),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(control),CBN_SELCHANGE),reinterpret_cast<LPARAM>(control),L"standard-combo-selection-notification");}
        else{symbolUiMessage(s,control,CB_SETCURSEL,static_cast<WPARAM>(-1),0,L"clear-old-combo-selection");symbolUiMessage(s,control,WM_SETTEXT,0,reinterpret_cast<LPARAM>(value.text.c_str()),L"actual-combo-text-set");symbolUiMessage(s,GetParent(control),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(control),CBN_EDITCHANGE),reinterpret_cast<LPARAM>(control),L"standard-combo-edit-notification");}
    }else if(value.kind==L"edit")symbolUiMessage(s,control,WM_SETTEXT,0,reinterpret_cast<LPARAM>(value.text.c_str()),L"actual-edit-text-set");
    else throw std::runtime_error("unknown requested symbol standard field kind");
    // Explicit programmatic commit notification, not an assertion of physical focus.
    // Values and actual created TCH text still require independent readback.
    if(s.kind!=L"section"&&value.kind==L"combo")symbolUiMessage(s,GetParent(control),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(control),CBN_KILLFOCUS),reinterpret_cast<LPARAM>(control),L"explicit-programmatic-combo-commit-no-physical-focus-claim");
    if(s.kind!=L"section"&&value.kind==L"edit")symbolUiMessage(s,GetParent(control),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(control),EN_KILLFOCUS),reinterpret_cast<LPARAM>(control),L"explicit-programmatic-edit-commit-no-physical-focus-claim");
}
void symbolUiCapture(SymbolUiOperation& s,const wchar_t* prompt){
    if(s.inside||s.configured)return;s.inside=true;
    try{symbolUiGuard(s);const auto before=symbolUiFresh(s);s.before=symbolUiReadFields(s,before);s.beforeSnapshot=elevationDialogSnapshotJson(before);s.captured=true;
        writeNewReport(s.paths.output+L"\\symbol-defaults-"+s.id+L".json","{\"contract\":\"stage1.symbol-defaults-before-setters.v1\",\"id\":"+quote(s.id)+",\"beforeFields\":"+symbolUiValuesJson(s.before)+",\"actualPanel\":"+s.beforeSnapshot+"}");
        s.configured=true;symbolUiProgress(s,L"actual-fields-captured-before-setters");
        if(s.mode==L"setup"){
            s.fieldSettersAttempted=true;const auto check=s.desired.find(L"showSheet");if(check!=s.desired.end()&&check->second.checked==BST_CHECKED)symbolUiSetOne(s,check->first,check->second);
            for(const auto& field:s.desired)if(field.first!=L"showSheet")symbolUiSetOne(s,field.first,field.second);
            if(check!=s.desired.end()&&check->second.checked!=BST_CHECKED)symbolUiSetOne(s,check->first,check->second);
        }
        const auto after=symbolUiFresh(s);s.after=symbolUiReadFields(s,after);s.afterSnapshot=elevationDialogSnapshotJson(after);s.configurationVerified=s.desired.empty()?!s.after.empty():symbolUiValuesEqual(s.after,s.desired);
        if(!s.configurationVerified)throw std::runtime_error("actual symbol fields differ from requested or independently reopened values");
        s.events.push_back("{\"event\":"+quote(prompt)+",\"captured\":true,\"configured\":true,\"callbackCommandIssued\":false}");
    }catch(const std::exception& e){s.error=familyError(e);s.configured=true;}catch(...){s.error=quote(L"unknown symbol UI capture failure");s.configured=true;}
    s.inside=false;try{symbolUiProgress(s,L"prompt-capture-returned");}catch(...){s.error=quote(L"symbol UI progress persistence failed");}
}
class SymbolUiObserver final:public AcEdInputContextReactor{
public:SymbolUiOperation* state{};explicit SymbolUiObserver(SymbolUiOperation& s):state(&s){}
    void beginGetPoint(const AcGePoint3d*,const ACHAR*,int,const ACHAR*)override{if(state)symbolUiCapture(*state,L"beginGetPoint");}
    void beginSSGet(const ACHAR*,int,const ACHAR*,const ACHAR*,const AcArray<AcGePoint3d>&,const resbuf*)override{if(state)symbolUiCapture(*state,L"beginSSGet");}
};
void symbolUiRun(SymbolUiOperation& s){
    s.pid=GetCurrentProcessId();s.thread=GetCurrentThreadId();s.safe=false;
    auto observer=std::make_unique<SymbolUiObserver>(s);AcEdInputPointManager* manager{};AcDbObjectIdArray beforeIds;std::pair<int,std::wstring> beforeCommand;
    try{symbolUiGuard(s);beforeIds=modelIds(*s.db);beforeCommand=elevationDialogCommandState();elevationDialogKnownMainFrame=adsw_acadMainWnd();manager=s.doc->inputPointManager();if(!manager)throw std::runtime_error("symbol operation input manager missing");
        s.addStatus=manager->addInputContextReactor(observer.get());check(static_cast<Acad::ErrorStatus>(s.addStatus),"attach symbol UI observer");
        {AxisNativeTraceOwner trace(s.doc,s.trace);symbolUiProgress(s,L"before-public-symbol-command");
            if(s.mode!=L"create")s.commandStatus=acedCommandS(RTSTR,s.command.c_str(),RTSTR,L"",RTNONE);
            else{
                std::vector<AcGePoint3d> ucs;for(const auto& p:s.inputPoints){AcGePoint3d q;if(textBridgeToUcs(p,q)!=RTNORM)throw std::runtime_error("symbol actual WCS/UCS transform failed");ucs.push_back(q);}
                ads_point tokens[4]{};if(ucs.size()>4)throw std::runtime_error("too many symbol point tokens");for(std::size_t i=0;i<ucs.size();++i){tokens[i][0]=ucs[i].x;tokens[i][1]=ucs[i].y;tokens[i][2]=ucs[i].z;}
                if(s.kind==L"slope"&&ucs.size()==2)s.commandStatus=acedCommandS(RTSTR,s.command.c_str(),RT3DPOINT,tokens[0],RT3DPOINT,tokens[1],RTSTR,L"",RTSTR,L"",RTNONE);
                else if((s.kind==L"section"||s.kind==L"pointer-index")&&ucs.size()==3)s.commandStatus=acedCommandS(RTSTR,s.command.c_str(),RT3DPOINT,tokens[0],RT3DPOINT,tokens[1],RT3DPOINT,tokens[2],RTSTR,L"",RTNONE);
                else if(s.kind==L"section-index"&&ucs.size()==4)s.commandStatus=acedCommandS(RTSTR,s.command.c_str(),RT3DPOINT,tokens[0],RT3DPOINT,tokens[1],RT3DPOINT,tokens[2],RT3DPOINT,tokens[3],RTSTR,L"",RTNONE);
                else throw std::runtime_error("symbol operation input fixture length invalid");
            }
            symbolUiProgress(s,L"public-symbol-command-returned");
        }
    }catch(const std::exception& e){s.error=familyError(e);}catch(...){s.error=quote(L"unknown symbol public command failure");}
    observer->state=nullptr;
    if(manager&&s.addStatus==Acad::eOk){s.removeStatus=manager->removeInputContextReactor(observer.get());s.observerRemoved=s.removeStatus==Acad::eOk||s.removeStatus==Acad::eInvalidKey;}
    else s.observerRemoved=s.addStatus!=Acad::eOk;
    if(!s.observerRemoved){symbolUiBridgeRetained=true;observer.release();acrxDynamicLinker->lockApplication(nativeApplicationId);}
    try{symbolUiGuard(s);s.panelClosed=!s.panel||!IsWindow(s.panel)||!IsWindowVisible(s.panel);
        if(!s.panelClosed&&s.observerRemoved){symbolUiFresh(s);symbolUiMessage(s,s.panel,WM_CLOSE,0,0,L"close-exact-owned-symbol-panel-after-command");s.panelClosed=!IsWindow(s.panel)||!IsWindowVisible(s.panel);}
        const auto now=modelIds(*s.db);for(int i=0;i<now.length();++i){bool old=false;for(int j=0;j<beforeIds.length();++j)if(now[i]==beforeIds[j])old=true;if(!old)s.newEntities.append(now[i]);}
        bool originalsPresent=true;for(int i=0;i<beforeIds.length();++i){bool present=false;for(int j=0;j<now.length();++j)if(beforeIds[i]==now[j])present=true;originalsPresent=originalsPresent&&present;}
        s.safe=s.observerRemoved&&!s.trace->retained&&!textBridgeHasUnbalancedCommandTrace(*s.trace)&&s.db->transactionManager()->numActiveTransactions()==0&&elevationDialogCommandState()==beforeCommand&&originalsPresent&&s.panelClosed;
        s.completed=s.safe&&s.error=="null"&&s.captured&&s.configurationVerified&&s.commandStatus==RTNORM&&(s.mode==L"create"?s.newEntities.length()==1:s.newEntities.isEmpty());
        if(!s.completed&&s.error=="null")s.error=quote(L"symbol field/lifecycle/new-object contract failed; requested fields are not business output");
        symbolUiProgress(s,L"symbol-operation-completed-with-facts");
    }catch(const std::exception& e){s.error=familyError(e);s.safe=false;s.completed=false;}catch(...){s.error=quote(L"unknown symbol operation cleanup failure");s.safe=false;s.completed=false;}
}
std::string symbolUiOperationJson(const SymbolUiOperation& s){
    std::vector<std::string> inputs,newObjects;for(const auto& p:s.inputPoints)inputs.push_back(point(p));for(int i=0;i<s.newEntities.length();++i)newObjects.push_back(familyCompactEntity(s.newEntities[i]));
    return "{\"id\":"+quote(s.id)+",\"mode\":"+quote(s.mode)+",\"command\":"+quote(s.command)+",\"kind\":"+quote(s.kind)+",\"processId\":"+std::to_string(s.pid)+",\"threadId\":"+std::to_string(s.thread)+",\"desktop\":"+quote(quietCurrentDesktop())+",\"beforeFields\":"+symbolUiValuesJson(s.before)+",\"afterFields\":"+symbolUiValuesJson(s.after)+",\"desiredFields\":"+symbolUiValuesJson(s.desired)+",\"beforePanel\":"+s.beforeSnapshot+",\"afterPanel\":"+s.afterSnapshot+",\"inputWcsPoints\":"+axisTraceArray(inputs)+",\"newEntities\":"+axisTraceArray(newObjects)+",\"fieldSettersAttempted\":"+plotBool(s.fieldSettersAttempted)+",\"configurationVerified\":"+plotBool(s.configurationVerified)+",\"commandStatus\":"+std::to_string(s.commandStatus)+",\"observerRemoved\":"+plotBool(s.observerRemoved)+",\"panelClosed\":"+plotBool(s.panelClosed)+",\"messages\":"+axisTraceArray(s.messages)+",\"inputTrace\":"+axisTraceArray(s.trace->inputs)+",\"commandTrace\":"+axisTraceArray(s.trace->commands)+",\"safeToContinue\":"+plotBool(s.safe)+",\"completed\":"+plotBool(s.completed)+",\"error\":"+s.error+"}";
}
