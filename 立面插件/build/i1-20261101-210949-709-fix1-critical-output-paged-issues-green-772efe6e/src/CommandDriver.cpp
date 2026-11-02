#include "AnalysisCommandAbi.h"
#include "CommandTranscript.h"
#include "ExpectedMembership.h"
#include "NativeEvidence.h"
#include "ProductBinding.h"
#include "SceneReader.h"
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
  for (auto name : {L"DBMOD", L"INSUNITS", L"LTSCALE", L"CELTSCALE",
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
  const auto mode = bce::command_test::Transcript::mode();
  const auto oldPath = bce::command_test::Transcript::pathVar(L"LOGFILEPATH");
  const auto outerGlobals = globals();
  bce::command_test::Transcript transcript(folder);
  const auto capturePath =
      bce::command_test::Transcript::pathVar(L"LOGFILEPATH");
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
      bce::command_test::Transcript::mode() == 1 &&
      bce::command_test::Transcript::sameFolder(
          bce::command_test::Transcript::pathVar(L"LOGFILEPATH"), capturePath);
  result.text = transcript.stop();
  result.path = transcript.file();
  result.restored =
      mode == bce::command_test::Transcript::mode() &&
      bce::command_test::Transcript::sameFolder(
          bce::command_test::Transcript::pathVar(L"LOGFILEPATH"), oldPath) &&
      outerGlobals == globals();
  return result;
}
void recordPrinted(bce::native::Evidence &evidence,
                   const PrintedCommand &result, const std::string &command) {
  using namespace bce::native;
  evidence.capability(object({{"ordinaryCommand", quote(command)},
                              {"transcript", quote(result.path)},
                              {"SHA256", quote(sha256(result.path))}}));
}
void foreignCommand() noexcept {}
struct LifecycleRun {
  bce::native::Config config;
  std::unique_ptr<bce::native::Evidence> evidence;
  std::wstring product;
  BceCopyAnalysisV1 copy{};
  AcApDocument *original{};
  AcApDocument *temporary{};
  std::string originalIdentity;
  bool pass{true};
  bool loaded{true};
  std::set<AcApDocument *> existingDocuments;
  std::set<AcDbDatabase *> existingDatabases;
  AcDbDatabase *temporaryDatabase{};
  std::unique_ptr<Monitor> newDocumentMonitor;
  std::string newGlobals, newResources, newNod;
};
std::unique_ptr<LifecycleRun> lifecycleRun;
void documentAfterNew() noexcept;
void documentFinish() noexcept;
void lifecycleExpect(const char *name, bool condition,
                     const std::string &details) {
  auto &r = *lifecycleRun;
  r.evidence->step(name, condition ? "passed" : "failed", details);
  r.pass = r.pass && condition;
}
BceAnalysisReportV1 query() {
  BceAnalysisReportV1 r{};
  r.size = sizeof(r);
  r.version = 1;
  return r;
}
void lifecycleFailure(const char *message) noexcept {
  try {
    if (lifecycleRun) {
      auto &r = *lifecycleRun;
      if (r.loaded) {
        r.loaded = !acrxDynamicLinker->unloadModule(r.product.c_str(), true);
      }
      r.evidence->error(message);
      r.evidence->finish(r.config, "failed");
    }
  } catch (...) {
  }
  // On failure leave owned documents in place for exact supervisor shutdown;
  // never close a modified or uncertain document merely to complete a test.
}
void documentCheckNew() noexcept {
  try {
    using namespace bce::native;
    require(lifecycleRun != nullptr, "lifecycle missing");
    auto &r = *lifecycleRun;
    require(acDocManager->curDocument() == r.temporary &&
                r.temporary->database() == r.temporaryDatabase,
            "new owned document for issues");
    require(r.newDocumentMonitor && r.newDocumentMonitor->changes == 0,
            "new document unchanged before issue command");
    r.newDocumentMonitor.reset();
    const auto printed = printedCommand(
        L"BCE_ISSUES", r.config.output + L"/issues-after-real-switch",
        *r.temporaryDatabase);
    recordPrinted(*r.evidence, printed,
                  "BCE_ISSUES after real document switch");
    lifecycleExpect("RealDocumentSwitchInvalidatesOrdinaryIssuePages",
                    printed.status == RTNORM && printed.readOnly &&
                        printed.restored &&
                        printed.text.find("BCE_ANALYZE") != std::string::npos &&
                        printed.text.find("[1/") == std::string::npos,
                    "actual ordinary user command cannot display previous "
                    "document's locations");
    r.newDocumentMonitor = std::make_unique<Monitor>(*r.temporaryDatabase);
    require(acedCommandS(RTSTR, L"BCE_ANALYZE", RTNONE) == RTNORM,
            "new document ordinary analyze");
    documentAfterNew();
  } catch (const std::exception &e) {
    lifecycleFailure(e.what());
  } catch (...) {
    lifecycleFailure("new issues lifecycle exception");
  }
}
void documentCheckReturn() noexcept {
  try {
    using namespace bce::native;
    require(lifecycleRun != nullptr, "lifecycle missing");
    auto &r = *lifecycleRun;
    require(acDocManager->curDocument() == r.original,
            "owned original after close");
    const auto printed = printedCommand(
        L"BCE_ISSUES", r.config.output + L"/issues-after-real-close",
        *r.original->database());
    recordPrinted(*r.evidence, printed,
                  "BCE_ISSUES after real close and return");
    lifecycleExpect("RealCloseInvalidatesOrdinaryIssuePages",
                    printed.status == RTNORM && printed.readOnly &&
                        printed.restored &&
                        printed.text.find("BCE_ANALYZE") != std::string::npos &&
                        printed.text.find("[1/") == std::string::npos,
                    "actual close/return cannot display locations of closed "
                    "document or prior analysis");
    require(acedCommandS(RTSTR, L"BCE_ANALYZE", RTNONE) == RTNORM,
            "return ordinary analyze");
    documentFinish();
  } catch (const std::exception &e) {
    lifecycleFailure(e.what());
  } catch (...) {
    lifecycleFailure("return issues lifecycle exception");
  }
}
void documentCreate() noexcept {
  try {
    using namespace bce::native;
    require(lifecycleRun != nullptr, "lifecycle missing");
    auto &r = *lifecycleRun;
    require(acDocManager->isApplicationContext(),
            "SDK application context required");
    require(acDocManager->curDocument() == r.original,
            "owned original mismatch");
    resbuf sdi{};
    require(acedGetVar(L"SDI", &sdi) == RTNORM && sdi.restype == RTSHORT &&
                sdi.resval.rint == 0,
            "MDI required; SDI is never changed");
    std::unique_ptr<AcApDocumentIterator> documents(
        acDocManager->newAcApDocumentIterator());
    for (; !documents->done(); documents->step()) {
      auto *existing = documents->document();
      r.existingDocuments.insert(existing);
      if (existing)
        r.existingDatabases.insert(existing->database());
    }
    r.evidence->checkpoint("before-owned-sdk-new-document");
    require(acDocManager->appContextNewDocument(
                L"C:/Users/PC/AppData/Local/Autodesk/AutoCAD "
                L"2020/R23.1/chs/Template/acadiso.dwt") == Acad::eOk,
            "SDK new owned document");
    r.temporary = acDocManager->curDocument();
    require(r.temporary && r.temporary != r.original,
            "new owned document identity");
    r.temporaryDatabase = r.temporary->database();
    require(r.temporaryDatabase &&
                r.existingDocuments.count(r.temporary) == 0 &&
                r.existingDatabases.count(r.temporaryDatabase) == 0 &&
                acDocManager->document(r.temporaryDatabase) == r.temporary,
            "fresh document and database absent from pre-create inventory");
    resbuf clean{};
    require(acedGetVar(L"DBMOD", &clean) == RTNORM &&
                clean.restype == RTSHORT && clean.resval.rint == 0,
            "created document initially unmodified");
    r.newGlobals = globals();
    r.newResources = resources(*r.temporaryDatabase);
    r.newNod = nod(*r.temporaryDatabase);
    r.newDocumentMonitor = std::make_unique<Monitor>(*r.temporaryDatabase);
    lifecycleExpect("FreshOwnedDocumentVerifiedBeforeAnalyze", true,
                    "SDK current document/DB absent from full startup "
                    "inventory, DBMOD=0, SDI=0 read-only");
    auto q = query();
    lifecycleExpect(
        "ActualDocumentSwitchInvalidatesOldReport", r.copy(&q, nullptr, 0) == 1,
        "actual SDK-created current document differs from original");
    require(acDocManager->sendStringToExecute(r.temporary,
                                              L"BCE_COMMAND_CHECK_STALE_NEW\n",
                                              true, false, false) == Acad::eOk,
            "queue ordinary new-document command");
  } catch (const std::exception &e) {
    lifecycleFailure(e.what());
  } catch (...) {
    lifecycleFailure("create lifecycle exception");
  }
}
void documentAfterNew() noexcept {
  try {
    using namespace bce::native;
    require(lifecycleRun != nullptr, "lifecycle missing");
    auto &r = *lifecycleRun;
    require(acDocManager->curDocument() == r.temporary, "new document current");
    auto q = query();
    const int status = r.copy(&q, nullptr, 0);
    lifecycleExpect("OrdinaryAnalyzeUsesNewDocumentIdentity",
                    status == 2 && q.documentIdentity != r.originalIdentity,
                    "actual ordinary command in owned newly created document");
    lifecycleExpect("NewDocumentAnalyzeIsReadOnly",
                    r.newDocumentMonitor &&
                        r.newDocumentMonitor->changes == 0 &&
                        r.newGlobals == globals() &&
                        r.newResources == resources(*r.temporaryDatabase) &&
                        r.newNod == nod(*r.temporaryDatabase),
                    "new document events, DBMOD, global settings, resources "
                    "and NOD unchanged");
    r.newDocumentMonitor.reset();
    resbuf v{};
    require(acedGetVar(L"DBMOD", &v) == RTNORM && v.restype == RTSHORT,
            "new doc DBMOD");
    require(v.resval.rint == 0, "refuse close of modified document");
    require(acDocManager->sendStringToExecute(r.temporary,
                                              L"BCE_COMMAND_CLOSE_NEW\n", true,
                                              false, false) == Acad::eOk,
            "queue owned close session");
  } catch (const std::exception &e) {
    lifecycleFailure(e.what());
  } catch (...) {
    lifecycleFailure("new document lifecycle exception");
  }
}
void documentClose() noexcept {
  try {
    using namespace bce::native;
    require(lifecycleRun != nullptr, "lifecycle missing");
    auto &r = *lifecycleRun;
    require(acDocManager->isApplicationContext(),
            "close requires SDK application context");
    require(r.temporary && r.temporary != r.original &&
                acDocManager->curDocument() == r.temporary,
            "owned close identity");
    require(r.temporary->database() == r.temporaryDatabase &&
                r.existingDocuments.count(r.temporary) == 0 &&
                r.existingDatabases.count(r.temporaryDatabase) == 0 &&
                acDocManager->document(r.temporaryDatabase) == r.temporary,
            "revalidate owned doc/database before close");
    resbuf v{};
    require(acedGetVar(L"DBMOD", &v) == RTNORM && v.restype == RTSHORT &&
                v.resval.rint == 0,
            "refuse modified owned close");
    require(acDocManager->appContextCloseDocument(r.temporary) == Acad::eOk,
            "SDK close owned zero-DBMOD document");
    r.temporary = nullptr;
    r.temporaryDatabase = nullptr;
    require(acDocManager->curDocument() == r.original,
            "original resumed after close");
    auto q = query();
    lifecycleExpect(
        "ActualCloseAndReturnInvalidateReport", r.copy(&q, nullptr, 0) == 1,
        "actual SDK close then original current; no cross-document cache");
    require(acDocManager->sendStringToExecute(
                r.original, L"BCE_COMMAND_CHECK_STALE_RETURN\n", true, false,
                false) == Acad::eOk,
            "queue resumed ordinary analyze");
  } catch (const std::exception &e) {
    lifecycleFailure(e.what());
  } catch (...) {
    lifecycleFailure("close lifecycle exception");
  }
}
void documentFinish() noexcept {
  try {
    using namespace bce::native;
    require(lifecycleRun != nullptr, "lifecycle missing");
    auto &r = *lifecycleRun;
    require(acDocManager->curDocument() == r.original,
            "finish original identity");
    auto q = query();
    const int status = r.copy(&q, nullptr, 0);
    lifecycleExpect("OrdinaryAnalyzeAfterRealCloseWorks", status == 2,
                    "fresh context after actual close and return");
    if (status == 2) {
      std::vector<char> data(static_cast<std::size_t>(q.requiredBytes));
      require(r.copy(&q, data.data(), data.size()) == 0, "resumed report");
      const std::string body(data.data());
      lifecycleExpect("OriginalUnsavedMembersSurviveDocumentLifecycle",
                      body.find("roots\t742\n") != std::string::npos &&
                          body.find(u8"I1未保存中文内容") != std::string::npos,
                      "742 owned roots and original unsaved text preserved");
    }
    r.evidence->capability(
        object({{"phase", quote(std::string("before-final-lifecycle-unload"))},
                {"state", moduleState(r.product)}}));
    const bool finalUnloaded =
        acrxDynamicLinker->unloadModule(r.product.c_str(), true);
    r.evidence->capability(
        object({{"phase", quote(std::string("after-final-lifecycle-unload"))},
                {"sdkReturn", finalUnloaded ? "true" : "false"},
                {"state", moduleState(r.product)}}));
    require(finalUnloaded, "final lifecycle product unload");
    r.loaded = false;
    lifecycleExpect("LifecycleUnloadRemovesAllProductCommands",
                    !acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE") &&
                        !acedRegCmds->lookupGlobalCmd(L"BCE_INFO") &&
                        !acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES"),
                    "normal SDK unload after real document close");
    require(sha256(r.config.scheme) == r.config.schemeSHA256 &&
                sha256(r.config.reference) == r.config.referenceSHA256,
            "final frozen inputs");
    r.evidence->finish(r.config, r.pass ? "passed" : "failed");
    lifecycleRun.reset();
  } catch (const std::exception &e) {
    lifecycleFailure(e.what());
  } catch (...) {
    lifecycleFailure("finish lifecycle exception");
  }
}
void run() noexcept {
  bce::native::Config c;
  std::unique_ptr<bce::native::Evidence> e;
  std::wstring product;
  bool loaded = false;
  try {
    using namespace bce::native;
    c = load_config(moduleHandle);
    e = std::make_unique<Evidence>(c);
    validate_identity(c);
    require(c.caseName == L"PublicAnalyzeCommandLifecycle", "case mismatch");
    bool pass = true;
    std::vector<std::string> expectedIssueRows;
    auto expect = [&](const char *name, bool ok, const std::string &details) {
      e->step(name, ok ? "passed" : "failed", details);
      pass = pass && ok;
    };
    product = c.modulePath.substr(0, c.modulePath.find_last_of(L"\\/") + 1) +
              L"BuildingElevation2020.arx";
    require(sha256(product) == BCE_PRODUCT_SHA256,
            "formal product SHA mismatch");
    e->capability(object(
        {{"formalModulePath", quote(product)},
         {"formalModuleSHA256", quote(std::string(BCE_PRODUCT_SHA256))}}));
    require(!acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE") &&
                !acedRegCmds->lookupGlobalCmd(L"BCE_INFO"),
            "preexisting command");
    loaded = acrxDynamicLinker->loadModule(product.c_str(), false, true);
    e->capability(object({{"phase", quote(std::string("after-formal-load"))},
                          {"sdkReturn", loaded ? "true" : "false"},
                          {"state", moduleState(product)}}));
    require(loaded, "formal module load");
    expect("FormalCommandsRegistered",
           acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE") &&
               acedRegCmds->lookupGlobalCmd(L"BCE_INFO") &&
               acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES") &&
               !acedRegCmds->lookupGlobalCmd(L"BCE_BUILD") &&
               !acedRegCmds->lookupGlobalCmd(L"BCE_CONFIG"),
           "only useful public commands");
    expect("PublicCallbacksBelongToFrozenProduct",
           commandOwnedBy(L"BCE_ANALYZE", GetModuleHandleW(product.c_str())) &&
               commandOwnedBy(L"BCE_INFO", GetModuleHandleW(product.c_str())) &&
               commandOwnedBy(L"BCE_ISSUES", GetModuleHandleW(product.c_str())),
           "VirtualQuery verifies all three public callback allocation bases");
    auto copy = reinterpret_cast<BceCopyAnalysisV1>(GetProcAddress(
        GetModuleHandleW(product.c_str()), "BceCopyLastAnalysisV1"));
    require(copy != nullptr, "read-only ABI missing");
    auto *doc = acDocManager->curDocument();
    require(doc && doc->database(), "current doc");
    auto &db = *doc->database();
    AcDbDatabase source(false, true);
    require(source.readDwgFile(c.scheme.c_str(),
                               AcDbDatabase::kForReadAndReadShare,
                               false) == Acad::eOk,
            "scheme read");
    source.closeInput(true);
    auto sourceIds = roots(source);
    require(sourceIds.length() == 741, "source root count");
    AcDbIdMapping mapping;
    require(source.wblockCloneObjects(sourceIds,
                                      acdbSymUtil()->blockModelSpaceId(&db),
                                      mapping, AcDb::kDrcIgnore) == Acad::eOk,
            "current-memory scheme clone");
    AcDbObjectId currentMarker;
    {
      AcDbDatabase fixture(true, true);
      struct WorkingScope {
        AcDbDatabase *previous;
        explicit WorkingScope(AcDbDatabase &target)
            : previous(acdbHostApplicationServices()->workingDatabase()) {
          acdbHostApplicationServices()->setWorkingDatabase(&target);
        }
        ~WorkingScope() {
          acdbHostApplicationServices()->setWorkingDatabase(previous);
        }
      } workingFixture(fixture);
      AcDbObjectId styleId;
      {
        Open<AcDbTextStyleTable> styles(fixture.textStyleTableId(),
                                        AcDb::kForWrite);
        auto style = std::make_unique<AcDbTextStyleTableRecord>();
        require(style->setName(L"I1_ColdFont") == Acad::eOk &&
                    style->setFileName(L"simhei.ttf") == Acad::eOk,
                "cold fixture style");
        require(styles.p->add(styleId, style.get()) == Acad::eOk,
                "cold style append");
        style.release()->close();
      }
      AcDbObjectId textId;
      {
        Open<AcDbBlockTableRecord> model(
            acdbSymUtil()->blockModelSpaceId(&fixture), AcDb::kForWrite);
        auto text = std::make_unique<AcDbText>(AcGePoint3d(-500000, -500000, 0),
                                               L"I1未保存中文内容", styleId, 3);
        require(model.p->appendAcDbEntity(textId, text.get()) == Acad::eOk,
                "unsaved scratch text");
        text.release()->close();
      }
      AcDbObjectIdArray ids;
      ids.append(textId);
      AcDbIdMapping textMapping;
      require(fixture.wblockCloneObjects(
                  ids, acdbSymUtil()->blockModelSpaceId(&db), textMapping,
                  AcDb::kDrcIgnore) == Acad::eOk,
              "raw clone cold text to actual current source");
      AcDbIdPair marker;
      marker.setKey(textId);
      require(textMapping.compute(marker) && marker.isCloned(),
              "marker mapping");
      currentMarker = marker.value();
    }

    const auto loggingModeBefore = bce::command_test::Transcript::mode();
    const auto loggingPathBefore =
        bce::command_test::Transcript::pathVar(L"LOGFILEPATH");
    bce::command_test::Transcript analyzeTranscript(c.output +
                                                    L"/analyze-output");
    const auto before = roots(db);
    const auto beforeGlobals = globals(), beforeResources = resources(db),
               beforeNod = nod(db);
    auto *const working = acdbHostApplicationServices()->workingDatabase();
    Monitor watch(db);
    e->checkpoint("before-ordinary-public-command");
    const auto commandStatus = acedCommandS(RTSTR, L"BCE_ANALYZE", RTNONE);
    BceAnalysisReportV1 report{};
    report.size = sizeof(report);
    report.version = 1;
    const auto result = copy(&report, nullptr, 0);
    expect("OrdinaryPublicAnalyzeReturnsCurrentReport",
           commandStatus == RTNORM && result == 2 && report.requiredBytes > 1,
           "command=" + std::to_string(commandStatus) +
               " ABI=" + std::to_string(result));
    if (result == 2 && report.requiredBytes > 1) {
      std::vector<char> bytes(static_cast<std::size_t>(report.requiredBytes));
      require(copy(&report, bytes.data(), bytes.size()) == 0, "report copy");
      const std::string body(bytes.data());
      {
        std::istringstream input(body);
        std::string line;
        std::vector<std::vector<std::string>> issues;
        while (std::getline(input, line)) {
          if (line.rfind("issue\t", 0) != 0)
            continue;
          std::vector<std::string> fields;
          std::istringstream row(line);
          std::string field;
          while (std::getline(row, field, '\t'))
            fields.push_back(field);
          require(fields.size() >= 5, "issue report row");
          issues.push_back(fields);
        }
        for (std::size_t i = 0; i < issues.size(); ++i) {
          const auto &fields = issues[i];
          std::string expected = "[" + std::to_string(i + 1) + "/" +
                                 std::to_string(issues.size()) + "] " +
                                 fields[1];
          if (!fields[3].empty())
            expected += " [" + fields[3] + "]";
          if (!fields[2].empty())
            expected += " [" + fields[2] + "]";
          expectedIssueRows.push_back(expected);
        }
      }
      e->capability(object({{"publicReport", quote(body)}}));
      expect("RealFourElevationsTwoSections",
             body.find("elevations\t4\n") != std::string::npos &&
                 body.find("sections\t2\n") != std::string::npos,
             "ordinary command counts");
      expect("MissingInputsRemainBlocked",
             body.find("UNIT_REQUIRED") != std::string::npos &&
                 body.find("PLAN_REQUIRED") != std::string::npos,
             "no assumed unit or plan");
      expect("UnsavedChineseRead",
             body.find(u8"I1未保存中文内容") != std::string::npos,
             "no disk save required");
      bool members = true;
      std::map<std::string, std::string> actual;
      std::istringstream lines(body);
      std::string line;
      while (std::getline(lines, line)) {
        if (line.rfind("member\t", 0) == 0) {
          const auto split = line.find('\t', 7);
          members =
              split != std::string::npos &&
              actual.emplace(line.substr(7, split - 7), line.substr(split + 1))
                  .second &&
              members;
        }
      }
      for (const auto &item : expectedMembership()) {
        auto translate = [&](const std::string &h) {
          AcDbIdPair pair;
          pair.setKey(bce::native_scene::objectId(source, h));
          require(mapping.compute(pair) && pair.isCloned(),
                  "membership mapping");
          return bce::native_scene::handle(pair.value());
        };
        const auto root = translate(item.first), title = translate(item.second);
        const auto found = actual.find(root);
        members = members && found != actual.end() && found->second == title;
      }
      expect("All741SourceMembersMatchIndependentFixture",
             members && actual.size() >= 741,
             "source mapping checked individually");
      const auto snapshot = report;
      int workerStatus = -1;
      std::thread abiWorker([&] {
        auto metadata = query();
        workerStatus = copy(&metadata, nullptr, 0);
      });
      abiWorker.join();
      expect("WorkerAbiRejectedBeforeSdkAccess", workerStatus == 3,
             "read-only ABI is main-thread-only");
      expect("NullAbiRequestRejected", copy(nullptr, nullptr, 0) == 3,
             "null metadata rejected");
      auto wrongSize = snapshot;
      wrongSize.size -= 1;
      expect("WrongAbiSizeRejected", copy(&wrongSize, nullptr, 0) == 3,
             "wrong metadata size rejected");
      auto noBuffer = snapshot;
      expect("NullBufferWithCapacityRejected", copy(&noBuffer, nullptr, 4) == 3,
             "null output with nonzero capacity rejected");
      BceAnalysisReportV1 bad{};
      bad.size = sizeof(bad);
      bad.version = 2;
      expect("IllegalAbiVersionRejected", copy(&bad, nullptr, 0) == 3,
             "version rejected");
      bad = snapshot;
      bad.sequence += 1;
      expect("StaleAbiSequenceRejected",
             copy(&bad, bytes.data(), bytes.size()) == 4,
             "wrong sequence rejected");
      bad = snapshot;
      bad.documentIdentity[0] = 'X';
      expect("ForeignAbiIdentityRejected",
             copy(&bad, bytes.data(), bytes.size()) == 4,
             "wrong identity rejected");
      bad = snapshot;
      char tinyBuffer[2] = {'x', 'y'};
      expect("SmallAbiBufferRemainsUntouched",
             copy(&bad, tinyBuffer, 2) == 2 && tinyBuffer[0] == 'x' &&
                 tinyBuffer[1] == 'y',
             "bounded copy");
    }
    expect("CommandDoesNotModifySource",
           watch.changes == 0 && before == roots(db) &&
               acDocManager->curDocument() == doc,
           "database events=" + std::to_string(watch.changes));
    expect("GlobalsResourcesNodAndDbmodUnchanged",
           beforeGlobals == globals() && beforeResources == resources(db) &&
               beforeNod == nod(db) &&
               working == acdbHostApplicationServices()->workingDatabase(),
           "12 global values including DBMOD, resources, NOD and working DB");
    expect("AnalyzePreservesActiveTranscriptSettings",
           bce::command_test::Transcript::mode() == 1 &&
               bce::command_test::Transcript::sameFolder(
                   std::filesystem::path(analyzeTranscript.file())
                       .parent_path()
                       .wstring(),
                   bce::command_test::Transcript::pathVar(L"LOGFILEPATH")),
           "logging remains exactly at owned capture state throughout ordinary "
           "analysis");
    const auto analyzePrinted = analyzeTranscript.stop();
    e->capability(
        object({{"ordinaryAnalyzeTranscript", quote(analyzeTranscript.file())},
                {"SHA256", quote(sha256(analyzeTranscript.file()))}}));
    expect("OrdinaryAnalyzePrintsAllCriticalMissingInputs",
           analyzePrinted.find("PLAN_REQUIRED") != std::string::npos &&
               analyzePrinted.find("UNIT_REQUIRED") != std::string::npos &&
               analyzePrinted.find("PROJECT_NOT_FOUND") != std::string::npos,
           "actual native command transcript includes all three critical "
           "missing codes");
    expect("OrdinaryAnalyzeShowsUserIssueEntry",
           analyzePrinted.find("BCE_ISSUES") != std::string::npos &&
               acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES"),
           "actual command output and ordinary public command, not QA ABI");
    expect(
        "TranscriptRestoresActualLoggingGlobals",
        bce::command_test::Transcript::mode() == loggingModeBefore &&
            bce::command_test::Transcript::sameFolder(
                bce::command_test::Transcript::pathVar(L"LOGFILEPATH"),
                loggingPathBefore),
        "original logging mode and path restored after real command capture");
    const auto infoPrinted =
        printedCommand(L"BCE_INFO", c.output + L"/info-output", db);
    recordPrinted(*e, infoPrinted, "BCE_INFO");
    expect("OrdinaryInfoExplainsIssueEntry",
           infoPrinted.status == RTNORM &&
               infoPrinted.text.find("BCE_ISSUES") != std::string::npos &&
               infoPrinted.text.find("BCE_ANALYZE") != std::string::npos &&
               infoPrinted.readOnly && infoPrinted.restored,
           "actual INFO output and full source/global guards");
    if (acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES")) {
      bool pagesReadOnly = true;
      std::string pageText;
      const auto pageCount = (expectedIssueRows.size() + 11) / 12;
      for (std::size_t page = 0; page < pageCount; ++page) {
        const auto printed = printedCommand(
            L"BCE_ISSUES",
            c.output + L"/issues-page-" + std::to_wstring(page + 1), db);
        recordPrinted(*e, printed, "BCE_ISSUES");
        pagesReadOnly = pagesReadOnly && printed.status == RTNORM &&
                        printed.readOnly && printed.restored;
        pageText += printed.text;
      }
      bool allRows = !expectedIssueRows.empty();
      for (const auto &row : expectedIssueRows) {
        const auto first = pageText.find(row);
        allRows = allRows && first != std::string::npos &&
                  pageText.find(row, first + row.size()) == std::string::npos;
      }
      expect("OrdinaryIssuePagesDisplayEveryLocatedIssueOnce", allRows,
             "actual printed ordinal/code/entity/view for " +
                 std::to_string(expectedIssueRows.size()) + " issues across " +
                 std::to_string(pageCount) + " ordinary calls");
      expect(
          "OrdinaryIssuesPreserveDrawingAndLoggingGlobals", pagesReadOnly,
          "each actual page independently checks DB "
          "events/roots/resources/NOD/global/working DB and restores logging");
      const auto restarted =
          printedCommand(L"BCE_ISSUES", c.output + L"/issues-restart", db);
      recordPrinted(*e, restarted, "BCE_ISSUES restart");
      expect("CompletedIssuePagingRestartsAtFirstIssue",
             restarted.readOnly && restarted.restored &&
                 !expectedIssueRows.empty() &&
                 restarted.text.find(expectedIssueRows.front()) !=
                     std::string::npos,
             "ordinary command wraps after final page");
      {
        Open<AcDbText> marker(currentMarker, AcDb::kForWrite);
        require(marker.p->setTextString(L"I1未保存中文内容-修改后") ==
                    Acad::eOk,
                "owned unsaved marker edit");
      }
      const auto stale = printedCommand(
          L"BCE_ISSUES", c.output + L"/issues-after-owned-edit", db);
      recordPrinted(*e, stale, "BCE_ISSUES after source edit");
      auto staleMeta = query();
      expect("SourceEditImmediatelyInvalidatesOrdinaryIssuePages",
             stale.readOnly && stale.restored &&
                 stale.text.find("BCE_ANALYZE") != std::string::npos &&
                 stale.text.find("[1/") == std::string::npos &&
                 copy(&staleMeta, nullptr, 0) == 1,
             "only owned marker edited before command gate; no stale issue "
             "locations displayed");
    }
    e->capability(object({{"phase", quote(std::string("before-formal-unload"))},
                          {"state", moduleState(product)}}));
    const bool unloaded =
        acrxDynamicLinker->unloadModule(product.c_str(), true);
    e->capability(object({{"phase", quote(std::string("after-formal-unload"))},
                          {"sdkReturn", unloaded ? "true" : "false"},
                          {"state", moduleState(product)}}));
    loaded = !unloaded;
    expect("FormalUnloadRemovesOnlyOwnCommands",
           unloaded && !acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE") &&
               !acedRegCmds->lookupGlobalCmd(L"BCE_INFO") &&
               !acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES") &&
               acedRegCmds->lookupGlobalCmd(L"BCE_TEST_RUN"),
           "matched asCmd=true SDK unload; state recorded");
    for (const auto *conflictName : {L"BCE_INFO", L"BCE_ISSUES"}) {
      require(acedRegCmds->addCommand(L"BCE_COMMAND_FOREIGN", conflictName,
                                      conflictName, ACRX_CMD_MODAL,
                                      foreignCommand) == Acad::eOk,
              "foreign fixture command");
      auto *const foreign = acedRegCmds->lookupGlobalCmd(conflictName);
      const bool conflictLoad =
          acrxDynamicLinker->loadModule(product.c_str(), false, true);
      e->capability(
          object({{"phase", quote(std::string("after-conflicting-load"))},
                  {"sdkReturn", conflictLoad ? "true" : "false"},
                  {"state", moduleState(product)}}));
      expect(conflictName == std::wstring(L"BCE_INFO")
                 ? "ConflictLoadPreservesForeignCommand"
                 : "IssuesConflictLoadPreservesForeignCommand",
             !conflictLoad &&
                 acedRegCmds->lookupGlobalCmd(conflictName) == foreign &&
                 !acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE"),
             "foreign command preserved on rejected init");
      if (conflictLoad) {
        acrxDynamicLinker->unloadModule(product.c_str(), true);
      }
      require(acedRegCmds->removeCmd(L"BCE_COMMAND_FOREIGN", conflictName) ==
                  Acad::eOk,
              "remove owned conflict fixture");
    }
    loaded = acrxDynamicLinker->loadModule(product.c_str(), false, true);
    require(loaded, "formal reload");
    copy = reinterpret_cast<BceCopyAnalysisV1>(GetProcAddress(
        GetModuleHandleW(product.c_str()), "BceCopyLastAnalysisV1"));
    require(copy != nullptr, "reloaded ABI");
    BceAnalysisReportV1 empty{};
    empty.size = sizeof(empty);
    empty.version = 1;
    expect("ReloadHasNoOldResult", copy(&empty, nullptr, 0) == 1,
           "old cache and callbacks discarded");
    if (acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES")) {
      const auto emptyPage =
          printedCommand(L"BCE_ISSUES", c.output + L"/issues-after-reload", db);
      recordPrinted(*e, emptyPage, "BCE_ISSUES after reload");
      expect("ReloadHasNoOrdinaryIssuePages",
             emptyPage.readOnly && emptyPage.restored &&
                 emptyPage.text.find("BCE_ANALYZE") != std::string::npos &&
                 emptyPage.text.find("[1/") == std::string::npos,
             "actual command after unload/reload rejects old page state");
    }
    require(acedCommandS(RTSTR, L"BCE_ANALYZE", RTNONE) == RTNORM,
            "reloaded ordinary command");
    expect("ReloadedOrdinaryCommandWorks", copy(&empty, nullptr, 0) == 2,
           "real reloaded command");
    lifecycleRun = std::make_unique<LifecycleRun>();
    lifecycleRun->config = c;
    lifecycleRun->evidence = std::move(e);
    lifecycleRun->product = product;
    lifecycleRun->copy = copy;
    lifecycleRun->original = doc;
    lifecycleRun->originalIdentity = empty.documentIdentity;
    lifecycleRun->pass = pass;
    loaded = false;
    require(acDocManager->sendStringToExecute(doc, L"BCE_COMMAND_CREATE_NEW\n",
                                              true, false, false) == Acad::eOk,
            "queue owned document lifecycle session");

  } catch (const std::exception &x) {
    if (!e && lifecycleRun)
      lifecycleFailure(x.what());
    try {
      if (e) {
        e->error(x.what());
        e->finish(c, "failed");
      }
    } catch (...) {
    }
  } catch (...) {
    try {
      if (e) {
        e->error("unknown ABI exception");
        e->finish(c, "failed");
      }
    } catch (...) {
    }
  }
  try {
    for (const auto *foreignName : {L"BCE_INFO", L"BCE_ISSUES"}) {
      auto *foreign = acedRegCmds->lookupGlobalCmd(foreignName);
      if (foreign && foreign->functionAddr() == foreignCommand)
        acedRegCmds->removeCmd(L"BCE_COMMAND_FOREIGN", foreignName);
    }
  } catch (...) {
  }
  if (loaded) {
    try {
      acrxDynamicLinker->unloadModule(product.c_str(), true);
    } catch (...) {
    }
  }
}
} // namespace
BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    moduleHandle = instance;
    DisableThreadLibraryCalls(instance);
  }
  return TRUE;
}
extern "C" __declspec(dllexport) AcRx::AppRetCode
    acrxEntryPoint(AcRx::AppMsgCode message, void *id) noexcept {
  try {
    if (message == AcRx::kInitAppMsg) {
      acrxDynamicLinker->unlockApplication(id);
      acrxRegisterAppMDIAware(id);
      struct Entry {
        const wchar_t *name;
        Adesk::Int32 flags;
        AcRxFunctionPtr callback;
      };
      const Entry entries[] = {
          {L"BCE_TEST_RUN", ACRX_CMD_MODAL, run},
          {L"BCE_COMMAND_CREATE_NEW", ACRX_CMD_SESSION, documentCreate},
          {L"BCE_COMMAND_CHECK_STALE_NEW", ACRX_CMD_MODAL, documentCheckNew},
          {L"BCE_COMMAND_CHECK_STALE_RETURN", ACRX_CMD_MODAL,
           documentCheckReturn},
          {L"BCE_COMMAND_AFTER_NEW", ACRX_CMD_MODAL, documentAfterNew},
          {L"BCE_COMMAND_CLOSE_NEW", ACRX_CMD_SESSION, documentClose},
          {L"BCE_COMMAND_FINISH", ACRX_CMD_MODAL, documentFinish}};
      for (const auto &entry : entries)
        if (acedRegCmds->lookupGlobalCmd(entry.name))
          return AcRx::kRetError;
      for (const auto &entry : entries)
        if (acedRegCmds->addCommand(L"BCE_COMMAND_CASES", entry.name,
                                    entry.name, entry.flags,
                                    entry.callback) != Acad::eOk) {
          acedRegCmds->removeGroup(L"BCE_COMMAND_CASES");
          return AcRx::kRetError;
        }
      return AcRx::kRetOK;
    }
    if (message == AcRx::kUnloadAppMsg) {
      if (lifecycleRun) {
        lifecycleFailure("driver unloading with pending lifecycle");
        lifecycleRun.reset();
      }
      return acedRegCmds->removeGroup(L"BCE_COMMAND_CASES") == Acad::eOk
                 ? AcRx::kRetOK
                 : AcRx::kRetError;
    }
    return AcRx::kRetOK;
  } catch (...) {
    return AcRx::kRetError;
  }
}
