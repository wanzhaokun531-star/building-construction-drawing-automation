#include "ElevationModel.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include <limits>
#include <utility>

namespace bce {
namespace {
template<class T>
Result<T> failure(const char* code, const char* message, IssueSource source = {}) {
    return {std::nullopt, {{code, message, std::move(source), true}}};
}

std::optional<std::wstring> utf16Symbol(const std::string& text) {
    if (text.empty() || text.find('\0') != std::string::npos ||
        text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) { return std::nullopt; }
    const auto length = static_cast<int>(text.size());
    const auto required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    if (required <= 0) { return std::nullopt; }
    std::wstring decoded(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, decoded.data(), required) != required) {
        return std::nullopt;
    }
    return decoded;
}

std::optional<bool> sameResourceName(const ResourceDefinition& left, const ResourceDefinition& right) {
    const auto leftKind = utf16Symbol(left.kind);
    const auto rightKind = utf16Symbol(right.kind);
    const auto leftName = utf16Symbol(left.name);
    const auto rightName = utf16Symbol(right.name);
    if (!leftKind || !rightKind || !leftName || !rightName) { return std::nullopt; }
    const auto kind = CompareStringOrdinal(leftKind->data(), static_cast<int>(leftKind->size()),
        rightKind->data(), static_cast<int>(rightKind->size()), TRUE);
    const auto name = CompareStringOrdinal(leftName->data(), static_cast<int>(leftName->size()),
        rightName->data(), static_cast<int>(rightName->size()), TRUE);
    if (kind == 0 || name == 0) { return std::nullopt; }
    return kind == CSTR_EQUAL && name == CSTR_EQUAL;
}

std::optional<std::uint64_t> handleValue(const std::string& text) noexcept {
    if (text.empty()) { return std::nullopt; }
    std::uint64_t value = 0;
    for (const char character : text) {
        std::uint64_t digit = 0;
        if (character >= '0' && character <= '9') { digit = static_cast<std::uint64_t>(character - '0'); }
        else if (character >= 'A' && character <= 'F') { digit = static_cast<std::uint64_t>(character - 'A') + 10; }
        else if (character >= 'a' && character <= 'f') { digit = static_cast<std::uint64_t>(character - 'a') + 10; }
        else { return std::nullopt; }
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 16) { return std::nullopt; }
        value = value * 16 + digit;
    }
    if (value == 0) { return std::nullopt; }
    return value;
}

bool validSnapshotKey(const EntityKey& key, const SceneSnapshot& snapshot) noexcept {
    return handleValue(key.handle).has_value() && key.databaseIdentity == snapshot.databaseIdentity &&
        key.revision == snapshot.revision;
}

IssueSource sourceFor(const SceneSnapshot& snapshot, const WriteRequest& request,
    std::optional<EntityKey> entity = std::nullopt) {
    return {snapshot.documentIdentity, request.viewIdentity, std::move(entity)};
}
}

bool operator==(const EntityKey& left, const EntityKey& right) noexcept {
    if (left.databaseIdentity != right.databaseIdentity || left.revision != right.revision) { return false; }
    const auto leftHandle = handleValue(left.handle);
    const auto rightHandle = handleValue(right.handle);
    return leftHandle && rightHandle && *leftHandle == *rightHandle;
}

Result<UnitScale> resolveUnits(const UnitDeclaration& declaration) {
    const auto confirmed = declaration.confirmedMmPerInputUnit;
    if (confirmed && (!std::isfinite(*confirmed) || *confirmed <= 0.0)) {
        return failure<UnitScale>(diagnostic::unitRequired, u8"确认的单位比例必须是有限正数。");
    }
    std::optional<double> trusted;
    if (declaration.trustedInsunits) {
        if (declaration.insunits == 4) { trusted = 1.0; }
        else if (declaration.insunits == 6) { trusted = 1000.0; }
    }
    if (trusted && confirmed && *trusted != *confirmed) {
        return failure<UnitScale>(diagnostic::unitConflict, u8"可信单位声明与明确确认的比例冲突，请核实两者后重新提交。");
    }
    if (trusted) { return {UnitScale{*trusted}, {}}; }
    if (confirmed) { return {UnitScale{*confirmed}, {}}; }
    return failure<UnitScale>(diagnostic::unitRequired, u8"缺少可信单位，请明确确认每输入单位对应的毫米数。");
}

