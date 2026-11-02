#pragma once
#include "../../阶段2/src/PlanRules.h"
#include <algorithm>
#include <stdexcept>
#include <limits>
namespace bcpproduct {
struct PlanPanel { bcp::Plan plan;double xmin{},xmax{}; };
inline std::vector<PlanPanel> splitPlanPanels(const bcp::Plan& source,std::size_t count){
 if(count!=1&&count!=2)throw std::invalid_argument("only confirmed one/two-plan partition supported");
 if(source.walls.empty())throw std::invalid_argument("input contains no readable walls");
 std::vector<std::pair<double,double>> spans;for(const auto& w:source.walls)spans.push_back({std::min(w.start.x,w.end.x),std::max(w.start.x,w.end.x)});
 std::sort(spans.begin(),spans.end());std::vector<std::pair<double,double>> joined;for(const auto& s:spans){if(joined.empty()||s.first>joined.back().second+bcp::numericalEpsilonMm)joined.push_back(s);else joined.back().second=std::max(joined.back().second,s.second);}
 double cut=0;if(count==2){double gap=0;for(std::size_t i=1;i<joined.size();++i)if(joined[i].first-joined[i-1].second>gap){gap=joined[i].first-joined[i-1].second;cut=(joined[i].first+joined[i-1].second)/2;}if(gap<=bcp::numericalEpsilonMm)throw std::invalid_argument("two plans cannot be separated by a clear X gap");}
 std::vector<PlanPanel> out(count);for(auto& p:out){p.plan=source;p.plan.walls.clear();p.plan.openings.clear();p.plan.rooms.clear();p.plan.axisExclusions.clear();p.plan.sourceObjectCount=0;p.xmin=-std::numeric_limits<double>::infinity();p.xmax=std::numeric_limits<double>::infinity();}if(count==2){out[0].xmax=cut;out[1].xmin=cut;}
 const auto panel=[&](double a,double b){if(count==1)return 0;if(std::min(a,b)<cut&&std::max(a,b)>cut)throw std::invalid_argument("architectural object crosses confirmed plan divider");return (a+b)/2<cut?0:1;};
 for(const auto& w:source.walls)out[panel(w.start.x,w.end.x)].plan.walls.push_back(w);
 for(const auto& o:source.openings)out[panel(o.a.x,o.b.x)].plan.openings.push_back(o);
 for(const auto& r:source.rooms)out[panel(r.min.x,r.max.x)].plan.rooms.push_back(r);
 for(const auto& h:source.axisExclusions)for(auto& p:out)if(std::any_of(p.plan.walls.begin(),p.plan.walls.end(),[&](const bcp::Wall& w){return w.handle==h;}))p.plan.axisExclusions.push_back(h);
 for(const auto& p:out)if(p.plan.walls.empty())throw std::invalid_argument("plan partition has no walls");return out;
}
}
