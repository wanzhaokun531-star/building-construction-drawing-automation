#include "GenerationState.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace bce {
namespace {
bool nonempty(const std::string& s) { return !s.empty() && s.find('\0') == std::string::npos; }
bool validKey(const EntityKey& key) { return nonempty(key.databaseIdentity) && key == key; }
bool sameEntity(EntityKey left, EntityKey right) {
    left.revision = 0; right.revision = 0;
    return left == right;
}
bool complete(const EntityFingerprint& f) {
    return validKey(f.key) && nonempty(f.geometry) && nonempty(f.text) &&
        nonempty(f.resources) && nonempty(f.nativeClass);
}
bool sameFingerprint(const EntityFingerprint& a, const EntityFingerprint& b) {
    return sameEntity(a.key, b.key) && a.geometry == b.geometry && a.text == b.text &&
        a.resources == b.resources && a.nativeClass == b.nativeClass;
}
const EntityFingerprint* fingerprint(const ObservedScene& o, const EntityKey& key) {
    const auto i = std::find_if(o.fingerprints.begin(), o.fingerprints.end(),
        [&key](const EntityFingerprint& f) { return sameEntity(f.key, key); });
    return i == o.fingerprints.end() ? nullptr : &*i;
}
const SceneEntity* target(const ObservedScene& o, const EntityKey& key) {
    const auto i = std::find_if(o.scene.targets.begin(), o.scene.targets.end(),
        [&key](const SceneEntity& e) { return sameEntity(e.key, key); });
    return i == o.scene.targets.end() ? nullptr : &*i;
}
Issue issue(const ObservedScene& o, const std::string& view, const char* code,
    const char* message, std::optional<EntityKey> entity = std::nullopt) {
    return {code, message, {o.scene.documentIdentity, view, std::move(entity)}, true};
}
template<class T>
Result<T> failed(const ObservedScene& o, const std::string& view, const char* code, const char* message) {
    return {std::nullopt, {issue(o, view, code, message)}};
}
bool validObservation(const ObservedScene& o) {
    const auto& s = o.scene;
    if (!nonempty(s.documentIdentity) || !nonempty(s.databaseIdentity)) { return false; }
    std::vector<EntityKey> seen;
    for (const auto* entities : {&s.targets, &s.preserved}) {
        for (const auto& e : *entities) {
            if (!validKey(e.key) || e.key.databaseIdentity != s.databaseIdentity || e.key.revision != s.revision ||
                !std::isfinite(e.bounds.minimum.x) || !std::isfinite(e.bounds.minimum.y) ||
                !std::isfinite(e.bounds.maximum.x) || !std::isfinite(e.bounds.maximum.y) ||
                e.bounds.minimum.x > e.bounds.maximum.x || e.bounds.minimum.y > e.bounds.maximum.y ||
                std::any_of(seen.begin(), seen.end(), [&e](const EntityKey& k) { return sameEntity(k, e.key); })) { return false; }
            seen.push_back(e.key);
            const auto f = fingerprint(o, e.key);
            if (!f || !complete(*f) || !(f->key == e.key)) { return false; }
        }
    }
    if (seen.size() != o.fingerprints.size()) { return false; }
    for (std::size_t i = 0; i < o.fingerprints.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (sameEntity(o.fingerprints[i].key, o.fingerprints[j].key)) { return false; }
        }
    }
    return true;
}
bool validState(const GenerationState& state, const ObservedScene& o) {
    if (state.documentIdentity != o.scene.documentIdentity || state.databaseIdentity != o.scene.databaseIdentity) { return false; }
    std::vector<EntityKey> owned;
    for (std::size_t i = 0; i < state.views.size(); ++i) {
        const auto& v = state.views[i];
        if (!nonempty(v.viewIdentity) || !nonempty(v.moduleIdentity) || !nonempty(v.moduleVersion) ||
            !nonempty(v.generationId) || !nonempty(v.sourceSignature) || !nonempty(v.configSignature) ||
            !nonempty(v.commitEvidence) || !nonempty(v.postCommitEvidence) || v.outputs.empty() || v.sourceEntities.empty()) { return false; }
        for (std::size_t j = 0; j < i; ++j) {
            if (v.viewIdentity == state.views[j].viewIdentity || v.generationId == state.views[j].generationId) { return false; }
        }
        for (const auto& f : v.sourceEntities) {
            if (!complete(f) || f.key.databaseIdentity != state.databaseIdentity) { return false; }
        }
        for (const auto& f : v.outputs) {
            if (!complete(f) || f.key.databaseIdentity != state.databaseIdentity ||
                std::any_of(owned.begin(), owned.end(), [&f](const EntityKey& k) { return sameEntity(k, f.key); }) ||
                std::any_of(v.sourceEntities.begin(), v.sourceEntities.end(), [&f](const EntityFingerprint& s) { return sameEntity(s.key, f.key); })) { return false; }
            owned.push_back(f.key);
        }
    }
    return true;
}
bool qualified(const CommandQualification& q, const GenerationCommandPlan& p) {
    return q.verified && nonempty(q.evidenceSource) && q.documentIdentity == p.documentIdentity &&
        q.databaseIdentity == p.databaseIdentity && q.revision == p.revision && q.viewIdentity == p.viewIdentity &&
        q.sourceSignature == p.sourceSignature && q.configSignature == p.configSignature;
}
bool ready(const GenerationCommandPlan& p) {
    return qualified(p.readiness.rules, p) && qualified(p.readiness.nativePreparation, p) &&
        qualified(p.readiness.glyphs, p) && qualified(p.readiness.layout, p);
}
void append(std::ostringstream& out, const std::string& text) { out << text.size() << ':' << text << ';'; }
void append(std::ostringstream& out, const Confirmation& c) {
    append(out, c.value); append(out, c.source); out << c.confirmed << ';';
}
bool confirmed(const Confirmation& c) { return c.confirmed && nonempty(c.value) && nonempty(c.source); }
bool verified(const GenerationCheckEvidence& e) { return e.verified && nonempty(e.source); }
bool matchesObserved(const EntityFingerprint& expected, const ObservedScene& o) {
    const auto actual = fingerprint(o, expected.key);
    return actual && sameFingerprint(expected, *actual);
}
}

