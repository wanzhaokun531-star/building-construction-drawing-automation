#pragma once
#include "ApprovedArea.h"
#include <map>
#include <array>
#include <string>
#include <istream>
#include <iomanip>
namespace bcpproduct {
struct LayoutNumber {std::string handle,number;std::vector<std::array<double,3>> core;std::array<double,3> target;};
struct LayoutPolygon {std::string text;double coefficient;std::vector<std::pair<std::string,int>> refs;};
struct LayoutAxis {std::string handle;int vertical,side;std::array<double,3> low,high;double innerLow,innerHigh;};
struct ApprovedLayout {int floor{};std::string sha;std::map<std::string,std::string> keep,clone;std::vector<LayoutNumber> numbers;std::map<std::string,std::vector<std::string>> groups;std::vector<LayoutPolygon> areas;std::string totalText;double porchWidth{},porchDepth{};std::vector<LayoutAxis> axes;};
inline ApprovedLayout readApprovedLayout(std::istream& in){
 auto demand=[](bool ok){if(!ok)throw std::runtime_error("invalid approved annotation configuration");};std::string tag;ApprovedLayout out;
 demand(bool(in>>tag>>out.floor>>out.sha)&&tag=="BCPAPPROVED1"&&(out.floor==1||out.floor==2)&&out.sha.size()==64);
 const auto count=[&](const char* expected){int n;demand(bool(in>>tag>>n)&&tag==expected&&n>=0&&n<10000);return n;};
 for(auto pair:{std::make_pair("KEEP",&out.keep),std::make_pair("CLONE",&out.clone)}){int n=count(pair.first);for(int i=0;i<n;++i){std::string h,t;demand(bool(in>>tag>>h>>t)&&tag==(pair.first[0]=='K'?"K":"C")&&pair.second->emplace(h,t).second);}}
 int n=count("NUMBERS");for(int i=0;i<n;++i){LayoutNumber x;int size;demand(bool(in>>tag>>x.handle>>std::quoted(x.number)>>size)&&tag=="N"&&size>=3&&size<100);for(auto& p:x.target)demand(bool(in>>p)&&std::isfinite(p));x.core.resize(size);for(auto& p:x.core)for(auto& v:p)demand(bool(in>>v)&&std::isfinite(v));demand(out.keep.count(x.handle)>0);out.numbers.push_back(x);}
 n=count("GROUPS");for(int i=0;i<n;++i){std::string name;int size;demand(bool(in>>tag>>name>>size)&&tag=="G"&&size>0&&size<1000&&out.groups.count(name)==0);auto& g=out.groups[name];for(int j=0;j<size;++j){std::string h;demand(bool(in>>h)&&out.clone.count(h)>0);g.push_back(h);}}
 n=count("AREAS");for(int i=0;i<n;++i){LayoutPolygon x;int size;demand(bool(in>>tag>>x.text>>x.coefficient>>size)&&tag=="A"&&size>=3&&size<1000&&out.clone.count(x.text)>0);for(int j=0;j<size*2;++j){std::string h;int edge;demand(bool(in>>h>>edge)&&edge>=0&&edge<4&&out.keep.count(h)>0);x.refs.push_back({h,edge});}out.areas.push_back(x);}
 demand(bool(in>>tag>>out.totalText>>out.porchWidth>>out.porchDepth)&&tag=="PORCH"&&out.clone.count(out.totalText)>0&&out.porchWidth>=0&&out.porchDepth>=0);
 n=count("AXES");for(int i=0;i<n;++i){LayoutAxis x;demand(bool(in>>tag>>x.handle>>x.vertical>>x.side)&&tag=="X"&&out.clone.count(x.handle)>0);for(auto& v:x.low)demand(bool(in>>v));for(auto& v:x.high)demand(bool(in>>v));demand(bool(in>>x.innerLow>>x.innerHigh));out.axes.push_back(x);}
 demand(bool(in>>tag)&&tag=="END");in>>std::ws;demand(in.eof());for(const auto& p:out.keep)demand(out.clone.count(p.first)==0);return out;
}
}
