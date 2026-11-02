#include "ApronGeometry.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
int checks = 0, failures = 0;
void check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
bool near(double a, double b) { return std::abs(a - b) <= 1e-6; }
bool same(bcp::Point a, bcp::Point b) { return near(a.x,b.x) && near(a.y,b.y); }
bool has(const std::vector<bcp::Point>& v, bcp::Point p) {
    return std::any_of(v.begin(),v.end(),[&](bcp::Point q){return same(p,q);});
}
double segmentDistance(bcp::Point p,bcp::Point a,bcp::Point b) {
    const double dx=b.x-a.x,dy=b.y-a.y;
    const double t=std::max(0.0,std::min(1.0,((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy)));
    return std::hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);
}
bool outerWidth(const bcp::ApronOutline& outline,double width) {
    for(std::size_t i=0;i<outline.offsetOuterContour.size();++i) {
        const auto a=outline.offsetOuterContour[i];
        const auto b=outline.offsetOuterContour[(i+1)%outline.offsetOuterContour.size()];
        const bcp::Point mid{(a.x+b.x)/2,(a.y+b.y)/2};
        double distance=std::numeric_limits<double>::infinity();
        for(std::size_t j=0;j<outline.innerContour.size();++j)
            distance=std::min(distance,segmentDistance(mid,outline.innerContour[j],outline.innerContour[(j+1)%outline.innerContour.size()]));
        // Mitred convex extensions can lie beyond the finite host segment;
        // their Euclidean nearest distance can exceed width on a short edge.
        const auto hostA=outline.innerContour[i];
        const auto hostB=outline.innerContour[(i+1)%outline.innerContour.size()];
        const double hostDx=hostB.x-hostA.x,hostDy=hostB.y-hostA.y;
        const double perpendicular=std::abs((mid.x-hostA.x)*hostDy-(mid.y-hostA.y)*hostDx)/std::hypot(hostDx,hostDy);
        if(!near(perpendicular,width) || distance<width-1e-6)return false;
    }
    return true;
}
template<class F> void test(const char* name, F f) {
    try { f(); } catch (const std::exception& e) {
        ++checks; ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n';
    }
}
template<class F> bool rejected(F f) {
    try { f(); } catch (const std::invalid_argument&) { return true; }
    return false;
}
const std::vector<bcp::Point> rectangle{{0,0},{6000,0},{6000,4000},{0,4000}};
std::vector<bcp::ApronInterruption> boxCuts(const std::vector<bcp::Point>& polygon,
    double xmin,double ymin,double xmax,double ymax) {
    std::vector<bcp::ApronInterruption> result;
    for (std::size_t i=0;i<polygon.size();++i) {
        const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
        if (near(a.x,b.x) && a.x>=xmin-1e-6 && a.x<=xmax+1e-6) {
            const double lo=std::max(std::min(a.y,b.y),ymin);
            const double hi=std::min(std::max(a.y,b.y),ymax);
            if(hi-lo>1e-6)result.push_back({{a.x,lo},{a.x,hi}});
        } else if (near(a.y,b.y) && a.y>=ymin-1e-6 && a.y<=ymax+1e-6) {
            const double lo=std::max(std::min(a.x,b.x),xmin);
            const double hi=std::min(std::max(a.x,b.x),xmax);
            if(hi-lo>1e-6)result.push_back({{lo,a.y},{hi,a.y}});
        }
    }
    return result;
}
} // namespace

