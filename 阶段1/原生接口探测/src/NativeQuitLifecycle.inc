// Actual native notifications; a queued QUIT is not a normal process exit.
class QuietQuitLifecycle final : public AcEditorReactor {
public:
    std::wstring session,nonce,desktop;
    DWORD processId{},threadId{};
    bool attached{};
    unsigned sequence{};
    void record(const wchar_t* event,const wchar_t* command=nullptr) noexcept {
        try {
            if(GetCurrentProcessId()!=processId||quietCurrentDesktop()!=desktop)return;
            const auto data="{\"contract\":\"stage1.native-quit-event.v1\",\"sequence\":"+std::to_string(++sequence)
                +",\"tickMs\":"+std::to_string(GetTickCount64())+",\"processId\":"+std::to_string(processId)
                +",\"threadId\":"+std::to_string(GetCurrentThreadId())+",\"nonce\":"+quote(nonce)
                +",\"desktop\":"+quote(desktop)+",\"event\":"+quote(event)
                +",\"command\":"+(command?quote(command):std::string("null"))+"}\n";
            const auto path=session+L"\\native-quit-events.jsonl";
            if(!below(path,session))return;
            HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)return;
            DWORD written{};WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&written,nullptr);FlushFileBuffers(file);CloseHandle(file);
        }catch(...){}
    }
    void commandWillStart(const ACHAR* c)override{record(L"commandWillStart",c);}
    void commandEnded(const ACHAR* c)override{record(L"commandEnded",c);}
    void commandCancelled(const ACHAR* c)override{record(L"commandCancelled",c);}
    void commandFailed(const ACHAR* c)override{record(L"commandFailed",c);}
    void beginQuit()override{record(L"beginQuit");}
    void quitAborted()override{record(L"quitAborted");}
    void quitWillStart()override{record(L"quitWillStart");}
    void beginCloseAll()override{record(L"beginCloseAll");}
    void docCloseWillStart(AcDbDatabase*)override{record(L"docCloseWillStart");}
};
QuietQuitLifecycle* quietQuitLifecycle{};
void quietStartQuitLifecycle(const std::wstring& session,const std::wstring& nonce,const std::wstring& desktop){
    if(quietQuitLifecycle)throw std::runtime_error("native quit lifecycle already attached");
    auto observer=std::make_unique<QuietQuitLifecycle>();observer->session=session;observer->nonce=nonce;observer->desktop=desktop;
    observer->processId=GetCurrentProcessId();observer->threadId=GetCurrentThreadId();
    acedEditor->addReactor(observer.get());observer->attached=true;quietQuitLifecycle=observer.release();quietQuitLifecycle->record(L"observerAttached");
}
void quietQuitAppMessage(const wchar_t* event)noexcept{if(quietQuitLifecycle)quietQuitLifecycle->record(event);}
void quietStopQuitLifecycle()noexcept{
    auto* observer=quietQuitLifecycle;if(!observer)return;observer->record(L"observerRemovingOnUnload");
    if(observer->attached)acedEditor->removeReactor(observer);quietQuitLifecycle=nullptr;delete observer;
}
