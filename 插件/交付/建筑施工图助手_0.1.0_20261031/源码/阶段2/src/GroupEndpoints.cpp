#include "PlanRules.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace bcp {
std::optional<std::pair<Point,Point>> resolveGroupEndpoints(const Opening& opening,const std::vector<Wall>& walls){
 std::vector<std::pair<Point,Point>> solutions;
 for(std::size_t i=0;i<opening.endpointCandidates.size();++i)for(std::size_t j=i+1;j<opening.endpointCandidates.size();++j){auto a=opening.endpointCandidates[i],b=opening.endpointCandidates[j];if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(b.x)||!std::isfinite(b.y))continue;if(std::abs(std::hypot(a.x-b.x,a.y-b.y)-opening.width)>numericalEpsilonMm)continue;int hosts=0;
  for(const auto& w:walls){const double len=std::hypot(w.end.x-w.start.x,w.end.y-w.start.y);if(len<=numericalEpsilonMm)continue;const auto c=bodyCentreStart(w);const double ux=(w.end.x-w.start.x)/len,uy=(w.end.y-w.start.y)/len;const auto on=[&](Point p){const double along=(p.x-w.start.x)*ux+(p.y-w.start.y)*uy;return std::abs((p.x-c.x)*uy-(p.y-c.y)*ux)<=numericalEpsilonMm&&along>=-numericalEpsilonMm&&along<=len+numericalEpsilonMm;};if(on(a)&&on(b))++hosts;}
  if(hosts!=1)continue;if(a.x>b.x||(a.x==b.x&&a.y>b.y))std::swap(a,b);const auto same=[&](const std::pair<Point,Point>& v){return std::hypot(v.first.x-a.x,v.first.y-a.y)<=numericalEpsilonMm&&std::hypot(v.second.x-b.x,v.second.y-b.y)<=numericalEpsilonMm;};if(std::none_of(solutions.begin(),solutions.end(),same))solutions.push_back({a,b});
 }
 if(solutions.size()!=1)return {};return solutions.front();
}
void readEndpointSupplement(std::istream& input,Plan& plan){
 std::string tag,hash,line;if(!std::getline(input,line))throw std::invalid_argument("missing endpoint supplement header");std::istringstream h(line);h>>tag>>hash;std::string extra;if(!h||h>>extra||tag!="S2NATIVEENDPOINTS1"||hash!=plan.sourceSha)throw std::invalid_argument("endpoint supplement source mismatch");std::size_t count=0;
 while(std::getline(input,line)){std::istringstream in(line);std::string handle;Point point;in>>handle>>point.x>>point.y;if(!in||in>>extra||!std::isfinite(point.x)||!std::isfinite(point.y)||++count>100000)throw std::invalid_argument("invalid endpoint supplement record");auto it=std::find_if(plan.openings.begin(),plan.openings.end(),[&](const Opening& o){return o.handle==handle&&o.observedKind=="G";});if(it==plan.openings.end())throw std::invalid_argument("endpoint supplement does not name actual group");it->endpointCandidates.push_back(point);}
 if(input.bad())throw std::invalid_argument("endpoint supplement read failure");
}
}
