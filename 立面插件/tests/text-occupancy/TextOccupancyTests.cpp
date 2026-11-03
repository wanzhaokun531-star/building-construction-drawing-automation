#include "../../src/TextOccupancy.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace bce;
namespace {
void require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
void near(double a, double b) { require(std::abs(a-b)<1e-9, "coordinate mismatch"); }
TextRenderContext context() { return {"device", "frame", "font", "source", 144, 144, {0,0}}; }
void backwardOnce() {
    TextRangeInput i; i.localRange={{-2,-1},{5,3}}; i.backward=true;
    i.coordinates=TextRangeCoordinates::beforeDisplayBackward;
    auto r=transformTextRange(i); require(r.canCommit(), "range failed");
    near(r.value->bounds.minimum.x,-5); near(r.value->bounds.maximum.x,2);
    i.coordinates=TextRangeCoordinates::afterDisplayBackward;
    r=transformTextRange(i); near(r.value->bounds.minimum.x,-2); near(r.value->bounds.maximum.x,5);
}
void allFourCorners() {
    TextRangeInput i; i.localRange={{-2,-1},{5,3}};
    i.coordinates=TextRangeCoordinates::afterDisplayBackward;
    i.localX={0,1}; i.localY={-1,0}; i.position={10,20};
    i.modelToWorld={2,1,7,-1,3,-4};
    const auto r=transformTextRange(i); require(r.canCommit(), "range failed");
    // Hand calculation: (x,y)->(10-y,20+x)->(47+x-2y,46+3x+y).
    const std::array<Point2,4> expected{{{47,39},{54,60},{46,64},{39,43}}};
    for(std::size_t k=0;k<4;++k){near(r.value->corners[k].x,expected[k].x);near(r.value->corners[k].y,expected[k].y);}
    near(r.value->bounds.minimum.x,39); near(r.value->bounds.maximum.x,54);
    near(r.value->bounds.minimum.y,39); near(r.value->bounds.maximum.y,64);
}
void completePixelCells() {
    std::vector<std::uint8_t> rgb(5*5*3,255); rgb[(2*5+2)*3]=254;
    const auto r=measureTextFootprint({rgb.data(),rgb.size(),5,5,15},{},context());
    require(r.canCommit(), "footprint failed"); require(r.value->inkPixels==1,"count");
    near(r.value->deviceCellBounds.minimum.x,2); near(r.value->deviceCellBounds.maximum.x,3);
    near(r.value->world.corners[2].x,3); near(r.value->world.corners[2].y,3);
}
void actualRotatedBackward() {
    TextRangeInput i; i.localRange={{0,-3.885780586188048e-16},{29.999999999999993,7}};
    i.coordinates=TextRangeCoordinates::beforeDisplayBackward; i.backward=true;
    i.position={88,28}; i.localX={0.891568288195329,0.4528862853790683};
    i.localY={-0.4528862853790683,0.891568288195329};
    const auto r=transformTextRange(i);require(r.canCommit(),"actual range");
    const std::array<Point2,4> expected{{{88,28},{61.25295135414014,14.41341143862795},
        {58.08274735648666,20.65438945599525},{84.82979600234653,34.240978017367304}}};
    for(std::size_t k=0;k<4;++k){near(r.value->corners[k].x,expected[k].x);near(r.value->corners[k].y,expected[k].y);}
}
void unknownConvention() {
    TextRangeInput i; i.localRange={{0,0},{1,1}};
    require(!transformTextRange(i).canCommit(),"unknown accepted");
    i.coordinates=static_cast<TextRangeCoordinates>(99);require(!transformTextRange(i).canCommit(),"invalid enum");
}
void invalidRangeTransforms() {
    TextRangeInput i; i.localRange={{0,0},{1,1}};i.coordinates=TextRangeCoordinates::afterDisplayBackward;
    i.localY={2,0};require(!transformTextRange(i).canCommit(),"singular basis");
    i.localY={0,1};i.modelToWorld={1,2,0,2,4,0};require(!transformTextRange(i).canCommit(),"singular net");
    i.modelToWorld={};i.localX.x=std::numeric_limits<double>::infinity();require(!transformTextRange(i).canCommit(),"nonfinite basis");
    i.localX={1,0};i.localRange.minimum.y=std::numeric_limits<double>::quiet_NaN();require(!transformTextRange(i).canCommit(),"nan range");
    i.localRange={{1,0},{0,1}};require(!transformTextRange(i).canCommit(),"reversed range");
}
void shearAndReflectedBasis() {
    TextRangeInput i; i.localRange={{-2,-1},{5,3}};i.coordinates=TextRangeCoordinates::afterDisplayBackward;
    i.localX={1,0};i.localY={2,-1}; // Explicit -normal handedness and shear.
    const auto r=transformTextRange(i);require(r.canCommit(),"sheared basis");
    near(r.value->corners[0].x,-4);near(r.value->corners[2].x,11);
    near(r.value->bounds.minimum.y,-3);near(r.value->bounds.maximum.y,1);
}
void fullScanStrideAndInverseCorners() {
    std::vector<std::uint8_t> rgb(5*19,255);
    // Nonwhite padding must not count. Second ink lies on last interior row.
    for(std::size_t y=0;y<5;++y)rgb[y*19+18]=0;
    rgb[1*19+1*3+1]=254;rgb[3*19+3*3+2]=1;
    auto c=context();c.pixelBoundaryPhase={0.25,0.75};
    const TextAffine2 m={1,1,0,-1,1,0};
    const auto r=measureTextFootprint({rgb.data(),rgb.size(),5,5,19},m,c);
    require(r.canCommit() && r.value->inkPixels==2,"full scan or stride");
    const std::array<Point2,4> expected{{{0,1},{1.5,2.5},{0,4},{-1.5,2.5}}};
    for(std::size_t k=0;k<4;++k){near(r.value->world.corners[k].x,expected[k].x);near(r.value->world.corners[k].y,expected[k].y);}
    near(r.value->world.bounds.minimum.x,-1.5);near(r.value->world.bounds.maximum.x,1.5);
    require(r.value->context.frameIdentity==c.frameIdentity && r.value->context.fontIdentity==c.fontIdentity &&
        r.value->context.deviceIdentity==c.deviceIdentity && r.value->context.sourceIdentity==c.sourceIdentity,"context identity");
    near(r.value->context.pixelBoundaryPhase.x,0.25);near(r.value->worldToPixel.c,-1);
}
void rejectBlankAndEachEdge() {
    std::vector<std::uint8_t> rgb(5*5*3,255);
    const TextRgbBuffer b={rgb.data(),rgb.size(),5,5,15};
    require(!measureTextFootprint(b,{},context()).canCommit(),"blank accepted");
    for(const auto p:std::array<std::size_t,4>{{2,10,14,22}}){
        rgb[p*3]=0;const auto r=measureTextFootprint(b,{},context());
        require(!r.canCommit() && r.issues[0].code=="TEXT_RGB_EDGE","edge accepted");rgb[p*3]=255;
    }
}
void rejectBadBuffer() {
    std::vector<std::uint8_t> rgb(75,255);rgb[36]=0;
    const TextRgbBuffer good={rgb.data(),rgb.size(),5,5,15};
    auto b=good;b.bytes=nullptr;require(!measureTextFootprint(b,{},context()).canCommit(),"null");
    b=good;b.width=0;require(!measureTextFootprint(b,{},context()).canCommit(),"zero width");
    b=good;b.rowStride=14;require(!measureTextFootprint(b,{},context()).canCommit(),"short stride");
    b=good;b.byteLength=74;require(!measureTextFootprint(b,{},context()).canCommit(),"short length");
    b=good;b.width=std::numeric_limits<std::size_t>::max();require(!measureTextFootprint(b,{},context()).canCommit(),"row overflow");
    b=good;b.rowStride=std::numeric_limits<std::size_t>::max();require(!measureTextFootprint(b,{},context()).canCommit(),"stride overflow");
}
void rejectBadMappingAndContext() {
    std::vector<std::uint8_t> rgb(75,255);rgb[36]=0;const TextRgbBuffer b={rgb.data(),rgb.size(),5,5,15};
    require(!measureTextFootprint(b,{1,2,0,2,4,0},context()).canCommit(),"singular mapping");
    auto m=TextAffine2{};m.tx=std::numeric_limits<double>::infinity();require(!measureTextFootprint(b,m,context()).canCommit(),"nonfinite mapping");
    auto c=context();c.fontIdentity.clear();require(!measureTextFootprint(b,{},c).canCommit(),"font identity");
    c=context();c.dpiX=0;require(!measureTextFootprint(b,{},c).canCommit(),"dpi");
    c=context();c.pixelBoundaryPhase.y=std::numeric_limits<double>::quiet_NaN();require(!measureTextFootprint(b,{},c).canCommit(),"phase");
}
void rejectRangeCollapsedByLargeTranslation() {
    TextRangeInput i; i.localRange={{0,0},{4,0.1}};
    i.coordinates=TextRangeCoordinates::afterDisplayBackward;
    i.localX={1,1}; i.localY={1,-1}; i.position={1e16,1e16};
    const auto collapsed=transformTextRange(i);
    require(!collapsed.canCommit(),"large translation returned a collapsed range");
    require(collapsed.issues[0].code=="TEXT_RANGE_NUMERIC","collapse must be numeric rejection");
    // The same finite translation is valid when the actual corners retain area.
    i.localRange.maximum.y=4;
    require(transformTextRange(i).canCommit(),"representable large translated range rejected");
}
void rejectInversePixelMappingCollapse() {
    std::vector<std::uint8_t> rgb(27,255); rgb[12]=0;
    const TextRgbBuffer b={rgb.data(),rgb.size(),3,3,9};
    const auto collapsed=measureTextFootprint(b,{0.125,0.125,-2.5e15,5,-5,0},context());
    require(!collapsed.canCommit(),"inverse mapping returned a collapsed cell");
    require(collapsed.issues[0].code=="TEXT_PIXEL_NUMERIC","inverse collapse must be numeric rejection");
    require(measureTextFootprint(b,{0.125,0.125,-2.5e15,0.125,-0.125,0},context()).canCommit(),
        "representable large inverse-mapped cell rejected");
}
}
int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests={
        {"backward reflects exactly once",backwardOnce},{"all four asymmetric transformed corners",allFourCorners},
        {"AA pixel occupies complete cell",completePixelCells},
        {"N9a actual rotated backward range and direction",actualRotatedBackward},
        {"unknown convention rejected",unknownConvention},{"invalid range and transforms rejected",invalidRangeTransforms},
        {"explicit sheared and reflected basis",shearAndReflectedBasis},
        {"full scan padded RGB and four inverse corners",fullScanStrideAndInverseCorners},
        {"blank and four buffer edges rejected",rejectBlankAndEachEdge},
        {"buffer length and overflow rejected",rejectBadBuffer},{"mapping and context rejected",rejectBadMappingAndContext},
        {"large translated range collapse rejected",rejectRangeCollapsedByLargeTranslation},
        {"inverse pixel mapping collapse rejected",rejectInversePixelMappingCollapse}};
    int failed=0; for(const auto& t:tests){try{t.second();std::cout<<"PASS "<<t.first<<'\n';}
        catch(const std::exception& e){++failed;std::cout<<"FAIL "<<t.first<<": "<<e.what()<<'\n';}}
    std::cout<<tests.size()-failed<<'/'<<tests.size()<<" passed\n";return failed?1:0;
}
