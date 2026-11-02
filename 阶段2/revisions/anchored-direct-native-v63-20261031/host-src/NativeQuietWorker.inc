// File-queue orchestration for an exclusive non-input Windows desktop only.
// No COM, no global foreground/visibility manipulation, no worker thread calling CAD.
struct QuietWorkerState;
QuietWorkerState* quietWorker{};
std::wstring quietModuleIni(){wchar_t path[32768]{};const auto n=GetModuleFileNameW(moduleHandle,path,32768);if(!n||n>=32768)throw std::runtime_error("worker module path");std::wstring v(path,n);return v.substr(0,v.find_last_of(L'.'))+L".ini";}
std::wstring quietIni(const std::wstring& file,const wchar_t* section,const wchar_t* key){wchar_t b[32768]{};GetPrivateProfileStringW(section,key,L"",b,32768,file.c_str());return b;}
std::wstring quietDesktopName(HANDLE handle){wchar_t b[512]{};DWORD size{};if(!handle||!GetUserObjectInformationW(handle,UOI_NAME,b,sizeof(b),&size))return L"";return b;}
std::wstring quietCurrentDesktop(){return quietDesktopName(GetThreadDesktop(GetCurrentThreadId()));}
std::wstring quietInputDesktop(){HDESK d=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!d)return L"";const auto name=quietDesktopName(d);CloseDesktop(d);return name;}
bool quietSafeToken(const std::wstring& v){if(v.empty()||v.size()>64)return false;for(auto c:v)if(!((c>=L'0'&&c<=L'9')||(c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||c==L'_'))return false;return true;}
class QuietCommandReactor:public AcEditorReactor{public:void commandWillStart(const ACHAR*)override;void commandEnded(const ACHAR*)override;void commandCancelled(const ACHAR*)override;void commandFailed(const ACHAR*)override;};
struct QuietWorkerState{
 std::wstring session,nonce,desktop,ini,serial,command,run;
 UINT_PTR timer{};QuietCommandReactor reactor;bool reactorAdded{},started{},ended{},failed{},insideTimer{};int queueStatus=-9999;std::map<std::wstring,bool> seen;std::string error="null";
 void guard()const{const auto d=quietCurrentDesktop(),input=quietInputDesktop();if(d!=desktop||d!=L"CodexS1_"+nonce||(input.empty()||input==d))throw std::runtime_error("worker must remain on its private non-input desktop");}
};
void QuietCommandReactor::commandWillStart(const ACHAR* c){if(quietWorker&&c&&!_wcsicmp(c,quietWorker->command.c_str()))quietWorker->started=true;}
void QuietCommandReactor::commandEnded(const ACHAR* c){if(quietWorker&&c&&!_wcsicmp(c,quietWorker->command.c_str()))quietWorker->ended=true;}
void QuietCommandReactor::commandCancelled(const ACHAR* c){if(quietWorker&&c&&!_wcsicmp(c,quietWorker->command.c_str())){quietWorker->ended=true;quietWorker->failed=true;}}
void QuietCommandReactor::commandFailed(const ACHAR* c){commandCancelled(c);}
void quietAtomic(const std::wstring& file,const std::string& data,const std::wstring& root){
 if(!below(file,root))throw std::runtime_error("worker mutable report escaped session");
 static LONG sequence{};const auto temp=file+L"."+std::to_wstring(InterlockedIncrement(&sequence))+L".tmp";
 HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("worker atomic create");
 DWORD written{};const bool wrote=WriteFile(h,data.data(),static_cast<DWORD>(data.size()),&written,nullptr)!=0&&written==data.size();const bool flushed=FlushFileBuffers(h)!=0;CloseHandle(h);
 bool published=false;if(wrote&&flushed)for(int attempt=0;attempt<9;++attempt){if(MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){published=true;break;}const auto error=GetLastError();if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_ACCESS_DENIED)break;if(attempt<8)Sleep(5);}
 if(!published){DeleteFileW(temp.c_str());throw std::runtime_error("worker atomic publish");}}
