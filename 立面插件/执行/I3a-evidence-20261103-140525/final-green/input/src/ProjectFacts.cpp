#include "ProjectFacts.h"
#include <cmath>
#include <utility>

namespace bce {
namespace {
constexpr double planeTolerance = 1e-9;
bool sameHandle(const std::string& left, const std::string& right) {
    // Reuse the existing nonzero uint64 hex identity/overflow contract.
    return EntityKey{"", left, 0} == EntityKey{"", right, 0};
}
bool sameRead(const FactReadProvenance& left, const FactReadProvenance& right) {
    return !left.documentIdentity.empty() && !left.databaseIdentity.empty() && left.revision != 0 &&
        left.documentIdentity == right.documentIdentity && left.databaseIdentity == right.databaseIdentity &&
        left.revision == right.revision;
}
bool finite(FactPoint3 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}
bool samePoint(FactPoint3 left, FactPoint3 right) {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}
bool onPlane(FactPoint3 point, double z) {
    return finite(point) && std::abs(point.z - z) <= planeTolerance;
}
bool xyNormal(FactPoint3 normal) {
    return finite(normal) && normal.x == 0.0 && normal.y == 0.0 && normal.z != 0.0;
}
bool xyDirection(FactPoint3 direction) {
    return finite(direction) && direction.z == 0.0 && (direction.x != 0.0 || direction.y != 0.0);
}
FactPoint3 normalized(FactPoint3 direction) {
    // Scaling first also handles very large finite directions without overflowing hypot.
    const auto scale = std::max(std::abs(direction.x), std::abs(direction.y));
    const auto x = direction.x / scale;
    const auto y = direction.y / scale;
    const auto length = std::hypot(x, y);
    return {x / length, y / length, 0.0};
}
Issue issue(const char* code, const char* message, const std::string& role,
    const FactReadProvenance& read, const FactSource* source = nullptr) {
    IssueSource location{read.documentIdentity, role, std::nullopt};
    if (source) { location.entity = EntityKey{read.databaseIdentity, source->rootHandle, read.revision}; }
    return {code, message, std::move(location), true};
}
template<class T> Result<T> failed(Issue problem) {
    return {std::nullopt, {std::move(problem)}};
}
bool validText(const FactText& text, double z) {
    return !text.text.empty() && text.text.find('\0') == std::string::npos && !text.hasFields &&
        onPlane(text.position, z) && onPlane(text.alignmentPoint, z) && xyNormal(text.normal) &&
        std::isfinite(text.rotation) && std::isfinite(text.height) && text.height > 0.0 &&
        std::isfinite(text.widthFactor) && text.widthFactor > 0.0 && std::isfinite(text.oblique) &&
        text.horizontalMode >= 0 && text.horizontalMode <= 5 && text.verticalMode >= 0 && text.verticalMode <= 3 &&
        text.thickness == 0.0;
}
std::optional<Issue> sourceProblem(const FactSource& source, const FactReadProvenance& read,
    const std::string& owner, double z, const std::string& role, bool textRequired) {
    if (!sameRead(source.read, read)) {
        return issue("FACT_READ_INVALID", u8"依赖不是同一次文档/数据库/修订的读取。", role, read, &source);
    }
    bool definitionSupported = false;
    if (const auto* line = std::get_if<FactLine>(&source.definition)) {
        definitionSupported = !textRequired && source.exactClass == "AcDbLine" &&
            onPlane(line->start, z) && onPlane(line->end, z) && xyNormal(line->normal) && line->thickness == 0.0;
    } else if (const auto* dimension = std::get_if<FactAlignedDimension>(&source.definition)) {
        definitionSupported = !textRequired && source.exactClass == "AcDbAlignedDimension" &&
            onPlane(dimension->xLine1Point, z) && onPlane(dimension->xLine2Point, z) && xyNormal(dimension->normal);
    } else if (const auto* text = std::get_if<FactText>(&source.definition)) {
        definitionSupported = textRequired && source.exactClass == "AcDbText" && validText(*text, z);
    }
    if (!sameHandle(source.rootHandle, source.rootHandle) || !sameHandle(source.ownerHandle, owner) ||
        !source.directRoot || !source.effectiveVisible || !definitionSupported) {
        return issue("FACT_SOURCE_UNSUPPORTED", u8"依赖须为模型空间直接可见的受支持exact类、合法句柄及XY平面定义；文字不能含field。",
            role, read, &source);
    }
    return std::nullopt;
}
bool sameDefinition(const FactSource& left, const FactSource& right) {
    if (left.definition.index() != right.definition.index()) { return false; }
    if (const auto* a = std::get_if<FactLine>(&left.definition)) {
        const auto& b = std::get<FactLine>(right.definition);
        return samePoint(a->start, b.start) && samePoint(a->end, b.end) &&
            samePoint(a->normal, b.normal) && a->thickness == b.thickness;
    }
    if (const auto* a = std::get_if<FactAlignedDimension>(&left.definition)) {
        const auto& b = std::get<FactAlignedDimension>(right.definition);
        return samePoint(a->xLine1Point, b.xLine1Point) && samePoint(a->xLine2Point, b.xLine2Point) &&
            samePoint(a->normal, b.normal);
    }
    if (const auto* a = std::get_if<FactText>(&left.definition)) {
        const auto& b = std::get<FactText>(right.definition);
        return a->text == b.text && samePoint(a->position, b.position) && samePoint(a->alignmentPoint, b.alignmentPoint) &&
            samePoint(a->normal, b.normal) && a->rotation == b.rotation && a->height == b.height &&
            a->widthFactor == b.widthFactor && a->oblique == b.oblique && a->horizontalMode == b.horizontalMode &&
            a->verticalMode == b.verticalMode && a->mirroredX == b.mirroredX && a->mirroredY == b.mirroredY &&
            a->thickness == b.thickness && a->hasFields == b.hasFields;
    }
    return false;
}
struct Dependency {
    const FactSource* source;
    std::string role;
    bool textRequired;
};
std::vector<Dependency> dependencies(const ProjectFactConfirmation& input) {
    std::vector<Dependency> result{{&input.title, "title", true}, {&input.datumSource, "datum", false}};
    for (const auto& declaration : input.levelDeclarations) {
        result.push_back({&declaration.source, "level:" + declaration.role, true});
    }
    return result;
}
std::optional<FactPoint3> selectedPoint(const ProjectFactConfirmation& input) {
    if (const auto* line = std::get_if<FactLine>(&input.datumSource.definition)) {
        if (input.pointSelector == DatumPointSelector::lineStart) { return line->start; }
        if (input.pointSelector == DatumPointSelector::lineEnd) { return line->end; }
    } else if (const auto* dimension = std::get_if<FactAlignedDimension>(&input.datumSource.definition)) {
        if (input.pointSelector == DatumPointSelector::dimXLine1) { return dimension->xLine1Point; }
        if (input.pointSelector == DatumPointSelector::dimXLine2) { return dimension->xLine2Point; }
    }
    return std::nullopt;
}
std::optional<std::array<FactPoint3, 4>> windowCorners(const FactWindow& window) {
    if (!finite(window.origin) || !xyDirection(window.u) || !xyDirection(window.v) ||
        !std::isfinite(window.width) || window.width <= 0.0 || !std::isfinite(window.height) || window.height <= 0.0 ||
        std::abs(std::hypot(window.u.x, window.u.y) - 1.0) > planeTolerance ||
        std::abs(std::hypot(window.v.x, window.v.y) - 1.0) > planeTolerance ||
        std::abs(window.u.x * window.v.x + window.u.y * window.v.y) > planeTolerance) { return std::nullopt; }
    const auto corner = [&window](double u, double v) -> FactPoint3 {
        return {window.origin.x + window.u.x * u + window.v.x * v,
            window.origin.y + window.u.y * u + window.v.y * v, window.origin.z};
    };
    std::array<FactPoint3, 4> result{{window.origin, corner(window.width, 0),
        corner(window.width, window.height), corner(0, window.height)}};
    for (const auto& point : result) { if (!finite(point)) { return std::nullopt; } }
    // Reject finite dimensions that disappear through floating-point rounding at a huge origin.
    if (samePoint(result[0], result[1]) || samePoint(result[0], result[3]) ||
        samePoint(result[1], result[2]) || samePoint(result[2], result[3])) { return std::nullopt; }
    return result;
}
bool validUnits(const FactUnits& units) {
    return std::isfinite(units.mmPerInputUnit) && units.mmPerInputUnit > 0.0 && !units.confirmationSource.empty();
}
bool validHistory(const ProjectFacts& history) {
    const auto rebuilt = confirmProjectFacts(history.confirmed);
    if (history.subsetVersion != 1 || !rebuilt.value || !samePoint(history.datumPoint, rebuilt.value->datumPoint) ||
        !samePoint(history.unitHeightDirection, rebuilt.value->unitHeightDirection)) { return false; }
    for (std::size_t i = 0; i < history.windowCorners.size(); ++i) {
        if (!samePoint(history.windowCorners[i], rebuilt.value->windowCorners[i])) { return false; }
    }
    return true;
}
FactSubsetCheck checked(FactSubsetStatus status, Issue problem) {
    return {status, {std::move(problem)}};
}
} // namespace

Result<ProjectFacts> confirmProjectFacts(const ProjectFactConfirmation& input) {
    if (input.persistentProjectId.empty() || !validUnits(input.units) || !sameRead(input.read, input.read) ||
        !sameHandle(input.modelSpaceOwnerHandle, input.modelSpaceOwnerHandle) ||
        input.datumConfirmationSource.empty() || input.selectionConfirmationSource.empty() ||
        input.levelDeclarations.empty()) {
        return failed<ProjectFacts>(issue("FACT_CONFIRMATION_REQUIRED", u8"缺少项目、已确认单位/来源、批次身份、模型owner或人工确认来源。", "project", input.read));
    }
    for (const auto& level : input.levelDeclarations) {
        if (level.role.empty()) {
            return failed<ProjectFacts>(issue("FACT_CONFIRMATION_REQUIRED", u8"楼层文字须有明确关联角色。", "level", input.read, &level.source));
        }
    }
    const auto corners = windowCorners(input.window);
    if (!corners) {
        return failed<ProjectFacts>(issue("FACT_WINDOW_UNSUPPORTED", u8"窗口须为有限正尺寸的XY正交单位基矩形，且四角可表示。", "window", input.read));
    }
    if (!std::isfinite(input.datumMeters) || !xyDirection(input.heightDirection)) {
        return failed<ProjectFacts>(issue("FACT_DATUM_UNSUPPORTED", u8"基准米值须有限，高度方向须为明确非零XY向量。", "datum", input.read, &input.datumSource));
    }
    const auto required = dependencies(input);
    for (std::size_t i = 0; i < required.size(); ++i) {
        const auto& dependency = required[i];
        if (const auto problem = sourceProblem(*dependency.source, input.read, input.modelSpaceOwnerHandle,
            input.window.origin.z, dependency.role, dependency.textRequired)) { return failed<ProjectFacts>(*problem); }
        for (std::size_t j = 0; j < i; ++j) {
            if (sameHandle(dependency.source->rootHandle, required[j].source->rootHandle) &&
                (dependency.source->exactClass != required[j].source->exactClass ||
                    !sameDefinition(*dependency.source, *required[j].source))) {
                return failed<ProjectFacts>(issue("FACT_SOURCE_AMBIGUOUS", u8"同一根句柄出现矛盾定义。", dependency.role, input.read, dependency.source));
            }
        }
    }
    const auto point = selectedPoint(input);
    if (!point) {
        return failed<ProjectFacts>(issue("FACT_SELECTOR_UNSUPPORTED", u8"必须明确选择与源exact类匹配的定义点getter。", "datum", input.read, &input.datumSource));
    }
    return {ProjectFacts{1, input, *point, normalized(input.heightDirection), *corners}, {}};
}

FactSubsetCheck checkProjectFacts(const ProjectFacts& history, const std::optional<ProjectFactObservation>& current) {
    const auto& confirmed = history.confirmed;
    if (!validHistory(history)) {
        return checked(FactSubsetStatus::unsupported, issue("FACT_HISTORY_INVALID", u8"历史有限事实版本、定义或派生值无效。", "project", confirmed.read));
    }
    if (!current) {
        return checked(FactSubsetStatus::stale, issue("FACT_READ_REQUIRED", u8"历史出处不能代替当前依赖读取。", "project", confirmed.read));
    }
    const auto& now = *current;
    if (now.persistentProjectId.empty() || now.persistentProjectId != confirmed.persistentProjectId) {
        return checked(FactSubsetStatus::orphan, issue("FACT_PROJECT_ORPHAN", u8"当前项目身份缺失或不同，历史事实不能重绑。", "project", now.read));
    }
    if (!now.readCompleted) {
        return checked(FactSubsetStatus::stale, issue("FACT_READ_REQUIRED", u8"尚未完成当前依赖读取。", "project", now.read));
    }
    if (!sameRead(now.read, now.read) || !validUnits(now.units) || !sameHandle(now.modelSpaceOwnerHandle, now.modelSpaceOwnerHandle)) {
        return checked(FactSubsetStatus::unsupported, issue("FACT_READ_INVALID", u8"当前批次身份、单位或模型owner无效。", "project", now.read));
    }
    if (now.units.mmPerInputUnit != confirmed.units.mmPerInputUnit || now.units.confirmationSource != confirmed.units.confirmationSource) {
        return checked(FactSubsetStatus::stale, issue("FACT_UNITS_CHANGED", u8"已确认单位比例或来源已改变。", "units", now.read));
    }
    for (std::size_t i = 0; i < now.sources.size(); ++i) {
        const auto& source = now.sources[i];
        if (!sameRead(source.read, now.read) || !sameHandle(source.rootHandle, source.rootHandle) ||
            !sameHandle(source.ownerHandle, source.ownerHandle)) {
            return checked(FactSubsetStatus::unsupported, issue("FACT_READ_INVALID", u8"观察依赖不属于当前批次，或句柄无效。", "observation", now.read, &source));
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (sameHandle(source.rootHandle, now.sources[j].rootHandle)) {
                return checked(FactSubsetStatus::unsupported, issue("FACT_SOURCE_AMBIGUOUS", u8"当前观察重复同一根句柄，不能选取任意一项。", "observation", now.read, &source));
            }
        }
    }
    for (const auto& dependency : dependencies(confirmed)) {
        const auto& old = *dependency.source;
        const auto found = std::find_if(now.sources.begin(), now.sources.end(), [&old](const FactSource& source) {
            return sameHandle(old.rootHandle, source.rootHandle);
        });
        if (found == now.sources.end()) {
            return checked(FactSubsetStatus::stale, issue("FACT_SOURCE_MISSING", u8"本次读取未找到已选依赖。", dependency.role, now.read, &old));
        }
        if (!sameHandle(found->ownerHandle, old.ownerHandle) || !sameHandle(now.modelSpaceOwnerHandle, confirmed.modelSpaceOwnerHandle)) {
            return checked(FactSubsetStatus::stale, issue("FACT_OWNER_CHANGED", u8"已选依赖或模型空间owner改变。", dependency.role, now.read, &old));
        }
        if (const auto problem = sourceProblem(*found, now.read, now.modelSpaceOwnerHandle, confirmed.window.origin.z,
            dependency.role, dependency.textRequired)) { return checked(FactSubsetStatus::unsupported, *problem); }
        if (found->exactClass != old.exactClass || !sameDefinition(old, *found)) {
            return checked(FactSubsetStatus::stale, issue("FACT_DEFINITION_CHANGED", u8"所需有限定义值已改变，须重新确认。", dependency.role, now.read, &old));
        }
    }
    return {FactSubsetStatus::currentSubsetMatches, {}};
}

Result<double> projectFactElevationMeters(const ProjectFacts& facts, FactPoint3 point) {
    if (!validHistory(facts) || !onPlane(point, facts.datumPoint.z)) {
        return failed<double>(issue("FACT_DATUM_UNSUPPORTED", u8"有限事实或待量点无效，首域仅支持同一XY平面。", "datum", facts.confirmed.read));
    }
    const auto dx = point.x - facts.datumPoint.x;
    const auto dy = point.y - facts.datumPoint.y;
    const auto projected = dx * facts.unitHeightDirection.x + dy * facts.unitHeightDirection.y;
    const auto meters = facts.confirmed.datumMeters + projected * (facts.confirmed.units.mmPerInputUnit / 1000.0);
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(projected) || !std::isfinite(meters)) {
        return failed<double>(issue("FACT_DATUM_UNSUPPORTED", u8"实际高度换算超出有限数值范围。", "datum", facts.confirmed.read));
    }
    return {meters, {}};
}
}
