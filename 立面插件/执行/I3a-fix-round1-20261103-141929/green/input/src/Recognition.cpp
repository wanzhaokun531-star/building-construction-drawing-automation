#include "Recognition.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <regex>
#include <sstream>

namespace bce {
namespace {
bool finite(Point2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool finite(Bounds2 b) {
    return finite(b.minimum) && finite(b.maximum) && b.minimum.x <= b.maximum.x && b.minimum.y <= b.maximum.y;
}
Issue issue(const SceneSnapshot& scene, const char* code, std::string message,
    std::optional<EntityKey> entity = std::nullopt, std::string view = {}, bool blocking = true) {
    return {code, std::move(message), {scene.documentIdentity, std::move(view), std::move(entity)}, blocking};
}
Issue transformIssue() { return {diagnostic::invalidTransform, "变换非有限、奇异、非二维，或当前几何值类型无法忠实表达变换结果。", {}, true}; }
double determinant(const Affine2& a) { return a.xx*a.yy-a.xy*a.yx; }
bool valid(const Affine2& a) {
    if (!a.planar || !std::isfinite(a.xx) || !std::isfinite(a.xy) || !std::isfinite(a.yx) ||
        !std::isfinite(a.yy) || !std::isfinite(a.tx) || !std::isfinite(a.ty)) { return false; }
    const double scale = (std::max)({std::abs(a.xx),std::abs(a.xy),std::abs(a.yx),std::abs(a.yy)});
    const double det = determinant(a);
    return scale > 0 && std::isfinite(det) && std::abs(det) > std::numeric_limits<double>::epsilon()*scale*scale*64;
}
Point2 apply(const Affine2& a, Point2 p) { return {a.xx*p.x+a.xy*p.y+a.tx, a.yx*p.x+a.yy*p.y+a.ty}; }
Point2 vectorApply(const Affine2& a, Point2 p) { return {a.xx*p.x+a.xy*p.y, a.yx*p.x+a.yy*p.y}; }
double norm(Point2 p) { return std::hypot(p.x,p.y); }
std::optional<Point2> unitDirection(Point2 direction) {
    if(!finite(direction)) { return std::nullopt; }
    const double scale=(std::max)(std::abs(direction.x),std::abs(direction.y));
    if(scale==0) { return std::nullopt; }
    const Point2 scaled{direction.x/scale,direction.y/scale};
    const double length=norm(scaled);
    if(!std::isfinite(length)||length==0) { return std::nullopt; }
    return Point2{scaled.x/length,scaled.y/length};
}
std::optional<double> associatedHeightMm(const LevelAssociation& a) {
    const auto direction=unitDirection(a.heightDirection);
    if(!direction || !finite(a.lowerPoint) || !finite(a.upperPoint) || !std::isfinite(a.mmPerInputUnit) || a.mmPerInputUnit<=0) { return std::nullopt; }
    const Point2 difference{a.upperPoint.x-a.lowerPoint.x,a.upperPoint.y-a.lowerPoint.y};
    if(!finite(difference)) { return std::nullopt; }
    const double projected=difference.x*direction->x+difference.y*direction->y;
    if(!std::isfinite(projected)) { return std::nullopt; }
    const double measured=projected*a.mmPerInputUnit;
    return std::isfinite(measured) ? std::optional<double>{measured} : std::nullopt;
}
bool similarity(const Affine2& a) {
    const double s1 = std::hypot(a.xx,a.yx), s2 = std::hypot(a.xy,a.yy);
    return std::abs(s1-s2) <= s1*1e-10 && std::abs(a.xx*a.xy+a.yx*a.yy) <= s1*s2*1e-10;
}
Bounds2 unionBounds(Bounds2 a, Bounds2 b) {
    return {{(std::min)(a.minimum.x,b.minimum.x),(std::min)(a.minimum.y,b.minimum.y)},
        {(std::max)(a.maximum.x,b.maximum.x),(std::max)(a.maximum.y,b.maximum.y)}};
}
Bounds2 movedBounds(Bounds2 b, const Affine2& a) {
    const std::array<Point2,4> corners{{b.minimum,{b.minimum.x,b.maximum.y},b.maximum,{b.maximum.x,b.minimum.y}}};
    const auto initial = apply(a,corners.front());
    Bounds2 result{initial,initial};
    for (const auto p : corners) { const auto q = apply(a,p); result=unionBounds(result,{q,q}); }
    return result;
}
std::string canonicalHandle(std::string handle) {
    for (auto& c : handle) { if (c >= 'a' && c <= 'f') { c = static_cast<char>(c-'a'+'A'); } }
    const auto start = handle.find_first_not_of('0');
    return start == std::string::npos ? "0" : handle.substr(start);
}
std::optional<ViewKind> titleKind(const Primitive2& p) {
    if (!p.effectiveVisible || p.kind != PrimitiveKind::text || !p.text) { return std::nullopt; }
    const bool elevation = p.text->find("立面") != std::string::npos || p.text->find("elevation") != std::string::npos;
    const bool section = p.text->find("剖面") != std::string::npos || p.text->find("section") != std::string::npos;
    if (!elevation && !section) { return std::nullopt; }
    return elevation == section ? ViewKind::ambiguous : elevation ? ViewKind::elevation : ViewKind::section;
}
bool isLevelLabel(const Primitive2& p) {
    return p.text && (p.text->find("层") != std::string::npos || p.text->find("标高") != std::string::npos || p.text->find("level") != std::string::npos);
}
std::optional<double> levelMm(const Primitive2& p) {
    if (!isLevelLabel(p)) { return std::nullopt; }
    static const std::regex declaration("([+-]?[0-9]+(?:\\.[0-9]+)?)\\s*(mm|m)(?:\\s|$)");
    std::smatch match;
    if (!std::regex_search(*p.text,match,declaration)) { return std::nullopt; }
    std::istringstream number(match[1].str()); number.imbue(std::locale::classic());
    double parsed=0;
    if(!(number >> parsed)) { return std::nullopt; }
    const double value=parsed*(match[2].str()=="m" ? 1000.0 : 1.0);
    return std::isfinite(value) ? std::optional<double>{value} : std::nullopt;
}
void appendString(std::ostream& out, const std::string& s) { out << s.size() << ':' << s; }
void appendPoint(std::ostream& out, const std::optional<Point2>& p) {
    out << (p ? "p" : "-"); if (p) { out << p->x << ',' << p->y << ';'; }
}
void appendNumber(std::ostream& out, const std::optional<double>& n) { out << (n ? "n" : "-"); if (n) { out << *n << ';'; } }
std::string signature(const SceneSnapshot& scene, std::vector<std::size_t> members) {
    std::sort(members.begin(),members.end(),[&](std::size_t a,std::size_t b) {
        return canonicalHandle(scene.primitives[a].key.handle) < canonicalHandle(scene.primitives[b].key.handle);
    });
    // Full canonical value, rather than a lossy hash: equality cannot hide a changed vertex.
    std::ostringstream out; out.imbue(std::locale::classic()); out << "bce-geometry-v2;" << std::hexfloat;
    for (const auto index : members) {
        const auto& p=scene.primitives[index]; appendString(out,canonicalHandle(p.key.handle));
        out << static_cast<int>(p.kind) << ';' << p.topologyComplete << ';';
        appendString(out,p.layer);
        if (p.bounds) { out << 'b' << p.bounds->minimum.x << ',' << p.bounds->minimum.y << ',' << p.bounds->maximum.x << ',' << p.bounds->maximum.y << ';'; }
        else { out << '-'; }
        appendPoint(out,p.start); appendPoint(out,p.end); appendPoint(out,p.textPosition);
        appendNumber(out,p.textHeight); appendNumber(out,p.textRotation); appendNumber(out,p.dimensionMeasurement);
        out << p.textMirroredX << ',' << p.textMirroredY << ';';
        out << (p.text ? "t" : "-"); if (p.text) { appendString(out,*p.text); }
        if (p.arc) { out << 'a' << p.arc->center.x << ',' << p.arc->center.y << ',' << p.arc->radius << ',' << p.arc->startAngle << ',' << p.arc->sweepAngle << ';'; }
        else { out << '-'; }
        if(!p.nativeGeometrySignature.empty() || !p.sourceRootHandle.empty() || !p.instancePath.empty() || !p.effectiveVisible) {
            out << "native;"; appendString(out,p.sourceRootHandle); appendString(out,p.instancePath);
            appendString(out,p.nativeGeometrySignature); out << p.effectiveVisible << ';';
        }
    }
    return out.str();
}
struct Located { std::size_t primitive; Bounds2 local; bool title; };
struct Gap { bool exists{false}; bool alongX{true}; double boundary{0}; double score{0}; };
Gap chooseGap(const std::vector<Located>& group) {
    Gap best;
    const auto titleCount=std::count_if(group.begin(),group.end(),[](const Located& p){return p.title;});
    for (const bool alongX : {true,false}) {
        auto sorted=group;
        const auto low=[alongX](const Located& p){return alongX ? p.local.minimum.x : p.local.minimum.y;};
        const auto high=[alongX](const Located& p){return alongX ? p.local.maximum.x : p.local.maximum.y;};
        std::sort(sorted.begin(),sorted.end(),[&](const Located& a,const Located& b){return low(a)<low(b);});
        double end=high(sorted.front()), maximum=end;
        for(const auto& p:sorted) { maximum=(std::max)(maximum,high(p)); }
        const double span=maximum-low(sorted.front());
        if(!std::isfinite(span)||span<=0) { continue; }
        auto leftTitles=sorted.front().title ? std::ptrdiff_t{1} : std::ptrdiff_t{0};
        for(std::size_t i=1;i<sorted.size();++i) {
            const double start=low(sorted[i]);
            const double tolerance=std::numeric_limits<double>::epsilon()*(std::max)({1.0,std::abs(end),std::abs(start)})*64;
            if(start>end+tolerance && leftTitles>0 && leftTitles<titleCount && span>0) {
                const double width=start-end;
                const double boundary=end+width/2;
                const double score=width/span;
                if(std::isfinite(width) && std::isfinite(boundary) && std::isfinite(score) &&
                    boundary>end && boundary<start && (!best.exists || score>best.score)) {
                    best={true,alongX,boundary,score};
                }
            }
            end=(std::max)(end,high(sorted[i]));
            if(sorted[i].title) { ++leftTitles; }
        }
    }
    return best;
}
void markUnassigned(const SceneSnapshot& scene, const std::vector<Located>& group, AnalysisResult& result, const char* message) {
    for(const auto& p:group) {
        const auto& key=scene.primitives[p.primitive].key;
        result.unassigned.push_back(key); result.preserved.push_back(key);
        result.issues.push_back(issue(scene,diagnostic::ambiguousFeature,message,key));
    }
}
void partition(const SceneSnapshot& scene, const std::vector<Located>& group, AnalysisResult& result) {
    if(group.empty()) { return; }
    const auto count=std::count_if(group.begin(),group.end(),[](const Located& p){return p.title;});
    if(count==0) { markUnassigned(scene,group,result,"没有标题依据，保留为未归属内容。"); return; }
    if(count>1) {
        const auto gap=chooseGap(group);
        if(gap.exists) {
            std::vector<Located> left,right;
            for(const auto& p:group) { ((gap.alongX ? p.local.maximum.x : p.local.maximum.y)<gap.boundary ? left : right).push_back(p); }
            if(left.empty() || right.empty() || left.size()>=group.size() || right.size()>=group.size()) {
                markUnassigned(scene,group,result,"空白带无法形成严格缩小的两个非空分区，保留全部内容待确认。"); return;
            }
            partition(scene,left,result); partition(scene,right,result); return;
        }
        markUnassigned(scene,group,result,"多个标题占位竞争且无可证二维空白带，不能强制划分。");
        return;
    }
    const auto at=std::find_if(group.begin(),group.end(),[](const Located& p){return p.title;});
    const auto& title=scene.primitives[at->primitive];
    View view; view.titleKey=title.key; view.identity="view:"+canonicalHandle(title.key.handle); view.kind=*titleKind(title);
    if(view.kind==ViewKind::ambiguous) { markUnassigned(scene,group,result,"标题同时声明立面和剖面，需要确认。"); return; }
    view.bounds=*scene.primitives[group.front().primitive].bounds;
    std::vector<std::size_t> indices;
    Feature unresolved; unresolved.kind="unresolvedGeometry";
    for(const auto& p:group) {
        const auto& primitive=scene.primitives[p.primitive];
        view.members.push_back(primitive.key); indices.push_back(p.primitive);
        if(primitive.kind!=PrimitiveKind::text) { unresolved.sources.push_back(primitive.key); }
        view.bounds=unionBounds(view.bounds,*primitive.bounds);
        if(view.kind==ViewKind::section) { result.preserved.push_back(primitive.key); }
        if(!primitive.topologyComplete) {
            view.unconfirmed.push_back(issue(scene,diagnostic::ambiguousFeature,"仅有可见bounds，来源拓扑尚未完整读取；不能认定边界完整。",primitive.key,view.identity));
        }
    }
    if(!unresolved.sources.empty()) {
        // A title/blank-band partition proves spatial membership, not the ownership or
        // role of every detached object. Keep even distant extras explicitly pending.
        result.model.features.push_back(std::move(unresolved));
        view.unconfirmed.push_back(issue(scene,diagnostic::ambiguousFeature,"空间分区中的几何构件（含孤立额外内容）角色仍为候选；不能据此删除或认作门窗。",title.key,view.identity));
    }
    view.geometrySignature=signature(scene,std::move(indices));
    result.views.push_back(std::move(view));
}
bool samePoint(Point2 a,Point2 b) {
    const double scale=(std::max)({1.0,std::abs(a.x),std::abs(a.y),std::abs(b.x),std::abs(b.y)});
    return norm({a.x-b.x,a.y-b.y}) <= std::numeric_limits<double>::epsilon()*scale*64;
}
void rectangles(const SceneSnapshot& scene,AnalysisResult& result) {
    std::vector<std::size_t> lines;
    for(std::size_t i=0;i<scene.primitives.size();++i) {
        const auto& p=scene.primitives[i];
        if(p.effectiveVisible && p.kind==PrimitiveKind::line && p.topologyComplete && p.start && p.end && finite(*p.start) && finite(*p.end) && !samePoint(*p.start,*p.end)) { lines.push_back(i); }
    }
    std::vector<bool> visited(lines.size(),false);
    for(std::size_t root=0;root<lines.size();++root) {
        if(visited[root]) { continue; }
        std::vector<std::size_t> component{root}; visited[root]=true;
        for(std::size_t at=0;at<component.size();++at) {
            const auto& p=scene.primitives[lines[component[at]]];
            for(std::size_t j=0;j<lines.size();++j) {
                if(visited[j]) { continue; } const auto& q=scene.primitives[lines[j]];
                if(samePoint(*p.start,*q.start)||samePoint(*p.start,*q.end)||samePoint(*p.end,*q.start)||samePoint(*p.end,*q.end)) { visited[j]=true; component.push_back(j); }
            }
        }
        if(component.size()!=4) { continue; }
        std::vector<Point2> vertices; std::vector<std::vector<Point2>> directions;
        Feature rectangle; rectangle.kind="rectangle";
        for(const auto index:component) {
            const auto& p=scene.primitives[lines[index]]; rectangle.sources.push_back(p.key);
            for(const bool first:{true,false}) {
                const auto a=first ? *p.start : *p.end, b=first ? *p.end : *p.start;
                const auto v=std::find_if(vertices.begin(),vertices.end(),[&](Point2 point){return samePoint(point,a);});
                const auto vi=static_cast<std::size_t>(std::distance(vertices.begin(),v));
                if(v==vertices.end()) { vertices.push_back(a); directions.emplace_back(); }
                directions[vi].push_back({b.x-a.x,b.y-a.y});
            }
        }
        bool rectangular=vertices.size()==4;
        for(const auto& d:directions) {
            if(d.size()!=2 || std::abs(d[0].x*d[1].x+d[0].y*d[1].y)>norm(d[0])*norm(d[1])*1e-10) { rectangular=false; }
        }
        if(!rectangular) { continue; }
        result.model.features.push_back(rectangle);
        for(const auto& k:rectangle.sources) { result.issues.push_back(issue(scene,diagnostic::ambiguousFeature,"闭合矩形仅为几何候选；缺少洞口角色及配套平面确认，不能认定为窗。",k)); }
    }
}
bool contains(const View& view,const EntityKey& key) { return std::find(view.members.begin(),view.members.end(),key)!=view.members.end(); }
void levels(const SceneSnapshot& scene,View& view,AnalysisResult& result) {
    for(const auto& p:scene.primitives) {
        if(!p.effectiveVisible || !contains(view,p.key)) { continue; }
        if(!isLevelLabel(p)) { continue; }
        const auto declared=levelMm(p);
        Feature level; level.kind="level"; level.sources={p.key}; level.declaredMm=declared;
        if(!declared) {
            view.unconfirmed.push_back(issue(scene,diagnostic::ambiguousFeature,"楼层文字缺少可读取的有限数值及单位，保留候选，不填零。",p.key,view.identity));
            view.levelCandidates.push_back(std::move(level)); continue;
        }
        const auto assoc=std::find_if(scene.levelAssociations.begin(),scene.levelAssociations.end(),[&](const LevelAssociation& a){return a.declarationText==p.key;});
        if(assoc!=scene.levelAssociations.end() && contains(view,assoc->lowerGeometry) && contains(view,assoc->upperGeometry)) {
            level.sources.push_back(assoc->lowerGeometry); level.sources.push_back(assoc->upperGeometry);
            level.geometryMm=associatedHeightMm(*assoc);
            if(!level.geometryMm) {
                auto unread=issue(scene,diagnostic::ambiguousFeature,"关联几何高差或方向无法有限表达（"+assoc->lowerGeometry.handle+","+assoc->upperGeometry.handle+"）；保留未读取几何候选。",p.key,view.identity);
                view.unconfirmed.push_back(unread); result.issues.push_back(std::move(unread));
            } else if(std::abs(*level.geometryMm-*declared)>1e-7) {
                std::ostringstream values; values.imbue(std::locale::classic()); values << std::setprecision(17) << "已关联几何高差 " << *level.geometryMm << " mm（" << assoc->lowerGeometry.handle << "," << assoc->upperGeometry.handle << "），文本声明 " << *declared << " mm（" << p.key.handle << "）；需确认。";
                auto conflict=issue(scene,diagnostic::levelConflict,values.str(),p.key,view.identity);
                view.unconfirmed.push_back(conflict); result.issues.push_back(std::move(conflict));
            }
        } else {
            view.unconfirmed.push_back(issue(scene,diagnostic::ambiguousFeature,"楼层文字仅为候选，尚无完整几何基准关联。",p.key,view.identity));
        }
        view.levelCandidates.push_back(std::move(level));
    }
}
}

Result<Affine2> compose(const Affine2& outer,const Affine2& inner) {
    if(!valid(outer)||!valid(inner)) { return {std::nullopt,{transformIssue()}}; }
    const Affine2 result{outer.xx*inner.xx+outer.xy*inner.yx,outer.xx*inner.xy+outer.xy*inner.yy,
        outer.yx*inner.xx+outer.yy*inner.yx,outer.yx*inner.xy+outer.yy*inner.yy,
        outer.xx*inner.tx+outer.xy*inner.ty+outer.tx,outer.yx*inner.tx+outer.yy*inner.ty+outer.ty,true};
    if(!valid(result)) { return {std::nullopt,{transformIssue()}}; }
    return {result,{}};
}
Result<Primitive2> transformed(const Primitive2& primitive,const Affine2& a) {
    if(!valid(a)) { return {std::nullopt,{transformIssue()}}; }
    auto p=primitive;
    if(p.kind==PrimitiveKind::text && !similarity(a)) { return {std::nullopt,{transformIssue()}}; }
    if(p.bounds) { if(!finite(*p.bounds)) { return {std::nullopt,{transformIssue()}}; } p.bounds=movedBounds(*p.bounds,a); if(!finite(*p.bounds)) { return {std::nullopt,{transformIssue()}}; } }
    for(auto* point:{&p.start,&p.end,&p.textPosition}) { if(*point) { **point=apply(a,**point); if(!finite(**point)) { return {std::nullopt,{transformIssue()}}; } } }
    if(p.textRotation) {
        if(!std::isfinite(*p.textRotation)||!similarity(a)) { return {std::nullopt,{transformIssue()}}; }
        const auto v=vectorApply(a,{std::cos(*p.textRotation),std::sin(*p.textRotation)}); p.textRotation=std::atan2(v.y,v.x);
        // A*R = R' * scale * diag(1, sign(det A)). Preserve the existing
        // glyph-X sign and accumulate the reflection on glyph Y, so both bases
        // remain correct through nested rotations and reflections.
        if(determinant(a)<0) { p.textMirroredY=!p.textMirroredY; }
    } else if(p.kind==PrimitiveKind::text && determinant(a)<0) {
        return {std::nullopt,{transformIssue()}}; // Missing orientation cannot be invented.
    }
    if(p.textHeight) {
        if(!std::isfinite(*p.textHeight)||!similarity(a)) { return {std::nullopt,{transformIssue()}}; }
        *p.textHeight*=std::hypot(a.xx,a.yx);
        if(!std::isfinite(*p.textHeight)) { return {std::nullopt,{transformIssue()}}; }
    }
    if(p.arc) {
        if(!similarity(a)||!finite(p.arc->center)||!std::isfinite(p.arc->radius)||p.arc->radius<=0 || !std::isfinite(p.arc->startAngle)||!std::isfinite(p.arc->sweepAngle)) { return {std::nullopt,{transformIssue()}}; }
        const auto v=vectorApply(a,{std::cos(p.arc->startAngle),std::sin(p.arc->startAngle)});
        p.arc->center=apply(a,p.arc->center); p.arc->radius*=std::hypot(a.xx,a.yx); p.arc->startAngle=std::atan2(v.y,v.x);
        if(determinant(a)<0) { p.arc->sweepAngle=-p.arc->sweepAngle; }
        if(!finite(p.arc->center)||!std::isfinite(p.arc->radius)) { return {std::nullopt,{transformIssue()}}; }
    }
    return {p,{}};
}
Result<AxisChain> projectAxes(const PlanSnapshot& plan,Point2 direction) {
    const auto failure=[](const char* code,const char* message) { return Result<AxisChain>{std::nullopt,{{code,message,{},true}}}; };
    if(plan.axes.empty()||plan.databaseIdentity.empty()) { return failure(diagnostic::planRequired,"缺少本项目配套平面，不能从参考轴号建立映射。"); }
    if(!plan.confirmedMmPerInputUnit || !std::isfinite(*plan.confirmedMmPerInputUnit)||*plan.confirmedMmPerInputUnit<=0) { return failure(diagnostic::unitRequired,"平面轴位单位需确认。"); }
    const auto normalized=unitDirection(direction);
    if(!normalized) { return failure(diagnostic::ambiguousFeature,"轴投影方向无效。"); }
    direction=*normalized;
    std::vector<std::pair<double,const Axis*>> axes;
    for(const auto& axis:plan.axes) {
        if(!finite(axis.position)||axis.label.empty()) { return failure(diagnostic::ambiguousFeature,"平面轴位或编号缺失。"); }
        const double station=(axis.position.x*direction.x+axis.position.y*direction.y)* *plan.confirmedMmPerInputUnit;
        if(!std::isfinite(station)) { return failure(diagnostic::ambiguousFeature,"投影轴位非有限。"); }
        axes.emplace_back(station,&axis);
    }
    std::sort(axes.begin(),axes.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    AxisChain chain;
    for(std::size_t i=0;i<axes.size();++i) {
        chain.labels.push_back(axes[i].second->label); chain.sources.push_back(axes[i].second->key);
        if(i>0) {
            const double spacing=axes[i].first-axes[i-1].first;
            if(!std::isfinite(spacing)) { return failure(diagnostic::ambiguousFeature,"轴投影间距无法有限表达，不能接受此轴链。"); }
            if(spacing<=0) { return failure(diagnostic::ambiguousFeature,"轴投影重合，需要人工确认映射。"); }
            chain.spacingsMm.push_back(spacing);
        }
    }
    return {chain,{}};
}
AnalysisResult analyse(const SceneSnapshot& scene,const PlanSnapshot& plan,const ProjectConfig& config) {
    AnalysisResult result; result.sourceSha256=scene.sourceSha256;
    std::vector<Located> group; std::optional<double> direction; bool consistent=true;
    for(const auto& p:scene.primitives) {
        if(titleKind(p)) {
            if(!p.textRotation||!std::isfinite(*p.textRotation)) { consistent=false; }
            else if(!direction) { direction=*p.textRotation; }
            else if(std::abs(std::sin(*p.textRotation-*direction))>1e-10 || std::cos(*p.textRotation-*direction)<0) { consistent=false; }
        }
    }
    const double angle=direction.value_or(0);
    const Affine2 local{std::cos(angle),std::sin(angle),-std::sin(angle),std::cos(angle),0,0,true};
    for(std::size_t i=0;i<scene.primitives.size();++i) {
        const auto& p=scene.primitives[i];
        if(!p.bounds||!finite(*p.bounds)) {
            result.unassigned.push_back(p.key); result.preserved.push_back(p.key);
            result.issues.push_back(issue(scene,diagnostic::ambiguousFeature,"缺失或非有限的实际bounds，保留且不参与空间断言。",p.key)); continue;
        }
        const auto localBounds=movedBounds(*p.bounds,local);
        if(!finite(localBounds)) {
            result.unassigned.push_back(p.key); result.preserved.push_back(p.key);
            result.issues.push_back(issue(scene,diagnostic::ambiguousFeature,"标题局部坐标bounds无法有限表示，保留且不参与空间断言。",p.key)); continue;
        }
        group.push_back({i,localBounds,titleKind(p).has_value()});
    }
    if(!consistent) { markUnassigned(scene,group,result,"标题方向未读取或互不一致，不能在单一局部坐标中明确分组。"); }
    else { partition(scene,group,result); }
    rectangles(scene,result);
    const bool missingPlan=plan.axes.empty()||plan.databaseIdentity.empty();
    if(missingPlan) { result.issues.push_back(issue(scene,diagnostic::planRequired,"没有本项目配套平面；轴号及轴位映射待补，禁止借用参考平面。")); }
    for(auto& view:result.views) {
        levels(scene,view,result);
        if(missingPlan) { view.unconfirmed.push_back(issue(scene,diagnostic::planRequired,"配套平面缺失，轴映射未确认。",view.titleKey,view.identity)); }
        else {
            const auto binding=std::find_if(config.views.begin(),config.views.end(),[&](const ViewConfig& v){return v.viewIdentity==view.identity;});
            if(config.databaseIdentity==scene.databaseIdentity && binding!=config.views.end() && binding->geometrySignature==view.geometrySignature && binding->axisMapping.confirmed && !binding->axisMapping.source.empty() && binding->axisDirection) {
                const auto projected=projectAxes(plan,*binding->axisDirection);
                if(projected.canCommit()) { view.projectedAxes=projected.value; }
                else { view.unconfirmed.insert(view.unconfirmed.end(),projected.issues.begin(),projected.issues.end()); result.issues.insert(result.issues.end(),projected.issues.begin(),projected.issues.end()); }
            } else { view.unconfirmed.push_back(issue(scene,diagnostic::ambiguousFeature,"轴方向及本项目平面映射尚未确认。",view.titleKey,view.identity)); }
        }
    }
    return result;
}
Result<ProjectConfig> validateConfig(const SceneSnapshot& scene,const ProjectConfig& config) {
    if(config.databaseIdentity.empty()||config.databaseIdentity!=scene.databaseIdentity) {
        return {std::nullopt,{issue(scene,diagnostic::staleDocument,"配置属于其他数据库或缺少数据库身份，不能沿用确认。")}};
    }
    auto validated=config; Result<ProjectConfig> result;
    const auto current=analyse(scene,{},{});
    for(auto& v:validated.views) {
        const auto found=std::find_if(current.views.begin(),current.views.end(),[&](const View& view){return view.identity==v.viewIdentity;});
        if(found==current.views.end()||found->geometrySignature!=v.geometrySignature) {
            v.material.confirmed=false; v.axisMapping.confirmed=false;
            result.issues.push_back(issue(scene,diagnostic::configChanged,"此视图几何签名变化或无法明确识别，材料与轴映射需重新确认；保留原确认来源。",std::nullopt,v.viewIdentity,false));
        }
    }
    result.value=std::move(validated); return result;
}
}