bool quietNativePending(){return symbolUiBridgeRetained||symbolDiscoveryRetained||elevationUiBridgeRetained||elevationDialogTimerRetained||pendingNativeControlOpen||pendingNativeControlOpenCompletion||pendingAxisLabelsAsync||pendingAxisLabelsCompletion||pendingAxisPickfirst||retainedAxisNativeTraces;}
std::string quietBase(const QuietWorkerState& s){return "\"nonce\":"+quote(s.nonce)+",\"session\":"+quote(s.session)+",\"processId\":"+std::to_string(GetCurrentProcessId())+",\"desktop\":"+quote(quietCurrentDesktop())+",\"inputDesktop\":"+quote(quietInputDesktop());}
bool quietIdle(int& active,int& pending){
 active=-1;pending=-1;if(!acDocManager||!acDocManager->curDocument())return false;resbuf a{},n{};const int ar=acedGetVar(L"CMDACTIVE",&a),nr=acedGetVar(L"CMDNAMES",&n);bool names=nr==RTNORM&&n.restype==RTSTR&&n.resval.rstring&&!n.resval.rstring[0];if(nr==RTNORM&&n.restype==RTSTR&&n.resval.rstring)acutDelString(n.resval.rstring);if(ar==RTNORM&&a.restype==RTSHORT)active=a.resval.rint;pending=acDocManager->inputPending(acDocManager->curDocument());return active==0&&names&&pending==0&&!quietNativePending();}
void quietHeartbeat(QuietWorkerState& s){int active=-1,pending=-1;const bool idle=quietIdle(active,pending);const bool complete=!s.serial.empty()&&s.started&&s.ended&&idle;const wchar_t* phase=s.failed?L"failed":s.serial.empty()?L"ready":complete?L"idle":L"busy";auto* doc=acDocManager?acDocManager->curDocument():nullptr;
 const auto data="{\"contract\":\"stage1.quiet-worker-state.v1\","+quietBase(s)+",\"serial\":"+quote(s.serial)+",\"command\":"+quote(s.command)+",\"run\":"+quote(s.run)+",\"phase\":"+quote(phase)+",\"commandStarted\":"+(s.started?"true":"false")+",\"commandEnded\":"+(s.ended?"true":"false")+",\"commandFailed\":"+(s.failed?"true":"false")+",\"cmdActive\":"+std::to_string(active)+",\"inputPending\":"+std::to_string(pending)+",\"nativePending\":"+(quietNativePending()?"true":"false")+",\"currentDWG\":"+(doc&&doc->fileName()?quote(fullPath(doc->fileName())):"null")+",\"error\":"+s.error+"}";quietAtomic(s.session+L"\\worker-state.json",data,s.session);}
