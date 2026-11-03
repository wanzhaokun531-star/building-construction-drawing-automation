#include "AnalysisCommandAbi.h"
#include "UnitConfigTranscript.h"
#include "PrintedCommandAcceptance.h"
#include "ExpectedMembership.h"
#include "NativeEvidence.h"
#include "ProductBinding.h"
#include "SceneReader.h"
#include "ProjectStore.h"
#include "AnalysisReader.h"
#include <dbxrecrd.h>
#include <dbtrans.h>
#include <rxevent.h>
#include <acdocman.h>
#include <aced.h>
#include <acedCmdNF.h>
#include <acutads.h>
#include <acutmem.h>
#include <adscodes.h>
#include <dbapserv.h>
#include <dbdict.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <memory>
#include <rxregsvc.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
namespace {
HMODULE moduleHandle{};
template <class T> class Open final {
public:
  Open(const Open &) = delete;
  Open &operator=(const Open &) = delete;
  explicit Open(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
    bce::native::require(acdbOpenObject(p, id, mode) == Acad::eOk,
                         "open fixture");
  }
  ~Open() {
    if (p)
      p->close();
  }
  T *p{};
};
struct Monitor final : AcDbDatabaseReactor {
  AcDbDatabase &db;
  unsigned changes{};
  explicit Monitor(AcDbDatabase &d) : db(d) {
    bce::native::require(db.addReactor(this) == Acad::eOk, "monitor");
  }
  ~Monitor() { db.removeReactor(this); }
  void objectAppended(const AcDbDatabase *, const AcDbObject *) override {
    ++changes;
  }
  void objectModified(const AcDbDatabase *, const AcDbObject *) override {
    ++changes;
  }
  void objectOpenedForModify(const AcDbDatabase *,
                             const AcDbObject *) override {
    ++changes;
  }
  void objectErased(const AcDbDatabase *, const AcDbObject *,
                    Adesk::Boolean) override {
    ++changes;
  }
  void headerSysVarChanged(const AcDbDatabase *, const ACHAR *,
                           Adesk::Boolean ok) override {
    if (ok)
      ++changes;
  }
};
AcDbObjectIdArray roots(AcDbDatabase &db) {
  Open<AcDbBlockTableRecord> model(acdbSymUtil()->blockModelSpaceId(&db));
  AcDbBlockTableRecordIterator *raw{};
  bce::native::require(model.p->newIterator(raw) == Acad::eOk, "iterator");
  std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);
  AcDbObjectIdArray ids;
  for (; !it->done(); it->step()) {
    AcDbObjectId id;
    bce::native::require(it->getEntityId(id) == Acad::eOk, "id");
    ids.append(id);
  }
  return ids;
}
std::string globals() {
  std::ostringstream out;
  for (auto name : {L"INSUNITS", L"LTSCALE", L"CELTSCALE",
                    L"DIMSTYLE", L"TEXTSTYLE", L"FRAME", L"WIPEOUTFRAME",
                    L"IMAGEFRAME", L"XCLIPFRAME", L"FONTALT", L"MIRRTEXT"}) {
    resbuf value{};
    bce::native::require(acedGetVar(name, &value) == RTNORM, "global");
    out << bce::native::utf8(name) << ':' << value.restype << ':';
    if (value.restype == RTSTR) {
      out << bce::native::utf8(value.resval.rstring);
      acutDelString(value.resval.rstring);
    } else if (value.restype == RTSHORT)
      out << value.resval.rint;
    else if (value.restype == RTLONG)
      out << value.resval.rlong;
    else if (value.restype == RTREAL)
      out.precision(17), out << value.resval.rreal;
    else
      throw std::runtime_error("global type");
    out << '\n';
  }
  return out.str();
}
std::string resources(AcDbDatabase &db) {
  std::ostringstream out;
  for (const auto &r : bce::native_scene::readResources(db))
    out << r.kind.size() << ':' << r.kind << r.name.size() << ':' << r.name
        << r.definitionSignature.size() << ':' << r.definitionSignature;
  return out.str();
}
std::string nod(AcDbDatabase &db) {
  Open<AcDbDictionary> d(db.namedObjectsDictionaryId());
  std::unique_ptr<AcDbDictionaryIterator> it(d.p->newIterator());
  std::ostringstream out;
  for (; !it->done(); it->next())
    out << bce::native::utf8(it->name()) << ':'
        << bce::native_scene::handle(it->objectId()) << '\n';
  return out.str();
}
bool commandOwnedBy(const wchar_t *name, HMODULE module) {
  auto *command = acedRegCmds->lookupGlobalCmd(name);
  if (!command || !module)
    return false;
  MEMORY_BASIC_INFORMATION memory{};
  return VirtualQuery(reinterpret_cast<const void *>(command->functionAddr()),
                      &memory, sizeof(memory)) == sizeof(memory) &&
         memory.AllocationBase == module;
}
std::string moduleState(const std::wstring &product) {
  using namespace bce::native;
  const auto module = GetModuleHandleW(product.c_str());
  const auto boolean = [](bool value) { return value ? "true" : "false"; };
  return object(
      {{"win32ModulePresent", boolean(module != nullptr)},
       {"sdkLocked", boolean(module && acrxDynamicLinker->isApplicationLocked(
                                           product.c_str()))},
       {"sdkBusy",
        boolean(module && acrxDynamicLinker->isAppBusy(product.c_str()))},
       {"analyzePresent",
        boolean(acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE") != nullptr)},
       {"infoPresent",
        boolean(acedRegCmds->lookupGlobalCmd(L"BCE_INFO") != nullptr)},
       {"issuesPresent",
        boolean(acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES") != nullptr)},
       {"testRunPresent",
        boolean(acedRegCmds->lookupGlobalCmd(L"BCE_TEST_RUN") != nullptr)},
       {"analyzeCallbackOwnedByProduct",
        boolean(commandOwnedBy(L"BCE_ANALYZE", module))},
       {"infoCallbackOwnedByProduct",
        boolean(commandOwnedBy(L"BCE_INFO", module))}});
}
struct PrintedCommand {
  std::string text;
  std::wstring path;
  bool readOnly{}, restored{};
  int status{};
};
PrintedCommand printedCommand(const wchar_t *command,
                              const std::wstring &folder, AcDbDatabase &db) {
  using namespace bce::native;
  const auto mode = bce::unitconfig_test::Transcript::mode();
  const auto oldPath = bce::unitconfig_test::Transcript::pathVar(L"LOGFILEPATH");
  const auto outerGlobals = globals();
  bce::unitconfig_test::Transcript transcript(folder);
  const auto capturePath =
      bce::unitconfig_test::Transcript::pathVar(L"LOGFILEPATH");
  const auto preGlobals = globals(), preResources = resources(db),
             preNod = nod(db);
  const auto preRoots = roots(db);
  auto *doc = acDocManager->curDocument();
  auto *working = acdbHostApplicationServices()->workingDatabase();
  Monitor monitor(db);
  PrintedCommand result;
  result.status = acedCommandS(RTSTR, command, RTNONE);
  result.readOnly =
      monitor.changes == 0 && preRoots == roots(db) &&
      preGlobals == globals() && preResources == resources(db) &&
      preNod == nod(db) && doc == acDocManager->curDocument() &&
      working == acdbHostApplicationServices()->workingDatabase() &&
      bce::unitconfig_test::Transcript::mode() == 1 &&
      bce::unitconfig_test::Transcript::sameFolder(
          bce::unitconfig_test::Transcript::pathVar(L"LOGFILEPATH"), capturePath);
  result.text = transcript.stop();
  result.path = transcript.file();
  result.restored =
      mode == bce::unitconfig_test::Transcript::mode() &&
      bce::unitconfig_test::Transcript::sameFolder(
          bce::unitconfig_test::Transcript::pathVar(L"LOGFILEPATH"), oldPath) &&
      outerGlobals == globals();
  return result;
}
bool recordPrinted(bce::native::Evidence &evidence,
                   const PrintedCommand &result, const std::string &command,
                   bool expectedText) {
  using namespace bce::native;
  const auto boolean = [](bool value) { return value ? "true" : "false"; };
  const bool accepted = bce::unitconfig_test::acceptPrintedCommand(
      result.status, RTNORM, result.readOnly, result.restored, expectedText);
  evidence.capability(object({{"ordinaryCommand", quote(command)},
                              {"status", std::to_string(result.status)},
                              {"successStatus", std::to_string(RTNORM)},
                              {"readOnly", boolean(result.readOnly)},
                              {"restored", boolean(result.restored)},
                              {"expectedText", boolean(expectedText)},
                              {"accepted", boolean(accepted)},
                              {"transcript", quote(result.path)},
                              {"SHA256", quote(sha256(result.path))}}));
  return accepted;
}
void foreignCommand() noexcept {}

struct ChangeWatch final : AcDbDatabaseReactor {
  AcDbDatabase& db;
  std::set<AcDbObjectId> modified;
  unsigned headers{};
  explicit ChangeWatch(AcDbDatabase& d):db(d) { bce::native::require(db.addReactor(this)==Acad::eOk,"watch"); }
  ~ChangeWatch(){db.removeReactor(this);}
  void objectAppended(const AcDbDatabase*,const AcDbObject* o) override {modified.insert(o->objectId());}
  void objectModified(const AcDbDatabase*,const AcDbObject* o) override {modified.insert(o->objectId());}
  void objectOpenedForModify(const AcDbDatabase*,const AcDbObject* o) override {modified.insert(o->objectId());}
  void objectErased(const AcDbDatabase*,const AcDbObject* o,Adesk::Boolean) override {modified.insert(o->objectId());}
  void headerSysVarChanged(const AcDbDatabase*,const ACHAR*,Adesk::Boolean ok) override {if(ok)++headers;}
  bool onlyStore() const {
    std::set<AcDbObjectId> allowed{db.namedObjectsDictionaryId()};
    Open<AcDbDictionary> n(db.namedObjectsDictionaryId());AcDbObjectId d,r;
    if(n.p->getAt(bce::projectStoreDictionary,d)==Acad::eOk){allowed.insert(d);Open<AcDbDictionary> x(d);if(x.p->getAt(bce::projectStoreRecord,r)==Acad::eOk)allowed.insert(r);}
    return headers==0 && std::all_of(modified.begin(),modified.end(),[&](AcDbObjectId id){return allowed.count(id)!=0;});
  }
};
struct UndoWatch final:AcEditorReactor {
  unsigned started{},ended{},failed{};
  UndoWatch(){acedEditor->addReactor(this);} ~UndoWatch(){acedEditor->removeReactor(this);}
  void commandWillStart(const ACHAR* n) override {if(_wcsicmp(n,L"U")==0)++started;}
  void commandEnded(const ACHAR* n) override {if(_wcsicmp(n,L"U")==0)++ended;}
  void commandFailed(const ACHAR* n) override {if(_wcsicmp(n,L"U")==0)++failed;}
  void commandCancelled(const ACHAR* n) override {if(_wcsicmp(n,L"U")==0)++failed;}
};
struct State {
 bce::native::Config config; std::unique_ptr<bce::native::Evidence> evidence;
 AcApDocument* original{};AcApDocument* second{};AcDbDatabase* secondDb{};
 std::set<AcApDocument*> previousDocs;std::set<AcDbDatabase*> previousDbs;
 std::wstring product;BceCopyAnalysisV1 copy{};bool loaded{},pass{true};int phase{};
 std::string shape,settings,resourceState,nodState,oldBytes,persistentId,originSource,escapeBytes;
 unsigned configCancelled{};
 std::unique_ptr<ChangeWatch> watch;std::unique_ptr<UndoWatch> undo;
 std::unique_ptr<bce::unitconfig_test::Transcript> transcript;
 AcDbObjectId marker;
};
std::unique_ptr<State> state;
std::uint64_t traceSequence{};
bool terminal{};
std::string pendingCommands, lastReaderStage, lastReaderHandle;
void trace(const std::string& event) noexcept {
 try {
  if(!state)return;
  using namespace bce::native;
  auto* doc=acDocManager->curDocument();
  const auto body=object({{"contract",quote(std::string("bce.unitconfig-trace.v1"))},
    {"sequence",std::to_string(++traceSequence)},{"tick",std::to_string(GetTickCount64())},
    {"thread",std::to_string(GetCurrentThreadId())},{"event",quote(event)},
    {"phase",std::to_string(state->phase)},{"terminal",terminal?"true":"false"},
    {"currentDocument",std::to_string(reinterpret_cast<std::uintptr_t>(doc))},
    {"currentDatabase",std::to_string(reinterpret_cast<std::uintptr_t>(doc?doc->database():nullptr))},
    {"originalDocument",std::to_string(reinterpret_cast<std::uintptr_t>(state->original))},
    {"pendingCommands",quote(pendingCommands)}, {"lastReaderStage",quote(lastReaderStage)},
    {"sourceHandle",quote(lastReaderHandle)}});
  auto f=create_new(state->config.output+L"/unitconfig-trace-"+std::to_wstring(traceSequence)+L".json");
  write_flushed(f.get(),body);
 }catch(...){}
}
struct CommandTrace final:AcEditorReactor {
 CommandTrace(){acedEditor->addReactor(this);}~CommandTrace(){acedEditor->removeReactor(this);}
 void commandWillStart(const ACHAR* name)override{trace("commandWillStart:"+bce::native::utf8(name));}
 void commandEnded(const ACHAR* name)override{trace("commandEnded:"+bce::native::utf8(name));}
 void commandCancelled(const ACHAR* name)override{if(state&&_wcsicmp(name,L"BCE_CONFIG")==0)++state->configCancelled;trace("commandCancelled:"+bce::native::utf8(name));}
 void commandFailed(const ACHAR* name)override{trace("commandFailed:"+bce::native::utf8(name));}
};
std::unique_ptr<CommandTrace> commandTrace;
BceAnalysisReportV1 query(){BceAnalysisReportV1 q{};q.size=sizeof(q);q.version=1;return q;}
void expect(const char* name,bool pass,const std::string& detail){state->evidence->step(name,pass?"passed":"failed",detail);state->pass &= pass;}
void queue(const std::wstring& text){pendingCommands=bce::native::utf8(text);trace("queue");bce::native::require(acDocManager->sendStringToExecute(acDocManager->curDocument(),text.c_str(),false,false,false)==Acad::eOk,"queue ordinary command");}
AcDbDatabase& db(){auto* d=acDocManager->curDocument();bce::native::require(d&&d->database(),"current db");return *d->database();}
std::string shape(AcDbDatabase& d){
 bce::NativeSceneContext context;
 bce::native::require(context.currentDatabase()==&d,"shape must read actual current memory");
 bce::AnalysisReadOptions options;
 options.diagnosticObserver=[](const char* stage,const bce::EntityKey& key){lastReaderStage=stage;lastReaderHandle=key.handle;};
 lastReaderStage="preflight";lastReaderHandle.clear();trace("shape-readCurrentAnalysis-begin revision="+std::to_string(context.revision()));
 const auto a=bce::readCurrentAnalysis(context,options);
 std::string diagnostics;
 for(const auto& issue:a.issues)diagnostics+=issue.code+":"+issue.message+" handle="+(issue.source.entity?issue.source.entity->handle:"")+";";
 trace("shape-readCurrentAnalysis-end "+diagnostics);
 bce::native::require(a.value.has_value(),"shape read: "+diagnostics);
 std::ostringstream out;out.precision(17);
 for(const auto& g:a.value->geometry)out<<g.root.handle<<':'<<g.leaf.handle<<':'<<g.instancePath<<':'<<g.canonicalGeometry<<':'<<g.effectiveVisible<<'\n';
 for(const auto& p:a.value->scene.primitives)out<<p.key.handle<<':'<<(p.text?*p.text:"")<<':'<<p.nativeGeometrySignature<<'\n';
 return out.str();
}
bool preserved(){return acDocManager->curDocument()==state->original && shape(db())==state->shape && globals()==state->settings && resources(db())==state->resourceState;}
std::string report(){auto q=query();if(state->copy(&q,nullptr,0)!=2)return {};std::vector<char> v(static_cast<std::size_t>(q.requiredBytes));bce::native::require(state->copy(&q,v.data(),v.size())==0,"read-only report copy");return v.data();}
void analyze(){bce::native::require(acedCommandS(RTSTR,L"BCE_ANALYZE",RTNONE)==RTNORM,"ordinary analyze");}
void configInput(double scale){bce::native::require(acedCommandS(RTSTR,L"BCE_CONFIG",RTREAL,scale,RTNONE)==RTNORM,"ordinary configure numeric input");}
// BEGIN RAW_TRANSCRIPT_METADATA: raw AutoCAD bytes never enter JSON strings.
std::string rawTranscriptEvidence(const std::wstring& path) {
 using namespace bce::native;
 return object({{"ordinaryTranscript",quote(path)}, {"SHA256",quote(sha256(path))},
   {"encoding",quote(std::string("raw AutoCAD log bytes; not decoded; retained at path"))}});
}
// END RAW_TRANSCRIPT_METADATA
void closeTranscript() {
 if(state->transcript){state->transcript->stop();state->evidence->capability(rawTranscriptEvidence(state->transcript->file()));state->transcript.reset();}
}
// Only a strict, lossless decode may be used for the one Unicode INFO assertion.
// Native log bytes remain in their original file; no decoded text enters JSON.
bool logContainsUnicode(const std::string& bytes, const std::wstring& expected) {
 UINT page=CP_UTF8;
 int length=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
 if(!length){page=GetACP();length=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);}
 if(length<=0)return false;
 std::wstring wide(static_cast<std::size_t>(length),L'\0');
 if(MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),wide.data(),length)!=length)return false;
 BOOL defaultUsed{};
 const auto flags=page==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS;
 const auto count=WideCharToMultiByte(page,flags,wide.data(),length,nullptr,0,nullptr,page==CP_UTF8?nullptr:&defaultUsed);
 if(count<=0||defaultUsed)return false;
 std::string back(static_cast<std::size_t>(count),'\0');
 if(WideCharToMultiByte(page,flags,wide.data(),length,back.data(),count,nullptr,page==CP_UTF8?nullptr:&defaultUsed)!=count||defaultUsed||back!=bytes)return false;
 state->evidence->capability(bce::native::object({{"infoAssertionCodePage",std::to_string(page)},{"strictRoundtrip", "true"}}));
 return wide.find(expected)!=std::wstring::npos;
}
void finish() noexcept {
 using namespace bce::native;
 if(!state||terminal){trace("duplicate-finish-ignored");return;}
 auto& s=*state;terminal=true;trace("finish-begin");s.watch.reset();s.undo.reset();
 const auto cleanupError=[&](const std::string& message) noexcept {
  s.pass=false;trace("cleanup-error:"+message);
  try{if(s.evidence)s.evidence->error("cleanup: "+message);}catch(...){}
 };
 try{closeTranscript();}catch(const std::exception& e){cleanupError(e.what());}
 catch(...){cleanupError("unknown transcript failure");}
 // The logger destructor attempts its strict restore again; failure is already
 // recorded above. It must not prevent the report handle from being closed.
 s.transcript.reset();
 try{
  if(s.loaded){const bool unloaded=acrxDynamicLinker->unloadModule(s.product.c_str(),true);s.loaded=!unloaded;
   expect("FourCommandsUnloadOwnedOnly",unloaded&&!acedRegCmds->lookupGlobalCmd(L"BCE_CONFIG")&&!acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE")&&!acedRegCmds->lookupGlobalCmd(L"BCE_INFO")&&!acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES"),"SDK unload four command callbacks");}
 }catch(const std::exception& e){cleanupError(std::string("product unload: ")+e.what());}catch(...){cleanupError("product unload unknown");}
 try{expect("FrozenSourcesUnchanged",sha256(s.config.scheme)==s.config.schemeSHA256&&sha256(s.config.reference)==s.config.referenceSHA256,"source/reference hash readback; supervisor checks six sources");}
 catch(const std::exception& e){cleanupError(std::string("source hash: ")+e.what());}catch(...){cleanupError("source hash unknown");}
 try{if(s.evidence)s.evidence->finish(s.config,s.pass?"passed":"failed");}
 catch(const std::exception& e){trace(std::string("report-finalization-error:")+e.what());}catch(...){trace("report-finalization-error:unknown");}
 s.evidence.reset();pendingCommands.clear();trace("finish-report-closed");
}
template<class F>void boundary(F f)noexcept{try{f();}catch(const std::exception& e){try{if(state&&!terminal){trace(std::string("boundary-exception:")+e.what());state->pass=false;state->evidence->error(e.what());finish();}}catch(...){}}catch(...){try{if(state&&!terminal){trace("boundary-exception:unknown");state->pass=false;state->evidence->error("unknown unitconfig driver exception");finish();}}catch(...){}}}
std::string raw(){const auto r=bce::readProjectStoreBytes(db());bce::native::require(r.canCommit(),"read raw archive");return *r.value;}
bce::ProjectArchive archive(){const auto r=bce::readProjectStore(db());bce::native::require(r.readable(),"read archive");return *r.value;}
std::uint64_t stateSequence{};
void recordState(const std::string& label, AcDbDatabase& target, int sdkStatus=0) {
 using namespace bce;using namespace bce::native;
 auto& s=*state;
 const auto bytes=readProjectStoreBytes(target);const auto a=readProjectStore(target);
 std::string hex;
 constexpr char digits[]="0123456789ABCDEF";
 if(bytes.value)for(unsigned char c:*bytes.value){hex+=digits[c>>4];hex+=digits[c&15];}
 std::string issues;for(const auto& i:a.issues)issues+=i.code+":"+i.message+";";
 Fields fields{{"contract",quote(std::string("bce.unitconfig-state.v1"))},{"label",quote(label)},
 {"phase",std::to_string(s.phase)},{"sdkStatus",std::to_string(sdkStatus)},
 {"currentDocument",std::to_string(reinterpret_cast<std::uintptr_t>(acDocManager->curDocument()))},
 {"targetDatabase",std::to_string(reinterpret_cast<std::uintptr_t>(&target))},
 {"archiveReadable",a.readable()?"true":"false"},{"archiveBound",a.value&&a.value->bound?"true":"false"},
 {"archiveCanCommit",a.canCommit()?"true":"false"},{"archiveIssues",quote(issues)},
 {"archiveRawHex",quote(hex)},{"matchesExpectedOldBytes",bytes.value&&*bytes.value==s.oldBytes?"true":"false"},
 {"matchesEscapeBeforeBytes",bytes.value&&*bytes.value==s.escapeBytes?"true":"false"},
 {"expectedPersistentId",quote(s.persistentId)},{"configCancelled",std::to_string(s.configCancelled)},
 {"undoStarted",s.undo?std::to_string(s.undo->started):"null"},
 {"undoEnded",s.undo?std::to_string(s.undo->ended):"null"},
 {"undoFailed",s.undo?std::to_string(s.undo->failed):"null"}};
 if(a.value){const auto& p=a.value->project;std::ostringstream scale;scale.precision(17);if(p.config.units.confirmedMmPerInputUnit)scale<<*p.config.units.confirmedMmPerInputUnit;
  fields.push_back({"actualUnit",quote(scale.str())});fields.push_back({"actualPersistentId",quote(p.persistentProjectId)});
  fields.push_back({"unitSource",quote(p.config.unitConfirmationSource)});fields.push_back({"generationViews",std::to_string(p.generation.views.size())});}
 if(acDocManager->curDocument()==s.original&&s.original->database()==&target){
  const bool sameShape=shape(target)==s.shape;const bool sameGlobals=globals()==s.settings;const bool sameResources=resources(target)==s.resourceState;const bool sameNod=nod(target)==s.nodState;
  auto q=query();fields.push_back({"abiQueryStatus",std::to_string(s.copy(&q,nullptr,0))});
  fields.push_back({"sourceShapeSame",sameShape?"true":"false"});fields.push_back({"sourceGlobalsSame",sameGlobals?"true":"false"});fields.push_back({"sourceResourcesSame",sameResources?"true":"false"});fields.push_back({"sourceNodSameAsBeforeCreate",sameNod?"true":"false"});
  fields.push_back({"sourceRootCount",std::to_string(roots(target).length())});
  resbuf undo{};const int status=acedGetVar(L"UNDOCTL",&undo);fields.push_back({"undoCtlGetStatus",std::to_string(status)});fields.push_back({"undoCtl",status==RTNORM?std::to_string(undo.resval.rint):"null"});
 }
 const auto path=s.config.output+L"/unitconfig-state-"+std::to_wstring(++stateSequence)+L".json";
 {auto f=create_new(path);write_flushed(f.get(),object(fields));}
 s.evidence->capability(object({{"stateLabel",quote(label)},{"statePath",quote(path)},{"SHA256",quote(sha256(path))}}));
}
void setBytes(const std::string& bytes){
 using namespace bce::native;AcDbObjectId dictionary,record;
 {Open<AcDbDictionary> n(db().namedObjectsDictionaryId());require(n.p->getAt(bce::projectStoreDictionary,dictionary)==Acad::eOk,"test store id");}
 {Open<AcDbDictionary> d(dictionary);require(d.p->getAt(bce::projectStoreRecord,record)==Acad::eOk,"test record id");}
 struct Delete {void operator()(resbuf* p)const{if(p)acutRelRb(p);}};std::unique_ptr<resbuf,Delete> first;resbuf* tail{};
 auto append=[&](const std::wstring& text){std::unique_ptr<resbuf,Delete> node(acutNewRb(1));require(node!=nullptr,"rb allocation");node->rbnext=nullptr;node->resval.rstring=nullptr;require(acutNewString(text.c_str(),node->resval.rstring)==Acad::eOk,"rb string");auto* next=node.get();if(tail)tail->rbnext=node.release();else first=std::move(node);tail=next;};
 append(L"BCE.PROJECT.HEX.1");constexpr wchar_t digits[]=L"0123456789ABCDEF";
 for(std::size_t i=0;i<bytes.size();i+=120){std::wstring part;for(auto j=i;j<(std::min)(bytes.size(),i+120);++j){const auto b=static_cast<unsigned char>(bytes[j]);part+=digits[b>>4];part+=digits[b&15];}append(part);}
 Open<AcDbXrecord> r(record,AcDb::kForWrite);require(r.p->setFromRbChain(*first)==Acad::eOk,"test-only archive fixture");
}
void rejectFixtures(){
 using namespace bce;using namespace bce::native;const auto good=raw();const auto base=archive().project;
 for(int test=0;test<9;++test){
   auto p=base;
   if(test==0)p.config.zeroLevel={"0","test-only confirmed zero",true};
   if(test==1||test==2||test==4)p.config.views.push_back({"test-view","test-shape",{},{},{}});
   if(test==2){RuleView v;v.viewIdentity="test-view";v.familyId="test-family";p.rules.views.push_back(v);p.ruleInputsConfirmationSource="test-only rules";}
   if(test==3)p.confirmedEntities.push_back({{p.config.databaseIdentity,bce::native_scene::handle(state->marker),0},"test-explicit-geometry","test-explicit-text","test-explicit-resources","AcDbText"});
   if(test==4){GeneratedViewState g;g.viewIdentity="test-view";g.moduleIdentity="test-module";g.moduleVersion="1";g.generationId="test-generation";g.sourceSignature="test-source";g.configSignature="test-config";g.commitEvidence="test-commit";g.postCommitEvidence="test-post";g.sourceEntities.push_back({{p.config.databaseIdentity,bce::native_scene::handle(state->marker),0},"test-geometry","test-text","test-resources","AcDbText"});auto ids=roots(db());g.outputs.push_back({{p.config.databaseIdentity,bce::native_scene::handle(ids[0]),0},"test-output","test-text","test-resources","AcDbEntity"});p.generation.documentIdentity="test-history";p.generation.databaseIdentity=p.config.databaseIdentity;p.generation.views.push_back(g);}
   if(test==7)p.generation.documentIdentity="empty-history-metadata-is-not-unit-only";
   if(test==8)p.ruleInputsConfirmationSource="rule-source-with-empty-collections";
   auto encoded=encodeProject(p);require(encoded.canCommit(),"valid history fixture encode");auto bytes=*encoded.value;
   if(test==5)bytes[0]='X';if(test==6)bytes.back()^=1;
   setBytes(bytes);ChangeWatch watch(db());configInput(1);auto q=query();
   expect("HistoryOrForeignRejectedWithoutWrite",raw()==bytes&&watch.modified.empty()&&watch.headers==0&&state->copy(&q,nullptr,0)==1,"fixture index="+std::to_string(test)+" exact raw bytes retained");
   analyze();const auto body=report();expect("UnboundHistoryNeverSuppliesUnitOrGeneration",body.find("generationAuthorized\tfalse")!=std::string::npos && body.find("unitSource\tBCE_CONFIG") == std::string::npos,"fixture index="+std::to_string(test));
   setBytes(good);
 }
}
void run()noexcept{if(state){trace("BCE_TEST_RUN-duplicate-entry-rejected");return;}boundary([]{
 using namespace bce::native;require(!state,"already active");auto s=std::make_unique<State>();s->config=load_config(moduleHandle);validate_identity(s->config);require(s->config.caseName==L"PublicUnitConfigLifecycle","case mismatch");s->evidence=std::make_unique<Evidence>(s->config);s->original=acDocManager->curDocument();state=std::move(s);auto& t=*state;commandTrace=std::make_unique<CommandTrace>();trace("BCE_TEST_RUN-initial-entry");t.evidence->checkpoint("driver-identity-validated");
 resbuf titled{};require(acedGetVar(L"DWGTITLED",&titled)==RTNORM&&titled.resval.rint==0&&roots(db()).length()==0,"fresh unnamed current document required");
 t.product=t.config.modulePath.substr(0,t.config.modulePath.find_last_of(L"\\/")+1)+L"BuildingElevation2020.arx";require(sha256(t.product)==BCE_PRODUCT_SHA256,"frozen product hash");t.evidence->capability(object({{"formalModulePath",quote(t.product)},{"formalModuleSHA256",quote(std::string(BCE_PRODUCT_SHA256))}}));
 for(auto n:{L"BCE_CONFIG",L"BCE_ANALYZE",L"BCE_INFO",L"BCE_ISSUES"})require(!acedRegCmds->lookupGlobalCmd(n),"preexisting public command");t.loaded=acrxDynamicLinker->loadModule(t.product.c_str(),false,true);require(t.loaded,"product load");
 auto m=GetModuleHandleW(t.product.c_str());bool owned=true;for(auto n:{L"BCE_CONFIG",L"BCE_ANALYZE",L"BCE_INFO",L"BCE_ISSUES"})owned &= commandOwnedBy(n,m);expect("FourPublicCallbacksOwnedByProduct",owned&&!acedRegCmds->lookupGlobalCmd(L"BCE_BUILD"),moduleState(t.product));t.copy=reinterpret_cast<BceCopyAnalysisV1>(GetProcAddress(m,"BceCopyLastAnalysisV1"));require(t.copy!=nullptr,"read-only ABI");
 AcDbDatabase source(false,true);require(source.readDwgFile(t.config.scheme.c_str(),AcDbDatabase::kForReadAndReadShare,false)==Acad::eOk,"source read");source.closeInput(true);auto ids=roots(source);require(ids.length()==741,"real fixture roots");AcDbIdMapping map;require(source.wblockCloneObjects(ids,acdbSymUtil()->blockModelSpaceId(&db()),map,AcDb::kDrcIgnore)==Acad::eOk,"clone 741 roots into unsaved memory");
 {Open<AcDbBlockTableRecord> model(acdbSymUtil()->blockModelSpaceId(&db()),AcDb::kForWrite);auto text=std::make_unique<AcDbText>(AcGePoint3d(-500000,-500000,0),L"I2未保存中文内容",db().textstyle(),3);require(model.p->appendAcDbEntity(t.marker,text.get())==Acad::eOk,"append unsaved Chinese");text.release()->close();}
 {
  unsigned callbacks{};bce::AnalysisReadOptions invalidOptions;
  invalidOptions.diagnosticObserver=[&](const char*,const bce::EntityKey&){++callbacks;};
  const auto rejected=bce::readDatabaseAnalysis(db(),"I2-revision-zero-negative-control",0,invalidOptions);
  expect("ZeroRevisionRejectedBeforeReaderGetters",!rejected.value&&rejected.issues.size()==1&&rejected.issues[0].code=="INVALID_DATABASE_IDENTITY"&&callbacks==0,"reproduces prior caller failure before Reader.run or any geometry getter");
  trace("zero-revision-negative-control-complete");
 }
 t.shape=shape(db());t.settings=globals();t.resourceState=resources(db());t.nodState=nod(db());
 t.transcript=std::make_unique<bce::unitconfig_test::Transcript>(t.config.output+L"/ordinary-unitconfig-output");
 analyze();expect("InitialAnalyzeRequiresUnits",report().find("UNIT_REQUIRED")!=std::string::npos,"no config assumption");
 t.watch=std::make_unique<ChangeWatch>(db());t.phase=1;queue(L"BCE_CONFIG\n1\nBCE_I2_VERIFY\n");
 });}
