#ifndef BCE_RECOGNITION_H
#define BCE_RECOGNITION_H
#include "ElevationModel.h"
#include "RuleModel.h"

namespace bce {
namespace diagnostic {
inline constexpr char planRequired[] = "PLAN_REQUIRED";
inline constexpr char levelConflict[] = "LEVEL_CONFLICT";
inline constexpr char configChanged[] = "VIEW_CONFIG_CHANGED";
inline constexpr char invalidTransform[] = "INVALID_TRANSFORM";
}
struct Affine2 {
    double xx{1}, xy{0}, yx{0}, yy{1}, tx{0}, ty{0};
    bool planar{true};
};
[[nodiscard]] Result<Affine2> compose(const Affine2& outer, const Affine2& inner);
[[nodiscard]] Result<Primitive2> transformed(const Primitive2& primitive, const Affine2& transform);
enum class ViewKind { elevation, section, ambiguous };
enum class FeatureRole { candidate, confirmed };
struct Feature {
    std::string kind;
    FeatureRole role{FeatureRole::candidate};
    std::vector<EntityKey> sources;
    std::optional<double> declaredMm;
    std::optional<double> geometryMm;
    std::string confirmationSource;
};
struct Axis {
    EntityKey key;
    std::string label;
    Point2 position;
};
struct PlanSnapshot {
    std::string databaseIdentity;
    std::vector<Axis> axes;
    std::optional<double> confirmedMmPerInputUnit;
};
struct AxisChain {
    std::vector<std::string> labels;
    std::vector<EntityKey> sources;
    std::vector<double> spacingsMm;
};
struct Confirmation {
    std::string value;
    std::string source;
    bool confirmed{false};
};
struct ViewConfig {
    std::string viewIdentity;
    std::string geometrySignature;
    Confirmation material;
    Confirmation axisMapping;
    std::optional<Point2> axisDirection;
};
struct ProjectConfig {
    std::string databaseIdentity;
    UnitDeclaration units;
    std::string unitConfirmationSource;
    Confirmation zeroLevel;
    std::vector<ViewConfig> views;
};
struct View {
    std::string identity;
    EntityKey titleKey;
    ViewKind kind{ViewKind::ambiguous};
    std::vector<EntityKey> members;
    Bounds2 bounds;
    std::string geometrySignature;
    std::vector<Feature> levelCandidates;
    std::vector<Issue> unconfirmed;
    std::optional<AxisChain> projectedAxes;
};
struct FeatureModel {
    std::vector<Feature> features;
    RuleInputs rules; // Explicit semantic confirmation; R1 candidates are never promoted here.
};
struct AnalysisResult {
    std::string sourceSha256;
    std::vector<View> views;
    FeatureModel model;
    std::vector<EntityKey> preserved;
    std::vector<EntityKey> unassigned;
    std::vector<Issue> issues;
};
[[nodiscard]] Result<AxisChain> projectAxes(const PlanSnapshot& plan, Point2 direction);
[[nodiscard]] AnalysisResult analyse(const SceneSnapshot&, const PlanSnapshot&, const ProjectConfig&);
[[nodiscard]] Result<ProjectConfig> validateConfig(const SceneSnapshot&, const ProjectConfig&);
}
#endif
