#ifndef BCE_PROJECT_FACTS_H
#define BCE_PROJECT_FACTS_H
#include "ElevationModel.h"
#include <array>
#include <variant>

namespace bce {
// Version 1 is only a dependency subset, never a complete scene fingerprint.
struct FactPoint3 { double x{0.0}; double y{0.0}; double z{0.0}; };
struct FactReadProvenance {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::uint64_t revision{0}; // Zero is unavailable provenance, even within a consistent batch.
};
struct FactLine {
    FactPoint3 start;
    FactPoint3 end;
    FactPoint3 normal{0.0, 0.0, 1.0};
    double thickness{0.0};
};
struct FactAlignedDimension {
    FactPoint3 xLine1Point;
    FactPoint3 xLine2Point;
    FactPoint3 normal{0.0, 0.0, 1.0};
    // Measurement, displayed text and DimStyle are deliberately outside this subset.
};
struct FactText {
    std::string text;
    FactPoint3 position;
    FactPoint3 alignmentPoint;
    FactPoint3 normal{0.0, 0.0, 1.0};
    double rotation{0.0};
    double height{1.0};
    double widthFactor{1.0};
    double oblique{0.0};
    int horizontalMode{0};
    int verticalMode{0};
    bool mirroredX{false};
    bool mirroredY{false};
    double thickness{0.0};
    bool hasFields{false};
};
struct FactSource {
    std::string rootHandle;
    std::string ownerHandle;
    std::string exactClass;
    bool directRoot{false};
    bool effectiveVisible{false};
    FactReadProvenance read;
    std::variant<std::monostate, FactLine, FactAlignedDimension, FactText> definition;
};
struct FactUnits {
    double mmPerInputUnit{0.0};
    std::string confirmationSource;
};
enum class DatumPointSelector { lineStart, lineEnd, dimXLine1, dimXLine2 };
struct FactWindow {
    FactPoint3 origin;
    FactPoint3 u{1.0, 0.0, 0.0};
    FactPoint3 v{0.0, 1.0, 0.0};
    double width{0.0};
    double height{0.0};
};
struct FactTextDependency {
    std::string role; // Explicit association, e.g. floor-name or level-value; no inferred grouping.
    FactSource source;
};
struct ProjectFactConfirmation {
    std::string persistentProjectId;
    FactUnits units;
    // All sources must come from this same completed, current read batch.
    FactReadProvenance read;
    std::string modelSpaceOwnerHandle;
    FactSource title;
    std::vector<FactTextDependency> levelDeclarations;
    FactSource datumSource;
    DatumPointSelector pointSelector{DatumPointSelector::lineStart};
    double datumMeters{0.0};
    std::string datumConfirmationSource;
    FactPoint3 heightDirection;
    FactWindow window;
    std::string selectionConfirmationSource;
};
struct ProjectFacts {
    unsigned subsetVersion{1};
    ProjectFactConfirmation confirmed;
    FactPoint3 datumPoint; // Derived solely from the explicitly selected typed getter.
    FactPoint3 unitHeightDirection;
    std::array<FactPoint3, 4> windowCorners;
};
struct ProjectFactObservation {
    bool readCompleted{false}; // An adapter assertion, not recoverable from historical facts.
    std::string persistentProjectId;
    FactUnits units;
    FactReadProvenance read;
    std::string modelSpaceOwnerHandle;
    std::vector<FactSource> sources;
};
enum class FactSubsetStatus { currentSubsetMatches, stale, unsupported, orphan };
struct FactSubsetCheck {
    FactSubsetStatus status{FactSubsetStatus::stale};
    std::vector<Issue> issues;
    // No archive binding, write plan, rule input or generation qualification is produced.
};
[[nodiscard]] Result<ProjectFacts> confirmProjectFacts(const ProjectFactConfirmation&);
[[nodiscard]] FactSubsetCheck checkProjectFacts(
    const ProjectFacts&, const std::optional<ProjectFactObservation>& current);
// Actual projected elevation; datumMeters may be nonzero and direction need not be +Y.
[[nodiscard]] Result<double> projectFactElevationMeters(const ProjectFacts&, FactPoint3 point);
} // namespace bce
#endif
