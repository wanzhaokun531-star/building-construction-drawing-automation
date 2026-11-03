#include "ProjectFactsReader.h"
#include <acdocman.h>
#include <dbapserv.h>
#include <dbdim.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <set>
#include <stdexcept>
#include <utility>

namespace bce {
namespace {
void check(Acad::ErrorStatus status, const char* operation) {
    if (status != Acad::eOk) {
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(status));
    }
}
// A successful public kForRead open is owned by this scope; SDK-owned objects
// are always closed, never deleted. No object pointer leaves the reader.
template<class T> class Read final {
public:
    explicit Read(AcDbObjectId id) {
        check(acdbOpenObject(value_, id, AcDb::kForRead), "read-open fact source");
    }
    ~Read() noexcept { if (value_) value_->close(); }
    Read(const Read&) = delete;
    Read& operator=(const Read&) = delete;
    T* get() const noexcept { return value_; }
    T* operator->() const noexcept { return value_; }
private:
    T* value_{};
};
FactReadProvenance provenance(const NativeSceneContext& context) {
    const auto identity = context.documentIdentity();
    return {identity, identity + "/database", context.revision()};
}
bool sameRead(const FactReadProvenance& actual, const FactReadProvenance& expected) {
    return !actual.documentIdentity.empty() && !actual.databaseIdentity.empty() && actual.revision != 0 &&
        actual.documentIdentity == expected.documentIdentity && actual.databaseIdentity == expected.databaseIdentity &&
        actual.revision == expected.revision;
}
bool currentMatches(const ProjectFactsReadRequest& request, AcDbDatabase* database,
                    const AcApDocument* document) {
    const auto* current = acDocManager->curDocument();
    const auto* active = acDocManager->mdiActiveDocument();
    return database && document && current == document && active && current->database() == database &&
        active->database() == database && request.context.currentDatabase() == database &&
        acdbHostApplicationServices()->workingDatabase() == database &&
        sameRead(provenance(request.context), request.expected);
}
Issue problem(const char* code, const std::string& message, const FactReadProvenance& read,
              const std::string& handle = {}) {
    IssueSource location{read.documentIdentity, {}, std::nullopt};
    if (!handle.empty()) location.entity = EntityKey{read.databaseIdentity, handle, read.revision};
    return {code, message, std::move(location), true};
}
Result<ProjectFactSourceRead> stale(const FactReadProvenance& read) {
    return {std::nullopt, {problem("FACT_STALE_CONTEXT", u8"当前/活动/工作库、上下文或预期身份修订不一致，须重新选择并读取。", read)}};
}
std::optional<std::string> canonicalHandle(const std::string& input) {
    // Use the established core contract, including uint64 overflow, nonzero and
    // arbitrarily many leading zeros. Never turn an untrusted integer into a raw ID.
    const EntityKey key{"", input, 0};
    if (!(key == key)) return std::nullopt;
    std::string result = input.substr(input.find_first_not_of('0'));
    for (auto& c : result) if (c >= 'a' && c <= 'f') c = static_cast<char>(c - 'a' + 'A');
    return result;
}
FactPoint3 point(const AcGePoint3d& value) { return {value.x, value.y, value.z}; }
FactPoint3 vector(const AcGeVector3d& value) { return {value.x, value.y, value.z}; }
FactText textDefinition(const AcDbText& text) {
    AcString raw;
    check(text.textString(raw), "raw textString");
    FactText result;
    result.text = native_scene::utf8(std::wstring(raw.kACharPtr(), static_cast<std::size_t>(raw.length())));
    result.position = point(text.position());
    result.alignmentPoint = point(text.alignmentPoint());
    result.normal = vector(text.normal());
    result.rotation = text.rotation();
    result.height = text.height();
    result.widthFactor = text.widthFactor();
    result.oblique = text.oblique();
    result.horizontalMode = static_cast<int>(text.horizontalMode());
    result.verticalMode = static_cast<int>(text.verticalMode());
    result.mirroredX = text.isMirroredInX() != Adesk::kFalse;
    result.mirroredY = text.isMirroredInY() != Adesk::kFalse;
    result.thickness = text.thickness();
    result.hasFields = text.hasFields(); // Presence only; no field lookup/evaluation.
    return result;
}
FactSource readSource(AcDbDatabase& database, AcDbObjectId id, AcDbObjectId model,
                      const FactReadProvenance& read, std::vector<Issue>& issues) {
    Read<AcDbObject> object(id);
    if (object->database() != &database || object->objectId() != id || !object->isA()) {
        throw std::runtime_error("read-open object identity/database unavailable");
    }
    const auto owner = object->ownerId();
    if (owner.isNull() || owner.database() != &database) {
        throw std::runtime_error("fact source owner unavailable or belongs to another database");
    }
    FactSource result;
    result.rootHandle = native_scene::handle(id);
    result.ownerHandle = native_scene::handle(owner);
    result.exactClass = native_scene::utf8(object->isA()->name());
    result.read = read;
    const auto* entity = AcDbEntity::cast(object.get());
    result.directRoot = entity && owner == model;
    if (!result.directRoot) {
        issues.push_back(problem("FACT_SOURCE_NON_DIRECT", u8"源不是模型空间直接实体；保留真实类与owner，不展开块或替换源。", read, result.rootHandle));
        return result;
    }
    // Direct model-space visibility only: entity visibility plus its own layer
    // on/unfrozen. This makes no viewport/clipping/drawability claim.
    const auto layerId = entity->layerId();
    if (layerId.isNull() || layerId.database() != &database) throw std::runtime_error("source layer database unavailable");
    {
        Read<AcDbLayerTableRecord> layer(layerId);
        if (layer->database() != &database) throw std::runtime_error("read-open layer database mismatch");
        result.effectiveVisible = entity->visibility() == AcDb::kVisible && !layer->isOff() && !layer->isFrozen();
    }
    // Check exact runtime identity before any typed cast. Derived TCH/Text,
    // attributes, MText and block references cannot enter the typed subset.
    if (object->isA() == AcDbLine::desc()) {
        const auto* line = AcDbLine::cast(object.get());
        result.definition = FactLine{point(line->startPoint()), point(line->endPoint()), vector(line->normal()), line->thickness()};
    } else if (object->isA() == AcDbAlignedDimension::desc()) {
        const auto* dim = AcDbAlignedDimension::cast(object.get());
        result.definition = FactAlignedDimension{point(dim->xLine1Point()), point(dim->xLine2Point()), vector(dim->normal())};
    } else if (object->isA() == AcDbText::desc()) {
        auto text = textDefinition(*AcDbText::cast(object.get()));
        if (text.hasFields) issues.push_back(problem("FACT_SOURCE_FIELD", u8"文字含field；仅保留原始文字和存在标志，不求值，由核心保持unsupported。", read, result.rootHandle));
        result.definition = std::move(text);
    } else {
        issues.push_back(problem("FACT_SOURCE_UNSUPPORTED", u8"仅支持exact AcDbLine/AcDbAlignedDimension/AcDbText；保留真实类，不伪装为其它源。", read, result.rootHandle));
    }
    return result;
}
} // namespace