void cloneProjectRecordForTest(AcDbDatabase&,AcDbDatabase&);
void step()noexcept{trace("BCE_I2_STEP-entry");pendingCommands.clear();if(terminal)return;boundary([]{
 using namespace bce;using namespace bce::native;auto& s=*state;require(acDocManager->curDocument()==s.original,"original current");
 if(s.phase==1){
   const auto a=readProjectStore(db());auto q=query();const bool saved=a.readable()&&a.value->project.config.units.confirmedMmPerInputUnit==std::optional<double>(1)&&!a.value->project.config.unitConfirmationSource.empty();
   expect("OrdinaryConfigPersistsExplicitUnit",saved,"ordinary BCE_CONFIG input 1, G2 archive readback");expect("ConfigInvalidatesAnalysisAndPages",s.copy(&q,nullptr,0)==1,"old read-only report no longer available");expect("OnlyProjectDictionaryWritten",s.watch&&s.watch->onlyStore(),"event whitelist NOD + exact owned dictionary/record; zero global writes");s.watch.reset();expect("All741AndUnsavedChineseResourcesUnchanged",preserved(),"all canonical root/leaf geometry, text, resources and global values");
   if(!saved){finish();return;}
   s.persistentId=a.value->project.persistentProjectId;s.originSource=a.value->project.config.unitConfirmationSource;s.oldBytes=raw();
   s.undo=std::make_unique<UndoWatch>();recordState("before-first-U",db());s.phase=2;queue(L"_.U\nBCE_I2_STEP\n");return;
 }
 if(s.phase==2){recordState("after-first-U",db());closeTranscript();const auto a=readProjectStore(db());expect("OneActualUndoRemovesOnlyNewConfig",s.undo&&s.undo->started==1&&s.undo->ended==1&&s.undo->failed==0&&!a.readable()&&nod(db())==s.nodState&&preserved(),"real U after config; 741 roots and prior Chinese edit retained");s.undo.reset();s.phase=3;queue(L"BCE_CONFIG\n1\nBCE_I2_STEP\n");return;}
 if(s.phase==3){s.oldBytes=raw();s.persistentId=archive().project.persistentProjectId;s.originSource=archive().project.config.unitConfirmationSource;
   const auto stale=printedCommand(L"BCE_ISSUES",s.config.output+L"/issues-after-config",db());expect("ConfigInvalidatesOrdinaryIssuePages",recordPrinted(*s.evidence,stale,"BCE_ISSUES after CONFIG",stale.text.find("[1/")==std::string::npos&&stale.text.find("BCE_ANALYZE")!=std::string::npos),"actual output");
   analyze();const auto body=report();s.evidence->capability(object({{"configuredAnalysis",quote(body)}}));expect("AnalyzeConsumesOnlyExplicitUnit",body.find("UNIT_REQUIRED")==std::string::npos&&body.find("unitMmPerInputUnit\t1\n")!=std::string::npos&&body.find("unitSource\tBCE_CONFIG")!=std::string::npos&&body.find("PLAN_REQUIRED")!=std::string::npos&&body.find("generationAuthorized\tfalse")!=std::string::npos,"unit provenance + missing plan + false authorization");expect("RecognizesFourElevationsTwoSectionsAndChinese",body.find("elevations\t4\n")!=std::string::npos&&body.find("sections\t2\n")!=std::string::npos&&body.find(u8"I2未保存中文内容")!=std::string::npos,"current memory, no save");
s.phase=4;queue(L"BCE_CONFIG\n2\nBCE_I2_VERIFY\n");return;}
 if(s.phase==4){const auto a=archive();auto q=query();expect("UpdatePreservesProjectIdRefreshesConfirmation",a.project.persistentProjectId==s.persistentId&&a.project.config.units.confirmedMmPerInputUnit==std::optional<double>(2)&&a.project.config.unitConfirmationSource!=s.originSource&&s.copy(&q,nullptr,0)==1&&preserved(),"explicit input 2; no geometry authorization");s.undo=std::make_unique<UndoWatch>();recordState("before-update-U",db());s.phase=5;queue(L"_.U\nBCE_I2_STEP\n");return;}
 if(s.phase==5){recordState("after-update-U",db());expect("OneActualUndoRestoresOldUnitBytes",s.undo&&s.undo->started==1&&s.undo->ended==1&&s.undo->failed==0&&raw()==s.oldBytes&&preserved(),"one U restores exact previous archive and prior unsaved edits");s.undo.reset();
   for(double n:{0.0,-1.0}){ChangeWatch w(db());const auto old=raw();analyze();configInput(n);auto q=query();expect("InvalidNumericInputWritesNothing",raw()==old&&w.modified.empty()&&w.headers==0&&s.copy(&q,nullptr,0)==1,"input="+std::to_string(n));}
   {ChangeWatch w(db());const auto old=raw();analyze();require(acedCommandS(RTSTR,L"BCE_CONFIG",RTSTR,L"",RTNONE)==RTNORM,"empty cancel input");auto q=query();expect("EmptyCancelWritesNothing",raw()==old&&w.modified.empty()&&w.headers==0&&s.copy(&q,nullptr,0)==1,"null response cancels without default");}
   {
    auto* manager=db().transactionManager();auto* tx=manager->startTransaction();require(tx!=nullptr,"outer test transaction");
    struct Abort {AcDbTransactionManager* manager;~Abort(){manager->abortTransaction();}} abort{manager};
    const auto old=raw();ChangeWatch w(db());configInput(3);
    expect("CallerTransactionRejectedWithoutWrite",manager->topTransaction()==tx&&manager->numActiveTransactions()==1&&raw()==old&&w.modified.empty()&&w.headers==0,"ordinary CONFIG within caller transaction; ownership retained");
   }
   {
    require(acedCommandS(RTSTR,L"_.UNDO",RTSTR,L"_BEgin",RTNONE)==RTNORM,"start caller Undo group");
    struct End {~End(){acedCommandS(RTSTR,L"_.UNDO",RTSTR,L"_End",RTNONE);}} end;
    const auto old=raw();ChangeWatch w(db());configInput(3);
    expect("CallerUndoGroupRejectedWithoutWrite",raw()==old&&w.modified.empty()&&w.headers==0,"ordinary CONFIG refuses caller Undo boundary");
   }
   s.escapeBytes=raw();recordState("before-Esc",db());s.phase=6;queue(L"BCE_CONFIG\n\x03" L"BCE_I2_STEP\n");return;
 }
 if(s.phase==6){recordState("after-Esc",db());expect("EscapeCancelPreservesArchive",raw()==s.oldBytes&&preserved(),"actual queued Esc");rejectFixtures();expect("HistoryTestsPreserveCurrentDrawing",preserved(),"test archive restored; geometry unchanged");
   recordState("before-clone",db());AcDbDatabase* cloneRaw{};const auto cloneStatus=db().wblock(cloneRaw);require(cloneStatus==Acad::eOk&&cloneRaw,"test-only independent database clone status="+std::to_string(cloneStatus));std::unique_ptr<AcDbDatabase> clone(cloneRaw);recordState("clone-before-explicit-project-copy",*clone,static_cast<int>(cloneStatus));cloneProjectRecordForTest(db(),*clone);recordState("clone-before-save",*clone,static_cast<int>(cloneStatus));const auto path=s.config.output+L"/unit-config-copy.dwg";require(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES,"new test path");require(clone->saveAs(path.c_str())==Acad::eOk,"test-only clone save");clone.reset();AcDbDatabase reopened(false,true);const auto reopenStatus=reopened.readDwgFile(path.c_str(),AcDbDatabase::kForReadAndReadShare,false);require(reopenStatus==Acad::eOk,"independent reopen status="+std::to_string(reopenStatus));reopened.closeInput(true);recordState("reopened-copy",reopened,static_cast<int>(reopenStatus));recordState("current-after-reopen",db());const auto r=readProjectStore(reopened);expect("IndependentReopenKeepsUnitWithoutAuthority",r.readable()&&!r.canCommit()&&!r.value->bound&&r.value->project.persistentProjectId==s.persistentId&&r.value->project.config.units.confirmedMmPerInputUnit==std::optional<double>(1)&&r.value->project.generation.views.empty(),"copy only; current unsaved document never saved/closed/reopened");
   analyze();queue(L"BCE_I2_NEW\n");return;
 }
 });}