int main() {
    using namespace bcp;
    test("rectangle",[]{
        const auto r=makeApronOutline(rectangle,600);
        check(r.paths.size()==1 && r.paths.front().closed,"uncut outline is one closed path");
        check(r.offsetOuterContour.size()==4 && has(r.offsetOuterContour,{-600,-600}) &&
            has(r.offsetOuterContour,{6600,4600}),"rectangle is exactly 600 outside every wall");
        check(r.convexCornerJoints.size()==4,"rectangle has four 45 degree corner joints");
        check(outerWidth(r,600),"independent segment distances verify every rectangle offset edge");
    });
    test("L and winding",[]{
        std::vector<Point> l{{0,0},{6000,0},{6000,2000},{3000,2000},{3000,5000},{0,5000}};
        const auto r=makeApronOutline(l,600);
        check(has(r.offsetOuterContour,{3600,2600}),"L concave corner is outward offset intersection");
        check(r.convexCornerJoints.size()==5,"L emits convex joints only");
        check(outerWidth(r,600),"independent segment distances verify every L offset edge");
        std::reverse(l.begin(),l.end());const auto reversed=makeApronOutline(l,600);
        check(r.offsetOuterContour.size()==reversed.offsetOuterContour.size() &&
            std::all_of(r.offsetOuterContour.begin(),r.offsetOuterContour.end(),[&](Point p){return has(reversed.offsetOuterContour,p);}),"CW and CCW produce the same outer geometry");
    });
    test("cleanup",[]{
        const auto r=makeApronOutline({{0,0},{0,0},{3000,0},{6000,1e-9},{6000,4000},{0,4000},{0,0}},600);
        check(r.innerContour.size()==4,"zero length and collinear vertices removed using numerical epsilon");
    });
    test("900 door",[]{
        const auto r=makeApronOutline(rectangle,600,{{{2000,0},{2900,0}}});
        check(r.paths.size()==1 && !r.paths.front().closed,"door produces one continuous open path");
        const auto& p=r.paths.front().points;
        check(same(p.front(),{2900,0}) && same(p[1],{2900,-600}) && same(p[p.size()-2],{2000,-600}) && same(p.back(),{2000,0}),"door caps align to actual 900 opening with perpendicular 600 caps");
        check(!has(p,{2000,0}) || !same(p.front(),p.back()),"no duplicate closing wall stroke");
        const auto q=makeApronOutline(rectangle,600,{{{2900,0},{2000,0}}});
        check(q.paths.size()==r.paths.size() && same(q.paths.front().points.front(),p.front()),"cut endpoint order has no effect");
    });
    test("merged cuts",[]{
        const auto r=makeApronOutline(rectangle,600,{{{1000,0},{2000,0}},{{3000,0},{2000,0}},{{1500,0},{2500,0}}});
        check(r.interruptionComponentCount==1 && r.paths.size()==1,"touching overlapping same-edge cuts merge");
        check(same(r.paths.front().points.front(),{3000,0}) && same(r.paths.front().points.back(),{1000,0}),"merged interval preserves its exact extreme endpoints");
        const auto two=makeApronOutline(rectangle,600,{{{1000,0},{2000,0}},{{2000.01,0},{3000,0}}});
        check(two.paths.size()==2,"real 0.01 gap is not merged as construction tolerance");
    });
    test("corner platform",[]{
        const auto r=makeApronOutline(rectangle,600,{{{5000,0},{6000,0}},{{6000,0},{6000,1000}}});
        check(r.interruptionComponentCount==1 && r.paths.size()==1,"platform cuts on neighboring edges join around corner");
        const auto& p=r.paths.front().points;
        check(same(p.front(),{6000,1000}) && same(p[1],{6600,1000}) && same(p[p.size()-2],{5000,-600}) && same(p.back(),{5000,0}),"corner platform has aligned caps on both host edges");
        check(r.convexCornerJoints.size()==3,"joint omitted at interrupted corner");
        const auto seam=makeApronOutline(rectangle,600,{{{0,0},{1000,0}},{{0,1000},{0,0}}});
        check(seam.interruptionComponentCount==1 && seam.paths.size()==1,"adjacent cuts merge through perimeter origin");
    });
    test("fully interrupted",[]{
        std::vector<ApronInterruption> cuts;
        for(std::size_t i=0;i<rectangle.size();++i)cuts.push_back({rectangle[i],rectangle[(i+1)%rectangle.size()]});
        const auto r=makeApronOutline(rectangle,600,cuts);
        check(r.paths.empty() && r.convexCornerJoints.empty(),"fully blocked perimeter has no residual strokes");
    });
    test("concave cap boundaries",[]{
        const std::vector<Point> l{{0,0},{6000,0},{6000,2000},{3000,2000},{3000,5000},{0,5000}};
        const auto close=makeApronOutline(l,600,{{{3000,2100},{3000,2500}}});
        check(close.paths.size()==2 && std::count_if(close.paths.begin(),close.paths.end(),[](const ApronPath& p){return p.closed;})==1,"short cut wholly before concave miter keeps exact closed outer edge plus open cut boundary");
        const auto exact=makeApronOutline(l,600,{{{3000,2600},{3000,3500}}});
        check(exact.paths.size()==1,"cap at exact 600 concave clearance is valid");
        const auto shortfall=makeApronOutline(l,600,{{{3000,2599.99},{3000,3500}}});
        check(shortfall.paths.size()==1,"0.01 concave overlap is cut at its true coordinates");
    });
    test("validation",[]{
        check(rejected([]{makeApronOutline(rectangle,0);}),"zero width rejected");
        check(rejected([]{makeApronOutline(rectangle,-600);}),"negative width rejected");
        check(rejected([]{makeApronOutline(rectangle,std::numeric_limits<double>::infinity());}),"nonfinite width rejected");
        check(rejected([]{makeApronOutline(rectangle,600,{{{1000,1},{1900,1}}});}),"off-wall cut rejected");
        check(rejected([]{makeApronOutline(rectangle,600,{{{1000,0},{1000,900}}});}),"cut crossing into building rejected");
        check(rejected([]{makeApronOutline(rectangle,600,{{{0,0},{0,0}}});}),"zero-length cut rejected");
        check(rejected([]{makeApronOutline({{0,0},{6000,0},{6000,4000},{3000,4000},{3000,-1000},{0,-1000}},600);}),"self intersecting input rejected");
        check(rejected([]{makeApronOutline({{0,0},{6000,0},{6000,6000},{3500,6000},{3500,2000},{2500,2000},{2500,6000},{0,6000}},600);}),"1000 narrow recess rejects intersecting 600 offset");
        check(rejected([]{makeApronOutline({{0,0},{6000,0},{6001,4000},{0,4000}},600);}),"nonorthogonal contour rejected");
        check(rejected([]{makeApronOutline({{0,0},{1,0},{0,0}},600);}),"degenerate contour rejected");
        check(rejected([]{makeApronOutline({{0,0},{6000,0},{6000,std::numeric_limits<double>::quiet_NaN()},{0,4000}},600);}),"nonfinite contour rejected");
        check(rejected([]{makeApronOutline(rectangle,600,{{{1000,0},{std::numeric_limits<double>::infinity(),0}}});}),"nonfinite cut rejected");
        check(rejected([]{makeApronOutline(rectangle,600,{{{-1,0},{900,0}}});}),"cut beyond actual wall endpoint rejected");
        check(rejected([]{makeApronOutline(rectangle,600,{{{5000,0},{6000,1000}}});}),"one diagonal cut cannot imply an unknown route around a corner");
        check(rejected([]{makeApronOutline({{0,0},{6000,0},{6000,6000},{3600,6000},{3600,2000},{2400,2000},{2400,6000},{0,6000}},600);}),"exact 1200 recess collapses 600 offset and is rejected");
    });
    test("v63 actual building and south porch",[]{
        // Frozen 5BC; v63 analysis.wallOuterBodyContours[0], not a grey courtyard line.
        const std::vector<Point> body{
            {32811.89025917224,18441.263551952827},{24011.890259172244,18441.263551952798},
            {24011.890259172244,20341.263551952827},{20611.890259172244,20341.263551952827},
            {20611.890259172244,16441.263551952827},{16211.890259172244,16441.263551952798},
            {16211.890259172244,24041.263551952827},{15111.890259172247,24041.263551952827},
            {15111.89025917189,32041.263551952827},{19211.890259172244,32041.263551952827},
            {19211.890259172244,30841.263551952827},{21211.890259172244,30841.263551952827},
            {21211.890259172244,34841.26355195283},{29211.890259172244,34841.26355195283},
            {29211.890259172244,32841.26355195283},{32811.89025917224,32841.26355195283}};
        const auto cuts=boxCuts(body,20611.89025917,18441.26355195,24011.89025917,20341.26355195);
        check(cuts.size()==3,"actual porch bounds cut three neighboring perimeter edges");
        const auto r=makeApronOutline(body,600,cuts);
        check(r.offsetOuterContour.size()==16 && r.paths.size()==1 && r.interruptionComponentCount==1,"v63 real footprint supports 600 and one merged porch interruption");
        check(outerWidth(r,600),"independent segment distances verify every actual v63 offset edge");
        const auto& path=r.paths.front().points;
        check(near(path.front().y,18441.26355195) && near(path.back().y,18441.26355195),"actual porch caps end at aligned platform front");
        check(near(std::abs(path[1].x-path.front().x)+std::abs(path[1].y-path.front().y),600),"actual porch first cap is exactly 600");
        check(near(std::hypot(path[path.size()-2].x-path.back().x,path[path.size()-2].y-path.back().y),600),"actual porch last cap is exactly 600");
        check((near(path.front().x,20611.89025917) && near(path.back().x,24011.89025917)) ||
            (near(path.back().x,20611.89025917) && near(path.front().x,24011.89025917)),"actual porch interruption spans only its exact two side edges");
        bool wallsRepeated=false;
        for(std::size_t i=1;i+2<path.size();++i)
            for(std::size_t j=0;j<body.size();++j)
                if(segmentDistance(path[i],body[j],body[(j+1)%body.size()])<1e-6 &&
                    segmentDistance(path[i+1],body[j],body[(j+1)%body.size()])<1e-6)wallsRepeated=true;
        check(!wallsRepeated,"actual generated paths do not repeat the wall outline");
        const ApronInterruption northwestDoor{{19371.890259172244,30841.263551952827},{20271.890259172244,30841.263551952827}};
        const ApronInterruption kitchenDoor{{21211.890259172244,30941.263551952943},{21211.890259172244,31841.263551952943}};
        const ApronInterruption northeastDoor{{29331.89025917795,32841.263551952827},{30231.890259177955,32841.263551952827}};
        const auto door1=makeApronOutline(body,600,{northwestDoor});
        check(door1.paths.size()==1 && !door1.paths.front().closed,"actual CD8B door 160 from concave corner is supported");
        const auto& p1=door1.paths.front().points;
        check((same(p1.front(),northwestDoor.a) && same(p1.back(),northwestDoor.b)) ||
            (same(p1.front(),northwestDoor.b) && same(p1.back(),northwestDoor.a)),"CD8B retains exact original 900 wall opening endpoints");
        check(has(p1,{19371.890259172244,31441.263551952827}) && has(p1,{19811.890259172244,31441.263551952827}),"CD8B has exact 600 normal plus 440 L cap to the existing outer miter");
        const auto door2=makeApronOutline(body,600,{kitchenDoor});
        check(door2.paths.size()==1 && !door2.paths.front().closed,"actual D00A door 100 from concave corner is supported");
        const auto& p2=door2.paths.front().points;
        check((same(p2.front(),kitchenDoor.a) && same(p2.back(),kitchenDoor.b)) ||
            (same(p2.front(),kitchenDoor.b) && same(p2.back(),kitchenDoor.a)),"D00A retains exact original 900 wall opening endpoints");
        check(has(p2,{20611.890259172244,30941.263551952943}) && has(p2,{20611.890259172244,31441.263551952827}),"D00A has exact 600 normal plus 500 L cap to the existing outer miter");
        auto allCuts=cuts;allCuts.push_back(northwestDoor);allCuts.push_back(kitchenDoor);allCuts.push_back(northeastDoor);
        const auto all=makeApronOutline(body,600,allCuts);
        check(all.paths.size()==4,"actual porch plus all three body-perimeter doors produce four open paths");
        check(std::all_of(all.paths.begin(),all.paths.end(),[](const ApronPath& p){return !p.closed;}),"actual near-corner cuts leave no residual closed islands");
        auto reversedBody=body;std::reverse(reversedBody.begin(),reversedBody.end());
        auto reorderedCuts=allCuts;std::reverse(reorderedCuts.begin(),reorderedCuts.end());
        for(auto& cut:reorderedCuts)std::swap(cut.a,cut.b);
        const auto reversedAll=makeApronOutline(reversedBody,600,reorderedCuts);
        bool identical=all.paths.size()==reversedAll.paths.size();
        if(identical)for(std::size_t i=0;i<all.paths.size();++i) {
            if(all.paths[i].points.size()!=reversedAll.paths[i].points.size()){identical=false;break;}
            for(std::size_t j=0;j<all.paths[i].points.size();++j)
                if(!same(all.paths[i].points[j],reversedAll.paths[i].points[j]))identical=false;
        }
        check(identical,"actual Boolean paths are invariant under winding and cut order/direction");
        bool crossedDoor=false;
        for(const auto& open:all.paths)for(std::size_t i=0;i+1<open.points.size();++i) {
            const Point mid{(open.points[i].x+open.points[i+1].x)/2,(open.points[i].y+open.points[i+1].y)/2};
            if(mid.x>19371.890259172244+1e-6 && mid.x<20271.890259172244-1e-6 &&
                mid.y>30841.263551952827+1e-6 && mid.y<31441.263551952827-1e-6)crossedDoor=true;
            if(mid.x>20611.890259172244+1e-6 && mid.x<21211.890259172244-1e-6 &&
                mid.y>30941.263551952943+1e-6 && mid.y<31841.263551952943-1e-6)crossedDoor=true;
            if(mid.x>29331.89025917795+1e-6 && mid.x<30231.890259177955-1e-6 &&
                mid.y>32841.263551952827+1e-6 && mid.y<33441.263551952827-1e-6)crossedDoor=true;
        }
        check(!crossedDoor,"no retained path crosses any actual 600-deep exterior door cut interior");
    });
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures==0?0:1;
}
