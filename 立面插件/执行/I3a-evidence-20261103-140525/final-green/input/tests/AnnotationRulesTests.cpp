#include "TestSupport.h"
#include "../src/AnnotationRules.h"
#include <cmath>
#include <limits>

namespace bce::test {
namespace {
EntityKey ruleKey(const std::string& h) { return {"rules-db", h, 1}; }
SourcePoint sp(double x, double y, const std::string& h) { return {{x, y}, ruleKey(h)}; }
SemanticFeature feature(const char* id, SemanticRole role, std::vector<SourcePoint> points) {
    SemanticFeature f; f.identity = id; f.viewIdentity = "front"; f.role = role;
    f.confirmed = true; f.confirmationSource = "project-confirmation"; f.points = std::move(points); return f;
}
ReferenceStandard standard() {
    ReferenceStandard s; s.referenceSha256 = "109BDCFEA4C55219793F2B6F9D8C8B619344B51A4C73E9129378C38532B2AF4D";
    ReferenceFamily a; a.identity = "main-800"; a.approved = true; a.approvalSource = "reference-role-review";
    a.roles = {SemanticRole::bodyContour, SemanticRole::level, SemanticRole::opening, SemanticRole::eave,
        SemanticRole::ridge, SemanticRole::localMoulding, SemanticRole::planAxis};
    // Fixture chooses explicit reviewed lane counts; these are not an inference from real buildings.
    a.laneOffsetsPaperMm = {8, 16, 24}; a.variant = "main-100";
    auto b = a; b.identity = "main-831"; b.laneOffsetsPaperMm = {8.31877151438, 16.63754302876};
    auto small = a; small.identity = "11B93-small25"; small.approved = false; small.textHeightPaperMm = 0.74375;
    small.variant = "block-25"; s.families = {a, b, small}; return s;
}
ProjectConfig config() {
    ProjectConfig c; c.databaseIdentity = "rules-db"; c.units.confirmedMmPerInputUnit = 1;
    c.unitConfirmationSource = "project-mm"; c.zeroLevel = {"first-floor +0", "user", true}; return c;
}
FeatureModel model() {
    FeatureModel m; RuleView v; v.viewIdentity = "front"; v.familyId = "main-800";
    v.datum = sp(0, 0, "10"); v.datumConfirmationSource = "first-floor-geometry-confirmation"; m.rules.views = {v};
    m.rules.semanticFeatures = {
        feature("body", SemanticRole::bodyContour, {sp(0,0,"11"), sp(10000,0,"12"), sp(10000,6000,"13"), sp(0,6000,"14")}),
        feature("levels", SemanticRole::level, {sp(0,0,"20"),sp(0,3000,"21"),sp(0,6000,"22")}),
        feature("ridge", SemanticRole::ridge, {sp(5000,7500,"23")}),
        feature("axes", SemanticRole::planAxis, {sp(0,0,"30"),sp(4000,0,"31"),sp(10000,0,"32")})};
    m.rules.semanticFeatures.back().fromConfirmedPlan = true; return m;
}
const DimensionChain& chain(const AnnotationPlan& p, const std::string& purpose, const std::string& id = "") {
    for (const auto& c : p.chains) { if (c.purpose == purpose && (id.empty() || c.featureIdentity == id)) { return c; } }
    throw std::runtime_error("required chain missing: " + purpose);
}
void close(double actual, double expected, const char* message) { require(std::abs(actual - expected) < 1e-8, message); }
}

// Catches copied reference heights and level text coordinates substituted for real datum geometry.
void projectLevelsDoNotUseReferenceNumbers() {
    const auto r = planAnnotations(model(), standard(), config()); require(r.canCommit(), "confirmed actual levels must plan");
    require(r.value->levels.size() == 4, "all project levels and ridge required");
    const std::vector<std::string> displays = {"±0.000", "+3.000", "+6.000", "+7.500"};
    for (std::size_t i = 0; i < displays.size(); ++i) { require(r.value->levels[i].display == displays[i], "actual project level display"); }
    close(chain(*r.value, "body-horizontal").segments.at(0).measuredMm, 10000, "actual body width");
    close(chain(*r.value, "body-vertical").segments.at(0).measuredMm, 6000, "actual body height");
    auto m = model(); m.rules.views[0].datumMeters = -0.125;
    const auto negative = planAnnotations(m, standard(), config()); require(negative.canCommit(), "negative actual datum");
    require(negative.value->levels[0].display == "-0.125", "negative sign retained");
}
void openingDimensionsUseActualEdges() {
    auto m = model(); auto o = feature("window", SemanticRole::opening,
        {sp(1200,900,"40"),sp(2400,900,"41"),sp(2400,2400,"42"),sp(1200,2400,"43")});
    o.confirmedRectangle = true; m.rules.semanticFeatures.push_back(o);
    auto r = planAnnotations(m, standard(), config()); require(r.canCommit(), "confirmed rectangular opening");
    const auto& width = chain(*r.value,"opening-width","window");
    close(width.segments.at(0).measuredMm,1200,"opening width from edges");
    close(chain(*r.value,"opening-height","window").segments.at(0).measuredMm,1500,"opening height from edges");
    close(chain(*r.value,"opening-horizontal-location","window").segments.at(0).measuredMm,1200,"opening actual horizontal location");
    close(chain(*r.value,"opening-vertical-location","window").segments.at(0).measuredMm,900,"opening actual vertical location");
    require(width.stations.front().sourcePoints.front().source == ruleKey("40"), "opening edge source retained");
    require(width.stations.front().sourcePoints.front().position.y == 900, "source geometry never replaced by projection");
    m.rules.semanticFeatures.back().confirmedRectangle = false;
    r = planAnnotations(m,standard(),config()); require(!r.canCommit() && hasCode(r,"RECTANGLE_CONFIRMATION_REQUIRED"),"unconfirmed rectangle rejected");
    m.rules.semanticFeatures.back().confirmedRectangle = true; m.rules.semanticFeatures.back().points[2].position.x = 2500;
    require(!planAnnotations(m,standard(),config()).canCommit(),"nonrectangular geometry rejected even with claimed confirmation");

    // A 30-degree rotation introduces different last-bit projections for the same mathematical edge.
    m = model(); m.rules.semanticFeatures.push_back(o);
    const auto radians = std::acos(-1.0) / 6.0;
    const auto cosine = std::cos(radians), sine = std::sin(radians);
    for (auto& f : m.rules.semanticFeatures) {
        for (auto& p : f.points) {
            const auto original = p.position;
            p.position = {cosine * original.x - sine * original.y,
                sine * original.x + cosine * original.y};
        }
    }
    m.rules.views[0].heightDirection = {-sine, cosine};
    const auto rotatedPoints = m.rules.semanticFeatures.back().points;
    r = planAnnotations(m, standard(), config());
    require(r.canCommit(), "confirmed 30-degree rectangle must survive floating-point projection error");
    close(chain(*r.value, "opening-width", "window").segments.at(0).measuredMm, 1200, "30-degree actual opening width");
    close(chain(*r.value, "opening-height", "window").segments.at(0).measuredMm, 1500, "30-degree actual opening height");
    const auto& evidence = r.value->semanticEvidence.back().points;
    for (std::size_t i = 0; i < evidence.size(); ++i) {
        require(evidence[i].position.x == rotatedPoints[i].position.x &&
            evidence[i].position.y == rotatedPoints[i].position.y && evidence[i].source == rotatedPoints[i].source,
            "rectangle classification must not snap original geometry or provenance");
    }
    // A real 0.000001-input-unit deformation is many orders above this projection's roundoff.
    auto malformed = m;
    malformed.rules.semanticFeatures.back().points[2].position.x += cosine * 0.000001;
    malformed.rules.semanticFeatures.back().points[2].position.y += sine * 0.000001;
    const auto invalid = planAnnotations(malformed, standard(), config());
    require(!invalid.canCommit() && hasCode(invalid, "INVALID_OPENING_GEOMETRY"),
        "30-degree trapezoid must not be accepted by rectangle roundoff tolerance");
}
void translationDoesNotChangeMeasurements() {
    auto m = model(); const auto original = planAnnotations(m,standard(),config()); require(original.canCommit(),"base plan");
    for (auto& f : m.rules.semanticFeatures) { for (auto& p : f.points) { p.position.x += 100000; p.position.y -= 50000; } }
    m.rules.views[0].datum->position = {100000,-50000};
    const auto moved = planAnnotations(m,standard(),config()); require(moved.canCommit(),"translated plan");
    require(moved.value->chains.size() == original.value->chains.size(),"translated chain count");
    for (std::size_t i=0;i<original.value->chains.size();++i) {
        const auto& a=original.value->chains[i]; const auto& b=moved.value->chains[i]; require(a.segments.size()==b.segments.size(),"segment count invariant");
        for(std::size_t j=0;j<a.segments.size();++j) { require(a.segments[j].measuredMm==b.segments[j].measuredMm,"translation measurement invariant"); }
    }
    for(std::size_t i=0;i<original.value->levels.size();++i) { require(original.value->levels[i].meters==moved.value->levels[i].meters,"translated level invariant"); }
    require(moved.value->levels[3].actualPoint.position.x==105000,"actual points remain translated");
}
void nonRoundCoordinatesArePreserved() {
    auto m=model(); auto& axes=m.rules.semanticFeatures.back(); axes.points={sp(0.125,0.25,"30"),sp(1234.875,0.25,"31"),sp(1234.875001,0.25,"32")};
    const auto r=planAnnotations(m,standard(),config()); require(r.canCommit(),"nonround axes"); const auto& c=chain(*r.value,"plan-axis");
    require(c.segments.size()==2,"nearby distinct points never snapped together");
    require(c.segments[0].measuredMm==1234.75 && c.segments[0].display=="1235","display rounded separately");
    require(c.stations[1].sourcePoints[0].position.x==1234.875,"original double retained");
    require(c.stations[2].sourcePoints[0].position.x==1234.875001 && c.segments[1].measuredMm>0,"noninteger station survives");
}
void duplicateStationsNeverCreateZeroSegments() {
    auto m=model(); m.rules.semanticFeatures.back().points={sp(4000,0,"31"),sp(0,0,"30"),sp(4000,0,"33"),sp(10000,0,"32"),sp(4000,0,"31")};
    const auto r=planAnnotations(m,standard(),config()); require(r.canCommit(),"duplicate axes allowed with provenance"); const auto& c=chain(*r.value,"plan-axis");
    require(c.stations.size()==3 && c.segments.size()==2,"equal projected stations deduplicated");
    require(c.stations[1].sourcePoints.size()==3,"all original duplicate provenance retained");
    require(c.segments[0].measuredMm==4000 && c.segments[1].measuredMm==6000,"strictly positive ordered spacings");
}
void dimensionFamilyKeepsItsVerifiedSpacing() {
    auto m=model(); auto r=planAnnotations(m,standard(),config()); require(r.canCommit(),"800 family");
    require(r.value->layouts[0].laneOffsetsInput==std::vector<double>({800,1600,2400}),"800 explicit lanes retained");
    close(r.value->layouts[0].textHeightInput,297.5,"actual reference text height");
    m.rules.views[0].familyId="main-831"; r=planAnnotations(m,standard(),config()); require(r.canCommit(),"831 family");
    close(r.value->layouts[0].laneOffsetsInput[0],831.877151438,"831 spacing retained"); require(r.value->layouts[0].laneOffsetsInput.size()==2,"explicit lane count retained");
    m.rules.views[0].scaleDenominator=50; r=planAnnotations(m,standard(),config()); require(r.canCommit(),"1:50 family");
    close(r.value->layouts[0].textHeightInput,148.75,"paper text height invariant"); close(r.value->layouts[0].laneOffsetsInput[0],415.938575719,"paper spacing invariant");
    require(chain(*r.value,"body-horizontal").stations.back().sourcePoints[0].position.x==10000,"building geometry unscaled");
    m.rules.views[0].familyId=""; require(hasCode(planAnnotations(m,standard(),config()),"REFERENCE_FAMILY_REQUIRED"),"missing explicit family");
    m.rules.views[0].familyId="11B93-small25"; require(hasCode(planAnnotations(m,standard(),config()),"REFERENCE_FAMILY_REQUIRED"),"unapproved small block family rejected");
    m.rules.views[0].familyId="main-800"; auto s=standard(); s.families[0].roles={SemanticRole::bodyContour};
    require(!planAnnotations(m,s,config()).canCommit(),"family role approval required");
}
void missingDatumOrSemanticRoleBlocksAnnotation() {
    auto m=model(); m.rules.views[0].datum.reset(); auto r=planAnnotations(m,standard(),config());
    require(!r.canCommit()&&hasCode(r,"DATUM_REQUIRED"),"zero text rule without actual datum blocked");
    m=model(); m.rules.semanticFeatures[0].confirmed=false; r=planAnnotations(m,standard(),config());
    require(!r.canCommit()&&hasCode(r,"SEMANTIC_CONFIRMATION_REQUIRED"),"candidate not promoted");
    bool located=false; for(const auto& i:r.issues) { if(i.source.viewIdentity=="front"&&i.source.entity.has_value()) { located=true; } } require(located,"diagnostic is located");
    m=model(); m.rules.semanticFeatures.erase(m.rules.semanticFeatures.begin()); require(hasCode(planAnnotations(m,standard(),config()),"SEMANTIC_ROLE_REQUIRED"),"missing body blocked");
    auto c=config(); c.units={}; require(hasCode(planAnnotations(model(),standard(),c),"UNIT_REQUIRED"),"unknown units blocked");
    m=model(); m.rules.semanticFeatures[0].points[0].position.x=std::numeric_limits<double>::infinity(); require(!planAnnotations(m,standard(),config()).canCommit(),"infinite source blocked");
    m=model(); m.rules.views[0].datumMeters=std::numeric_limits<double>::quiet_NaN(); require(!planAnnotations(m,standard(),config()).canCommit(),"NaN datum blocked");
    m=model(); m.rules.views[0].scaleDenominator=std::numeric_limits<double>::max(); require(!planAnnotations(m,standard(),config()).canCommit(),"overflowing layout blocked");
    m=model(); m.rules.semanticFeatures[0].points[0].position.x=-std::numeric_limits<double>::max();m.rules.semanticFeatures[0].points[1].position.x=std::numeric_limits<double>::max();
    require(!planAnnotations(m,standard(),config()).canCommit(),"overflowing measurement blocked");
}
void axisChainRequiresConfirmedPlan() {
    auto m=model(); m.rules.semanticFeatures.pop_back(); auto r=planAnnotations(m,standard(),config()); require(!r.canCommit()&&hasCode(r,"PLAN_REQUIRED"),"missing plan cannot invent axis chain");
    m=model(); m.rules.semanticFeatures.back().fromConfirmedPlan=false; r=planAnnotations(m,standard(),config()); require(!r.canCommit()&&hasCode(r,"PLAN_REQUIRED"),"elevation marks cannot impersonate plan axes");
}
void featureRolesAndDirectionsRemainTraceable() {
    auto m=model(); m.rules.semanticFeatures.push_back(feature("eave",SemanticRole::eave,{sp(0,6200,"50"),sp(10000,6200,"51")}));
    m.rules.semanticFeatures.push_back(feature("trim",SemanticRole::localMoulding,{sp(500,2000,"52"),sp(9500,2100,"53")}));
    auto s=standard(); s.nativeCapabilities={CapabilityState::unsupported,CapabilityState::unverified};
    auto r=planAnnotations(m,s,config()); require(r.canCommit(),"explicit eave and local geometry");
    close(chain(*r.value,"component-horizontal","eave").segments[0].measuredMm,10000,"eave actual width");
    close(chain(*r.value,"component-vertical","trim").segments[0].measuredMm,100,"local actual height");
    require(r.value->nativeCapabilities.fieldSetter==CapabilityState::unsupported&&r.value->nativeCapabilities.glyphPreparation==CapabilityState::unverified,"native capability state never promoted");
    require(r.value->referenceSha256==s.referenceSha256,"reference SHA retained");
    // Rotate every actual point and the datum basis by +90 degrees: dimensions still describe the same geometry.
    for(auto& f:m.rules.semanticFeatures) { for(auto& p:f.points) { p.position={-p.position.y,p.position.x}; } }
    m.rules.views[0].heightDirection={-1,0}; r=planAnnotations(m,s,config()); require(r.canCommit(),"confirmed rotated height direction");
    close(chain(*r.value,"body-horizontal").segments[0].measuredMm,10000,"rotated actual width");
    require(r.value->levels[3].display=="+7.500","rotated height projection");
}
void individuallyConfirmedFloorsFormOneChain() {
    auto m=model();m.rules.semanticFeatures.erase(m.rules.semanticFeatures.begin()+1);
    m.rules.semanticFeatures.push_back(feature("floor-2",SemanticRole::level,{sp(0,6000,"22")}));
    m.rules.semanticFeatures.push_back(feature("floor-0",SemanticRole::level,{sp(0,0,"20")}));
    m.rules.semanticFeatures.push_back(feature("floor-1",SemanticRole::level,{sp(0,3000,"21"),sp(0,3000,"24")}));
    const auto r=planAnnotations(m,standard(),config());require(r.canCommit(),"individually confirmed floor geometry grouped per explicit view");
    const auto& c=chain(*r.value,"floor-levels");require(c.segments.size()==2&&c.segments[0].measuredMm==3000&&c.segments[1].measuredMm==3000,"one monotonic actual floor chain");
    require(c.semanticFeatureIdentities.size()==3&&c.stations[1].sourcePoints.size()==2,"all floor feature and duplicate source evidence retained");
}
void annotationPlanRetainsConfirmationAndUnitEvidence() {
    auto c=config();c.units.confirmedMmPerInputUnit=1000;
    auto m=model();for(auto& f:m.rules.semanticFeatures) {for(auto& p:f.points) {p.position.x/=1000;p.position.y/=1000;}}
    const auto r=planAnnotations(m,standard(),c);require(r.canCommit(),"confirmed meters converted to model parameters");
    require(r.value->mmPerInputUnit==1000,"unit evidence retained");
    require(r.value->viewEvidence.at(0).datumConfirmationSource=="first-floor-geometry-confirmation","actual datum confirmation retained");
    require(r.value->semanticEvidence.at(0).confirmationSource=="project-confirmation","semantic confirmation retained");
    close(chain(*r.value,"body-horizontal").segments[0].measuredMm,10000,"meter source actual width in mm");
    close(r.value->layouts[0].textHeightInput,0.2975,"paper text converted to meter input units");
    require(r.value->levels[3].meters==7.5,"actual meters remain meters");
}
}