void verifyBeforeUndo() noexcept {
 if(!state || (state->phase!=1 && state->phase!=4)) {
   boundary([]{bce::native::require(false,"read-only undo verification phase mismatch");});return;
 }
 step();
}
void cloneProjectRecordForTest(AcDbDatabase& source,AcDbDatabase& target) {
 using namespace bce;using namespace bce::native;
 AcDbObjectId sourceDictionary,sourceRecord;
 {Open<AcDbDictionary> nod(source.namedObjectsDictionaryId());require(nod.p->getAt(projectStoreDictionary,sourceDictionary)==Acad::eOk,"source project dictionary");}
 {Open<AcDbDictionary> dict(sourceDictionary);require(dict.p->getAt(projectStoreRecord,sourceRecord)==Acad::eOk,"source project record");}
 AcDbObjectId targetDictionary;
 {Open<AcDbDictionary> nod(target.namedObjectsDictionaryId(),AcDb::kForWrite);
  require(!nod.p->has(projectStoreDictionary),"test target project dictionary unexpectedly exists");
  auto dict=std::make_unique<AcDbDictionary>();require(nod.p->setAt(projectStoreDictionary,dict.get(),targetDictionary)==Acad::eOk,"test target dictionary create");dict.release()->close();}
 AcDbObjectIdArray ids;ids.append(sourceRecord);AcDbIdMapping mapping;
 const auto status=source.wblockCloneObjects(ids,targetDictionary,mapping,AcDb::kDrcIgnore);
 require(status==Acad::eOk,"test project record clone status="+std::to_string(status));
 AcDbIdPair pair;pair.setKey(sourceRecord);require(mapping.compute(pair)&&pair.isCloned(),"project record mapping");
 {Open<AcDbDictionary> dict(targetDictionary,AcDb::kForWrite);
  std::unique_ptr<AcDbDictionaryIterator> it(dict.p->newIterator());std::wstring actualName;
  for(;!it->done();it->next())if(it->objectId()==pair.value())actualName=it->name();
  require(!actualName.empty(),"cloned record destination name");
  if(actualName!=projectStoreRecord)require(dict.p->setName(actualName.c_str(),projectStoreRecord),"rename cloned anonymous record");}
 require(readProjectStoreBytes(source).value==readProjectStoreBytes(target).value,"test cloned record bytes differ");
}
void newDocument()noexcept{boundary([]{using namespace bce::native;auto& s=*state;require(acDocManager->isApplicationContext()&&acDocManager->curDocument()==s.original,"owned application context");std::unique_ptr<AcApDocumentIterator> it(acDocManager->newAcApDocumentIterator());for(;!it->done();it->step()){s.previousDocs.insert(it->document());s.previousDbs.insert(it->document()->database());}require(acDocManager->appContextNewDocument(L"C:/Users/PC/AppData/Local/Autodesk/AutoCAD 2020/R23.1/chs/Template/acadiso.dwt")==Acad::eOk,"new owned project");s.second=acDocManager->curDocument();s.secondDb=s.second->database();require(!s.previousDocs.count(s.second)&&!s.previousDbs.count(s.secondDb),"fresh owned doc/db");auto q=query();expect("ActualSwitchInvalidatesReport",s.copy(&q,nullptr,0)==1,"new current doc");queue(L"BCE_I2_SECOND\n");});}
void second()noexcept{boundary([]{using namespace bce::native;auto& s=*state;require(acDocManager->curDocument()==s.second&&&db()==s.secondDb,"owned second doc");const auto stale=printedCommand(L"BCE_ISSUES",s.config.output+L"/issues-after-switch",db());expect("SwitchInvalidatesOrdinaryIssuePages",recordPrinted(*s.evidence,stale,"BCE_ISSUES after switch",stale.text.find("[1/")==std::string::npos&&stale.text.find("BCE_ANALYZE")!=std::string::npos),"ordinary output");configInput(1000);const auto a=archive();expect("IndependentProjectGetsOwnIdAndUnit",a.project.persistentProjectId!=s.persistentId&&a.project.config.units.confirmedMmPerInputUnit==std::optional<double>(1000),"second project explicit input 1000");analyze();const auto body=report();expect("SecondAnalysisDoesNotReuseFirstUnit",body.find("unitMmPerInputUnit\t1000\n")!=std::string::npos&&body.find("generationAuthorized\tfalse")!=std::string::npos,"actual second document report");
 if(static_cast<int>(db().insunits())==4)expect("ActualInsunitsConflictIsDiagnosed",body.find("UNIT_CONFLICT")!=std::string::npos&&body.find("unitResolved\tfalse")!=std::string::npos,"actual template INSUNITS 4 vs explicit 1000; global remains unchanged");queue(L"BCE_I2_RETURN\n");});}
