#include "AnnotationRules.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>

namespace bce {
namespace {
bool finite(Point2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool validKey(const EntityKey& key, const ProjectConfig& config) {
    return !config.databaseIdentity.empty() && key.databaseIdentity == config.databaseIdentity && key == key;
}
bool validSha(const std::string& sha) {
    return sha.size() == 64 && std::all_of(sha.begin(), sha.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    });
}
std::optional<EntityKey> source(const SemanticFeature* feature) {
    if (!feature) { return std::nullopt; }
    if (!feature->points.empty()) { return feature->points.front().source; }
    if (!feature->sources.empty()) { return feature->sources.front(); }
    return std::nullopt;
}
void issue(Result<AnnotationPlan>& r, const char* code, const char* message,
    const std::string& view = "", const SemanticFeature* feature = nullptr) {
    r.issues.push_back({code, message, {"", view, source(feature)}, true});
}
double projection(Point2 p, Point2 origin, Point2 direction) {
    return (p.x - origin.x) * direction.x + (p.y - origin.y) * direction.y;
}
std::string display(double value, int decimals, bool signedLevel = false) {
    // Normalize display-only negative zero without modifying measured geometry.
    const auto threshold = 0.5 * std::pow(10.0, -decimals);
    const auto shown = std::abs(value) < threshold ? 0.0 : value;
    std::ostringstream out; out.imbue(std::locale::classic());
    if (signedLevel) { out << (shown == 0.0 ? u8"±" : shown > 0.0 ? "+" : ""); }
    out << std::fixed << std::setprecision(decimals) << shown; return out.str();
}
const ReferenceFamily* familyFor(const RuleView& view, const ReferenceStandard& standard) {
    const ReferenceFamily* match = nullptr;
    for (const auto& family : standard.families) {
        if (family.identity != view.familyId) { continue; }
        if (match) { return nullptr; }
        match = &family;
    }
    if (!match || view.familyId.empty() || !match->approved || match->approvalSource.empty()) { return nullptr; }
    return match;
}
bool validFamily(const ReferenceFamily& family) {
    if (!std::isfinite(family.textHeightPaperMm) || family.textHeightPaperMm <= 0 ||
        !std::isfinite(family.textWidthFactor) || family.textWidthFactor <= 0 ||
        family.dimensionDecimals < 0 || family.dimensionDecimals > 10 || family.laneOffsetsPaperMm.empty()) { return false; }
    double previous = 0;
    for (const auto offset : family.laneOffsetsPaperMm) {
        if (!std::isfinite(offset) || offset <= previous) { return false; }
        previous = offset;
    }
    return true;
}
std::vector<SourcePoint> extremes(const std::vector<SourcePoint>& points, Point2 origin, Point2 direction) {
    std::vector<SourcePoint> selected;
    if (points.empty()) { return selected; }
    auto low = projection(points[0].position, origin, direction); auto high = low;
    for (const auto& p : points) { const auto c = projection(p.position, origin, direction); low = (std::min)(low, c); high = (std::max)(high, c); }
    for (const auto& p : points) { const auto c = projection(p.position, origin, direction); if (c == low || c == high) { selected.push_back(p); } }
    return selected;
}
bool addChain(Result<AnnotationPlan>& result, const RuleView& view, const ReferenceFamily& family,
    const std::string& identity, const char* purpose, const std::vector<SourcePoint>& points,
    Point2 direction, double unit, bool mustSpan = true, const std::vector<std::string>& semanticIds = {}) {
    DimensionChain chain; chain.viewIdentity = view.viewIdentity; chain.featureIdentity = identity;
    chain.purpose = purpose; chain.origin = view.datum->position; chain.direction = direction;
    chain.displayDecimals = family.dimensionDecimals;
    chain.semanticFeatureIdentities = semanticIds.empty() ? std::vector<std::string>{identity} : semanticIds;
    std::vector<std::pair<double, SourcePoint>> sorted;
    for (const auto& p : points) {
        const auto c = projection(p.position, chain.origin, direction);
        const Point2 projected{chain.origin.x + c * direction.x, chain.origin.y + c * direction.y};
        if (!std::isfinite(c) || !finite(projected)) {
            result.issues.push_back({"NONFINITE_RULE_GEOMETRY", u8"测点投影溢出或非有限，不能制定尺寸链。", {"", view.viewIdentity, p.source}, true}); return false;
        }
        sorted.push_back({c, p});
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& item : sorted) {
        if (chain.stations.empty() || chain.stations.back().coordinateInput != item.first) {
            chain.stations.push_back({item.first, {}, {}});
        }
        auto& station = chain.stations.back(); station.sourcePoints.push_back(item.second);
        station.projectedPoints.push_back({chain.origin.x + item.first * direction.x, chain.origin.y + item.first * direction.y});
    }
    if (chain.stations.size() < 2) {
        if (mustSpan) { issue(result,"INSUFFICIENT_RULE_GEOMETRY",u8"尺寸链需要至少两个不同的实际测站。",view.viewIdentity); return false; }
        return true; // Coincident positioning has no zero-length dimension.
    }
    for (std::size_t i = 1; i < chain.stations.size(); ++i) {
        const auto measured = (chain.stations[i].coordinateInput - chain.stations[i - 1].coordinateInput) * unit;
        if (!std::isfinite(measured) || measured <= 0) {
            issue(result,"NONFINITE_RULE_GEOMETRY",u8"实际尺寸测值溢出、非有限或不递增。",view.viewIdentity); return false;
        }
        chain.segments.push_back({measured, display(measured, family.dimensionDecimals)});
    }
    result.value->chains.push_back(std::move(chain)); return true;
}
std::optional<std::array<int, 4>> rectangleAxisClasses(const SemanticFeature& feature, Point2 direction) {
    struct Coordinate { double value; double error; std::size_t index; };
    std::array<Coordinate, 4> coordinates{};
    const auto origin = feature.points.front().position;
    // Classification only: gamma(16) bounds the local subtraction, normalized-basis projection
    // and a rounded planar rotation's short operation chain. It is a binary64 error budget,
    // not a model-unit drafting tolerance; translating the datum cannot inflate it.
    constexpr auto roundoff = std::numeric_limits<double>::epsilon() / 2.0;
    constexpr auto gamma16 = (16.0 * roundoff) / (1.0 - 16.0 * roundoff);
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        const auto p = feature.points[i].position;
        const auto x = (p.x - origin.x) * direction.x;
        const auto y = (p.y - origin.y) * direction.y;
        const auto error = gamma16 * (std::abs(x) + std::abs(y));
        const auto value = x + y;
        if (!std::isfinite(value) || !std::isfinite(error)) { return std::nullopt; }
        coordinates[i] = {value, error, i};
    }
    std::sort(coordinates.begin(), coordinates.end(), [](const Coordinate& a, const Coordinate& b) {
        return a.value < b.value;
    });
    const auto equalWithinRoundoff = [](const Coordinate& a, const Coordinate& b) {
        return std::abs(a.value - b.value) <= a.error + b.error;
    };
    // A rectangle has two separately resolvable edge stations, each represented twice.
    if (!equalWithinRoundoff(coordinates[0], coordinates[1]) ||
        !equalWithinRoundoff(coordinates[2], coordinates[3]) ||
        coordinates[2].value - coordinates[1].value <= coordinates[2].error + coordinates[1].error) {
        return std::nullopt;
    }
    std::array<int, 4> classes{};
    for (std::size_t i = 0; i < coordinates.size(); ++i) { classes[coordinates[i].index] = i < 2 ? 0 : 1; }
    return classes;
}
bool rectangle(const SemanticFeature& feature, Point2 width, Point2 height) {
    if (feature.points.size() != 4) { return false; }
    const auto xs = rectangleAxisClasses(feature, width), ys = rectangleAxisClasses(feature, height);
    if (!xs || !ys) { return false; }
    std::set<std::pair<int, int>> corners;
    for (std::size_t i = 0; i < feature.points.size(); ++i) { corners.emplace((*xs)[i], (*ys)[i]); }
    return corners.size() == 4;
}
void addLevels(Result<AnnotationPlan>& result, const SemanticFeature& feature, const RuleView& view,
    Point2 height, double unit) {
    for (const auto& p : feature.points) {
        const auto meters = projection(p.position,view.datum->position,height) * unit / 1000.0 + view.datumMeters;
        if (!std::isfinite(meters)) { issue(result,"NONFINITE_RULE_GEOMETRY",u8"实际标高投影溢出或非有限。",view.viewIdentity,&feature); continue; }
        result.value->levels.push_back({view.viewIdentity,feature.identity,p,*view.datum,height,meters,display(meters,3,true)});
    }
}
}