Result<ProjectFactSourceRead> readCurrentProjectFacts(const ProjectFactsReadRequest& request) {
    auto* database = request.context.currentDatabase();
    const auto* document = acDocManager->curDocument();
    std::vector<Issue> issues;
    try {
        if (!currentMatches(request, database, document)) return stale(request.expected);
        const auto read = provenance(request.context); // Actual current read, never an updated expected selection.
        ProjectFactSourceRead result;
        result.read = read;
        AcDbObjectId model;
        {
            Read<AcDbBlockTable> blocks(database->blockTableId());
            check(blocks->getAt(ACDB_MODEL_SPACE, model), "resolve model-space owner");
            Read<AcDbBlockTableRecord> space(model);
            if (space->database() != database || space->ownerId() != database->blockTableId()) {
                throw std::runtime_error("model-space owner identity/database mismatch");
            }
            result.modelSpaceOwnerHandle = native_scene::handle(model);
        }
        std::set<std::string> seen;
        bool unavailable = false;
        for (const auto& requested : request.sourceHandles) {
            const auto handle = canonicalHandle(requested);
            if (!handle) {
                // Malformed strings have no EntityKey. Keep the request in the
                // message rather than inventing an object identity from raw bits.
                issues.push_back(problem("FACT_SOURCE_UNAVAILABLE", u8"无效源句柄，无法证明对象缺失：" + requested, read));
                unavailable = true;
                continue;
            }
            if (!seen.insert(*handle).second) continue;
            try {
                AcDbObjectId id;
                const AcDbHandle nativeHandle(native_scene::utf16(*handle).c_str());
                // false is essential: unknown handles must not create SDK ID stubs.
                const auto status = database->getAcDbObjectId(id, false, nativeHandle);
                if (status == Acad::eUnknownHandle) {
                    issues.push_back(problem("FACT_SOURCE_MISSING", u8"当前库中不存在此有效句柄。", read, *handle));
                    continue;
                }
                check(status, "resolve explicit source handle");
                if (id.isNull() || id.database() != database || native_scene::handle(id) != *handle) {
                    throw std::runtime_error("resolved source identity/database mismatch");
                }
                if (id.isErased() || id.isEffectivelyErased()) {
                    issues.push_back(problem("FACT_SOURCE_MISSING", u8"当前库中此对象已删除。", read, *handle));
                    continue;
                }
                result.sources.push_back(readSource(*database, id, model, read, issues));
            } catch (const std::exception& failure) {
                issues.push_back(problem("FACT_SOURCE_UNAVAILABLE", failure.what(), read, *handle));
                unavailable = true;
            }
        }
        // Includes closure of every successful read-open. No partial/newer batch
        // may escape after edits, context invalidation or document/library changes.
        if (!currentMatches(request, database, document)) return stale(read);
        if (unavailable) return {std::nullopt, std::move(issues)};
        result.readCompleted = true;
        return {std::move(result), std::move(issues)};
    } catch (const std::exception& failure) {
        if (!currentMatches(request, database, document)) return stale(request.expected);
        issues.push_back(problem("FACT_SOURCE_UNAVAILABLE", failure.what(), request.expected));
        return {std::nullopt, std::move(issues)};
    }
}
} // namespace bce
