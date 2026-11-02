#include "ApronGeometry.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

namespace bcp {
namespace {
constexpr double eps = numericalEpsilonMm;
bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
Point add(Point a, Point b) { return {a.x+b.x,a.y+b.y}; }
Point sub(Point a, Point b) { return {a.x-b.x,a.y-b.y}; }
Point scale(Point p,double k) { return {p.x*k,p.y*k}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y; }
double cross(Point a,Point b) { return a.x*b.y-a.y*b.x; }
double length(Point p) { return std::hypot(p.x,p.y); }
bool near(double a,double b) { return std::abs(a-b)<=eps; }
bool same(Point a,Point b) { return near(a.x,b.x) && near(a.y,b.y); }
bool between(double value,double a,double b) {
    return value>=std::min(a,b)-eps && value<=std::max(a,b)+eps;
}
bool onSegment(Point p,Point a,Point b) {
    return finite(p) && between(p.x,a.x,b.x) && between(p.y,a.y,b.y) &&
        ((near(a.x,b.x) && near(p.x,a.x)) || (near(a.y,b.y) && near(p.y,a.y)));
}
bool intersects(Point a,Point b,Point c,Point d) {
    if (near(a.x,b.x) && near(c.x,d.x))
        return near(a.x,c.x) && std::max(std::min(a.y,b.y),std::min(c.y,d.y))<=std::min(std::max(a.y,b.y),std::max(c.y,d.y))+eps;
    if (near(a.y,b.y) && near(c.y,d.y))
        return near(a.y,c.y) && std::max(std::min(a.x,b.x),std::min(c.x,d.x))<=std::min(std::max(a.x,b.x),std::max(c.x,d.x))+eps;
    if (near(a.x,b.x)) return between(a.x,c.x,d.x) && between(c.y,a.y,b.y);
    return between(c.x,a.x,b.x) && between(a.y,c.y,d.y);
}
void requireSimple(const std::vector<Point>& polygon) {
    const auto n=polygon.size();
    for(std::size_t i=0;i<n;++i) {
        if(same(polygon[i],polygon[(i+1)%n]))throw std::invalid_argument("zero length contour edge");
        for(std::size_t j=i+1;j<n;++j) {
            if(j==i+1 || (i==0 && j==n-1))continue;
            if(intersects(polygon[i],polygon[(i+1)%n],polygon[j],polygon[(j+1)%n]))
                throw std::invalid_argument("contour self intersection or self touch");
        }
    }
}
double twiceArea(const std::vector<Point>& polygon) {
    double result=0;
    const auto origin=polygon.front();
    for(std::size_t i=1;i+1<polygon.size();++i)
        result+=cross(sub(polygon[i],origin),sub(polygon[i+1],origin));
    return result;
}
std::vector<Point> cleanContour(const std::vector<Point>& input) {
    std::vector<Point> p;
    for(const auto point:input) {
        if(!finite(point))throw std::invalid_argument("nonfinite contour point");
        if(p.empty() || !same(p.back(),point))p.push_back(point);
    }
    if(p.size()>1 && same(p.front(),p.back()))p.pop_back();
    if(p.size()<4)throw std::invalid_argument("orthogonal contour needs four corners");
    // Snap only numerical noise; never straighten a genuinely diagonal edge.
    for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<p.size();++i) {
        auto& next=p[(i+1)%p.size()];
        const bool vertical=near(p[i].x,next.x),horizontal=near(p[i].y,next.y);
        if(!vertical && !horizontal)throw std::invalid_argument("nonorthogonal contour");
        if(vertical)next.x=p[i].x;
        if(horizontal)next.y=p[i].y;
    }
    bool changed=true;
    while(changed && p.size()>=4) {
        changed=false;
        for(std::size_t i=0;i<p.size();++i) {
            const auto previous=p[(i+p.size()-1)%p.size()],next=p[(i+1)%p.size()];
            const auto incoming=sub(p[i],previous),outgoing=sub(next,p[i]);
            if(same(p[i],previous) || same(p[i],next)) {
                p.erase(p.begin()+static_cast<std::ptrdiff_t>(i));changed=true;break;
            }
            if((near(incoming.x,0) && near(outgoing.x,0)) || (near(incoming.y,0) && near(outgoing.y,0))) {
                if(dot(incoming,outgoing)<=0)throw std::invalid_argument("contour doubles back");
                p.erase(p.begin()+static_cast<std::ptrdiff_t>(i));changed=true;break;
            }
        }
    }
    if(p.size()<4)throw std::invalid_argument("degenerate contour");
    requireSimple(p);
    const double area=twiceArea(p);
    if(!std::isfinite(area) || std::abs(area)<=eps*eps)throw std::invalid_argument("invalid contour area");
    if(area<0)std::reverse(p.begin()+1,p.end());
    return p;
}
bool insideOrOn(Point p,const std::vector<Point>& polygon) {
    bool inside=false;
    for(std::size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        const auto a=polygon[j],b=polygon[i];
        if(onSegment(p,a,b))return true;
        if((a.y>p.y)!=(b.y>p.y)) {
            const double crossing=a.x+(p.y-a.y)*(b.x-a.x)/(b.y-a.y);
            if(p.x<crossing)inside=!inside;
        }
    }
    return inside;
}
struct Edge {
    Point a,b,unit,normal;
    double length=0,station=0;
};
std::vector<Edge> makeEdges(const std::vector<Point>& points) {
    std::vector<Edge> result;
    double station=0;
    for(std::size_t i=0;i<points.size();++i) {
        const auto delta=sub(points[(i+1)%points.size()],points[i]);
        const auto len=length(delta);
        const auto unit=scale(delta,1/len);
        result.push_back({points[i],points[(i+1)%points.size()],unit,{unit.y,-unit.x},len,station});
        station+=len;
    }
    if(!std::isfinite(station))throw std::invalid_argument("nonfinite contour perimeter");
    return result;
}
struct Interval { double low,high; };
std::vector<Interval> mergedCuts(const std::vector<ApronInterruption>& cuts,const std::vector<Edge>& edges) {
    std::vector<Interval> intervals;
    for(const auto& cut:cuts) {
        if(!finite(cut.a) || !finite(cut.b) || same(cut.a,cut.b))throw std::invalid_argument("invalid interruption endpoints");
        bool found=false;
        for(const auto& edge:edges) {
            if(!onSegment(cut.a,edge.a,edge.b) || !onSegment(cut.b,edge.a,edge.b))continue;
            const double a=std::max(0.0,std::min(edge.length,dot(sub(cut.a,edge.a),edge.unit)));
            const double b=std::max(0.0,std::min(edge.length,dot(sub(cut.b,edge.a),edge.unit)));
            if(std::abs(a-b)<=eps)throw std::invalid_argument("collapsed interruption");
            intervals.push_back({edge.station+std::min(a,b),edge.station+std::max(a,b)});
            found=true;break;
        }
        if(!found)throw std::invalid_argument("interruption is not on one actual wall edge");
    }
    std::sort(intervals.begin(),intervals.end(),[](Interval a,Interval b){return a.low<b.low;});
    std::vector<Interval> result;
    for(const auto interval:intervals) {
        if(!result.empty() && interval.low<=result.back().high+eps)result.back().high=std::max(result.back().high,interval.high);
        else result.push_back(interval);
    }
    return result;
}
struct CutBox { double xmin,ymin,xmax,ymax; };
CutBox boxBetween(Point a,Point b) {
    return {std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)};
}
std::vector<CutBox> cutBoxes(const std::vector<Interval>& cuts,const std::vector<Edge>& edges,
    const std::vector<Point>& outer,double width,double perimeter) {
    std::vector<CutBox> boxes;
    for(const auto interval:cuts)for(const auto& edge:edges) {
        const double low=std::max(interval.low,edge.station);
        const double high=std::min(interval.high,edge.station+edge.length);
        if(high-low<=eps)continue;
        const auto a=add(edge.a,scale(edge.unit,low-edge.station));
        const auto b=add(add(edge.a,scale(edge.unit,high-edge.station)),scale(edge.normal,width));
        boxes.push_back(boxBetween(a,b));
    }
    // When an interruption turns around a convex vertex, its two normal boxes
    // also remove their mitred corner square. Otherwise that square would remain
    // as an isolated island between two cuts that meet on the actual wall.
    for(std::size_t i=0;i<edges.size();++i) {
        if(cross(edges[(i+edges.size()-1)%edges.size()].unit,edges[i].unit)<=0)continue;
        const double s=edges[i].station;
        bool before=false,after=false;
        for(const auto cut:cuts) {
            if(cut.low<s-eps && cut.high>=s-eps)before=true;
            if(cut.low<=s+eps && cut.high>s+eps)after=true;
            if(near(s,0) && near(cut.high,perimeter))before=true;
        }
        if(before && after)boxes.push_back(boxBetween(edges[i].a,outer[i]));
    }
    return boxes;
}
bool strictlyInside(Point p,const std::vector<Point>& polygon) {
    bool inside=false;
    for(std::size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        const auto a=polygon[j],b=polygon[i];
        if((a.y>p.y)!=(b.y>p.y)) {
            const double x=a.x+(p.y-a.y)*(b.x-a.x)/(b.y-a.y);
            if(p.x<x)inside=!inside;
        }
    }
    return inside;
}
void uniqueCoordinates(std::vector<double>& coordinates) {
    std::sort(coordinates.begin(),coordinates.end());
    coordinates.erase(std::unique(coordinates.begin(),coordinates.end(),[](double a,double b){return near(a,b);}),coordinates.end());
}
void simplifyPath(ApronPath& path) {
    bool changed=true;
    while(changed && path.points.size()>(path.closed?4U:2U)) {
        changed=false;
        for(std::size_t i=path.closed?0U:1U;i<(path.closed?path.points.size():path.points.size()-1);++i) {
            const auto a=path.points[(i+path.points.size()-1)%path.points.size()];
            const auto b=path.points[i],c=path.points[(i+1)%path.points.size()];
            if((near(a.x,b.x) && near(b.x,c.x)) || (near(a.y,b.y) && near(b.y,c.y))) {
                path.points.erase(path.points.begin()+static_cast<std::ptrdiff_t>(i));changed=true;break;
            }
        }
    }
}
struct GridEdge { std::size_t from,to; bool wall=false; };
std::vector<ApronPath> stripBoundary(const std::vector<Point>& inner,const std::vector<Point>& outer,
    const std::vector<Edge>& wallEdges,const std::vector<CutBox>& boxes) {
    std::vector<double> xs,ys;
    for(const auto p:inner){xs.push_back(p.x);ys.push_back(p.y);}
    for(const auto p:outer){xs.push_back(p.x);ys.push_back(p.y);}
    for(const auto b:boxes){xs.push_back(b.xmin);xs.push_back(b.xmax);ys.push_back(b.ymin);ys.push_back(b.ymax);}
    uniqueCoordinates(xs);uniqueCoordinates(ys);
    const auto nx=xs.size(),ny=ys.size();
    std::vector<unsigned char> cells((nx-1)*(ny-1),0);
    for(std::size_t x=0;x+1<nx;++x)for(std::size_t y=0;y+1<ny;++y) {
        const Point mid{xs[x]+(xs[x+1]-xs[x])/2,ys[y]+(ys[y+1]-ys[y])/2};
        const bool cut=std::any_of(boxes.begin(),boxes.end(),[&](CutBox box){
            return mid.x>box.xmin && mid.x<box.xmax && mid.y>box.ymin && mid.y<box.ymax;
        });
        if(!cut && strictlyInside(mid,outer) && !strictlyInside(mid,inner))cells[x*(ny-1)+y]=1;
    }
    const auto occupied=[&](std::size_t x,std::size_t y){return cells[x*(ny-1)+y]!=0;};
    const auto node=[&](std::size_t x,std::size_t y){return x*ny+y;};
    const auto point=[&](std::size_t id){return Point{xs[id/ny],ys[id%ny]};};
    std::vector<GridEdge> boundary;
    const auto append=[&](std::size_t from,std::size_t to){
        const auto a=point(from),b=point(to);
        const bool wall=std::any_of(wallEdges.begin(),wallEdges.end(),[&](const Edge& e){return onSegment(a,e.a,e.b) && onSegment(b,e.a,e.b);});
        boundary.push_back({from,to,wall});
    };
    for(std::size_t x=0;x+1<nx;++x)for(std::size_t y=0;y+1<ny;++y)if(occupied(x,y)) {
        if(y==0 || !occupied(x,y-1))append(node(x,y),node(x+1,y));
        if(x+2==nx || !occupied(x+1,y))append(node(x+1,y),node(x+1,y+1));
        if(y+2==ny || !occupied(x,y+1))append(node(x+1,y+1),node(x,y+1));
        if(x==0 || !occupied(x-1,y))append(node(x,y+1),node(x,y));
    }
    std::map<std::size_t,std::size_t> incoming,outgoing;
    for(std::size_t i=0;i<boundary.size();++i) {
        if(!outgoing.emplace(boundary[i].from,i).second || !incoming.emplace(boundary[i].to,i).second)
            throw std::invalid_argument("apron Boolean boundary has ambiguous corner-only contact");
    }
    incoming.clear();outgoing.clear();
    for(std::size_t i=0;i<boundary.size();++i)if(!boundary[i].wall) {
        outgoing.emplace(boundary[i].from,i);incoming.emplace(boundary[i].to,i);
    }
    std::vector<unsigned char> visited(boundary.size(),0);
    std::vector<ApronPath> paths;
    const auto trace=[&](std::size_t first){
        ApronPath path;
        auto index=first;
        const auto start=boundary[first].from;
        path.points.push_back(point(start));
        while(true) {
            if(visited[index])throw std::invalid_argument("apron Boolean path revisits a boundary edge");
            visited[index]=1;
            const auto end=boundary[index].to;
            if(end==start){path.closed=true;break;}
            path.points.push_back(point(end));
            const auto next=outgoing.find(end);
            if(next==outgoing.end())break;
            index=next->second;
        }
        simplifyPath(path);
        if(!path.closed)for(const auto endpoint:{path.points.front(),path.points.back()}) {
            if(!std::any_of(wallEdges.begin(),wallEdges.end(),[&](const Edge& edge){return onSegment(endpoint,edge.a,edge.b);}))
                throw std::invalid_argument("open apron endpoint is not on actual wall contour");
        }
        paths.push_back(std::move(path));
    };
    for(std::size_t i=0;i<boundary.size();++i)
        if(!boundary[i].wall && !visited[i] && incoming.count(boundary[i].from)==0)trace(i);
    for(std::size_t i=0;i<boundary.size();++i)if(!boundary[i].wall && !visited[i])trace(i);
    return paths;
}
} // namespace