std::string generationConfigSignature(const ProjectConfig& c, const std::string& viewIdentity) {
    const auto v = std::find_if(c.views.begin(), c.views.end(), [&viewIdentity](const ViewConfig& view) { return view.viewIdentity == viewIdentity; });
    if (v == c.views.end() || std::count_if(c.views.begin(), c.views.end(), [&viewIdentity](const ViewConfig& view) { return view.viewIdentity == viewIdentity; }) != 1) { return {}; }
    std::ostringstream out;
    out.imbue(std::locale::classic()); out << std::setprecision(17);
    append(out, c.databaseIdentity); append(out, c.unitConfirmationSource); append(out, c.zeroLevel);
    out << c.units.insunits << ';' << c.units.trustedInsunits << ';' << c.units.confirmedMmPerInputUnit.has_value() << ';';
    if (c.units.confirmedMmPerInputUnit) { out << *c.units.confirmedMmPerInputUnit << ';'; }
    append(out, c.units.labelHint); append(out, v->viewIdentity); append(out, v->geometrySignature);
    append(out, v->material); append(out, v->axisMapping);
    out << v->axisDirection.has_value() << ';';
    if (v->axisDirection) { out << v->axisDirection->x << ';' << v->axisDirection->y << ';'; }
    return out.str();
}