Result<WriteScopePlan> planWriteScope(const SceneSnapshot& snapshot, const WriteRequest& request) {
    if (snapshot.documentIdentity.empty() || snapshot.databaseIdentity.empty() ||
        request.expectedDocumentIdentity != snapshot.documentIdentity ||
        request.expectedRevision != snapshot.revision) {
        return failure<WriteScopePlan>(diagnostic::staleDocument, u8"文档身份或场景修订已变化，必须重新读取。",
            sourceFor(snapshot, request));
    }
    for (const auto* entities : {&snapshot.targets, &snapshot.preserved}) {
        for (const auto& entity : *entities) {
            if (!validSnapshotKey(entity.key, snapshot)) {
                return failure<WriteScopePlan>(diagnostic::staleDocument, u8"场景实体属于其他数据库或修订，必须重新读取。",
                    sourceFor(snapshot, request, entity.key));
            }
        }
    }
    for (const auto& target : snapshot.targets) {
        const auto overlap = std::find_if(snapshot.preserved.begin(), snapshot.preserved.end(),
            [&target](const SceneEntity& entity) { return entity.key == target.key; });
        if (overlap != snapshot.preserved.end()) {
            return failure<WriteScopePlan>(diagnostic::outsideWriteScope, u8"目标实体与保留实体清单交叠，禁止制定写入计划。",
                sourceFor(snapshot, request, target.key));
        }
    }

    const auto units = resolveUnits(snapshot.units);
    if (!units.canCommit()) {
        Result<WriteScopePlan> result{std::nullopt, units.issues};
        for (auto& issue : result.issues) { issue.source = sourceFor(snapshot, request); }
        return result;
    }
    WriteScopePlan plan{snapshot.documentIdentity, snapshot.databaseIdentity, snapshot.revision,
        *units.value, {}, {}, {}};
    for (const auto& key : request.modifications) {
        if (!validSnapshotKey(key, snapshot)) {
            return failure<WriteScopePlan>(diagnostic::staleDocument, u8"请求实体的数据库、句柄或修订无效，必须重新读取。",
                sourceFor(snapshot, request, key));
        }
        const auto selected = std::find_if(snapshot.targets.begin(), snapshot.targets.end(),
            [&key](const SceneEntity& entity) { return entity.key == key; });
        if (selected == snapshot.targets.end()) {
            return failure<WriteScopePlan>(diagnostic::outsideWriteScope, u8"请求实体不在明确目标清单中，禁止扩散写入范围。",
                sourceFor(snapshot, request, key));
        }
        if (std::find(plan.writableEntities.begin(), plan.writableEntities.end(), key) == plan.writableEntities.end()) {
            plan.writableEntities.push_back(key);
        }
    }

    // Decode explicitly as UTF-8; neither ACP nor the machine's current locale establishes identity.
    for (const auto* resources : {&snapshot.resources, &request.requiredResources}) {
        for (const auto& resource : *resources) {
            if (!utf16Symbol(resource.kind) || !utf16Symbol(resource.name)) {
                return failure<WriteScopePlan>(diagnostic::resourceConflict,
                    u8"资源类型或名称不是有效的非空UTF-8符号名，禁止提交。", sourceFor(snapshot, request));
            }
        }
    }
    // No database mutation, global replacement, prefixing or automatic renaming.
    for (const auto& required : request.requiredResources) {
        std::optional<ResourceDefinition> actualMatch;
        for (const auto& actual : snapshot.resources) {
            const auto sameName = sameResourceName(actual, required);
            if (!sameName) {
                return failure<WriteScopePlan>(diagnostic::resourceConflict,
                    u8"资源名称比较失败，禁止将其作为新资源提交。", sourceFor(snapshot, request));
            }
            if (!*sameName) { continue; }
            if (actual.definitionSignature != required.definitionSignature) {
                return failure<WriteScopePlan>(diagnostic::resourceConflict,
                    u8"同名资源定义冲突，禁止覆盖、添加前缀或自动改名。", sourceFor(snapshot, request));
            }
            if (!actualMatch) { actualMatch = actual; }
        }
        bool alreadyPlanned = false;
        for (const auto* resources : {&plan.reusedResources, &plan.newResources}) {
            for (const auto& planned : *resources) {
                const auto sameName = sameResourceName(planned, required);
                if (!sameName) {
                    return failure<WriteScopePlan>(diagnostic::resourceConflict,
                        u8"资源名称比较失败，禁止提交。", sourceFor(snapshot, request));
                }
                if (!*sameName) { continue; }
                if (planned.definitionSignature != required.definitionSignature) {
                    return failure<WriteScopePlan>(diagnostic::resourceConflict,
                        u8"本次请求包含同名不同定义的资源，禁止提交。", sourceFor(snapshot, request));
                }
                alreadyPlanned = true;
            }
        }
        if (!alreadyPlanned) {
            if (actualMatch) { plan.reusedResources.push_back(*actualMatch); }
            else { plan.newResources.push_back(required); }
        }
    }
    return {std::move(plan), {}};
}
}
