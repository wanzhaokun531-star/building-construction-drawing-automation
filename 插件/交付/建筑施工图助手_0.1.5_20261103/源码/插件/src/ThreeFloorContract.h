#pragma once
#include "ApprovedArea.h"
#include <array>
#include <map>
#include <set>
#include <iomanip>
#include <istream>
namespace bcpproduct {
using LayoutPoint=std::array<double,3>;
struct ThreeSource {std::string type;std::vector<LayoutPoint> grips;std::map<std::string,double> parameters;};
inline bool threeParametersMatch(const std::map<std::string,double>& expected,const std::map<std::string,double>& actual){for(const auto& p:expected){const auto found=actual.find(p.first);if(found==actual.end()||!std::isfinite(found->second)||std::abs(found->second-p.second)>1e-6)return false;}return true;}
struct ThreeOpening {std::string handle,number;std::vector<LayoutPoint> core;LayoutPoint label;};
struct ThreeColumn {std::string handle;LayoutPoint oldCentre,newCentre;double sx{},sy{};};
struct ThreeLayout {
 std::string sha;
 std::map<std::string,ThreeSource> source;
 std::map<std::string,std::string> keep,clone;
 std::set<std::string> erase;
 std::map<std::string,std::array<LayoutPoint,2>> walls;
 std::vector<ThreeOpening> openings;
 std::vector<ThreeColumn> columns;
};
inline void threeRequire(bool condition){if(!condition)throw std::runtime_error("invalid three-floor resource configuration");}
inline LayoutPoint threePoint(std::istream& in){LayoutPoint p;for(auto& v:p)threeRequire(bool(in>>v)&&std::isfinite(v));return p;}
inline std::vector<LayoutPoint> threePoints(std::istream& in){int n;threeRequire(bool(in>>n)&&n>0&&n<100);std::vector<LayoutPoint> p;for(int i=0;i<n;++i)p.push_back(threePoint(in));return p;}
inline ThreeLayout readThreeLayout(std::istream& in){
 ThreeLayout out;std::string tag,h;threeRequire(bool(in>>tag>>out.sha)&&tag=="BCPTHREE1"&&out.sha.size()==64);
 bool end=false;
 while(in>>tag){
  if(tag=="END"){end=true;break;}
  threeRequire(bool(in>>h));
  if(tag=="SOURCE"){ThreeSource s;threeRequire(bool(in>>s.type));s.grips=threePoints(in);threeRequire(out.source.emplace(h,std::move(s)).second);}
  else if(tag=="PARAM"){std::string key;double value;threeRequire(out.source.count(h)&&bool(in>>std::quoted(key)>>value)&&std::isfinite(value)&&out.source.at(h).parameters.emplace(key,value).second);}
  else if(tag=="KEEP"||tag=="CLONE"){std::string type;threeRequire(bool(in>>type));threeRequire((tag=="KEEP"?out.keep:out.clone).emplace(h,type).second);}
  else if(tag=="ERASE")threeRequire(out.erase.insert(h).second);
  else if(tag=="WALL"){auto a=threePoint(in),b=threePoint(in);threeRequire(out.walls.emplace(h,std::array<LayoutPoint,2>{a,b}).second);}
  else if(tag=="OPEN"){ThreeOpening o;o.handle=h;threeRequire(bool(in>>std::quoted(o.number)));o.core=threePoints(in);o.label=threePoint(in);out.openings.push_back(std::move(o));}
  else if(tag=="COLUMN"){ThreeColumn c;c.handle=h;c.oldCentre=threePoint(in);c.newCentre=threePoint(in);threeRequire(bool(in>>c.sx>>c.sy)&&std::isfinite(c.sx)&&std::isfinite(c.sy)&&c.sx>0&&c.sy>0);out.columns.push_back(c);}
  else threeRequire(false);
 }
 in>>std::ws;threeRequire(end&&in.eof()&&!out.source.empty()&&!out.clone.empty());
 for(const auto& e:out.keep)threeRequire(!out.clone.count(e.first)&&!out.erase.count(e.first));
 for(const auto& e:out.source)threeRequire(out.keep.count(e.first)>0&&!e.second.parameters.empty());
 return out;
}
struct ThreeAreaRef {std::string handle;int edge{};double offset{};};
struct ThreeAreaPolygon {std::string text;bool total{};double coefficient{};std::vector<std::array<ThreeAreaRef,2>> points;};
inline std::vector<ThreeAreaPolygon> readThreeAreas(std::istream& in){
 std::string tag;threeRequire(bool(in>>tag)&&tag=="BCPAREAPOLY1");std::vector<ThreeAreaPolygon> out;bool end=false;
 while(in>>tag){if(tag=="END"){end=true;break;}ThreeAreaPolygon p;int n,total;threeRequire(tag=="POLY"&&bool(in>>p.text>>total>>p.coefficient>>n)&&n>=3&&n<10000&&std::isfinite(p.coefficient)&&(total==0||total==1));p.total=total!=0;
  for(int i=0;i<n;++i){std::array<ThreeAreaRef,2> point;for(int a=0;a<2;++a){auto& r=point[a];threeRequire(bool(in>>r.handle>>r.edge>>r.offset)&&std::isfinite(r.offset)&&(r.edge==a||r.edge==a+3));}p.points.push_back(point);}out.push_back(std::move(p));
 }in>>std::ws;threeRequire(end&&in.eof()&&!out.empty());return out;
}
inline double evaluateThreeArea(const ThreeAreaPolygon& polygon,const std::map<std::string,std::array<double,6>>& bounds){
 std::vector<AreaVertex> points;for(const auto& p:polygon.points)points.push_back({bounds.at(p[0].handle)[p[0].edge]+p[0].offset,bounds.at(p[1].handle)[p[1].edge]+p[1].offset});return polygon.coefficient*polygonAreaM2(points);
}
}