void returnDocument()noexcept{boundary([]{using namespace bce::native;auto& s=*state;require(acDocManager->isApplicationContext()&&acDocManager->curDocument()==s.second&&s.second->database()==s.secondDb,"second owned session context");require(acDocManager->setCurDocument(s.original,AcAp::kNone,true)==Acad::eOk,"return original");require(acDocManager->curDocument()==s.original,"original now current");queue(L"BCE_I2_FINISH\n");});}
void finalCheck()noexcept{boundary([]{using namespace bce::native;auto& s=*state;recordState("returned-original-before-analyze",db());auto q=query();expect("ReturnInvalidatesReportAndPreservesUnit",s.copy(&q,nullptr,0)==1&&raw()==s.oldBytes&&preserved(),"switch back; no current file save/close");analyze();s.evidence->capability(object({{"returnedAnalysis",quote(report())}}));expect("ReturnAnalysisUsesOriginalUnit",report().find("unitMmPerInputUnit\t1\n")!=std::string::npos,"no cross-document scale cache");const auto infoResult=printedCommand(L"BCE_INFO",s.config.output+L"/info",db());expect("InfoDisclosesUnitOnlyScope",recordPrinted(*s.evidence,infoResult,"BCE_INFO",infoResult.text.find("BCE_CONFIG")!=std::string::npos&&logContainsUnicode(infoResult.text,L"仅配置")),"ordinary info output");require(acrxDynamicLinker->unloadModule(s.product.c_str(),true),"unload before conflict");s.loaded=false;
 require(acedRegCmds->addCommand(L"BCE_I2_FOREIGN",L"BCE_CONFIG",L"BCE_CONFIG",ACRX_CMD_MODAL,foreignCommand)==Acad::eOk,"register foreign config");const bool loaded=acrxDynamicLinker->loadModule(s.product.c_str(),false,true);auto* foreign=acedRegCmds->lookupGlobalCmd(L"BCE_CONFIG");expect("ForeignConfigConflictIsPreserved",!loaded&&foreign&&foreign->functionAddr()==foreignCommand&&!acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE")&&!acedRegCmds->lookupGlobalCmd(L"BCE_INFO")&&!acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES"),"failed product load preserves foreign callback and registers no partial commands");if(loaded){s.loaded=true;require(acrxDynamicLinker->unloadModule(s.product.c_str(),true),"unexpected module unload");s.loaded=false;}require(acedRegCmds->removeCmd(L"BCE_I2_FOREIGN",L"BCE_CONFIG")==Acad::eOk,"remove own foreign fixture");s.loaded=acrxDynamicLinker->loadModule(s.product.c_str(),false,true);expect("ReloadAfterConflictRestoresFourCommands",s.loaded&&commandOwnedBy(L"BCE_CONFIG",GetModuleHandleW(s.product.c_str())),"ordinary SDK reload");finish();});}
} // namespace
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){moduleHandle=module;DisableThreadLibraryCalls(module);}return TRUE;}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* id)noexcept{
 try{if(message==AcRx::kInitAppMsg){acrxDynamicLinker->unlockApplication(id);acrxRegisterAppMDIAware(id);struct Entry{const wchar_t* name;Adesk::Int32 flags;AcRxFunctionPtr callback;};const Entry entries[]={{L"BCE_TEST_RUN",ACRX_CMD_MODAL,run},{L"BCE_I2_STEP",ACRX_CMD_MODAL,step},{L"BCE_I2_VERIFY",ACRX_CMD_MODAL | ACRX_CMD_NO_UNDO_MARKER,verifyBeforeUndo},{L"BCE_I2_NEW",ACRX_CMD_SESSION,newDocument},{L"BCE_I2_SECOND",ACRX_CMD_MODAL,second},{L"BCE_I2_RETURN",ACRX_CMD_SESSION,returnDocument},{L"BCE_I2_FINISH",ACRX_CMD_MODAL,finalCheck}};for(const auto& e:entries)if(acedRegCmds->lookupGlobalCmd(e.name))return AcRx::kRetError;for(const auto& e:entries)if(acedRegCmds->addCommand(L"BCE_I2_CASES",e.name,e.name,e.flags,e.callback)!=Acad::eOk){acedRegCmds->removeGroup(L"BCE_I2_CASES");return AcRx::kRetError;}}
 else if(message==AcRx::kUnloadAppMsg){if(state&&state->loaded)return AcRx::kRetError;commandTrace.reset();state.reset();return acedRegCmds->removeGroup(L"BCE_I2_CASES")==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;}return AcRx::kRetOK;}catch(...){return AcRx::kRetError;}
}
