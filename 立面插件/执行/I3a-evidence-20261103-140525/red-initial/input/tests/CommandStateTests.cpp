#include "TestSupport.h"
#include "../src/GenerationState.h"

namespace bce::test {
namespace {
EntityFingerprint fp(const EntityKey& key, const std::string& geometry = "geometry-1") {
    return {key, geometry, "no-text", "resource-1", "AcDbLine"};
}
ObservedScene observed() {
    auto snapshot = scene();
    return {snapshot, {fp(snapshot.targets[0].key), fp(snapshot.preserved[0].key)}};
}
ProjectConfig config() {
    ProjectConfig c;
    c.databaseIdentity = "database-1";
    c.units = {4, true, std::nullopt, "mm"};
    c.unitConfirmationSource = "drawing-insunits";
    c.zeroLevel = {"0", "confirmed-datum-point", true};
    c.views = {{"elevation-E", "source-1", {"paint", "material-confirmation", true},
        {"A-B", "confirmed-plan", true}, Point2{1, 0}}};
    return c;
}
GenerationRequest generationRequest(const ProjectConfig& c) {
    GenerationRequest r;
    r.moduleIdentity = "BuildingElevation";
    r.moduleVersion = "core-development";
    r.viewIdentity = "elevation-E";
    r.sourceSignature = "source-1";
    r.targets = {{"database-1", "E1", 7}};
    const CommandQualification q{"document-1", "database-1", 7, r.viewIdentity,
        "source-1", generationConfigSignature(c, r.viewIdentity), "synthetic-test-facts-explicit", true};
    r.readiness = {q, q, q, q};
    return r;
}
GenerationState emptyState() { return {"document-1", "database-1", {}}; }
GenerationCommandPlan plan() {
    const auto c = config();
    const auto result = planGenerationCommand(observed(), c, generationRequest(c));
    require(result.canCommit(), "explicit complete facts should yield plan");
    return *result.value;
}
ObservedScene postScene() {
    auto o = observed();
    o.scene.revision = 8;
    for (auto* entities : {&o.scene.targets, &o.scene.preserved}) {
        for (auto& e : *entities) { e.key.revision = 8; }
    }
    for (auto& f : o.fingerprints) { f.key.revision = 8; }
    o.scene.targets.push_back({{"database-1", "F1", 8}, "shared", {{0, 0}, {10, 10}}, "elevation-E"});
    o.fingerprints.push_back(fp({"database-1", "F1", 8}, "output-1"));
    return o;
}
ValidatedGenerationEvidence evidence() {
    ValidatedGenerationEvidence e;
    e.documentIdentity = "document-1"; e.databaseIdentity = "database-1";
    e.plannedRevision = 7; e.committedRevision = 8; e.generationId = "unique-generation-1";
    e.committed = true;
    e.commitEvidence = "test-adapter-commit"; e.postCommitEvidence = "test-adapter-readback";
    e.measurementsEvidence = {true, "complete-measurements"}; e.nativeClassesEvidence = {true, "actual-classes"};
    e.resourcesEvidence = {true, "actual-resources"}; e.glyphsEvidence = {true, "actual-glyphs"};
    e.preservedEvidence = {true, "preserved-readback"};
    e.targetsAfter = {fp({"database-1", "E1", 8})};
    e.outputs = {fp({"database-1", "F1", 8}, "output-1")};
    return e;
}
GenerationState registered() {
    GeneratedViewState view;
    view.viewIdentity = "elevation-E"; view.moduleIdentity = "BuildingElevation";
    view.moduleVersion = "core-development"; view.generationId = "unique-generation-1";
    view.sourceSignature = "source-1";
    view.configSignature = generationConfigSignature(config(), "elevation-E");
    view.committedRevision = 8;
    view.sourceEntities = {fp({"database-1", "E1", 8})};
    view.outputs = {fp({"database-1", "F1", 8}, "output-1")};
    view.commitEvidence = "historical-adapter-commit"; view.postCommitEvidence = "historical-adapter-readback";
    return {"document-1", "database-1", {view}};
}
}
// Removing any missing prerequisite check would authorize an incomplete command.
void missingConfigDoesNotWriteDrawing() {
    auto o = observed();
    o.scene.units = {};
    ProjectConfig c;
    GenerationRequest r;
    const auto result = planGenerationCommand(o, c, r);
    require(!result.canCommit() && !result.value, "missing facts cannot authorize write");
    for (const auto* code : {"CONFIG_REQUIRED", "UNIT_REQUIRED", "DATUM_REQUIRED", "PLAN_REQUIRED",
        "RULES_REQUIRED", "NATIVE_PREPARATION_REQUIRED", "GLYPHS_REQUIRED", "LAYOUT_REQUIRED", "TARGET_REQUIRED"}) {
        require(hasCode(result, code), "each missing prerequisite needs explicit diagnostic");
    }
    require(o.scene.targets.size() == 1 && o.scene.revision == 7, "pure planning must retain input");
    for (const auto& i : result.issues) { require(i.source.documentIdentity == "document-1", "diagnostic locates document"); }
    o.scene.targets.clear(); o.scene.preserved.clear(); o.fingerprints.clear();
    require(hasCode(planGenerationCommand(o, c, r), "INPUT_EMPTY"), "empty input needs an explicit diagnostic");
}
void documentSwitchRejectsStaleCommit() {
    const auto p = plan();
    auto o = observed();
    o.scene.documentIdentity = "other-document";
    require(hasCode(revalidateGenerationCommand(p, o, config()), "STALE_DOCUMENT"), "document switch rejects");
    o = observed(); o.scene.documentIdentity.clear();
    require(hasCode(revalidateGenerationCommand(p, o, config()), "STALE_DOCUMENT"), "destroyed document rejects");
    o = observed(); o.scene.revision = 9;
    require(hasCode(revalidateGenerationCommand(p, o, config()), "STALE_DOCUMENT"), "revision change rejects");
    o = observed(); o.fingerprints[0].geometry = "edited-source";
    require(hasCode(revalidateGenerationCommand(p, o, config()), "SOURCE_CHANGED"), "source fingerprint change rejects");
}
void repeatedBuildDoesNotDuplicate() {
    const auto state = registered();
    const auto c = config();
    const auto decision = decideGeneration(state, postScene(), c, generationRequest(c));
    require(decision.value && decision.value->action == GenerationAction::alreadyExists, "unchanged output must not duplicate");
    require(!decision.canCommit(), "already exists does not grant new write permission");
    require(state.views.size() == 1, "decision preserves old record");
    require(hasCode(registerGeneration(state, plan(), postScene(), evidence()), "DUPLICATE_GENERATION"), "duplicate registration rejects");
    auto otherSelection = generationRequest(c); otherSelection.targets[0].handle = "E2";
    auto o = postScene();
    o.scene.targets.push_back({{"database-1", "E2", 8}, "shared", {{0, 0}, {20, 20}}, "elevation-E"});
    o.fingerprints.push_back(fp({"database-1", "E2", 8}, "other-source"));
    const auto changedTargets = decideGeneration(state, o, c, otherSelection);
    require(changedTargets.value && changedTargets.value->action == GenerationAction::reconfirmAndPrepare,
        "a different target set cannot reuse historical generation");
}
void manualChangesAreNotSilentlyReplaced() {
    const auto state = registered(); const auto c = config(); const auto r = generationRequest(c);
    for (int mutation = 0; mutation < 4; ++mutation) {
        auto o = postScene();
        if (mutation == 0) { o.fingerprints.back().geometry = "user-moved"; }
        if (mutation == 1) { o.fingerprints.back().text = "user-text"; }
        if (mutation == 2) { o.fingerprints.back().resources = "user-resource"; }
        if (mutation == 3) { o.scene.targets.pop_back(); o.fingerprints.pop_back(); }
        const auto d = decideGeneration(state, o, c, r);
        require(d.value && d.value->action == GenerationAction::manualChange && !d.canCommit(), "manual changes must block replacement");
        require(hasCode(d, "MANUAL_CHANGE"), "manual change diagnostic required");
    }
    require(state.views[0].generationId == "unique-generation-1", "manual change cannot erase history");
}
void uncommittedEvidenceCannotRegister() {
    auto e = evidence(); e.committed = false;
    require(hasCode(registerGeneration(emptyState(), plan(), postScene(), e), "COMMIT_EVIDENCE_REQUIRED"), "uncommitted cannot register");
    e = evidence(); e.cancelled = true;
    require(!registerGeneration(emptyState(), plan(), postScene(), e).canCommit(), "cancelled cannot register");
    for (int missing = 0; missing < 7; ++missing) {
        e = evidence();
        std::string* fields[] = {&e.commitEvidence, &e.postCommitEvidence, &e.measurementsEvidence.source,
            &e.nativeClassesEvidence.source, &e.resourcesEvidence.source, &e.glyphsEvidence.source, &e.preservedEvidence.source};
        fields[missing]->clear();
        require(!registerGeneration(emptyState(), plan(), postScene(), e).canCommit(), "boolean committed alone is insufficient");
    }
    for (int failed = 0; failed < 5; ++failed) {
        e = evidence();
        GenerationCheckEvidence* checks[] = {&e.measurementsEvidence, &e.nativeClassesEvidence,
            &e.resourcesEvidence, &e.glyphsEvidence, &e.preservedEvidence};
        checks[failed]->verified = false;
        require(!registerGeneration(emptyState(), plan(), postScene(), e).canCommit(), "failed post-commit check cannot register");
    }
}
void changedViewInvalidatesOnlyItsState() {
    auto state = registered(); auto c = config();
    auto second = state.views.front(); second.viewIdentity = "elevation-W"; second.generationId = "unique-W";
    second.sourceEntities[0].key.handle = "E2"; second.outputs[0].key.handle = "F2";
    state.views.push_back(second);
    auto vc = c.views.front(); vc.viewIdentity = "elevation-W"; c.views.push_back(vc);
    View east; east.identity = "elevation-E"; east.geometrySignature = "changed";
    View west; west.identity = "elevation-W"; west.geometrySignature = "source-1";
    const auto updated = invalidateChangedGenerationViews(state, c, {east, west});
    require(updated.state.views.size() == 2 && updated.state.views[0].requiresReconfirmation, "changed record retained and invalidated");
    require(!updated.state.views[1].requiresReconfirmation, "other view generation remains");
    require(!updated.config.views[0].material.confirmed && !updated.config.views[0].axisMapping.confirmed, "changed confirmation invalidated");
    require(updated.config.views[1].material.confirmed && updated.config.views[1].axisMapping.confirmed, "other confirmations retained");
    auto o = postScene(); o.fingerprints[0].geometry = "new-source";
    const auto d = decideGeneration(state, o, c, generationRequest(c));
    require(d.value && d.value->action == GenerationAction::reconfirmAndPrepare && !d.canCommit(), "source changes require fresh preparation");
}
void sameLayerDoesNotConferOwnership() {
    const auto c = config(); const auto d = decideGeneration(emptyState(), observed(), c, generationRequest(c));
    require(d.canCommit() && d.value->action == GenerationAction::generate, "shared layer without record is not ownership");
    auto o = postScene();
    o.scene.targets.push_back({{"database-1", "A2", 8}, "shared", {{5, 5}, {6, 6}}, "elevation-E"});
    o.fingerprints.push_back(fp({"database-1", "A2", 8}, "unrelated-on-same-layer"));
    const auto repeated = decideGeneration(registered(), o, c, generationRequest(c));
    require(repeated.value && repeated.value->action == GenerationAction::alreadyExists, "unrelated same-layer entity ignored");
}
void foreignOrMissingOutputCannotRegister() {
    for (int mutation = 0; mutation < 8; ++mutation) {
        auto e = evidence(); auto o = postScene();
        if (mutation == 0) { e.outputs[0].key.databaseIdentity = "foreign"; }
        if (mutation == 1) { o.scene.targets.pop_back(); o.fingerprints.pop_back(); }
        if (mutation == 2) { e.outputs[0].geometry = "wrong-readback"; }
        if (mutation == 3) { e.outputs[0].key.handle = "0"; }
        if (mutation == 4) { e.outputs[0].text.clear(); }
        if (mutation == 5) { e.outputs.push_back(e.outputs[0]); e.outputs.back().key.handle = "000f1"; }
        if (mutation == 6) { e.committedRevision = 7; }
        if (mutation == 7) { o.fingerprints[1].geometry = "damaged-preserved"; }
        require(!registerGeneration(emptyState(), plan(), o, e).canCommit(), "foreign/missing/invalid output or preservation mismatch rejects");
    }
    auto e = evidence(); e.outputs[0].key.handle = "000f1";
    require(registerGeneration(emptyState(), plan(), postScene(), e).canCommit(), "valid handle spellings share identity");
}
void staleReadinessAndChangedConfigCannotAuthorize() {
    const auto c = config(); auto r = generationRequest(c);
    r.readiness.nativePreparation.revision = 6;
    require(hasCode(planGenerationCommand(observed(), c, r), "NATIVE_PREPARATION_REQUIRED"), "stale preparation rejects");
    auto changed = c; changed.views[0].material.value = "changed-material";
    require(!revalidateGenerationCommand(plan(), observed(), changed).canCommit(), "changed config must not reuse plan");
    auto o = observed(); o.scene.targets.clear(); o.fingerprints.erase(o.fingerprints.begin());
    require(!planGenerationCommand(o, c, generationRequest(c)).canCommit(), "missing actual target rejects");
}
void registrationRequiresExactTargetAndPreservedInventory() {
    for (int mutation = 0; mutation < 6; ++mutation) {
        auto e = evidence(); auto o = postScene();
        if (mutation == 0) { e.targetsAfter.clear(); }
        if (mutation == 1) { e.targetsAfter[0].key.handle = "B1"; }
        if (mutation == 2) { e.outputs[0] = e.targetsAfter[0]; }
        if (mutation == 3) { o.scene.targets.push_back({{"database-1", "AA", 8}, "shared", {{0, 0}, {1, 1}}, "elevation-E"}); o.fingerprints.push_back(fp({"database-1", "AA", 8})); }
        if (mutation == 4) { e.documentIdentity = "wrong-document"; }
        if (mutation == 5) { e.generationId.clear(); }
        require(!registerGeneration(emptyState(), plan(), o, e).canCommit(), "registration requires exact scoped inventory and identity");
    }
}
void manualChangeTakesPriorityOverSourceInvalidation() {
    const auto state = registered(); auto c = config();
    c.views[0].geometrySignature = "changed";
    auto r = generationRequest(c); r.sourceSignature = "changed";
    auto o = postScene(); o.fingerprints[0].geometry = "changed"; o.fingerprints.back().geometry = "user-changed-output";
    const auto d = decideGeneration(state, o, c, r);
    require(d.value && d.value->action == GenerationAction::manualChange && !d.canCommit(), "source invalidation cannot hide manual edits");
}
void unrelatedRevisionKeepsHistoricalOwnershipButStalesPreparation() {
    const auto state = registered(); const auto c = config(); auto o = postScene();
    o.scene.revision = 12;
    for (auto* entities : {&o.scene.targets, &o.scene.preserved}) {
        for (auto& entity : *entities) { entity.key.revision = 12; }
    }
    for (auto& f : o.fingerprints) { f.key.revision = 12; }
    o.fingerprints[1].geometry = "unrelated-section-edited";
    const auto d = decideGeneration(state, o, c, generationRequest(c));
    require(d.value && d.value->action == GenerationAction::alreadyExists, "unrelated revision must not imply manual output edit");
    require(c.views[0].material.confirmed && c.views[0].axisMapping.confirmed, "unrelated edit retains view confirmation");
    require(hasCode(revalidateGenerationCommand(plan(), o, c), "STALE_DOCUMENT"), "old command preparation still stale");
}
void malformedOwnershipCannotAuthorizeAnotherWrite() {
    const auto c = config();
    for (int mutation = 0; mutation < 5; ++mutation) {
        auto state = registered();
        if (mutation == 0) { state.databaseIdentity = "foreign"; }
        if (mutation == 1) { state.views[0].outputs[0].key.handle = "FFFFFFFFFFFFFFFFF"; }
        if (mutation == 2) { state.views[0].outputs.push_back(state.views[0].outputs[0]); }
        if (mutation == 3) { auto other = state.views[0]; other.viewIdentity = "other-view"; state.views.push_back(other); }
        if (mutation == 4) { state.views[0].outputs[0].nativeClass.clear(); }
        require(!decideGeneration(state, postScene(), c, generationRequest(c)).canCommit(), "malformed ownership cannot authorize");
        require(!registerGeneration(state, plan(), postScene(), evidence()).canCommit(), "malformed ownership cannot be extended");
    }
}
}
