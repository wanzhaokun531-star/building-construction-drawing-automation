#include "MaterialRules.h"
#include <cmath>
#include <set>
#include <utility>

namespace bce {
namespace {
bool finite(Point2 p) { return std::isfinite(p.x)&&std::isfinite(p.y); }
bool same(Point2 a,Point2 b) { return a.x==b.x&&a.y==b.y; }
bool validSha(const std::string& sha) {
    return sha.size()==64&&std::all_of(sha.begin(),sha.end(),[](char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');});
}
void issue(Result<MaterialPlan>& r,const char* code,const char* message,const SemanticFeature* f=nullptr) {
    const auto entity=f&&!f->sources.empty()?std::optional<EntityKey>(f->sources.front()):std::nullopt;
    r.issues.push_back({code,message,{"",f?f->viewIdentity:"",entity},true});
}
std::vector<Point2> vertices(const ClosedRing& r) {
    auto v=r.vertices;if(v.size()>1&&same(v.front(),v.back())) {v.pop_back();}return v;
}
double cross(Point2 a,Point2 b,Point2 c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
bool onSegment(Point2 p,Point2 a,Point2 b) {
    return p.x>=(std::min)(a.x,b.x)&&p.x<=(std::max)(a.x,b.x)&&p.y>=(std::min)(a.y,b.y)&&p.y<=(std::max)(a.y,b.y);
}
bool intersects(Point2 a,Point2 b,Point2 c,Point2 d,bool& valid) {
    const auto ac=cross(a,b,c),ad=cross(a,b,d),ca=cross(c,d,a),cb=cross(c,d,b);
    if(!std::isfinite(ac)||!std::isfinite(ad)||!std::isfinite(ca)||!std::isfinite(cb)) {valid=false;return true;}
    if((ac==0&&onSegment(c,a,b))||(ad==0&&onSegment(d,a,b))||(ca==0&&onSegment(a,c,d))||(cb==0&&onSegment(b,c,d))) {return true;}
    return ((ac>0&&ad<0)||(ac<0&&ad>0))&&((ca>0&&cb<0)||(ca<0&&cb>0));
}
enum class Location { outside, inside, boundary };
Location locate(Point2 p,const std::vector<Point2>& ring,bool& valid) {
    int winding=0;
    for(std::size_t i=0;i<ring.size();++i) {
        const auto a=ring[i],b=ring[(i+1)%ring.size()];const auto side=cross(a,b,p);
        if(!std::isfinite(side)) {valid=false;return Location::outside;}
        if(side==0&&onSegment(p,a,b)) {return Location::boundary;}
        if(a.y<=p.y&&b.y>p.y&&side>0) {++winding;}
        if(a.y>p.y&&b.y<=p.y&&side<0) {--winding;}
    }
    return winding==0?Location::outside:Location::inside;
}
std::optional<double> ringArea(const ClosedRing& input) {
    if(!input.closed) {return std::nullopt;}const auto ring=vertices(input);
    if(ring.size()<3||std::any_of(ring.begin(),ring.end(),[](Point2 p){return !finite(p);})) {return std::nullopt;}
    bool valid=true;double area=0;
    for(std::size_t i=0;i<ring.size();++i) {
        const auto a=ring[i],b=ring[(i+1)%ring.size()];if(same(a,b)) {return std::nullopt;}
        // A translated fan avoids the catastrophic cancellation of global-coordinate shoelace sums.
        const auto contribution=cross(ring.front(),a,b)/2.0;
        area+=contribution;if(!std::isfinite(contribution)||!std::isfinite(area)) {return std::nullopt;}
        for(std::size_t j=i+1;j<ring.size();++j) {
            const auto c=ring[j],d=ring[(j+1)%ring.size()];
            if(j==i+1||(i==0&&j+1==ring.size())) {
                // Adjacent edges may share only their common endpoint, never reverse along one another.
                const auto previous=ring[(i+ring.size()-1)%ring.size()];
                const auto turn=cross(previous,a,b);if(!std::isfinite(turn)) {return std::nullopt;}
                if(turn==0&&(onSegment(previous,a,b)||onSegment(b,previous,a))) {return std::nullopt;}
                continue;
            }
            if(intersects(a,b,c,d,valid)||!valid) {return std::nullopt;}
        }
    }
    area=std::abs(area);if(area<=0||!std::isfinite(area)) {return std::nullopt;}return area;
}
bool ringsIntersect(const std::vector<Point2>& a,const std::vector<Point2>& b,bool& valid) {
    for(std::size_t i=0;i<a.size();++i) {for(std::size_t j=0;j<b.size();++j) {
        if(intersects(a[i],a[(i+1)%a.size()],b[j],b[(j+1)%b.size()],valid)) {return true;}
    }}return false;
}
std::optional<double> regionArea(const PolygonRegion& region,double unit) {
    const auto outerArea=ringArea(region.outer);if(!outerArea) {return std::nullopt;}
    const auto outer=vertices(region.outer);double area=*outerArea;bool valid=true;
    for(std::size_t i=0;i<region.holes.size();++i) {
        const auto holeArea=ringArea(region.holes[i]);if(!holeArea) {return std::nullopt;}
        const auto hole=vertices(region.holes[i]);
        if(ringsIntersect(outer,hole,valid)||!valid) {return std::nullopt;}
        for(const auto p:hole) {if(locate(p,outer,valid)!=Location::inside||!valid) {return std::nullopt;}}
        for(std::size_t j=0;j<i;++j) {
            const auto other=vertices(region.holes[j]);
            if(ringsIntersect(hole,other,valid)||!valid||locate(hole.front(),other,valid)!=Location::outside||
                locate(other.front(),hole,valid)!=Location::outside||!valid) {return std::nullopt;}
        }
        area-=*holeArea;if(!std::isfinite(area)||area<=0) {return std::nullopt;}
    }
    const auto squareMm=(area*unit)*unit;
    if(!std::isfinite(squareMm)||squareMm<=0) {return std::nullopt;}return squareMm;
}
std::vector<Point2> comparisonVertices(const ClosedRing& ring) {
    auto points = vertices(ring);
    bool changed = true;
    while (changed && points.size() > 3) {
        changed = false;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto previous = points[(i + points.size() - 1) % points.size()];
            const auto next = points[(i + 1) % points.size()];
            if (cross(previous, points[i], next) == 0 && onSegment(points[i], previous, next)) {
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(i)); changed = true; break;
            }
        }
    }
    return points;
}
bool sameRing(const ClosedRing& a,const ClosedRing& b) {
    const auto av=comparisonVertices(a),bv=comparisonVertices(b);if(av.size()!=bv.size()||av.empty()) {return false;}
    for(std::size_t offset=0;offset<bv.size();++offset) {
        bool forward=true,reverse=true;
        for(std::size_t i=0;i<av.size();++i) {forward=forward&&same(av[i],bv[(offset+i)%bv.size()]);reverse=reverse&&same(av[i],bv[(offset+bv.size()-i)%bv.size()]);}
        if(forward||reverse) {return true;}
    }return false;
}
bool sameRegion(const PolygonRegion& a,const PolygonRegion& b) {
    if(!sameRing(a.outer,b.outer)||a.holes.size()!=b.holes.size()) {return false;}
    std::vector<bool> used(b.holes.size(),false);
    for(const auto& hole:a.holes) {
        bool found=false;for(std::size_t i=0;i<b.holes.size();++i) {if(!used[i]&&sameRing(hole,b.holes[i])) {used[i]=true;found=true;break;}}
        if(!found) {return false;}
    }return true;
}
const ReferenceFamily* familyFor(const RuleView& view,const ReferenceStandard& standard) {
    if (view.familyId.empty()) { return nullptr; }
    const ReferenceFamily* match=nullptr;
    for(const auto& f:standard.families) {if(f.identity==view.familyId) {if(match) {return nullptr;}match=&f;}}
    if(!match||!match->approved||match->approvalSource.empty()||
        std::find(match->roles.begin(),match->roles.end(),SemanticRole::materialRegion)==match->roles.end()) {return nullptr;}return match;
}
const MaterialPrototype* prototypeFor(const std::string& id,const ReferenceStandard& standard) {
    const MaterialPrototype* match=nullptr;
    for(const auto& p:standard.materialPrototypes) {if(p.expression.prototypeId==id) {if(match) {return nullptr;}match=&p;}}
    if(!match||id.empty()||!match->approved||match->approvalSource.empty()||!finite(match->expression.direction)||
        std::hypot(match->expression.direction.x,match->expression.direction.y)<=0||
        !std::isfinite(std::hypot(match->expression.direction.x,match->expression.direction.y))||
        !std::isfinite(match->expression.scale)||match->expression.scale<=0) {return nullptr;}return match;
}
struct MaterialDefinition {std::string key,number,name;std::string prototypeId;};
}

Result<MaterialPlan> planMaterials(const FeatureModel& model,const ReferenceStandard& standard,const ProjectConfig& config) {
    Result<MaterialPlan> result;result.value.emplace();result.value->referenceSha256=standard.referenceSha256;
    result.value->nativeCapabilities=standard.nativeCapabilities;
    if(!validSha(standard.referenceSha256)) {issue(result,"REFERENCE_STANDARD_REQUIRED",u8"参考标准缺少有效的源SHA256绑定。");}
    const auto units=resolveUnits(config.units);if(!units.canCommit()) {result.issues.insert(result.issues.end(),units.issues.begin(),units.issues.end());return result;}
    result.value->mmPerInputUnit=units.value->mmPerInputUnit;
    result.value->unitDeclaration=config.units;
    result.value->unitConfirmationSource=config.unitConfirmationSource;
    std::vector<MaterialDefinition> definitions={{"dark-stone","M1",u8"深灰色仿石漆",""},{"ivory-stone","M2",u8"米白色仿石漆",""},{"dark-roof","M3",u8"深灰色双波陶瓷瓦",""}};
    std::set<std::string> overrideKeys;
    for(const auto& o:model.rules.materialOverrides) {
        if(o.key.empty()||o.number.empty()||o.name.empty()||!overrideKeys.insert(o.key).second) {issue(result,"INVALID_MATERIAL_OVERRIDE",u8"材料覆盖需要唯一键、编号和名称。");continue;}
        auto found=std::find_if(definitions.begin(),definitions.end(),[&](const MaterialDefinition& d){return d.key==o.key;});
        const MaterialDefinition definition{o.key,o.number,o.name,o.prototypeId.value_or("")};
        if(found==definitions.end()) {definitions.push_back(definition);}else {*found=definition;}
    }
    std::set<std::string> viewIds;std::set<std::string> ambiguousViews;
    for(const auto& v:model.rules.views) {if(v.viewIdentity.empty()||!viewIds.insert(v.viewIdentity).second) {ambiguousViews.insert(v.viewIdentity);}}
    std::set<std::pair<std::string,std::string>> regionIds;std::size_t regionCount=0;
    for(const auto& f:model.rules.semanticFeatures) {
        if(f.role!=SemanticRole::materialRegion) {continue;}++regionCount;
        if(!f.confirmed||f.confirmationSource.empty()) {issue(result,"SEMANTIC_CONFIRMATION_REQUIRED",u8"材料区域必须有目标项目确认，不能复制参考绝对区域。",&f);continue;}
        if(f.identity.empty()||!regionIds.emplace(f.viewIdentity,f.identity).second) {issue(result,"DUPLICATE_MATERIAL_REGION",u8"材料区域ID为空或重复。",&f);continue;}
        const auto view=std::find_if(model.rules.views.begin(),model.rules.views.end(),[&](const RuleView& v){return v.viewIdentity==f.viewIdentity;});
        if(view==model.rules.views.end()||ambiguousViews.count(f.viewIdentity)!=0) {issue(result,"MATERIAL_ASSOCIATION_REQUIRED",u8"材料区域需要关联唯一明确的视图。",&f);continue;}
        if(!familyFor(*view,standard)) {issue(result,"REFERENCE_FAMILY_REQUIRED",u8"材料表达需要明确选择已按角色核准的参考排法。",&f);continue;}
        if((f.tableViewIdentity.empty()&&f.sharedTableIdentity.empty())||(!f.tableViewIdentity.empty()&&f.tableViewIdentity!=f.viewIdentity)||
            (!f.tableViewIdentity.empty()&&!f.sharedTableIdentity.empty())) {issue(result,"MATERIAL_ASSOCIATION_REQUIRED",u8"材料表和图例必须唯一关联本视图或明确的共享表ID。",&f);continue;}
        if(f.sources.empty()||config.databaseIdentity.empty()||std::any_of(f.sources.begin(),f.sources.end(),[&](const EntityKey& k){return k.databaseIdentity!=config.databaseIdentity||!(k==k);})) {
            issue(result,"MATERIAL_SOURCE_REQUIRED",u8"材料区域缺少有效的目标源对象身份。",&f);continue;
        }
        if(!f.region) {issue(result,"MATERIAL_REGION_REQUIRED",u8"缺少明确确认的外环及洞口边界。",&f);continue;}
        const auto area=regionArea(*f.region,units.value->mmPerInputUnit);
        if(!area) {issue(result,"INVALID_MATERIAL_TOPOLOGY",u8"仅支持有限、简单闭合外环和严格内部不相交孔洞；请修正开放、自交、触边或数值溢出区域。",&f);continue;}
        if(std::any_of(result.value->regions.begin(),result.value->regions.end(),[&](const MaterialRegionPlan& r){return r.viewIdentity==f.viewIdentity&&sameRegion(r.geometry,*f.region);})) {
            issue(result,"DUPLICATE_MATERIAL_REGION",u8"同一视图存在几何重复的材料区域。",&f);continue;
        }
        const auto definition=std::find_if(definitions.begin(),definitions.end(),[&](const MaterialDefinition& d){return d.key==f.materialKey;});
        if(definition==definitions.end()) {issue(result,"MATERIAL_DEFINITION_REQUIRED",u8"目标材料键没有明确配置。",&f);continue;}
        const auto& prototypeId=definition->prototypeId.empty()?f.materialPrototypeId:definition->prototypeId;
        const auto* prototype=prototypeFor(prototypeId,standard);
        if(!prototype) {issue(result,"MATERIAL_PROTOTYPE_REQUIRED",u8"材料表达必须绑定已核准的源原型ID、方向及比例。",&f);continue;}
        MaterialRegionPlan region;region.identity=f.identity;region.viewIdentity=f.viewIdentity;
        region.tableViewIdentity=f.tableViewIdentity;region.sharedTableIdentity=f.sharedTableIdentity;
        region.sources=f.sources;region.confirmationSource=f.confirmationSource;region.geometry=*f.region;region.areaSquareMm=*area;
        region.materialNumber=definition->number;region.materialName=definition->name;region.expression=prototype->expression;
        region.familyId=view->familyId;region.prototypeApprovalSource=prototype->approvalSource;
        if(f.requestConstructionNote) {
            if(f.construction.empty()||f.detailId.empty()) {issue(result,"CONSTRUCTION_DETAIL_REQUIRED",u8"构造注记需要目标项目的实际做法及详图编号。",&f);}
            else {region.constructionNote=f.construction+" / "+f.detailId;}
        }
        result.value->regions.push_back(std::move(region));
    }
    if(regionCount==0) {issue(result,"MATERIAL_REGION_REQUIRED",u8"尚无已确认材料区域，不能从参考复制绝对边界。");}
    return result;
}

bool materialContains(const MaterialRegionPlan& region,Point2 point) {
    if(!finite(point)) {return false;}const auto outer=vertices(region.geometry.outer);if(outer.size()<3) {return false;}
    bool valid=true;if(locate(point,outer,valid)==Location::outside||!valid) {return false;}
    for(const auto& h:region.geometry.holes) {const auto hole=vertices(h);if(hole.size()<3||locate(point,hole,valid)!=Location::outside||!valid) {return false;}}
    return true;
}
}
