#include "TestSupport.h"
#include "../src/Layout.h"
#include <cmath>
#include <limits>

namespace bce::test {
namespace {
LayoutContext context() { return {100,1,"geometry-v1","resolved-font-v1","glyphs-v1"}; }
VisibleOccupancy part(const char* id, OccupancyKind kind, Bounds2 b) {
    return {{id,kind},b,OccupancyEvidence::syntheticVerified,"synthetic_verified", "resolved-font-v1","glyphs-v1"};
}
ViewEnvelope view(const char* id="east",const char* handle="A1",double w=200,double h=100) {
    ViewEnvelope v;v.viewIdentity=id;v.targets={{"layout-db",handle,4}};
    v.occupancies={part("body",OccupancyKind::architecture,{{0,0},{w,h}})};
    v.requiredParts={{"body",OccupancyKind::architecture}};v.complete=true;v.source="synthetic_verified";v.context=context();return v;
}
LayoutRequest request(const std::vector<ViewEnvelope>& views, double w=420,double h=220) {
    LayoutRequest r;r.databaseIdentity="layout-db";r.revision=4;r.available={{0,0},{w,h}};r.clearance=20;
    for(const auto& v:views) {r.expectedViews.push_back({v.viewIdentity,v.context});}return r;
}
void syntheticResult(const Result<LayoutArrangement>& r) {
    require(r.value.has_value(),"verified synthetic occupancy should produce a pure arrangement");
    require(!r.canCommit()&&!r.value->nativeReady&&hasCode(r,"LAYOUT_SYNTHETIC_ONLY"),"synthetic fixture never authorizes native commit");
}
void rejects(const std::vector<ViewEnvelope>& v,const LayoutRequest& r,const char* code) {
    const auto result=arrange(v,r);require(!result.value&&!result.canCommit()&&hasCode(result,code),"invalid layout must fail with explicit diagnostic");
}
void addPart(ViewEnvelope& v,VisibleOccupancy p) {v.requiredParts.push_back(p.part);v.occupancies.push_back(p);}
}
void rowsDependOnAvailableArea() {
    const std::vector<ViewEnvelope> v{view(),view("west","A2")};
    const auto wide=arrange(v,request(v));syntheticResult(wide);
    require(wide.value->views[0].displacement.x==0&&wide.value->views[1].displacement.x==220&&wide.value->views[1].displacement.y==0,"420 wide uses one row");
    const auto narrow=arrange(v,request(v,250,220));syntheticResult(narrow);
    require(narrow.value->views[1].displacement.x==0&&narrow.value->views[1].displacement.y==120,"250 wide uses two rows");
}
void noFitDoesNotScaleArchitecture() {
    const std::vector<ViewEnvelope> v{view(),view("west","A2")};
    rejects(v,request(v,199,220),"NO_LAYOUT_SPACE");rejects(v,request(v,250,219),"NO_LAYOUT_SPACE");
    require(v[0].occupancies[0].bounds.maximum.x==200&&v[1].occupancies[0].bounds.maximum.y==100,"failure never mutates source geometry");
}
void protectedSectionIsFixedObstacle() {
    const std::vector<ViewEnvelope> v{view()};auto r=request(v,420,100);
    FixedEnvelope f;f.identity="section";f.targets={{"layout-db","B1",4}};f.context=context();f.complete=true;f.source="synthetic_verified";
    f.occupancies={part("section-body",OccupancyKind::architecture,{{0,0},{200,100}})};f.requiredParts={f.occupancies[0].part};r.fixed={f};
    const auto result=arrange(v,r);syntheticResult(result);
    require(result.value->views[0].displacement.x==220,"selected elevation avoids section");
    require(result.value->fixed[0].occupancies[0].bounds.minimum.x==0&&r.fixed[0].occupancies[0].bounds.maximum.x==200,"fixed section preserved");
    require(result.value->views[0].plan.translations.size()==1&&result.value->views[0].plan.translations[0].target.handle=="A1","section excluded from translations");
    // A positive gap smaller than the ULP at a large coordinate must never
    // round away and authorize touching geometry.
    const std::vector<ViewEnvelope> large{view("east","A1",16384,100)};
    r=request(large);r.available={{1e20,0},{1e20+65536,100}};r.clearance=1;
    f.occupancies[0].bounds={{1e20,0},{1e20+16384,100}};r.fixed={f};
    require(!arrange(large,r).value,"positive clearance must not round down to touching at large coordinates");
}
void collisionIncludesDimensionGlyphAndBaseline() {
    auto v=view();v.occupancies[0].bounds={{0,0},{10,10}};
    addPart(v,part("dimension-glyph",OccupancyKind::dimensionGlyph,{{100,100},{120,110}}));
    addPart(v,part("dimension-baseline",OccupancyKind::dimensionBaseline,{{100,90},{150,90}}));
    addPart(v,part("note",OccupancyKind::textGlyph,{{105,89},{115,105}}));
    auto r=checkInternalCollisions(v,{{"note","dimension-glyph"},{"note","dimension-baseline"}},0);
    require(!r.canCommit()&&r.value&&r.value->size()==2&&hasCode(r,"LAYOUT_INTERNAL_COLLISION"),"glyph and zero-height baseline outside root bounds both collide");
    r=checkInternalCollisions(v,{},0);require(r.canCommit()&&r.value->empty(),"unrequested architecture intersections are not errors");
    r=checkInternalCollisions(v,{{"body","missing"}},0);require(hasCode(r,"INVALID_LAYOUT_PAIR"),"unknown collision pair never silently ignored");
}
void paperSizeTracksSelectedScale() {
    const auto a=paperSizeInInput(3.5,100,1),b=paperSizeInInput(3.5,50,1),m=paperSizeInInput(3.5,100,1000);
    require(a.canCommit()&&*a.value==350&&b.canCommit()&&*b.value==175&&m.canCommit()&&*m.value==0.35,"paper mm tracks scale and input units");
    require(!paperSizeInInput(3.5,-100,1).canCommit()&&!paperSizeInInput(3.5,100,0).canCommit(),"invalid conversion factors blocked");
    require(!paperSizeInInput(1e308,1e308,1).canCommit(),"overflowing paper conversion blocked");
}
void incompleteGlyphEvidenceBlocksLayout() {
    auto v=view();addPart(v,part("note",OccupancyKind::textGlyph,{{10,10},{20,20}}));auto r=request({v});
    v.complete=false;rejects({v},r,"LAYOUT_OCCUPANCY_INCOMPLETE");v.complete=true;
    v.occupancies.pop_back();rejects({v},r,"LAYOUT_OCCUPANCY_INCOMPLETE");
    v=view();v.source="";rejects({v},request({v}),"LAYOUT_OCCUPANCY_INCOMPLETE");
    v=view();v.occupancies[0].evidence=OccupancyEvidence::unverified;rejects({v},request({v}),"LAYOUT_OCCUPANCY_INCOMPLETE");
    v=view();addPart(v,part("note",OccupancyKind::textGlyph,{{10,10},{20,20}}));v.occupancies[1].fontSignature="substitute";rejects({v},request({v}),"LAYOUT_GLYPH_MISMATCH");
    v=view();r=request({v});r.expectedViews[0].context.scaleDenominator=50;rejects({v},r,"LAYOUT_CONTEXT_MISMATCH");
    r=request({v});r.expectedViews[0].context.glyphSignature="glyphs-v2";rejects({v},r,"LAYOUT_CONTEXT_MISMATCH");
    r=request({v});r.expectedViews[0].context.fontSignature="font-v2";rejects({v},r,"LAYOUT_CONTEXT_MISMATCH");
    r=request({v});r.expectedViews[0].context.geometrySignature="geometry-v2";rejects({v},r,"LAYOUT_CONTEXT_MISMATCH");
    r=request({v});r.expectedViews[0].context.mmPerInputUnit=1000;rejects({v},r,"LAYOUT_CONTEXT_MISMATCH");
    v=view();addPart(v,part("dim",OccupancyKind::dimensionGlyph,{{0,110},{100,120}}));v.occupancies[1].glyphSignature="old-glyph";rejects({v},request({v}),"LAYOUT_GLYPH_MISMATCH");
}
void malformedOrOverflowingGeometryRejected() {
    auto v=view();auto r=request({v});r.clearance=-1;rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    r=request({v});r.available.maximum.x=std::numeric_limits<double>::infinity();rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    v.occupancies[0].bounds.minimum.x=201;rejects({v},request({v}),"INVALID_LAYOUT_GEOMETRY");
    v=view();v.occupancies[0].bounds={{-1e308,0},{1e308,100}};rejects({v},request({v}),"INVALID_LAYOUT_GEOMETRY");
    v=view();r=request({v});r.available={{-1e308,0},{1e308,220}};rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    v=view();v.context.scaleDenominator=std::numeric_limits<double>::quiet_NaN();rejects({v},request({v}),"INVALID_LAYOUT_GEOMETRY");
    v=view();v.occupancies[0].bounds={{-1e308,0},{-1e308,100}};r=request({v});r.available={{1e308,0},{1e308,220}};rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    // Rounded subtraction alone is insufficient: (1e16-1) rounds to 1e16,
    // hiding a one-unit shrink when the min moves by 1 but the max cannot.
    v=view("east","A1",1e16,2);r=request({v});r.available={{1,0},{1e16+10,10}};r.clearance=0;
    rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    // Nonzero extents can be distorted without collapsing: 1e16+3 rounds to
    // 1e16+4. The declared rigid translation must reject that width change.
    v=view("east","A1",3,2);r=request({v});r.available={{1e16,0},{1e16+10,10}};r.clearance=0;
    rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    require(v.occupancies[0].bounds.minimum.x==0&&v.occupancies[0].bounds.maximum.x==3,"unrepresentable translation does not alter source architecture");
    v=view("east","A1",5,2);rejects({v},r,"INVALID_LAYOUT_GEOMETRY"); // 5 rounds down to 4.
    v=view("east","A1",2,3);r=request({v});r.available={{0,1e16},{10,1e16+10}};r.clearance=0;
    rejects({v},r,"INVALID_LAYOUT_GEOMETRY");
    v=view("east","A1",10,10);addPart(v,part("note",OccupancyKind::textGlyph,{{0,0},{3,2}}));
    r=request({v});r.available={{1e16,0},{1e16+10,10}};r.clearance=0;
    rejects({v},r,"INVALID_LAYOUT_GEOMETRY"); // The union stays 10; its inner glyph still distorts.
    v=view("east","A1",4,2);r=request({v});r.available={{1e16,0},{1e16+10,10}};r.clearance=0;
    const auto exact=arrange({v},r);syntheticResult(exact);
    require(exact.value->views[0].occupiedBounds.maximum.x-exact.value->views[0].occupiedBounds.minimum.x==4,"exactly representable rigid displacement remains supported");
}
void searchLimitIsNotNoSpace() {
    const std::vector<ViewEnvelope> v{view(),view("west","A2")};auto r=request(v);r.searchBudget=1;
    const auto result=arrange(v,r);require(!result.value&&hasCode(result,"LAYOUT_SEARCH_LIMIT")&&!hasCode(result,"NO_LAYOUT_SPACE"),"budget exhaustion never claims no mathematical space");
    r.searchBudget=10000;syntheticResult(arrange(v,r));
}
void duplicateLayoutIdentityAndKeysRejected() {
    auto a=view(),b=view("west","A2");auto r=request({a,b});b.viewIdentity="east";rejects({a,b},r,"INVALID_LAYOUT_IDENTITY");
    b=view("west","000a1");rejects({a,b},request({a,b}),"INVALID_LAYOUT_IDENTITY");
    b=view("west","A2");b.targets[0].revision=5;rejects({a,b},request({a,b}),"INVALID_LAYOUT_IDENTITY");
    b.targets[0].revision=4;b.targets[0].databaseIdentity="other-db";rejects({a,b},request({a,b}),"INVALID_LAYOUT_IDENTITY");
    a.targets[0].handle="0xA1";rejects({a},request({a}),"INVALID_LAYOUT_IDENTITY");
    a=view();a.targets.push_back({"layout-db","000a1",4});rejects({a},request({a}),"INVALID_LAYOUT_IDENTITY");
    a=view();r=request({a});r.searchBudget=0;rejects({a},r,"LAYOUT_UNSUPPORTED_RANGE");
    r=request({a});r.searchBudget=1000001;rejects({a},r,"LAYOUT_UNSUPPORTED_RANGE");
}
void singleTranslationRetainsAllVisiblePartsAndTargets() {
    auto v=view();v.targets.push_back({"layout-db","A2",4});
    addPart(v,part("text",OccupancyKind::textGlyph,{{-50,-30},{10,0}}));addPart(v,part("symbol",OccupancyKind::symbol,{{210,30},{220,40}}));
    addPart(v,part("dim",OccupancyKind::dimensionGlyph,{{30,120},{40,130}}));addPart(v,part("base",OccupancyKind::dimensionBaseline,{{0,140},{200,140}}));
    addPart(v,part("table",OccupancyKind::materialTable,{{300,0},{350,100}}));
    auto r=request({v},500,300);r.available.minimum={1000,2000};r.available.maximum={1500,2300};
    const auto result=arrange({v},r);syntheticResult(result);const auto& p=result.value->views[0];
    require(p.displacement.x==1050&&p.displacement.y==2030&&p.occupancies.size()==6&&p.plan.translations.size()==2,"entire visible union determines translation");
    for(std::size_t i=0;i<v.occupancies.size();++i) {require(p.occupancies[i].bounds.minimum.x==v.occupancies[i].bounds.minimum.x+1050&&p.occupancies[i].bounds.maximum.y==v.occupancies[i].bounds.maximum.y+2030,"all occupancy coordinates translated exactly once");}
    for(std::size_t i=0;i<v.targets.size();++i) {require(p.plan.translations[i].target==v.targets[i]&&p.plan.translations[i].displacement.x==1050&&p.plan.translations[i].displacement.y==2030,"source keys retain one common displacement");}
    require(p.context.fontSignature=="resolved-font-v1"&&p.source=="synthetic_verified"&&result.value->occupiedBounds.maximum.x==1400,"source evidence and true merged occupancy retained");
    require(p.complete&&p.requiredParts.size()==6&&p.requiredParts[4].kind==OccupancyKind::dimensionBaseline,"complete original inventory remains independently auditable");
    require(result.value->available.minimum.x==1000&&result.value->clearance==20&&!result.value->allowReorder,"layout family constraints remain auditable");
}
void layoutBacktrackingAndReorderAreDeterministic() {
    // First-fit puts the second 2x2 at (3,0), blocking the 6x8. Backtracking
    // moves only its placement to (0,3), permitting the 6x8 at (3,0).
    auto a=view("small-a","A1",2,2),b=view("small-b","A2",2,2),c=view("large","A3",6,8);
    auto r=request({a,b,c},10,10);r.clearance=1;
    const auto backtracked=arrange({a,b,c},r);syntheticResult(backtracked);
    require(backtracked.value->views[1].displacement.x==0&&backtracked.value->views[1].displacement.y==3&&backtracked.value->views[2].displacement.x==3,"backtracking escapes first-fit dead end");
    b=view("tall","A2",2,8);c=view("wide","A3",6,2);r=request({a,b,c},10,10);r.clearance=1;
    rejects({a,b,c},r,"NO_LAYOUT_SPACE"); // Exhaustive for the fixed-order anchor family only.
    r.allowReorder=true;
    const auto first=arrange({a,b,c},r);syntheticResult(first);const auto second=arrange({a,b,c},r);syntheticResult(second);
    require(first.value->views[1].displacement.x==7&&first.value->views[2].displacement.y==3,"reorder search escapes fixed-order anchor dead end");
    require(second.value->views[1].displacement.x==7&&first.value->searchedCandidates==second.value->searchedCandidates,"identical requests produce deterministic result");
}
}
