#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace bcpdoor {
struct Point {double x{},y{};};
struct Box {double xmin{},ymin{},xmax{},ymax{};};
struct Stroke {bool arc=false;double x1{},y1{},x2{},y2{},cx{},cy{},radius{},angle{},sweep{};};
inline double angleDelta(double a){const double turn=6.283185307179586;while(a<0)a+=turn;while(a>=turn)a-=turn;return a;}
inline bool onArc(const Stroke& s,double x,double y){const auto a=std::atan2(y-s.cy,x-s.cx);return angleDelta(s.sweep>=0?a-s.angle:s.angle-a)<=std::abs(s.sweep)+1e-9;}
inline bool contains(const Box& b,double x,double y){return x>=b.xmin-1e-8&&x<=b.xmax+1e-8&&y>=b.ymin-1e-8&&y<=b.ymax+1e-8;}
inline bool hits(const Box& b,const Stroke& s){
 if(!s.arc){double lo=0,hi=1;const double d[2]={s.x2-s.x1,s.y2-s.y1},p[2]={s.x1,s.y1},a[2]={b.xmin,b.ymin},z[2]={b.xmax,b.ymax};for(int k=0;k<2;++k){if(std::abs(d[k])<1e-12){if(p[k]<a[k]||p[k]>z[k])return false;}else{auto x=(a[k]-p[k])/d[k],y=(z[k]-p[k])/d[k];if(x>y)std::swap(x,y);lo=(std::max)(lo,x);hi=(std::min)(hi,y);if(lo>hi)return false;}}return true;}
 const auto point=[&](double a){return Point{s.cx+s.radius*std::cos(a),s.cy+s.radius*std::sin(a)};};for(double a:{s.angle,s.angle+s.sweep}){auto p=point(a);if(contains(b,p.x,p.y))return true;}
 for(double x:{b.xmin,b.xmax}){double q=s.radius*s.radius-(x-s.cx)*(x-s.cx);if(q>=-1e-8)for(double y:{s.cy-std::sqrt((std::max)(0.0,q)),s.cy+std::sqrt((std::max)(0.0,q))})if(contains(b,x,y)&&onArc(s,x,y))return true;}
 for(double y:{b.ymin,b.ymax}){double q=s.radius*s.radius-(y-s.cy)*(y-s.cy);if(q>=-1e-8)for(double x:{s.cx-std::sqrt((std::max)(0.0,q)),s.cx+std::sqrt((std::max)(0.0,q))})if(contains(b,x,y)&&onArc(s,x,y))return true;}return false;
}
// Keep a safe, same-side existing position only when the lower blank cannot fit.
template<class IsSafe>
std::vector<Point> safeCandidates(const std::vector<Point>& lower,Point fallback,IsSafe isSafe,bool& kept){
 std::vector<Point> result;for(const auto& p:lower)if(isSafe(p))result.push_back(p);
 kept=result.empty();if(kept&&isSafe(fallback))result.push_back(fallback);return result;
}
inline std::vector<Point> candidates(Point a,Point b,int side,double textWidth,double textHeight,Box graphic){
 const bool vertical=std::abs(a.x-b.x)<1e-6;std::vector<Point> out;
 if(vertical){const double low=(std::min)(a.y,b.y),length=std::abs(a.y-b.y),x=a.x+side*(180+textWidth/2),bottom=low-textHeight/2-90;for(double dy=0;bottom+dy<=low+length*.75+1e-6;dy+=100)for(double dx=0;dx<=1500;dx+=150)out.push_back({x+side*dx,bottom+dy});}
 else{const double mid=(a.x+b.x)/2,length=std::abs(a.x-b.x),base=side>0?a.y+180+textHeight/2:graphic.ymin-textHeight/2-90;for(double dy=0;dy<=900;dy+=150){const double y=base+side*dy;if(side>0&&y>a.y+length*.65)break;for(double dx:{0.0,-100.0,100.0,-200.0,200.0,-300.0,300.0,-450.0,450.0,-600.0,600.0,-750.0,750.0,-900.0,900.0,-1050.0,1050.0})if(mid+dx>=(std::min)(a.x,b.x)-textWidth/2-180&&mid+dx<=(std::max)(a.x,b.x)+textWidth/2+180)out.push_back({mid+dx,y});}}
 return out;
}
}
