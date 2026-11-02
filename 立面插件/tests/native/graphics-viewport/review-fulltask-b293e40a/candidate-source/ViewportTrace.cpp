#include "ViewportTrace.h"
#include <aced.h>
#include <accmd.h>
#include <acdocman.h>
#include <dbapserv.h>
#include <dbmain.h>
#include <atomic>
#include <mutex>
#include <cstdint>
namespace bce::viewport {
using namespace bce::native;
namespace {
File file;
std::recursive_mutex mutex;
std::atomic<bool> failed{false};
std::size_t sequence{};
std::wstring nonce;
bool attached{};
std::string address(const void* p){return std::to_string(reinterpret_cast<std::uintptr_t>(p));}
std::wstring dbName(AcDbDatabase* db){const ACHAR* p{};if(db&&db->getFilename(p)==Acad::eOk&&p)return p;return {};}
struct Editor final:AcEditorReactor {
    void event(const char* name,const ACHAR* p) noexcept {try {trace(name,{{"command",quote(std::wstring(p?p:L""))}});}catch(...){failed=true;}}
    void commandWillStart(const ACHAR* p) override {event("editor.commandWillStart",p);}
    void commandEnded(const ACHAR* p) override {event("editor.commandEnded",p);}
    void commandCancelled(const ACHAR* p) override {event("editor.commandCancelled",p);}
    void commandFailed(const ACHAR* p) override {event("editor.commandFailed",p);}
} editor;
struct Commands final:AcEdCommandStackReactor {
    void event(const char* name,AcEdCommand* c,AcEdCommandEventArgs* args) noexcept {
        try {trace(name,{{"command",c&&c->globalName()?quote(std::wstring(c->globalName())):"null"},{"localName",c&&c->localName()?quote(std::wstring(c->localName())):"null"},{"group",args&&args->groupName()?quote(std::wstring(args->groupName())):"null"},{"commandStatus",args?std::to_string(args->status()):"null"},{"commandFlags",c?std::to_string(c->commandFlags()):"null"},{"handler",c?std::to_string(reinterpret_cast<std::uintptr_t>(c->functionAddr())):"null"}});}
        catch(...){failed=true;}
    }
    void commandAdded(AcEdCommand* c,AcEdCommandEventArgs* a) override {event("stack.commandAdded",c,a);}
    void commandRemoved(AcEdCommand* c,AcEdCommandEventArgs* a) override {event("stack.commandRemoved",c,a);}
    void commandWillStart(AcEdCommand* c,AcEdCommandEventArgs* a) override {event("stack.commandWillStart",c,a);}
    void commandEnded(AcEdCommand* c,AcEdCommandEventArgs* a) override {event("stack.commandEnded",c,a);}
} commands;
}
void trace(const char* event,Fields fields) noexcept {
    try {std::lock_guard<std::recursive_mutex> lock(mutex);if(!file.valid())return;
        auto* doc=acDocManager?acDocManager->curDocument():nullptr;auto* db=doc?doc->database():nullptr;
        auto* working=acdbHostApplicationServices()->workingDatabase();
        fields.push_back({"contract",quote(std::string("bce.viewport-command-trace.v1"))});
        fields.push_back({"event",quote(std::string(event))});fields.push_back({"sequence",std::to_string(++sequence)});
        fields.push_back({"tickMs",std::to_string(GetTickCount64())});fields.push_back({"pid",std::to_string(GetCurrentProcessId())});fields.push_back({"threadId",std::to_string(GetCurrentThreadId())});fields.push_back({"nonce",quote(nonce)});
        fields.push_back({"currentDocAddress",address(doc)});fields.push_back({"currentDbAddress",address(db)});fields.push_back({"currentDocPath",quote(dbName(db))});
        fields.push_back({"workingDbAddress",address(working)});fields.push_back({"workingDbPath",quote(dbName(working))});
        write_flushed(file.get(),object(fields)+"\n");
    }catch(...){failed=true;}
}
void initializeTrace(const Config& config){
    require(!file.valid()&&!attached,"trace already initialized");nonce=config.nonce;failed=false;sequence=0;
    file=create_new(config.output+L"\\viewport-command-trace.jsonl");
    require(acedEditor&&acedRegCmds,"trace editor/command stack unavailable");
    acedEditor->addReactor(&editor);acedRegCmds->addReactor(&commands);attached=true;
    trace("trace.attached",{{"moduleSHA256",quote(config.moduleSHA256)}});require(!failed,"trace initialization logging failed");
}
bool traceHealthy() noexcept{return file.valid()&&!failed;}
void closeTrace() noexcept {
    trace("trace.detaching");
    if(attached){acedRegCmds->removeReactor(&commands);acedEditor->removeReactor(&editor);attached=false;}
    file=File();
}
}
