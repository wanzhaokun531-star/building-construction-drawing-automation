#ifndef BCE_ELEVATION_MODEL_H
#define BCE_ELEVATION_MODEL_H

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bce {

namespace diagnostic {
inline constexpr char unitRequired[] = "UNIT_REQUIRED";
inline constexpr char unitConflict[] = "UNIT_CONFLICT";
inline constexpr char ambiguousFeature[] = "AMBIGUOUS_FEATURE";
inline constexpr char resourceConflict[] = "RESOURCE_CONFLICT";
inline constexpr char staleDocument[] = "STALE_DOCUMENT";
inline constexpr char outsideWriteScope[] = "OUTSIDE_WRITE_SCOPE";
inline constexpr char noLayoutSpace[] = "NO_LAYOUT_SPACE";
inline constexpr char unsupportedNativeWrite[] = "UNSUPPORTED_NATIVE_WRITE";
}

struct Point2 { double x{0.0}; double y{0.0}; };
struct Bounds2 { Point2 minimum; Point2 maximum; };

// revision is the snapshot revision, not an SDK object address or a live handle.
// handle is a nonzero unsigned 64-bit hexadecimal integer, without a prefix/sign/space.
// Case and leading zeros do not change identity. Malformed/overflowing keys cannot authorize writes.
struct EntityKey {
    std::string databaseIdentity;
    std::string handle;
    std::uint64_t revision{0};
};
bool operator==(const EntityKey& left, const EntityKey& right) noexcept;

struct IssueSource {
    std::string documentIdentity;
    std::string viewIdentity;
    std::optional<EntityKey> entity;
};
struct Issue {
    std::string code;
    std::string message; // UTF-8 Chinese, independent of host code page.
    IssueSource source;
    bool blocking{true};
};

template<class T>
struct Result {
    std::optional<T> value;
    std::vector<Issue> issues;
    [[nodiscard]] bool canCommit() const noexcept {
        return value.has_value() && std::none_of(issues.begin(), issues.end(),
            [](const Issue& issue) { return issue.blocking; });
    }
};

struct UnitDeclaration {
    int insunits{0}; // AutoCAD: 0 unspecified, 4 millimeters, 6 meters.
    bool trustedInsunits{false};
    std::optional<double> confirmedMmPerInputUnit;
    std::string labelHint; // A drawing's "mm" label is not confirmation.
};
struct UnitScale { double mmPerInputUnit{0.0}; };

struct SceneEntity {
    EntityKey key;
    std::string layer;
    Bounds2 bounds;
    std::string viewIdentity;
};
struct ResourceDefinition {
    // kind/name are nonempty valid UTF-8 with no embedded NUL. Symbol identity uses
    // Windows ordinal ignore-case comparison after UTF-8 decoding, including Chinese names.
    // Signatures are opaque and case-sensitive; original resource spelling is retained.
    std::string kind;
    std::string name;
    std::string definitionSignature;
    bool referencedByPreserved{false};
};
enum class PrimitiveKind { line, text, dimension, polyline, arc, other };
struct Arc2 {
    Point2 center;
    double radius{0.0};
    double startAngle{0.0};
    double sweepAngle{0.0}; // Signed: positive is counterclockwise.
};
struct Primitive2 {
    EntityKey key;
    PrimitiveKind kind{PrimitiveKind::other};
    std::string layer;
    std::optional<Bounds2> bounds;
    std::optional<Point2> start;
    std::optional<Point2> end;
    std::optional<std::string> text;
    std::optional<Point2> textPosition;
    std::optional<double> textHeight;
    std::optional<double> textRotation;
    std::optional<double> dimensionMeasurement;
    std::optional<Arc2> arc;
    bool topologyComplete{false};
    // Local text bases are R(rotation) * diag(mirrorX ? -1 : 1, mirrorY ? -1 : 1).
    bool textMirroredX{false};
    bool textMirroredY{false};
    // Optional SDK-read provenance. Empty defaults preserve historical fixtures.
    std::string sourceRootHandle;
    std::string instancePath;
    std::string nativeGeometrySignature;
    bool effectiveVisible{true};
};
// Explicit association from a reader/user, never inferred from a nearby text alone.
struct LevelAssociation {
    EntityKey lowerGeometry;
    EntityKey upperGeometry;
    EntityKey declarationText;
    Point2 lowerPoint;
    Point2 upperPoint;
    Point2 heightDirection{0.0, 1.0};
    double mmPerInputUnit{1.0};
};
struct SceneSnapshot {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::uint64_t revision{0};
    UnitDeclaration units;
    std::vector<SceneEntity> targets;
    std::vector<SceneEntity> preserved;
    std::vector<ResourceDefinition> resources;
    std::string sourceSha256;
    std::vector<Primitive2> primitives;
    std::vector<LevelAssociation> levelAssociations;
};
struct WriteRequest {
    std::string expectedDocumentIdentity;
    std::uint64_t expectedRevision{0};
    std::string viewIdentity;
    std::vector<EntityKey> modifications;
    std::vector<ResourceDefinition> requiredResources;
};
struct WriteScopePlan {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::uint64_t revision{0};
    UnitScale units;
    std::vector<EntityKey> writableEntities;
    std::vector<ResourceDefinition> reusedResources;
    std::vector<ResourceDefinition> newResources;
};

[[nodiscard]] Result<UnitScale> resolveUnits(const UnitDeclaration& declaration);
[[nodiscard]] Result<WriteScopePlan> planWriteScope(
    const SceneSnapshot& snapshot, const WriteRequest& request);

} // namespace bce
#endif