void CALLBACK quietTimer(HWND,UINT,UINT_PTR timer,DWORD)noexcept{
 auto* s=quietWorker;if(!s||s->timer!=timer||s->insideTimer)return;s->insideTimer=true;
 try{s->guard();quietHeartbeat(*s);int active{},pending{};const bool free=quietIdle(active,pending);if(free&&(s->serial.empty()||(s->started&&s->ended))){
  WIN32_FIND_DATAW item{};const auto pattern=s->session+L"\\queue\\*.ini";HANDLE search=FindFirstFileW(pattern.c_str(),&item);
  if(search!=INVALID_HANDLE_VALUE){std::wstring chosen;do{std::wstring name=item.cFileName;if(!(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&name.size()>4){auto id=name.substr(0,name.size()-4);if(quietSafeToken(id)&&s->seen.find(id)==s->seen.end()){chosen=id;break;}}}while(FindNextFileW(search,&item));FindClose(search);
   if(!chosen.empty()){s->seen[chosen]=true;const auto file=s->session+L"\\queue\\"+chosen+L".ini";const auto nonce=quietIni(file,L"job",L"nonce"),command=quietIni(file,L"job",L"command"),run=fullPath(quietIni(file,L"job",L"run"));std::string err="null";int status=-9999;
    try{if(nonce!=s->nonce)throw std::runtime_error("queue nonce mismatch");if(!quietSafeToken(command)||command.rfind(L"S1",0)!=0)throw std::runtime_error("queue command is not a single registered S1 name");auto* cmd=acedRegCmds->lookupGlobalCmd(command.c_str());MEMORY_BASIC_INFORMATION memory{};if(!cmd||!VirtualQuery(reinterpret_cast<const void*>(cmd->functionAddr()),&memory,sizeof(memory))||memory.AllocationBase!=moduleHandle)throw std::runtime_error("queue command does not belong to this exact native module");const auto p=paths();const auto attributes=GetFileAttributesW(run.c_str());if(!below(run,p.root+L"\\runs")||attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY))throw std::runtime_error("queue run is outside existing task runs");
     s->guard();if(!WritePrivateProfileStringW(L"probe",L"output",run.c_str(),s->ini.c_str()))throw std::runtime_error("worker cannot bind native run output");
     s->serial=chosen;s->command=command;s->run=run;s->started=s->ended=s->failed=false;s->error="null";
     status=acDocManager->sendStringToExecute(acDocManager->curDocument(),(command+L"\n").c_str(),true,false,false);s->queueStatus=status;if(status!=Acad::eOk)throw std::runtime_error("native command queue rejected request");
    }catch(const std::exception& e){err=familyError(e);s->error=err;}
    writeNewReport(s->session+L"\\dispatch_"+chosen+L".json","{\"contract\":\"stage1.quiet-native-dispatch.v1\","+quietBase(*s)+",\"serial\":"+quote(chosen)+",\"command\":"+quote(command)+",\"run\":"+quote(run)+",\"queueStatus\":"+std::to_string(status)+",\"error\":"+err+"}");
   }
  }
 }quietHeartbeat(*s);}catch(const std::exception& e){s->error=familyError(e);try{quietHeartbeat(*s);}catch(...){}}catch(...){s->error=quote(L"unknown native worker timer exception");}s->insideTimer=false;
}
void quietRequireIsolatedWorker(){if(!quietWorker)throw std::runtime_error("CAD writes require the dedicated non-input desktop worker");quietWorker->guard();}
void quietPrepareOwnedView(){quietRequireIsolatedWorker();const auto p=paths();auto* doc=acDocManager->curDocument();if(!doc||acDocManager->isApplicationContext()||acDocManager->mdiActiveDocument()!=doc||!doc->fileName()||_wcsicmp(fullPath(doc->fileName()).c_str(),fullPath(p.output+L"\\control.dwg").c_str())||doc->database()->transactionManager()->numActiveTransactions()!=0)throw std::runtime_error("private worker view requires owned current control and zero transactions");if(acedCommandS(RTSTR,L"_.ZOOM",RTSTR,L"_E",RTNONE)!=RTNORM||acedCommandS(RTSTR,L"_.REGEN",RTNONE)!=RTNORM)throw std::runtime_error("private worker view preparation failed");}
void nativeQuietStartImpl(){
 const auto p=paths();std::unique_ptr<QuietWorkerState> state(new QuietWorkerState);state->ini=quietModuleIni();state->nonce=quietIni(state->ini,L"worker",L"nonce");state->session=fullPath(quietIni(state->ini,L"worker",L"session"));state->desktop=L"CodexS1_"+state->nonce;std::string error="null";
 try{if(quietWorker)throw std::runtime_error("worker already started");if(state->nonce.size()!=32||!quietSafeToken(state->nonce)||!below(state->session,p.root+L"\\runs"))throw std::runtime_error("invalid isolated worker binding");state->guard();acedEditor->addReactor(&state->reactor);state->reactorAdded=true;state->timer=SetTimer(nullptr,0,250,quietTimer);if(!state->timer)throw std::runtime_error("native worker timer creation failed");}
 catch(const std::exception& e){error=familyError(e);if(state->reactorAdded){acedEditor->removeReactor(&state->reactor);state->reactorAdded=false;}}
 const auto report="{\"contract\":\"stage1.quiet-worker-ready.v1\","+quietBase(*state)+",\"timerStarted\":"+(state->timer?"true":"false")+",\"comUsed\":false,\"error\":"+error+"}";
 if(state->timer){quietWorker=state.release();try{nativePostStartupLoadControls();writeNewReport(quietWorker->session+L"\\quiet-worker-ready.json",report);}catch(...){KillTimer(nullptr,quietWorker->timer);acedEditor->removeReactor(&quietWorker->reactor);delete quietWorker;quietWorker=nullptr;throw;}}
 else {if(!below(state->session,p.root+L"\\runs"))throw std::runtime_error("invalid worker report destination");writeNewReport(state->session+L"\\quiet-worker-ready.json",report);}
}
void nativeQuietStart(){try{nativeQuietStartImpl();}catch(const std::exception& e){acutPrintf(L"\nQUIET_START_FAILED %hs\n",e.what());}catch(...){acutPrintf(L"\nQUIET_START_FAILED unknown native exception\n");}}
void nativeQuietPing(){try{if(!quietWorker)throw std::runtime_error("no isolated worker");quietWorker->guard();const auto p=paths();AcDbDatabase source(false,true);const auto read=source.readDwgFile(p.scheme.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(read,"quiet ping frozen read");const auto close=source.closeInput(true);check(close,"quiet ping close");bool wall=false;int count=-1;{WorkingDatabaseScope work(source);count=modelCount(source);AcDbObjectId id;check(source.getAcDbObjectId(id,false,AcDbHandle(L"CD83")),"quiet native wall");auto entity=open<AcDbEntity>(id);wall=!entity->isAProxy()&&!_wcsicmp(entity->isA()->name(),L"TDbWall");}writeNewReport(p.output+L"\\quiet-ping.json","{\"contract\":\"stage1.quiet-ping.v1\","+quietBase(*quietWorker)+",\"readStatus\":"+std::to_string(read)+",\"closeInputStatus\":"+std::to_string(close)+",\"modelCount\":"+std::to_string(count)+",\"nativeWall\":"+(wall?"true":"false")+",\"drawingWritten\":false}");}catch(const std::exception& e){acutPrintf(L"\nQUIET_PING_FAILED %hs\n",e.what());}}
#include "NativeQuitLifecycle.inc"
void nativeQuietExit(){
 auto* state=quietWorker;if(!state)return;
 try{state->guard();if(state->insideTimer||!acDocManager->isApplicationContext()||quietNativePending())throw std::runtime_error("quiet exit requires application context without pending business callbacks");
  auto* current=acDocManager->curDocument();if(!current||current!=acDocManager->mdiActiveDocument())throw std::runtime_error("private worker exit requires a current owned document");
  // Keep one clean document so the documented SESSION queue can execute QUIT.
  // Never clear DBMOD or answer a save prompt on behalf of another document.
  resbuf dbmod{};if(acedGetVar(L"DBMOD",&dbmod)!=RTNORM||((dbmod.restype!=RTSHORT||dbmod.resval.rint!=0)&&(dbmod.restype!=RTLONG||dbmod.resval.rlong!=0)))throw std::runtime_error("private worker final document is modified; QUIT withheld");
  std::vector<AcApDocument*> others;{std::unique_ptr<AcApDocumentIterator> it(acDocManager->newAcApDocumentIterator());for(;!it->done();it->step())if(it->document()!=current)others.push_back(it->document());}
  int closed=0;for(auto* doc:others){check(acDocManager->appContextCloseDocument(doc),"close other owned private-worker document without saving");++closed;}
  if(acDocManager->documentCount()!=1||acDocManager->curDocument()!=current)throw std::runtime_error("private worker exit document cohort differs");
  quietStartQuitLifecycle(state->session,state->nonce,state->desktop);
  const auto queued=acDocManager->sendStringToExecute(current,L"_.QUIT\n",true,false,false);check(queued,"queue native owned-host QUIT");
  quietQuitAppMessage(L"nativeQuitQueued");
  if(!KillTimer(nullptr,state->timer))throw std::runtime_error("quiet timer stop failed after QUIT queued");state->timer=0;acedEditor->removeReactor(&state->reactor);state->reactorAdded=false;
  writeNewReport(state->session+L"\\native.quit.done","{\"contract\":\"stage1.quiet-worker-exit.v2\","+quietBase(*state)+",\"run\":"+quote(state->session)+",\"documentsClosedWithoutSave\":"+std::to_string(closed)+",\"remainingCleanDocuments\":1,\"quitQueueStatus\":"+std::to_string(queued)+",\"timerStopped\":true,\"reactorRemoved\":true,\"scope\":\"native QUIT queued; process exit must be independently observed\",\"error\":null}");quietWorker=nullptr;delete state;
 }catch(const std::exception& e){state->error=familyError(e);try{quietAtomic(state->session+L"\\quiet-exit-error.json","{\"error\":"+state->error+"}",state->session);}catch(...){}}
}