Result<GenerationCommandPlan> planGenerationCommand(const ObservedScene& o, const ProjectConfig& c, const GenerationRequest& r) {
    GenerationCommandPlan p;
    p.documentIdentity = o.scene.documentIdentity; p.databaseIdentity = o.scene.databaseIdentity; p.revision = o.scene.revision;
    p.moduleIdentity = r.moduleIdentity; p.moduleVersion = r.moduleVersion; p.viewIdentity = r.viewIdentity;
    p.sourceSignature = r.sourceSignature; p.configSignature = generationConfigSignature(c, r.viewIdentity); p.readiness = r.readiness;
    Result<GenerationCommandPlan> result;
    const auto add = [&](const char* code, const char* message, std::optional<EntityKey> key = std::nullopt) {
        result.issues.push_back(issue(o, r.viewIdentity, code, message, std::move(key)));
    };
    if (!validObservation(o)) { add("INVALID_SNAPSHOT", u8"场景身份、句柄、实体清单或完整指纹无效，请重新采集。"); }
    if (o.scene.targets.empty()) { add("INPUT_EMPTY", u8"没有可分析的目标立面，禁止写图。"); }
    const auto view = std::find_if(c.views.begin(), c.views.end(), [&r](const ViewConfig& v) { return v.viewIdentity == r.viewIdentity; });
    if (c.databaseIdentity != o.scene.databaseIdentity || !nonempty(p.configSignature) ||
        !nonempty(r.moduleIdentity) || !nonempty(r.moduleVersion) || !nonempty(r.viewIdentity) ||
        view == c.views.end() || !confirmed(view->material)) { add("CONFIG_REQUIRED", u8"缺少当前库、立面、模块版本或已确认材料配置。"); }
    const auto sceneUnits = resolveUnits(o.scene.units); const auto configUnits = resolveUnits(c.units);
    if (!sceneUnits.canCommit() || !configUnits.canCommit() || !nonempty(c.unitConfirmationSource)) {
        add("UNIT_REQUIRED", u8"场景与项目均须有已核实单位及确认来源。");
    } else if (sceneUnits.value->mmPerInputUnit != configUnits.value->mmPerInputUnit) {
        add("UNIT_CONFLICT", u8"项目和当前图单位不一致，禁止复用准备结果。");
    }
    if (!confirmed(c.zeroLevel)) { add("DATUM_REQUIRED", u8"缺少已确认的实际楼层零点及核准来源。"); }
    if (view == c.views.end() || !confirmed(view->axisMapping) || !view->axisDirection ||
        !std::isfinite(view->axisDirection->x) || !std::isfinite(view->axisDirection->y) ||
        (view->axisDirection->x == 0 && view->axisDirection->y == 0)) { add("PLAN_REQUIRED", u8"缺少配套平面轴关联、方向或确认来源。"); }
    if (!nonempty(r.sourceSignature) || view == c.views.end() || view->geometrySignature != r.sourceSignature) {
        add("SOURCE_CHANGED", u8"源几何签名缺失或与已确认配置不符，须重新确认。");
    }
    if (!qualified(r.readiness.rules, p)) { add("RULES_REQUIRED", u8"缺少绑定当前文档、修订、源及配置的完整规则资格。"); }
    if (!qualified(r.readiness.nativePreparation, p)) { add("NATIVE_PREPARATION_REQUIRED", u8"缺少本轮真实原生准备资格。"); }
    if (!qualified(r.readiness.glyphs, p)) { add("GLYPHS_REQUIRED", u8"缺少本轮完整真实字形资格。"); }
    if (!qualified(r.readiness.layout, p)) { add("LAYOUT_REQUIRED", u8"缺少本轮完整排版资格。"); }
    if (r.targets.empty()) { add("TARGET_REQUIRED", u8"没有明确选择目标实体，禁止写图。"); }
    for (const auto& key : r.targets) {
        const auto actual = target(o, key); const auto f = fingerprint(o, key);
        if (!validKey(key) || key.databaseIdentity != o.scene.databaseIdentity || key.revision != o.scene.revision ||
            !actual || !(actual->key == key) || actual->viewIdentity != r.viewIdentity || !f ||
            std::any_of(p.targets.begin(), p.targets.end(), [&key](const EntityFingerprint& existing) { return sameEntity(key, existing.key); })) {
            add("OUTSIDE_WRITE_SCOPE", u8"目标句柄无效、重复、已过期或不属于选定立面。", key);
        } else { p.targets.push_back(*f); }
    }
    for (const auto& f : o.fingerprints) {
        if (std::none_of(p.targets.begin(), p.targets.end(), [&f](const EntityFingerprint& selected) { return sameEntity(f.key, selected.key); })) { p.preserved.push_back(f); }
    }
    if (result.issues.empty()) { result.value = std::move(p); }
    return result;
}

Result<GenerationCommandPlan> revalidateGenerationCommand(const GenerationCommandPlan& p, const ObservedScene& o, const ProjectConfig& c) {
    if (p.documentIdentity != o.scene.documentIdentity || p.databaseIdentity != o.scene.databaseIdentity ||
        p.revision != o.scene.revision || !nonempty(o.scene.documentIdentity)) {
        return failed<GenerationCommandPlan>(o, p.viewIdentity, "STALE_DOCUMENT", u8"活动文档已切换、销毁或修订已变化，请重新分析及准备。");
    }
    if (p.configSignature != generationConfigSignature(c, p.viewIdentity)) {
        return failed<GenerationCommandPlan>(o, p.viewIdentity, "CONFIG_CHANGED", u8"立面配置已变化，旧计划不可提交。");
    }
    for (const auto& f : p.targets) {
        if (!matchesObserved(f, o)) { return failed<GenerationCommandPlan>(o, p.viewIdentity, "SOURCE_CHANGED", u8"目标源实体已变化，旧计划不可提交。"); }
    }
    for (const auto& f : p.preserved) {
        if (!matchesObserved(f, o)) { return failed<GenerationCommandPlan>(o, p.viewIdentity, "PRESERVED_CHANGED", u8"保留实体已变化，须重新读取范围。"); }
    }
    GenerationRequest r{p.moduleIdentity, p.moduleVersion, p.viewIdentity, p.sourceSignature, {}, p.readiness};
    for (const auto& f : p.targets) { r.targets.push_back(f.key); }
    auto result = planGenerationCommand(o, c, r);
    if (result.value && result.value->preserved.size() != p.preserved.size()) {
        return failed<GenerationCommandPlan>(o, p.viewIdentity, "SOURCE_CHANGED", u8"场景实体清单变化，须重新制定计划。");
    }
    return result;
}

