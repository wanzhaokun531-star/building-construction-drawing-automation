#include "NativeInPlaceAudit.h"
#include "InPlaceWriter.h"
#include <rxregsvc.h>
#include <exception>
#include <functional>
#include <cmath>
#include <limits>
#include <dbapserv.h>
#include <dbidmap.h>

namespace {
using namespace bce;
using namespace bce::native;
using namespace bce::inplace_test;
HMODULE moduleHandle{};
enum class Phase { validated, setupComplete, rollbackComplete, cancelComplete, checked, committed, finished };
class UndoWatcher final : public AcEditorReactor {
public:
    unsigned started{0}, ended{0}, failed{0};
    UndoWatcher() { acedEditor->addReactor(this); }
    ~UndoWatcher() { acedEditor->removeReactor(this); }
    void commandWillStart(const ACHAR* name) override { if(_wcsicmp(name,L"U")==0) ++started; }
    void commandEnded(const ACHAR* name) override { if(_wcsicmp(name,L"U")==0) ++ended; }
    void commandCancelled(const ACHAR* name) override { if(_wcsicmp(name,L"U")==0) ++failed; }
    void commandFailed(const ACHAR* name) override { if(_wcsicmp(name,L"U")==0) ++failed; }
};
struct Case final {
    Config config;
    File report;
    File compatibilityReport;
    AcApDocument* document{}; // Borrowed only while validated command state remains current.
    AcDbObjectId target, preserved, oldText, unsavedText;
    std::unique_ptr<NativeSceneContext> context;
    Phase phase{Phase::validated};
    std::vector<std::string> assertions, checkpoints, errors, coldEvidence;
    Audit baseline, after, rollbackAfter, cancelAfter, undoAfter;
    std::vector<std::string> removedObjectEvidence;
    std::vector<std::string> diskBefore, diskAfter;
    std::unique_ptr<UndoWatcher> undoWatcher;
    std::optional<CommitReceipt> successReceipt;
    bool committed{false};
    bool failed{false};
};
std::unique_ptr<Case> active;
std::string q(const char* value) { return quote(std::string(value)); }
void write_report(const char* status) {
    auto& c = *active;
    const auto report = object({{"contract", q("bce.inplace-case.v1")}, {"caseName", quote(c.config.caseName)},
        {"status", q(status)}, {"nonce", quote(c.config.nonce)}, {"pid", std::to_string(GetCurrentProcessId())},
        {"desktop", quote(c.config.desktop)}, {"moduleSHA256", quote(c.config.moduleSHA256)}, {"iniSHA256", quote(c.config.iniSHA256)},
        {"sourceSHA256", object({{"scheme", quote(c.config.schemeSHA256)}, {"reference", quote(c.config.referenceSHA256)}})},
        {"assertions", array(c.assertions)}, {"checkpoints", array(c.checkpoints)}, {"errors", array(c.errors)},
        {"coldScopeEvidence", array(c.coldEvidence)},
        {"baseline", c.baseline.json()}, {"after", c.after.json()}, {"undoAfter", c.undoAfter.json()},
        {"rollbackAfter", c.rollbackAfter.json()}, {"cancelAfter", c.cancelAfter.json()}, {"removedObjectEvidence",array(c.removedObjectEvidence)},
        {"dwgFilesBefore",array(c.diskBefore)},{"dwgFilesAfter",array(c.diskAfter)},
        {"supportedWriteClasses", c.committed && !c.failed ? array({q("AcDbLine")}) : "[]"}, {"sourceDrawingWritten", "false"}, {"saveCloseReopenUsed", "false"},
        {"realUndoExecuted", c.undoWatcher && c.undoWatcher->ended==1 ? "true" : "false"},
        {"undoCommandsStarted",std::to_string(c.undoWatcher?c.undoWatcher->started:0)},
        {"undoCommandsEnded",std::to_string(c.undoWatcher?c.undoWatcher->ended:0)},
        {"limitations", array({q("Only explicit ordinary planar Line batch and layer capability is under test"),
            q("No TCH support, no full drawing generation; unsupported native types are rejected")})}});
    replace_owned_report(c.report.get(), report);
    replace_owned_report(c.compatibilityReport.get(), report);
}
void checkpoint(const char* text) {
    active->checkpoints.push_back(q(text));
    const auto path = active->config.output + L"\\inplace-checkpoint-" + std::to_wstring(active->checkpoints.size()) + L".json";
    auto file = create_new(path); write_flushed(file.get(), object({{"operation", q(text)}, {"nonce", quote(active->config.nonce)}}));
}
void expect(const char* nameValue, bool passed, const std::string& actual, const char* expected) {
    active->assertions.push_back(object({{"name", q(nameValue)}, {"status", q(passed ? "passed" : "failed")},
        {"actual", quote(actual)}, {"expected", q(expected)}}));
    if (!passed) active->failed = true;
}
std::string issue_codes(const std::vector<Issue>& issues) {
    std::string result;
    for (const auto& issue : issues) { if (!result.empty()) result += "; "; result += issue.code + ": " + issue.message; }
    return result;
}
void check_state(Phase expected) {
    require(active && active->phase == expected, "invalid in-place command state");
    require(acDocManager->curDocument() == active->document && active->document->database(), "STALE_DOCUMENT: test document changed");
    require(sha256(active->config.modulePath) == active->config.moduleSHA256 &&
        sha256(active->config.iniPath) == active->config.iniSHA256, "module/INI changed between commands");
}
void queue(const wchar_t* command) {
    ok(acDocManager->sendStringToExecute(active->document, command, false, false, false), "queue fixed command");
}
std::vector<std::string> dwgFiles() {
    std::vector<std::string> result;
    for(const auto& directory:{active->config.session,active->config.output}) {
        WIN32_FIND_DATAW data{}; const auto search=directory+L"\\*.dwg";
        HANDLE find=FindFirstFileW(search.c_str(),&data);
        if(find==INVALID_HANDLE_VALUE) { require(GetLastError()==ERROR_FILE_NOT_FOUND,"DWG inventory failed"); continue; }
        struct FindScope { HANDLE handle; ~FindScope() { FindClose(handle); } } scope{find};
        do { if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
            const auto path=directory+L"\\"+data.cFileName; result.push_back(object({{"path",quote(path)},{"sha256",quote(sha256(path))}}));
        } } while(FindNextFileW(find,&data));
        require(GetLastError()==ERROR_NO_MORE_FILES,"DWG inventory incomplete");
    }
    std::sort(result.begin(),result.end()); return result;
}
void finish() {
    require(sha256(active->config.scheme) == active->config.schemeSHA256 && sha256(active->config.reference) == active->config.referenceSHA256,
        "bound source bytes changed");
    active->diskAfter=dwgFiles();
    expect("UnnamedDocumentAndDiskInputsRemainUnchanged",active->diskBefore==active->diskAfter&&sysvar(L"DWGTITLED")=="0",
        array(active->diskAfter),"same session/output DWG files and unnamed current drawing; no save/close/reopen");
    checkpoint("final-verification-complete"); active->phase = Phase::finished;
    write_report(active->failed ? "failed" : "passed");
    active->report = File{}; active->compatibilityReport = File{};
    acutPrintf(L"\nBCE_INPLACE_CASE_FINAL; inspect inplace-case.json\n");
}
void boundary(const std::function<void()>& body) noexcept {
    try { body(); }
    catch (const std::exception& ex) {
        if (active) { try { active->failed = true; active->errors.push_back(quote(std::string(ex.what()))); write_report("failed");
            active->phase = Phase::finished; active->report = File{}; active->compatibilityReport = File{}; } catch (...) {} }
        acutPrintf(L"\nBCE_INPLACE_INFRASTRUCTURE_FAILURE\n");
    } catch (...) { acutPrintf(L"\nBCE_INPLACE_UNKNOWN_FAILURE\n"); }
}
void run() noexcept { boundary([] {
    require(!active, "case already started"); auto c = std::make_unique<Case>();
    c->config = load_config(moduleHandle); validate_identity(c->config);
    require(c->config.caseName == L"InPlaceBoundary" || c->config.caseName == L"InPlaceColdScope", "UNSUPPORTED_CASE");
    c->document = acDocManager->curDocument(); require(c->document && c->document->database(), "no document");
    const std::wstring filename = c->document->fileName();
    require(filename == L"Drawing1.dwg" || filename == L"Drawing1", "fixture requires isolated initial Drawing1");
    require(sysvar(L"DWGTITLED") == "0" && roots(*c->document->database()).empty(), "fixture document must be unnamed and empty");
    c->report = create_new(c->config.output + L"\\inplace-case.json");
    c->compatibilityReport = create_new(c->config.output + L"\\native-case.json");
    active = std::move(c); active->diskBefore=dwgFiles(); write_report("failed"); checkpoint("validated-before-fixture");
    queue(active->config.caseName == L"InPlaceColdScope" ? L"BCE_IP_COLD_SCOPE\n" : L"BCE_IP_SETUP\n");
}); }
// Independent N6 integration probe. No source getGeomExtents, display update, or
// readScene is called before the single cold writer preflight under observation.
class ColdMonitor final : public AcDbDatabaseReactor {
public:
    explicit ColdMonitor(AcDbDatabase& db):db_(db) { ok(db_.addReactor(this),"cold monitor attach"); }
    ~ColdMonitor() { db_.removeReactor(this); }
    unsigned changes{},fontEvents{};
    bool loggingFailed{};
    std::string phase;
    std::vector<std::string> events;
    void record(const char* event,const AcDbObject* obj) noexcept {
        ++changes;
        try {
            const std::wstring type=obj?obj->isA()->name():L"";
            if(type.find(L"AcDbFontTable")==0) ++fontEvents;
            events.push_back(object({{"event",q(event)},{"phase",quote(phase)},
                {"type",quote(type)},{"handle",quote(obj?handle(obj->objectId()):std::string{})}}));
        } catch(...) { loggingFailed=true; }
    }
    void objectModified(const AcDbDatabase*,const AcDbObject* p) override {record("modified",p);}
    void objectOpenedForModify(const AcDbDatabase*,const AcDbObject* p) override {record("openedForModify",p);}
    void objectAppended(const AcDbDatabase*,const AcDbObject* p) override {record("appended",p);}
    void objectUnAppended(const AcDbDatabase*,const AcDbObject* p) override {record("unappended",p);}
    void objectReAppended(const AcDbDatabase*,const AcDbObject* p) override {record("reappended",p);}
    void objectErased(const AcDbDatabase*,const AcDbObject* p,Adesk::Boolean) override {record("erased",p);}
    void headerSysVarChanged(const AcDbDatabase*,const ACHAR*,Adesk::Boolean success) override {if(success)record("header",nullptr);}
private:AcDbDatabase& db_;
};
void coldScope() noexcept { boundary([] {
    check_state(Phase::validated); auto& c=*active; auto& db=*c.document->database();
    require(db.transactionManager()->numActiveTransactions()==0,"cold fixture requires no outer transaction");
    ColdMonitor monitor(db); monitor.phase="install-cold-fixture";
    auto* workingBefore=acdbHostApplicationServices()->workingDatabase();
    {
        AcDbDatabase fixture(true,true);
        struct WorkingScope {
            AcDbDatabase* previous;
            explicit WorkingScope(AcDbDatabase& own):previous(acdbHostApplicationServices()->workingDatabase()) {
                acdbHostApplicationServices()->setWorkingDatabase(&own);
            }
            ~WorkingScope() {acdbHostApplicationServices()->setWorkingDatabase(previous);}
        } working(fixture);
        AcDbObjectId styleId;
        {
            Opened<AcDbTextStyleTable> styles(fixture.textStyleTableId(),AcDb::kForWrite);
            auto style=std::make_unique<AcDbTextStyleTableRecord>();
            ok(style->setName(L"N6_ColdFontSource"),"cold style name");
            ok(style->setFileName(L"simhei.ttf"),"cold font");
            ok(styles->add(styleId,style.get()),"cold style append");
            ok(style.release()->close(),"cold style close");
        }
        const auto textId=append(fixture,std::make_unique<AcDbText>(AcGePoint3d(10,20,0),L"未保存中文立面图",styleId,3.0));
        const auto lineId=append(fixture,std::make_unique<AcDbLine>(AcGePoint3d(0,0,0),AcGePoint3d(1000,0,0)));
        AcDbObjectIdArray ids; ids.append(textId); ids.append(lineId); AcDbIdMapping mapping;
        ok(fixture.wblockCloneObjects(ids,modelSpace(db),mapping,AcDb::kDrcIgnore),"clone cold fixture into Main");
        for(const auto& entry:{std::pair<AcDbObjectId,AcDbObjectId*>{lineId,&c.target},{textId,&c.unsavedText}}) {
            AcDbIdPair pair; pair.setKey(entry.first);
            require(mapping.compute(pair)&&pair.isCloned()&&!pair.value().isNull(),"cold fixture identity mapping");
            *entry.second=pair.value();
        }
    }
    const auto installationFonts=monitor.fontEvents;
    expect("ColdFixtureInstalledWithoutFontTableWrites",installationFonts==0&&!monitor.loggingFailed&&
        workingBefore==acdbHostApplicationServices()->workingDatabase(),
        "installationFontEvents="+std::to_string(installationFonts),"zero Main FontTable events during fixture clone; exact workingDatabase restored");
    require(installationFonts==0&&!monitor.loggingFailed,"cold fixture invalid: installation already warmed Main");
    c.context=std::make_unique<NativeSceneContext>();
    // Exact identity/target from the fixed fixture. Bounds of preserved Text are
    // intentionally not guessed; beginWriteScope must safely read actual state.
    SceneSnapshot snapshot; snapshot.documentIdentity=c.context->documentIdentity();
    snapshot.databaseIdentity=snapshot.documentIdentity+"/database"; snapshot.revision=c.context->revision();
    snapshot.units.confirmedMmPerInputUnit=1.0;
    const EntityKey targetKey{snapshot.databaseIdentity,handle(c.target),snapshot.revision};
    snapshot.targets.push_back({targetKey,"0",{{0,0},{1000,0}},""});
    const LayoutPlan plan{"cold-scope",{{targetKey,{500,200}}},{}};
    const auto beforeEvents=monitor.changes, beforeFonts=monitor.fontEvents;
    const auto beforeRevision=c.context->revision();
    monitor.phase="cold-baseline-public-properties"; c.baseline=capture();
    require(monitor.changes==beforeEvents&&!monitor.loggingFailed,"baseline accessor warmed Main");
    monitor.phase="beginWriteScope-cold-readScene";
    InPlaceWriter writer(*c.context); const auto result=writer.beginWriteScope(plan,snapshot);
    const auto afterEvents=monitor.changes, afterFonts=monitor.fontEvents;
    const auto afterRevision=c.context->revision();
    const auto txnCount=db.transactionManager()->numActiveTransactions();
    monitor.phase="after-scope-public-properties"; c.after=capture();
    const auto detail=object({{"issues",quote(issue_codes(result.issues))},
        {"sourceEvents",std::to_string(afterEvents-beforeEvents)},{"fontEvents",std::to_string(afterFonts-beforeFonts)},
        {"revisionBefore",std::to_string(beforeRevision)},{"revisionAfter",std::to_string(afterRevision)},
        {"transactionsAfter",std::to_string(txnCount)},{"events",array(monitor.events)},
        {"loggingFailed",monitor.loggingFailed?"true":"false"}});
    c.coldEvidence.push_back(detail);
    expect("ColdWriterScopeSucceeds",result.canCommit(),detail,"valid explicit ordinary Line scope succeeds with cold preserved Chinese Text");
    expect("ColdWriterScopeDoesNotWriteSource",afterEvents==beforeEvents&&afterFonts==beforeFonts&&
        afterRevision==beforeRevision&&!monitor.loggingFailed,detail,"zero source modification/open-for-write events and unchanged context revision");
    expect("ColdWriterScopePreservesContentsAndGlobals",c.baseline.sameContents(c.after)&&txnCount==0&&
        workingBefore==acdbHostApplicationServices()->workingDatabase(),c.after.json(),"all typed roots/resources/globals unchanged; no transaction opened; exact workingDatabase restored");
    c.phase=Phase::checked; finish();
}); }
void setup() noexcept { boundary([] {
    check_state(Phase::validated); auto& c = *active; auto& db = *c.document->database();
    require(db.transactionManager()->numActiveTransactions() == 0, "fixture outer transaction unexpected");
    AcDbObjectId layerId, styleId;
    {
        Opened<AcDbLayerTable> table(db.layerTableId(), AcDb::kForWrite);
        auto layer = std::make_unique<AcDbLayerTableRecord>(); ok(layer->setName(L"N2Fixture"), "fixture layer name");
        ok(table->add(layerId, layer.get()), "fixture layer add"); auto* owned = layer.release(); ok(owned->close(), "fixture layer close");
    }
    {
        Opened<AcDbTextStyleTable> table(db.textStyleTableId(), AcDb::kForWrite);
        auto style = std::make_unique<AcDbTextStyleTableRecord>(); ok(style->setName(L"Shared"), "fixture style name");
        ok(style->setFileName(L"txt.shx"), "fixture style font"); ok(style->setXScale(1.0), "fixture style width");
        ok(table->add(styleId, style.get()), "fixture style add"); auto* owned = style.release(); ok(owned->close(), "fixture style close");
    }
    const auto makeLine = [&](double y) {
        auto e = std::make_unique<AcDbLine>(AcGePoint3d(0.0,y,0.0), AcGePoint3d(1000.0,y,0.0));
        e->setDatabaseDefaults(&db); ok(e->setLayer(layerId), "fixture line layer"); return append(db, std::move(e));
    };
    c.target = makeLine(0.0); c.preserved = makeLine(100.0);
    const auto makeText = [&](const wchar_t* text, double y) {
        auto e = std::make_unique<AcDbText>(); e->setDatabaseDefaults(&db);
        ok(e->setLayer(layerId), "fixture text layer"); ok(e->setTextStyle(styleId), "fixture text style");
        ok(e->setHeight(50.0), "fixture text height"); ok(e->setPosition(AcGePoint3d(0.0,y,0.0)), "fixture text position");
        ok(e->setTextString(text), "fixture text content"); return append(db,std::move(e));
    };
    c.oldText = makeText(L"Protected old text", 300.0); c.unsavedText = makeText(L"保留未保存编辑", 400.0);
    c.context = std::make_unique<NativeSceneContext>(); c.phase = Phase::setupComplete;
    checkpoint("setup-command-fixture-complete-no-save"); queue(L"BCE_IP_APPLY\n");
}); }
bool hasIssue(const std::vector<Issue>& issues,const char* code) {
    return std::any_of(issues.begin(),issues.end(),[code](const Issue& issue){return issue.code==code;});
}
SceneSnapshot currentScene() {
    auto result=readScene({*active->context,{handle(active->target)},1.0});
    require(result.canCommit(),"scene prerequisite failed: "+issue_codes(result.issues)); return *result.value;
}
std::unique_ptr<PreparedBatch> batch() {
    auto result=PreparedBatch::create({{2000.0,0.0},{3000.0,0.0},"N2NewLayer",3});
    require(result.canCommit(),"prepared prerequisite failed: "+issue_codes(result.issues)); return std::move(*result.value);
}
LayoutPlan planFor(PreparedBatch& prepared,const SceneSnapshot& scene) {
    const auto layer=prepared.layerDefinition(); require(layer.canCommit(),"prepared layer not readable");
    return {"N2",{{scene.targets.front().key,{500.0,200.0}}},{*layer.value}};
}
class StopAfterTranslation final : public CommitObserver {
public:
    explicit StopAfterTranslation(bool cancel):cancel_(cancel) {}
    std::vector<CommitStage> stages;
    bool continueCommit(CommitStage stage) override {
        stages.push_back(stage);
        if(stage==CommitStage::targetsTranslated) { if(cancel_) return false; throw std::runtime_error("injected exception after all mutations"); }
        return true;
    }
    bool reachedAll() const { return stages==std::vector<CommitStage>{CommitStage::resourcesCreated,CommitStage::entitiesAppended,CommitStage::targetsTranslated}; }
private:bool cancel_;
};
bool removed(const CommitReceipt& receipt,const char* phase) {
    auto& db=*active->document->database(); bool all=true; std::vector<std::string> handles=receipt.createdResourceHandles;
    for(const auto& key:receipt.createdEntities) handles.push_back(key.handle);
    for(const auto& text:handles) {
        AcDbObjectId id; const auto wide=bce::native_scene::utf16(text); const auto status=db.getAcDbObjectId(id,false,AcDbHandle(wide.c_str()));
        const bool gone=status!=Acad::eOk || id.isNull() || !id.isValid() || id.isErased() || id.isEffectivelyErased(); all=all&&gone;
        active->removedObjectEvidence.push_back(object({{"phase",q(phase)},{"handle",quote(text)},{"lookupStatus",std::to_string(status)},
            {"null",id.isNull()?"true":"false"},{"valid",!id.isNull()&&id.isValid()?"true":"false"},
            {"erased",!id.isNull()&&id.isValid()&&id.isErased()?"true":"false"},{"absentOrErased",gone?"true":"false"}}));
    }
    return all && receipt.createdEntities.size()==1 && receipt.createdResourceHandles.size()==1;
}
void apply() noexcept { boundary([] {
    check_state(Phase::setupComplete); auto& c = *active;
    c.baseline = capture(); checkpoint("apply-command-baseline-after-setup-return");
    expect("UnsavedFixtureExists", c.baseline.entities.size() == 4 && c.baseline.dbmod != "0" &&
        sysvar(L"DWGTITLED") == "0", c.baseline.json(), "four SDK roots with unsaved Chinese Text in unnamed drawing");
    const auto scene = readScene({*c.context,{handle(c.target)},1.0});
    expect("ReadSceneIncludesUnsavedEdits", scene.canCommit() && scene.value->targets.size() == 1 && scene.value->preserved.size() == 3,
        issue_codes(scene.issues), "current unsaved scene read succeeds: one explicit target, three preserved roots");
    auto prepared = PreparedBatch::create({{2000.0,0.0},{3000.0,0.0},"N2NewLayer",3});
    expect("PreparedBatchOwnsIndependentDatabase", prepared.canCommit() && (*prepared.value)->database() &&
        (*prepared.value)->database() != c.document->database(), issue_codes(prepared.issues), "prepared Line database exists and differs from current document");
    if(!scene.canCommit() || !prepared.canCommit()) { c.after=capture(); c.phase=Phase::checked; queue(L"BCE_IP_VERIFY\n"); return; }
    auto& preparedBatch=**prepared.value; const auto plan=planFor(preparedBatch,*scene.value);
    StopAfterTranslation fault(false); InPlaceWriter writer(*c.context,&fault);
    const auto result=writer.applyToCurrentDocument(preparedBatch,plan,*scene.value);
    c.rollbackAfter=capture(); const bool gone=result.value && removed(*result.value,"rollback");
    expect("RollbackPreservesUnsavedEdits", !result.canCommit()&&hasIssue(result.issues,"INPLACE_COMMIT_FAILED")&&fault.reachedAll()&&
        gone&&c.baseline.sameContents(c.rollbackAfter),issue_codes(result.issues)+"; stages="+std::to_string(fault.stages.size()),
        "after resource/Line/target mutation injected exception: original roots/resources/globals restored and new handles invalid/erased");
    c.phase=Phase::rollbackComplete; checkpoint("rollback-verified"); queue(L"BCE_IP_CANCEL\n");
}); }
void cancel() noexcept { boundary([] {
    check_state(Phase::rollbackComplete); auto& c=*active; const auto scene=currentScene(); auto prepared=batch(); const auto plan=planFor(*prepared,scene);
    const auto before=capture(); StopAfterTranslation cancellation(true); InPlaceWriter writer(*c.context,&cancellation);
    const auto result=writer.applyToCurrentDocument(*prepared,plan,scene); c.cancelAfter=capture();
    const bool gone=result.value&&removed(*result.value,"cancel");
    expect("CancelDuringCommit",!result.canCommit()&&hasIssue(result.issues,"COMMIT_CANCELLED")&&cancellation.reachedAll()&&
        gone&&before.sameContents(c.cancelAfter),issue_codes(result.issues),"cancel after target translation follows same rollback; all unsaved edits/resources remain");
    c.phase=Phase::cancelComplete; checkpoint("cancel-verified"); queue(L"BCE_IP_CHECKS\n");
}); }
void checks() noexcept { boundary([] {
    check_state(Phase::cancelComplete); auto& c=*active; auto& db=*c.document->database(); auto prepared=batch();
    // Review P2 #1: mutate only independent prepared databases; no active-drawing entity is touched.
    for(const bool invisible:{false,true}) {
        auto altered=batch(); AcDbObjectId id;
        { Opened<AcDbBlockTableRecord> space(modelSpace(*altered->database())); AcDbBlockTableRecordIterator* raw{};
            ok(space->newIterator(raw),"prepared mutation iterator"); std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);
            require(!it->done(),"prepared Line missing"); ok(it->getEntityId(id),"prepared mutation id"); }
        { Opened<AcDbLine> line(id,AcDb::kForWrite);
            if(invisible) ok(line->setVisibility(AcDb::kInvisible),"prepared invisible mutation");
            else ok(line->setLineWeight(AcDb::kLnWt050),"prepared lineweight mutation"); }
        const bool accepted=altered->validateOrdinaryLine();
        expect(invisible?"PreparedInvisibleLineRejected":"PreparedCustomLineweightRejected",!accepted,
            "validateOrdinaryLine="+std::string(accepted?"true":"false"),"unsupported prepared Line expression change rejected");
    }
    // Root additional finding: characterize the SDK directly, then exercise the writer's scope guard.
    {
        AcDbLine precisionProbe(AcGePoint3d(0.0,0.0,0.0),AcGePoint3d(3.0,0.0,0.0));
        const auto status=precisionProbe.transformBy(AcGeMatrix3d::translation(AcGeVector3d(1.0e16,0.0,0.0)));
        const double length=precisionProbe.endPoint().distanceTo(precisionProbe.startPoint());
        const auto detail="SDK status="+std::to_string(status)+"; translated length="+number(length)+"; original length=3";
        expect("SdkLargeTranslationDistortionObserved",status==Acad::eOk&&length!=3.0,detail,"isolated SDK Line demonstrates actual finite-coordinate length distortion");
        auto fixture=std::make_unique<AcDbLine>(AcGePoint3d(0.0,0.0,0.0),AcGePoint3d(3.0,0.0,0.0)); fixture->setDatabaseDefaults(&db);
        const auto id=append(db,std::move(fixture));
        const auto shortScene=readScene({*c.context,{handle(id)},1.0}); require(shortScene.canCommit(),"short Line scene read failed");
        InPlaceWriter precisionWriter(*c.context);
        const LayoutPlan largePlan{"precision",{{shortScene.value->targets.front().key,{1.0e16,0.0}}},{}};
        const auto result=precisionWriter.beginWriteScope(largePlan,*shortScene.value);
        expect("WriterRejectsPrecisionLosingTranslation",!result.canCommit()&&hasIssue(result.issues,"INVALID_TRANSLATION_GEOMETRY"),
            detail+"; scope="+issue_codes(result.issues),"direct LayoutPlan cannot authorize SDK 3-to-4 distortion");
        { Opened<AcDbLine> line(id,AcDb::kForWrite); ok(line->erase(),"erase owned precision-only fixture"); }
    }
    auto scene=currentScene(); auto plan=planFor(*prepared,scene); const auto before=capture();
    auto shared=std::find_if(scene.resources.begin(),scene.resources.end(),[](const ResourceDefinition& r){return r.kind=="textstyle"&&r.name=="Shared";});
    require(shared!=scene.resources.end(),"real Shared style missing"); auto conflict=*shared; conflict.name="sHaReD"; conflict.definitionSignature+="different width";
    plan.requiredResources.push_back(conflict); InPlaceWriter writer(*c.context);
    const auto conflictResult=writer.applyToCurrentDocument(*prepared,plan,scene);
    expect("SharedStyleConflictKeepsOtherContent",!conflictResult.canCommit()&&hasIssue(conflictResult.issues,diagnostic::resourceConflict)&&before.sameContents(capture()),
        issue_codes(conflictResult.issues),"actual case-insensitive Shared style conflict rejected before any mutation");
    scene=currentScene(); plan=planFor(*prepared,scene);
    auto* manager=db.transactionManager(); auto* outer=manager->startTransaction(); require(outer,"outer transaction start");
    bool outerVerified=false; std::string outerIssues;
    try {
        AcDbObject* objectValue{}; ok(outer->getObject(objectValue,c.preserved,AcDb::kForWrite),"outer protected line");
        auto* line=AcDbLine::cast(objectValue); require(line,"outer typed line"); ok(line->setEndPoint(AcGePoint3d(1777.0,100.0,0.0)),"outer content");
        const auto result=writer.applyToCurrentDocument(*prepared,plan,scene); outerIssues=issue_codes(result.issues);
        outerVerified=!result.canCommit()&&hasIssue(result.issues,"OUTER_TRANSACTION_ACTIVE")&&manager->numActiveTransactions()==1&&
            manager->topTransaction()==outer&&line->endPoint()==AcGePoint3d(1777.0,100.0,0.0);
        require(manager->topTransaction()==outer,"writer consumed caller transaction"); ok(manager->abortTransaction(),"caller abort outer");
    } catch(...) { if(manager->numActiveTransactions()==1&&manager->topTransaction()==outer) manager->abortTransaction(); throw; }
    expect("OuterTransactionIsNotClosed",outerVerified&&before.sameContents(capture()),outerIssues,"outer transaction depth/top/content unchanged; only caller aborts");
    scene=currentScene(); plan=planFor(*prepared,scene);
    { Opened<AcDbLine> line(c.preserved,AcDb::kForWrite); ok(line->setEndPoint(AcGePoint3d(1250.0,100.0,0.0)),"later unsaved protected edit"); }
    const auto edited=capture(); const auto stale=writer.applyToCurrentDocument(*prepared,plan,scene);
    expect("StaleSceneRejectsWrite",!stale.canCommit()&&hasIssue(stale.issues,diagnostic::staleDocument)&&edited.sameContents(capture()),
        issue_codes(stale.issues),"actual protected geometry edit invalidates snapshot; no additional writer mutation");
    scene=currentScene(); plan=planFor(*prepared,scene);
    { NativeSceneContext other; InPlaceWriter otherWriter(other); const auto cross=otherWriter.applyToCurrentDocument(*prepared,plan,scene);
        expect("CrossContextSceneRejectsWrite",!cross.canCommit()&&hasIssue(cross.issues,diagnostic::staleDocument)&&edited.sameContents(capture()),
            issue_codes(cross.issues),"same drawing snapshot from another context rejected"); }
    // Baseline now includes the deliberately added, still-unsaved protected edit.
    c.baseline=capture(); c.phase=Phase::checked; checkpoint("scope-boundaries-verified"); queue(L"BCE_IP_SUCCESS\n");
}); }
void success() noexcept { boundary([] {
    check_state(Phase::checked); auto& c=*active; const auto scene=currentScene(); auto prepared=batch(); const auto plan=planFor(*prepared,scene);
    InPlaceWriter writer(*c.context); const auto result=writer.applyToCurrentDocument(*prepared,plan,scene); c.after=capture();
    c.committed=result.canCommit()&&result.value->committed; c.successReceipt=result.value;
    bool successEvidence=false;
    if(c.committed) {
        successEvidence=committedEvidenceMatches(c.baseline,c.after,*c.document->database(),c.target,c.preserved,c.oldText,c.unsavedText,*result.value);
    }
    bool layerDependencyCorrect=false;
    if(c.committed) {
        auto& db=*c.document->database(); Opened<AcDbLayerTable> layers(db.layerTableId()); AcDbObjectId layerId;
        ok(layers->getAt(L"N2NewLayer",layerId),"read committed new layer"); Opened<AcDbLayerTableRecord> layer(layerId);
        Opened<AcDbLinetypeTable> linetypes(db.linetypeTableId()); AcDbObjectId continuous;
        ok(linetypes->getAt(L"Continuous",continuous),"target Continuous id");
        layerDependencyCorrect=!layer->linetypeObjectId().isNull() && layer->linetypeObjectId()==continuous &&
            layer->color().colorIndex()==3 && layer->lineWeight()==AcDb::kLnWtByLwDefault && !layer->isOff() &&
            !layer->isFrozen() && !layer->isLocked() && layer->isPlottable();
    }
    expect("CommittedLayerUsesTargetDatabaseLinetype",layerDependencyCorrect,
        c.committed?c.after.json():issue_codes(result.issues),"new layer references this database's actual Continuous record and retains requested public definition");
    expect("SuccessfulCommitHasActualMutations",c.committed&&successEvidence,issue_codes(result.issues),"one new Line/layer; only E translated; S and both old Text remain");
    if(c.committed) {
        auto& db=*c.document->database(); auto* manager=db.transactionManager();
        require(result.value->createdEntities.size()==1,"one receipt Line required for counterexamples");
        const auto newId=bce::native_scene::objectId(db,result.value->createdEntities.front().handle);
        for(const int fault:{0,1,2,3}) {
            auto* txn=manager->startTransaction(); require(txn,"counterexample transaction");
            bool accepted=false;
            try {
                AcDbObject* raw{};
                if(fault<2) { ok(txn->getObject(raw,newId,AcDb::kForWrite),"counterexample new Line"); auto* line=AcDbLine::cast(raw); require(line,"counterexample typed Line");
                    if(fault==0) ok(line->setEndPoint(AcGePoint3d(9999.0,0.0,0.0)),"wrong emitted endpoint");
                    else ok(line->setLayer(L"0"),"wrong emitted layer"); }
                else if(fault==2) { ok(txn->getObject(raw,c.preserved,AcDb::kForWrite),"counterexample preserved Line");
                    auto* line=AcDbLine::cast(raw); require(line,"preserved typed Line"); ok(line->setLineWeight(AcDb::kLnWt050),"wrong preserved lineweight"); }
                else { Opened<AcDbText> text(c.oldText); ok(txn->getObject(raw,text->textStyle(),AcDb::kForWrite),"counterexample old style");
                    auto* style=AcDbTextStyleTableRecord::cast(raw); require(style,"typed style"); ok(style->setXScale(0.5),"wrong preserved style width"); }
                const auto altered=capture(); accepted=committedEvidenceMatches(c.baseline,altered,db,c.target,c.preserved,c.oldText,c.unsavedText,*result.value);
                ok(manager->abortTransaction(),"abort isolated audit counterexample");
            } catch(...) { if(manager->numActiveTransactions()==1&&manager->topTransaction()==txn) manager->abortTransaction(); throw; }
            const char* names[]={"AuditRejectsWrongNewLineEndpoint","AuditRejectsWrongNewLineLayer","AuditRejectsPreservedLineAttributeChange","AuditRejectsExistingStyleChange"};
            expect(names[fault],!accepted,"success verifier accepted="+std::string(accepted?"true":"false"),"actual SDK counterexample must fail the same successful-commit gate");
            require(c.after.sameContents(capture()),"audit counterexample transaction failed to restore committed scene");
        }
    }
    expect("GlobalVariablesRemainUnchanged",c.committed&&c.baseline.globalsJson==c.after.globalsJson,c.after.globalsJson,"actual successful mutation leaves every captured global unchanged");
    if(!c.committed) {
        expect("UndoRestoresSelectedAndProtectedContent",false,"NOT_RUN: commit did not occur","real single U restores baseline");
        queue(L"BCE_IP_VERIFY\n"); return;
    }
    c.undoWatcher=std::make_unique<UndoWatcher>(); c.phase=Phase::committed; checkpoint("successful-write-command-about-to-return");
    queue(L"_.U\nBCE_IP_VERIFY\n");
}); }
void verify() noexcept { boundary([] {
    require(active!=nullptr,"no active case");
    if(active->phase==Phase::committed) {
        check_state(Phase::committed); auto& c=*active; c.undoAfter=capture();
        const bool gone=c.successReceipt&&removed(*c.successReceipt,"undo");
        expect("UndoRestoresSelectedAndProtectedContent",c.undoWatcher&&c.undoWatcher->started==1&&c.undoWatcher->ended==1&&c.undoWatcher->failed==0&&
            gone&&c.baseline.sameContents(c.undoAfter),c.undoAfter.json(),"one observed native U after completed write command restores all baseline roots/resources/globals and preserves unsaved edits");
    } else check_state(Phase::checked);
    finish();
}); }
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { moduleHandle = module; DisableThreadLibraryCalls(module); } return TRUE;
}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    try {
        if (message == AcRx::kInitAppMsg) {
            acrxDynamicLinker->unlockApplication(id); acrxRegisterAppMDIAware(id);
            for (const auto& command : {std::pair<const wchar_t*, AcRxFunctionPtr>{L"BCE_TEST_RUN",run},
                {L"BCE_IP_COLD_SCOPE",coldScope}, {L"BCE_IP_SETUP",setup}, {L"BCE_IP_APPLY",apply}, {L"BCE_IP_CANCEL",cancel},
                {L"BCE_IP_CHECKS",checks}, {L"BCE_IP_SUCCESS",success}, {L"BCE_IP_VERIFY",verify}})
                if (acedRegCmds->addCommand(L"BCE_INPLACE_CASES", command.first, command.first, ACRX_CMD_MODAL, command.second) != Acad::eOk)
                    return AcRx::kRetError;
        } else if (message == AcRx::kUnloadAppMsg) {
            active.reset(); return acedRegCmds->removeGroup(L"BCE_INPLACE_CASES") == Acad::eOk ? AcRx::kRetOK : AcRx::kRetError;
        }
        return AcRx::kRetOK;
    } catch (...) { return AcRx::kRetError; }
}
