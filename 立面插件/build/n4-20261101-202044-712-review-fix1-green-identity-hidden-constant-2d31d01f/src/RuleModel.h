#ifndef BCE_RULE_MODEL_H
#define BCE_RULE_MODEL_H
#include "ElevationModel.h"

namespace bce {
enum class SemanticRole { bodyContour, level, opening, eave, ridge, localMoulding, planAxis, materialRegion };
struct SourcePoint { Point2 position; EntityKey source; };
struct ClosedRing { std::vector<Point2> vertices; bool closed{false}; };
struct PolygonRegion { ClosedRing outer; std::vector<ClosedRing> holes; };
enum class MaterialExpressionKind { hatch, lineDrawing, glassBlock };
struct MaterialExpression {
    MaterialExpressionKind kind{MaterialExpressionKind::hatch};
    std::string prototypeId;
    Point2 direction{1, 0};
    double scale{1};
    std::string patternName;
};
struct SemanticFeature {
    std::string identity;
    std::string viewIdentity;
    SemanticRole role{SemanticRole::bodyContour};
    bool confirmed{false};
    std::string confirmationSource;
    std::vector<SourcePoint> points;
    bool confirmedRectangle{false};
    bool fromConfirmedPlan{false};
    std::optional<PolygonRegion> region;
    std::vector<EntityKey> sources;
    std::string materialKey;
    std::string materialPrototypeId;
    std::string tableViewIdentity;
    std::string sharedTableIdentity;
    bool requestConstructionNote{false};
    std::string construction;
    std::string detailId;
};
struct RuleView {
    std::string viewIdentity;
    std::string familyId;
    double scaleDenominator{100};
    std::optional<SourcePoint> datum;
    std::string datumConfirmationSource;
    double datumMeters{0};
    Point2 heightDirection{0, 1};
    std::vector<SemanticRole> requiredRoles{SemanticRole::bodyContour, SemanticRole::level};
    bool requirePlanAxes{true};
};
enum class CapabilityState { unverified, unsupported, verified };
struct NativeRuleCapabilities {
    CapabilityState fieldSetter{CapabilityState::unverified};
    CapabilityState glyphPreparation{CapabilityState::unverified};
};
struct ReferenceFamily {
    std::string identity;
    bool approved{false};
    std::string approvalSource;
    std::vector<SemanticRole> roles;
    // Explicit layout lanes; their count is never inferred from building size.
    std::vector<double> laneOffsetsPaperMm;
    double textHeightPaperMm{2.975};
    double textWidthFactor{0.7058823529411765};
    int dimensionDecimals{0};
    std::string variant;
};
struct MaterialPrototype { MaterialExpression expression; bool approved{false}; std::string approvalSource; };
struct ReferenceStandard {
    std::string referenceSha256;
    std::vector<ReferenceFamily> families;
    std::vector<MaterialPrototype> materialPrototypes;
    NativeRuleCapabilities nativeCapabilities;
};
struct MaterialOverride {
    std::string key;
    std::string number;
    std::string name;
    std::optional<std::string> prototypeId;
};
struct RuleInputs {
    std::vector<RuleView> views;
    std::vector<SemanticFeature> semanticFeatures;
    std::vector<MaterialOverride> materialOverrides;
};
struct DimensionStation {
    double coordinateInput{0}; // Relative to the explicitly recorded chain origin.
    std::vector<SourcePoint> sourcePoints;
    std::vector<Point2> projectedPoints; // Derived points, never source geometry.
};
struct DimensionSegment {
    double measuredMm{0};
    std::string display;
};
struct DimensionChain {
    std::string viewIdentity;
    std::string featureIdentity;
    std::string purpose;
    Point2 origin;
    Point2 direction;
    std::vector<DimensionStation> stations;
    std::vector<DimensionSegment> segments;
    int displayDecimals{0};
    std::vector<std::string> semanticFeatureIdentities;
};
struct LevelMark {
    std::string viewIdentity;
    std::string featureIdentity;
    SourcePoint actualPoint;
    SourcePoint datum;
    Point2 heightDirection;
    double meters{0};
    std::string display;
};
struct AnnotationLayout {
    std::string viewIdentity;
    std::string familyId;
    double scaleDenominator{100};
    double textHeightInput{0};
    double textWidthFactor{0};
    std::vector<double> laneOffsetsInput;
};
struct AnnotationPlan {
    std::string referenceSha256;
    std::vector<DimensionChain> chains;
    std::vector<LevelMark> levels;
    std::vector<AnnotationLayout> layouts;
    NativeRuleCapabilities nativeCapabilities;
    double mmPerInputUnit{0};
    UnitDeclaration unitDeclaration;
    std::string unitConfirmationSource;
    std::vector<RuleView> viewEvidence;
    std::vector<SemanticFeature> semanticEvidence;
};
struct MaterialRegionPlan {
    std::string identity;
    std::string viewIdentity;
    std::string tableViewIdentity;
    std::string sharedTableIdentity;
    std::vector<EntityKey> sources;
    std::string confirmationSource;
    PolygonRegion geometry;
    double areaSquareMm{0};
    std::string materialNumber;
    std::string materialName;
    MaterialExpression expression;
    std::optional<std::string> constructionNote;
    std::string familyId;
    std::string prototypeApprovalSource;
};
struct MaterialPlan {
    std::string referenceSha256;
    std::vector<MaterialRegionPlan> regions;
    NativeRuleCapabilities nativeCapabilities;
    double mmPerInputUnit{0};
    UnitDeclaration unitDeclaration;
    std::string unitConfirmationSource;
};
}
#endif
