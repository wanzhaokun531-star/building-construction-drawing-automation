#ifndef BCE_LAYOUT_H
#define BCE_LAYOUT_H
#include "WriteContracts.h"
#include <cstddef>

namespace bce {
enum class OccupancyKind { architecture, textGlyph, symbol, dimensionGlyph, dimensionBaseline, materialTable };
enum class OccupancyEvidence { unverified, nativeVerified, syntheticVerified };
struct OccupancyPart { std::string identity; OccupancyKind kind{OccupancyKind::architecture}; };
struct VisibleOccupancy {
    OccupancyPart part;
    Bounds2 bounds;
    OccupancyEvidence evidence{OccupancyEvidence::unverified};
    std::string source;
    // Glyph-bearing parts must match the context, including the resolved font.
    std::string fontSignature;
    std::string glyphSignature;
};
struct LayoutContext {
    double scaleDenominator{0};
    double mmPerInputUnit{0};
    std::string geometrySignature;
    std::string fontSignature;
    std::string glyphSignature;
};
struct ViewEnvelope {
    std::string viewIdentity;
    std::vector<EntityKey> targets;
    // Reader/preparer declares the full visible-part inventory independently.
    std::vector<OccupancyPart> requiredParts;
    std::vector<VisibleOccupancy> occupancies;
    bool complete{false};
    std::string source;
    LayoutContext context;
};
struct ViewExpectation { std::string viewIdentity; LayoutContext context; };
struct FixedEnvelope {
    std::string identity;
    std::vector<EntityKey> targets;
    std::vector<VisibleOccupancy> occupancies;
    std::vector<OccupancyPart> requiredParts;
    bool complete{false};
    std::string source;
    LayoutContext context;
};
struct LayoutRequest {
    std::string databaseIdentity;
    std::uint64_t revision{0};
    Bounds2 available;
    double clearance{0}; // Input drawing units, independently confirmed by caller.
    std::vector<ViewExpectation> expectedViews;
    std::vector<FixedEnvelope> fixed;
    bool allowReorder{false}; // false preserves input order; true explores permutations.
    std::size_t searchBudget{10000};
};
struct PlacedView {
    std::string viewIdentity;
    Point2 displacement; // The sole rigid translation for all parts and targets.
    LayoutPlan plan;
    Bounds2 occupiedBounds;
    std::vector<VisibleOccupancy> occupancies;
    LayoutContext context;
    std::string source;
    std::vector<OccupancyPart> requiredParts;
    bool complete{false};
};
struct LayoutArrangement {
    std::vector<PlacedView> views; // Returned in input order, even with reorder search.
    Bounds2 occupiedBounds;
    std::vector<FixedEnvelope> fixed; // Read-only evidence, never in translations.
    bool nativeReady{false};
    std::size_t searchedCandidates{0};
    // Complete search only within positive-edge-anchor layouts: lower-left anchors
    // are area.minimum or the right/top edge + clearance of fixed/earlier views.
    std::string layoutFamily{"positive_edge_anchors_v1"};
    Bounds2 available;
    double clearance{0};
    bool allowReorder{false};
};
struct CollisionPair { std::string first; std::string second; };
struct OccupancyCollision { std::string first; std::string second; };
[[nodiscard]] Result<std::vector<OccupancyCollision>> checkInternalCollisions(
    const ViewEnvelope&, const std::vector<CollisionPair>&, double clearance);
// Synthetic envelopes may yield a value, but LAYOUT_SYNTHETIC_ONLY blocks canCommit().
// Supports <=16 views, <=256 fixed envelopes, <=10000 parts, budget <=1000000.
[[nodiscard]] Result<LayoutArrangement> arrange(const std::vector<ViewEnvelope>&, const LayoutRequest&);
[[nodiscard]] Result<double> paperSizeInInput(double paperMm, double scaleDenominator, double mmPerInputUnit);
}
#endif
