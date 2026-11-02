#include "PlanRules.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv){
 try{
  if(argc!=3)throw std::runtime_error("expected frozen cache and confirmed exclusions");
  std::ifstream cache(argv[1]),exclusions(argv[2]);if(!cache||!exclusions)throw std::runtime_error("fixture inputs missing");
  auto plan=bcp::readCache(cache);bcp::readAxisExclusions(exclusions,plan);
  if(plan.sourceSha!="5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E")throw std::runtime_error("wrong frozen drawing fixture");
  // Axis support uses walls and room topology; opening endpoints are independent.
  plan.openings.clear();const auto result=bcp::analyse(plan);int checks=0,failed=0;
  const auto check=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::cerr<<"FAIL "<<name<<'\n';}};
  const auto axis=[&](const char* label,bool vertical)->const bcp::Axis&{const auto it=std::find_if(result.axes.begin(),result.axes.end(),[&](const bcp::Axis& a){return a.vertical==vertical&&a.confirmedLabel==label;});if(it==result.axes.end())throw std::runtime_error("fixture axis absent");return *it;};
  check(!axis("3",true).sides.negative,"bottom 3 behind bedroom facade must be hidden even at 1400 spacing");
  check(axis("3",true).sides.positive,"top 3 has actual local exterior support and remains visible");
  check(axis("2",true).sides.negative&&axis("4",true).sides.negative,"both actual bottom bedroom boundary axes retained");
  check(axis("A",false).sides.negative&&!axis("A",false).sides.positive,"confirmed A left only preserved");
  check(!axis("B",false).sides.negative&&axis("B",false).sides.positive,"confirmed B right only preserved");
  check(!axis("C",false).sides.positive,"same facade obstruction is checked on right C");
  check(!axis("H",false).sides.negative&&!axis("H",false).sides.positive,"same facade obstruction is checked on both H ends");
  std::vector<double> bottom;for(const auto& a:result.axes)if(a.vertical&&a.sides.negative)bottom.push_back(a.coordinate);
  const auto intervals=bcp::axisIntervals(bottom);std::vector<double> lengths;for(const auto p:intervals)lengths.push_back(p.second-p.first);
  const std::vector<double> wanted{1100,4200,3600,5000,3600};check(lengths.size()==wanted.size(),"bottom dimension chain omits hidden 3");
  bool values=lengths.size()==wanted.size();for(std::size_t i=0;i<lengths.size()&&i<wanted.size();++i)values=values&&std::abs(lengths[i]-wanted[i])<1e-6;check(values,"bottom chain recomputes 2800 plus 1400 as actual 4200");
  check(result.axes.size()==20,"logical axes and global numbering preserved");
  double xmin=1e100,ymin=1e100,xmax=-1e100,ymax=-1e100;for(const auto p:result.topology.outerBodyContours.at(0)){xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);ymin=std::min(ymin,p.y);ymax=std::max(ymax,p.y);}
  const auto exteriorChain=[&](bool vertical,bool negative,const std::vector<double>& wanted){std::vector<double> positions;for(const auto& a:result.axes)if(a.vertical==vertical&&(negative?a.sides.negative:a.sides.positive))positions.push_back(a.coordinate);const auto nodes=bcp::exteriorDimensionStations(positions,vertical?xmin:ymin,vertical?xmax:ymax,100);bool ok=nodes.size()==wanted.size()+1;for(std::size_t i=1;i<nodes.size()&&i<=wanted.size();++i)ok=ok&&std::abs(nodes[i].coordinate-nodes[i-1].coordinate-wanted[i-1])<1e-6;return ok&&nodes.front().role==bcp::DimensionPointRole::exteriorFace&&nodes.back().role==bcp::DimensionPointRole::exteriorFace;};
  check(exteriorChain(true,false,{4000,2200,4000,3800,3700}),"top second row covers exterior faces");
  check(exteriorChain(true,true,{1200,4200,3600,5000,3700}),"bottom second row covers exterior faces and still omits 3");
  check(exteriorChain(false,true,{4100,1800,1800,3600,4200,2900}),"left second row covers exterior faces");
  check(exteriorChain(false,false,{2100,5600,3600,5000,2100}),"right B retains independent lower 2100 to wall exterior");
  std::cout<<"facade checks="<<checks<<" failed="<<failed<<'\n';return failed?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