ApronOutline makeApronOutline(const std::vector<Point>& contour,double width,
    const std::vector<ApronInterruption>& interruptions) {
    if(!std::isfinite(width) || width<=eps)throw std::invalid_argument("width must be finite and greater than numerical epsilon");
    ApronOutline result;
    result.innerContour=cleanContour(contour);
    const auto edges=makeEdges(result.innerContour);
    const auto n=edges.size();
    const double perimeter=edges.back().station+edges.back().length;
    for(std::size_t i=0;i<n;++i) {
        const auto offset=add(edges[(i+n-1)%n].normal,edges[i].normal);
        const auto vertex=add(result.innerContour[i],scale(offset,width));
        if(!finite(vertex))throw std::invalid_argument("nonfinite offset vertex");
        result.offsetOuterContour.push_back(vertex);
    }
    const auto& outer=result.offsetOuterContour;
    // An offset must retain edge direction; a collapsed/reversed notch is not
    // repaired into a different topology, even if the resulting ring is simple.
    for(std::size_t i=0;i<n;++i) {
        if(dot(sub(outer[(i+1)%n],outer[i]),edges[i].unit)<=eps)
            throw std::invalid_argument("offset collapses or reverses an edge");
        if(!near(dot(sub(outer[i],edges[i].a),edges[i].normal),width))
            throw std::invalid_argument("offset width cannot be represented accurately");
    }
    requireSimple(outer);
    const double outerArea=twiceArea(outer);
    if(!std::isfinite(outerArea) || outerArea<=twiceArea(result.innerContour))
        throw std::invalid_argument("offset does not enlarge building perimeter");
    for(const auto point:result.innerContour)if(!insideOrOn(point,outer))
        throw std::invalid_argument("offset does not enclose building contour");
    for(std::size_t i=0;i<n;++i)for(const auto& edge:edges)
        if(intersects(outer[i],outer[(i+1)%n],edge.a,edge.b))
            throw std::invalid_argument("offset intersects original building contour");

    const auto cuts=mergedCuts(interruptions,edges);
    if(cuts.empty())result.paths.push_back({outer,true});
    else {
        const auto boxes=cutBoxes(cuts,edges,outer,width,perimeter);
        result.paths=stripBoundary(result.innerContour,outer,edges,boxes);
        for(std::size_t i=0;i<cuts.size();++i) {
            const double start=cuts[i].high;
            const double stop=i+1<cuts.size()?cuts[i+1].low:cuts.front().low+perimeter;
            if(stop-start>eps)++result.interruptionComponentCount;
        }
        if(result.interruptionComponentCount==0)result.interruptionComponentCount=1;
    }
    for(std::size_t i=0;i<n;++i) {
        if(cross(edges[(i+n-1)%n].unit,edges[i].unit)<=0)continue;
        const double station=edges[i].station;
        const bool blocked=std::any_of(cuts.begin(),cuts.end(),[&](Interval cut){
            return (station>=cut.low-eps && station<=cut.high+eps) ||
                (near(station,0) && near(cut.high,perimeter));
        });
        if(!blocked)result.convexCornerJoints.push_back({result.innerContour[i],outer[i]});
    }
    return result;
}
} // namespace bcp