Result<GenerationDecision> decideGeneration(const GenerationState& state, const ObservedScene& o, const ProjectConfig& c, const GenerationRequest& r) {
    if (!validObservation(o) || !validState(state, o)) { return failed<GenerationDecision>(o, r.viewIdentity, "STATE_INVALID", u8"归属记录或当前完整观测无效，不得据此写图。"); }
    const auto old = std::find_if(state.views.begin(), state.views.end(), [&r](const GeneratedViewState& v) { return v.viewIdentity == r.viewIdentity; });
    if (old != state.views.end()) {
        for (const auto& f : old->outputs) {
            if (!matchesObserved(f, o)) { return {GenerationDecision{GenerationAction::manualChange}, {issue(o, r.viewIdentity, "MANUAL_CHANGE", u8"既有输出已被编辑、移动、删除或同句柄重用；保留记录并停止覆盖。", f.key)}}; }
        }
        const bool sourceChanged = std::any_of(old->sourceEntities.begin(), old->sourceEntities.end(), [&o](const EntityFingerprint& f) { return !matchesObserved(f, o); });
        const bool targetSetChanged = r.targets.size() != old->sourceEntities.size() ||
            std::any_of(old->sourceEntities.begin(), old->sourceEntities.end(), [&r](const EntityFingerprint& f) {
                return std::count_if(r.targets.begin(), r.targets.end(), [&f](const EntityKey& k) { return sameEntity(k, f.key); }) != 1;
            });
        if (old->requiresReconfirmation || sourceChanged || targetSetChanged || old->sourceSignature != r.sourceSignature ||
            old->configSignature != generationConfigSignature(c, r.viewIdentity) || old->moduleIdentity != r.moduleIdentity || old->moduleVersion != r.moduleVersion) {
            return {GenerationDecision{GenerationAction::reconfirmAndPrepare}, {issue(o, r.viewIdentity, "RECONFIRM_AND_PREPARE", u8"源、配置或版本已变化，须重新确认及准备；不自动擦除旧输出。")}};
        }
        return {GenerationDecision{GenerationAction::alreadyExists}, {issue(o, r.viewIdentity, "ALREADY_EXISTS", u8"本视图的源、配置及实际输出未变化，无需再生成。")}};
    }
    const auto planned = planGenerationCommand(o, c, r);
    if (!planned.canCommit()) { return {std::nullopt, planned.issues}; }
    return {GenerationDecision{GenerationAction::generate}, {}};
}