Result<AnnotationPlan> planAnnotations(const FeatureModel& model, const ReferenceStandard& standard, const ProjectConfig& config) {
    Result<AnnotationPlan> result; result.value.emplace(); result.value->referenceSha256 = standard.referenceSha256;
    result.value->nativeCapabilities = standard.nativeCapabilities;
    if (!validSha(standard.referenceSha256)) { issue(result,"REFERENCE_STANDARD_REQUIRED",u8"参考标准缺少有效的源SHA256绑定。"); }
    const auto units = resolveUnits(config.units);
    if (!units.canCommit()) { result.issues.insert(result.issues.end(),units.issues.begin(),units.issues.end()); return result; }
    const auto unit = units.value->mmPerInputUnit;
    result.value->mmPerInputUnit = unit;
    result.value->unitDeclaration = config.units;
    result.value->unitConfirmationSource = config.unitConfirmationSource;
    if (model.rules.views.empty()) { issue(result,"SEMANTIC_ROLE_REQUIRED",u8"缺少明确视图和已确认构件语义。"); return result; }
    std::set<std::string> viewIds;
    for (const auto& view : model.rules.views) {
        if (view.viewIdentity.empty() || !viewIds.insert(view.viewIdentity).second) {
            issue(result,"AMBIGUOUS_RULE_VIEW",u8"视图ID缺失或重复，无法唯一关联标注。",view.viewIdentity); continue;
        }
        const auto* family = familyFor(view,standard);
        if (!family || !validFamily(*family)) { issue(result,"REFERENCE_FAMILY_REQUIRED",u8"必须明确选择参数完整且按角色核准的参考排法。",view.viewIdentity); continue; }
        if (!std::isfinite(view.scaleDenominator) || view.scaleDenominator <= 0) { issue(result,"INVALID_RULE_SCALE",u8"出图比例分母必须为有限正数。",view.viewIdentity); continue; }
        AnnotationLayout layout; layout.viewIdentity=view.viewIdentity; layout.familyId=family->identity;
        layout.scaleDenominator=view.scaleDenominator; layout.textWidthFactor=family->textWidthFactor;
        layout.textHeightInput=family->textHeightPaperMm*view.scaleDenominator/unit;
        for(const auto offset:family->laneOffsetsPaperMm) { layout.laneOffsetsInput.push_back(offset*view.scaleDenominator/unit); }
        if (!std::isfinite(layout.textHeightInput) || layout.textHeightInput <= 0 ||
            std::any_of(layout.laneOffsetsInput.begin(),layout.laneOffsetsInput.end(),[](double x){return !std::isfinite(x)||x<=0;})) {
            issue(result,"NONFINITE_RULE_GEOMETRY",u8"纸面参数换算溢出或非有限。",view.viewIdentity); continue;
        }
        result.value->layouts.push_back(std::move(layout));
        if (!view.datum || view.datumConfirmationSource.empty() || !config.zeroLevel.confirmed || config.zeroLevel.source.empty()) {
            issue(result,"DATUM_REQUIRED",u8"一层零点规则必须关联已确认的实际基准点及确认来源。",view.viewIdentity); continue;
        }
        const auto norm=std::hypot(view.heightDirection.x,view.heightDirection.y);
        if(!finite(view.datum->position)||!validKey(view.datum->source,config)||!std::isfinite(view.datumMeters)||
            !finite(view.heightDirection)||!std::isfinite(norm)||norm<=0) {
            issue(result,"DATUM_REQUIRED",u8"实际基准点、来源、标高值或高度方向无效。",view.viewIdentity); continue;
        }
        const Point2 height{view.heightDirection.x/norm,view.heightDirection.y/norm}; const Point2 width{height.y,-height.x};
        result.value->viewEvidence.push_back(view);
        std::vector<const SemanticFeature*> confirmed; std::set<std::string> featureIds;
        for(const auto& f:model.rules.semanticFeatures) {
            if(f.viewIdentity!=view.viewIdentity || f.role==SemanticRole::materialRegion) { continue; }
            if(f.identity.empty()||!featureIds.insert(f.identity).second) {issue(result,"AMBIGUOUS_RULE_FEATURE",u8"构件ID缺失或重复。",view.viewIdentity,&f);continue;}
            if(!f.confirmed||f.confirmationSource.empty()) {issue(result,"SEMANTIC_CONFIRMATION_REQUIRED",u8"构件仍为候选，需明确角色和实际几何确认。",view.viewIdentity,&f);continue;}
            if(std::find(family->roles.begin(),family->roles.end(),f.role)==family->roles.end()) {issue(result,"REFERENCE_FAMILY_REQUIRED",u8"所选排法尚未核准用于该构件角色。",view.viewIdentity,&f);continue;}
            if(f.points.empty()||std::any_of(f.points.begin(),f.points.end(),[&](const SourcePoint& p){return !finite(p.position)||!validKey(p.source,config);})) {
                issue(result,"INSUFFICIENT_RULE_GEOMETRY",u8"缺少有效实际几何点或源对象身份。",view.viewIdentity,&f);continue;
            }
            if(std::any_of(f.points.begin(),f.points.end(),[&](const SourcePoint& p){return !std::isfinite(projection(p.position,view.datum->position,height))||!std::isfinite(projection(p.position,view.datum->position,width));})) {
                issue(result,"NONFINITE_RULE_GEOMETRY",u8"实际构件相对基准投影溢出。",view.viewIdentity,&f);continue;
            }
            confirmed.push_back(&f);
            result.value->semanticEvidence.push_back(f);
        }
        for(const auto role:view.requiredRoles) {
            if(std::none_of(confirmed.begin(),confirmed.end(),[&](const SemanticFeature* f){return f->role==role;})) {issue(result,"SEMANTIC_ROLE_REQUIRED",u8"所需构件角色缺少已确认几何。",view.viewIdentity);}
        }
        std::vector<const SemanticFeature*> bodies;
        for(const auto* f:confirmed) {if(f->role==SemanticRole::bodyContour) {bodies.push_back(f);}}
        if(bodies.size()!=1) {issue(result,"SEMANTIC_ROLE_REQUIRED",u8"每视图需要唯一确认的主体轮廓；多个主体须先明确分组。",view.viewIdentity);continue;}
        const auto& body=*bodies[0];
        const auto bx=extremes(body.points,view.datum->position,width); const auto by=extremes(body.points,view.datum->position,height);
        addChain(result,view,*family,body.identity,"body-horizontal",bx,width,unit);
        addChain(result,view,*family,body.identity,"body-vertical",by,height,unit);
        const auto left=*std::min_element(body.points.begin(),body.points.end(),[&](const SourcePoint& a,const SourcePoint& b){return projection(a.position,view.datum->position,width)<projection(b.position,view.datum->position,width);});
        bool actualPlan=false;
        std::vector<SourcePoint> floorPoints;
        std::vector<std::string> floorIdentities;
        for (const auto* f : confirmed) {
            if (f->role == SemanticRole::level) {
                floorPoints.insert(floorPoints.end(), f->points.begin(), f->points.end());
                floorIdentities.push_back(f->identity);
            }
        }
        if (!floorPoints.empty()) {
            addChain(result, view, *family, view.viewIdentity + ":floor-levels", "floor-levels",
                floorPoints, height, unit, true, floorIdentities);
        }
        for(const auto* f:confirmed) {
            switch(f->role) {
            case SemanticRole::bodyContour: break;
            case SemanticRole::level:
                addLevels(result,*f,view,height,unit); break;
            case SemanticRole::planAxis:
                if(!f->fromConfirmedPlan) {issue(result,"PLAN_REQUIRED",u8"轴位链必须来自已确认的配套平面，不能从立面猜轴。",view.viewIdentity,f);break;}
                actualPlan=true;addChain(result,view,*family,f->identity,"plan-axis",f->points,width,unit);break;
            case SemanticRole::opening: {
                if(!f->confirmedRectangle) {issue(result,"RECTANGLE_CONFIRMATION_REQUIRED",u8"洞口矩形边界尚未确认。",view.viewIdentity,f);break;}
                if(!rectangle(*f,width,height)) {issue(result,"INVALID_OPENING_GEOMETRY",u8"洞口需要四个有效的正交矩形实际角点。",view.viewIdentity,f);break;}
                addChain(result,view,*family,f->identity,"opening-width",extremes(f->points,view.datum->position,width),width,unit);
                addChain(result,view,*family,f->identity,"opening-height",extremes(f->points,view.datum->position,height),height,unit);
                const auto openingLeft=*std::min_element(f->points.begin(),f->points.end(),[&](const SourcePoint& a,const SourcePoint& b){return projection(a.position,view.datum->position,width)<projection(b.position,view.datum->position,width);});
                const auto openingBottom=*std::min_element(f->points.begin(),f->points.end(),[&](const SourcePoint& a,const SourcePoint& b){return projection(a.position,view.datum->position,height)<projection(b.position,view.datum->position,height);});
                addChain(result,view,*family,f->identity,"opening-horizontal-location",{left,openingLeft},width,unit,false);
                addChain(result,view,*family,f->identity,"opening-vertical-location",{*view.datum,openingBottom},height,unit,false); break;
            }
            case SemanticRole::eave: case SemanticRole::ridge: case SemanticRole::localMoulding:
                addChain(result,view,*family,f->identity,"component-horizontal",extremes(f->points,view.datum->position,width),width,unit,false);
                addChain(result,view,*family,f->identity,"component-vertical",extremes(f->points,view.datum->position,height),height,unit,false);
                addChain(result,view,*family,f->identity,"component-horizontal-location",{left,f->points.front()},width,unit,false);
                addChain(result,view,*family,f->identity,"component-vertical-location",{*view.datum,f->points.front()},height,unit,false);
                addLevels(result,*f,view,height,unit);break;
            case SemanticRole::materialRegion: break;
            }
        }
        if(view.requirePlanAxes&&!actualPlan) {issue(result,"PLAN_REQUIRED",u8"缺少已确认的配套平面轴位；保留轴网待补诊断。",view.viewIdentity);}
    }
    for(const auto& f:model.rules.semanticFeatures) {
        if(f.role!=SemanticRole::materialRegion&&viewIds.find(f.viewIdentity)==viewIds.end()) {issue(result,"AMBIGUOUS_RULE_VIEW",u8"构件没有关联到唯一明确的视图。",f.viewIdentity,&f);}
    }
    return result;
}
}
