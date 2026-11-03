#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace n8a {
constexpr double epsilon = 1e-6;
struct Point { double x{}, y{}, z{}; };
struct Edge { Point first, second; };
struct Path { std::vector<Point> nodes; std::vector<std::size_t> segments; };
inline Point minus(Point a, Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline double dot(Point a, Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline double distance(Point a, Point b) { const auto d=minus(a,b);return std::sqrt(dot(d,d)); }
inline void ensure(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
inline bool finite(Point p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
inline Path path(const std::vector<Edge>& edges, Point direction) {
    ensure(!edges.empty()&&finite(direction)&&std::abs(dot(direction,direction)-1)<=epsilon,"invalid path direction/empty edges");
    Path result;
    for(const auto& edge:edges) for(const auto p:{edge.first,edge.second}) {
        ensure(finite(p),"nonfinite path node");
        if(std::none_of(result.nodes.begin(),result.nodes.end(),[&](Point old){return distance(old,p)<=epsilon;})) result.nodes.push_back(p);
    }
    ensure(result.nodes.size()==edges.size()+1,"not a complete nonbranching path");
    std::sort(result.nodes.begin(),result.nodes.end(),[&](Point a,Point b){return dot(a,direction)<dot(b,direction);});
    for(std::size_t i=1;i<result.nodes.size();++i) ensure(dot(minus(result.nodes[i],result.nodes[i-1]),direction)>epsilon,"ambiguous projected node order");
    result.segments.resize(edges.size(),edges.size());
    for(std::size_t index=0;index<edges.size();++index) {
        const auto locate=[&](Point p){std::size_t found=result.nodes.size();for(std::size_t i=0;i<result.nodes.size();++i)if(distance(p,result.nodes[i])<=epsilon){ensure(found==result.nodes.size(),"ambiguous node cluster");found=i;}return found;};
        auto first=locate(edges[index].first),second=locate(edges[index].second);
        if(first>second)std::swap(first,second);
        ensure(second==first+1&&second<result.nodes.size(),"segment is not one adjacent path edge");
        ensure(result.segments[first]==edges.size(),"duplicate path edge");result.segments[first]=index;
    }
    for(auto edge:result.segments)ensure(edge<edges.size(),"missing path edge");return result;
}
inline std::size_t unique_grip(Point node, const std::vector<Point>& grips) {
    ensure(finite(node),"nonfinite target node");std::size_t found=grips.size();
    for(std::size_t i=0;i<grips.size();++i){ensure(finite(grips[i]),"nonfinite native grip");if(distance(node,grips[i])<=epsilon){ensure(found==grips.size(),"node grip is not unique");found=i;}}
    ensure(found<grips.size(),"node has no actual matching grip");return found;
}
inline std::vector<std::size_t> move_order(const std::vector<double>& current, const std::vector<double>& target) {
    ensure(current.size()==target.size()&&current.size()>=2,"station count mismatch");
    for(std::size_t i=0;i<current.size();++i){ensure(std::isfinite(current[i])&&std::isfinite(target[i]),"nonfinite station");if(i)ensure(current[i]>current[i-1]+epsilon&&target[i]>target[i-1]+epsilon,"duplicate or descending station");}
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<current.size();++i)if(target[i]<current[i]-epsilon)result.push_back(i);
    for(std::size_t i=current.size();i-->0;)if(target[i]>current[i]+epsilon)result.push_back(i);
    return result;
}
}