Result<GenerationState> registerGeneration(const GenerationState& state, const GenerationCommandPlan& p, const ObservedScene& o, const ValidatedGenerationEvidence& e) {
    const auto reject = [&](const char* code, const char* message) { return failed<GenerationState>(o, p.viewIdentity, code, message); };
    if (!validObservation(o) || !validState(state, o)) { return reject("STATE_INVALID", u8"归属或提交后观测无效，禁止登记成功。"); }
    if (!e.committed || e.cancelled || !nonempty(e.commitEvidence) || !nonempty(e.postCommitEvidence) ||
        !verified(e.measurementsEvidence) || !verified(e.nativeClassesEvidence) || !verified(e.resourcesEvidence) ||
        !verified(e.glyphsEvidence) || !verified(e.preservedEvidence)) {
        return reject("COMMIT_EVIDENCE_REQUIRED", u8"缺少实际提交与提交后全项核准状态/来源，禁止登记成功。");
    }
    if (e.documentIdentity != p.documentIdentity || e.databaseIdentity != p.databaseIdentity ||
        o.scene.documentIdentity != p.documentIdentity || o.scene.databaseIdentity != p.databaseIdentity ||
        e.plannedRevision != p.revision || e.committedRevision != o.scene.revision || e.committedRevision <= p.revision) {
        return reject("STALE_DOCUMENT", u8"提交证据与计划、文档、库或新修订不一致。");
    }
    if (!nonempty(e.generationId) || !nonempty(p.viewIdentity) || !nonempty(p.moduleIdentity) || !nonempty(p.moduleVersion) ||
        !nonempty(p.sourceSignature) || !nonempty(p.configSignature) || p.targets.empty() || !ready(p)) {
        return reject("COMMIT_EVIDENCE_REQUIRED", u8"缺少生成身份、完整计划或本轮准备资格。");
    }
    if (std::any_of(state.views.begin(), state.views.end(), [&e, &p](const GeneratedViewState& v) { return v.generationId == e.generationId || v.viewIdentity == p.viewIdentity; })) {
        return reject("DUPLICATE_GENERATION", u8"生成身份或视图已有记录；不得重复登记或覆盖历史记录。");
    }
    std::vector<EntityKey> expected;
    for (const auto* group : {&p.targets, &p.preserved}) {
        for (const auto& f : *group) {
            if (!complete(f) || f.key.databaseIdentity != p.databaseIdentity || f.key.revision != p.revision ||
                std::any_of(expected.begin(), expected.end(), [&f](const EntityKey& k) { return sameEntity(k, f.key); })) {
                return reject("INVALID_PLAN", u8"原计划实体清单不完整、交叠或句柄无效。");
            }
            expected.push_back(f.key);
        }
    }
    if (e.targetsAfter.size() != p.targets.size() || e.outputs.empty() ||
        expected.size() + e.outputs.size() != o.fingerprints.size()) { return reject("OUTPUT_MISMATCH", u8"提交后目标、输出或保留集合数量与计划不符。"); }
    std::vector<EntityKey> seen;
    for (const auto& f : e.targetsAfter) {
        const auto t = target(o, f.key);
        if (!complete(f) || f.key.revision != o.scene.revision || !matchesObserved(f, o) || !t || t->viewIdentity != p.viewIdentity ||
            std::none_of(p.targets.begin(), p.targets.end(), [&f](const EntityFingerprint& original) { return sameEntity(original.key, f.key); }) ||
            std::any_of(seen.begin(), seen.end(), [&f](const EntityKey& k) { return sameEntity(k, f.key); })) {
            return reject("TARGET_MISMATCH", u8"提交后目标不属于原选择范围或完整指纹不匹配。");
        }
        seen.push_back(f.key);
    }
    for (const auto& f : p.preserved) {
        if (!matchesObserved(f, o)) { return reject("PRESERVED_CHANGED", u8"提交后保留实体缺失或改变，不能登记成功。"); }
    }
    for (const auto& f : e.outputs) {
        const auto t = target(o, f.key);
        if (!complete(f) || f.key.databaseIdentity != p.databaseIdentity || f.key.revision != o.scene.revision ||
            !matchesObserved(f, o) || !t || t->viewIdentity != p.viewIdentity ||
            std::any_of(expected.begin(), expected.end(), [&f](const EntityKey& k) { return sameEntity(k, f.key); }) ||
            std::any_of(seen.begin(), seen.end(), [&f](const EntityKey& k) { return sameEntity(k, f.key); })) {
            return reject("OUTPUT_MISMATCH", u8"输出句柄无效、重复、跨库、缺失、重用源对象或指纹不符。");
        }
        for (const auto& v : state.views) {
            if (std::any_of(v.outputs.begin(), v.outputs.end(), [&f](const EntityFingerprint& old) { return sameEntity(old.key, f.key); })) { return reject("DUPLICATE_GENERATION", u8"输出已属于其他生成记录。"); }
        }
        seen.push_back(f.key);
    }
    GenerationState updated = state;
    updated.views.push_back({p.viewIdentity, p.moduleIdentity, p.moduleVersion, e.generationId,
        p.sourceSignature, p.configSignature, e.committedRevision, e.targetsAfter, e.outputs, false,
        e.commitEvidence, e.postCommitEvidence});
    return {std::move(updated), {}};
}

InvalidatedGenerationState invalidateChangedGenerationViews(const GenerationState& state, const ProjectConfig& config, const std::vector<View>& currentViews) {
    InvalidatedGenerationState result{config, state};
    const auto unchanged = [&currentViews](const std::string& id, const std::string& signature) {
        const auto count = std::count_if(currentViews.begin(), currentViews.end(), [&id](const View& v) { return v.identity == id; });
        return count == 1 && std::any_of(currentViews.begin(), currentViews.end(), [&id, &signature](const View& v) { return v.identity == id && nonempty(v.geometrySignature) && v.geometrySignature == signature; });
    };
    for (auto& v : result.config.views) {
        if (!unchanged(v.viewIdentity, v.geometrySignature)) { v.material.confirmed = false; v.axisMapping.confirmed = false; }
    }
    for (auto& v : result.state.views) {
        if (!unchanged(v.viewIdentity, v.sourceSignature)) { v.requiresReconfirmation = true; }
    }
    return result;
}
}
