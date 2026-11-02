#include "PlanRules.h"
#include "../src/ProjectInput.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
int wmain(int argc,wchar_t** argv){using namespace bcp;Plan p;p.majorWallThickness=240;p.partitionWallThickness=120;p.partitionAxes=true;
p.walls={{"A",{0,0},{0,4000},120,120,WallRole::majorInterior},{"B",{2000,0},{2000,4000},60,60,WallRole::interiorPartition},{"C",{4000,0},{4000,200},60,60,WallRole::doorPier},{"D",{6000,0},{6000,200},120,120,WallRole::localShort}};
auto r=analyse(p);if(r.axes.size()!=2||!r.axes[0].confirmed||!r.axes[1].confirmed)return 1;
p.partitionAxes=false;if(analyse(p).axes.size()!=1)return 2;
Plan old;old.walls={{"A",{0,0},{0,4000},100,100,WallRole::majorInterior},{"B",{2000,0},{2000,4000},50,50,WallRole::interiorPartition}};if(analyse(old).axes.size()!=1)return 3;
const auto faces=axisFacadeForContours({{{0,0},{4000,0},{4000,4000},{0,4000}},{{10000,0},{14000,0},{14000,4000},{10000,4000}}},true,12000,0,4000,120);if(!faces.resolved||!faces.negative||!faces.positive)return 5;
if(clearFaceStations({0,2080,2880,3000},120,120)!=std::vector<double>({120,2080,2880}))return 7;
Plan bathroom;bathroom.majorWallThickness=240;bathroom.partitionWallThickness=120;bathroom.partitionAxes=true;bathroom.walls={{"A",{0,0},{5000,0},120,120},{"B",{5000,0},{5000,4000},120,120},{"C",{5000,4000},{0,4000},120,120},{"D",{0,4000},{0,0},120,120},{"E",{2500,0},{2500,4000},60,60}};bathroom.rooms={{"F",{900,1900},{1100,2100},u8"公卫"}};auto bathResult=analyse(bathroom);auto omitted=bathroomPartitionAnnotations(bathroom,bathResult.topology);if(std::find(omitted.begin(),omitted.end(),"E")==omitted.end()||std::none_of(bathResult.axes.begin(),bathResult.axes.end(),[](const Axis& a){return a.confirmed&&a.vertical&&a.coordinate==2500;}))return 8;
Plan two=old;for(auto w:old.walls){w.handle+="2";w.start.x+=50000;w.end.x+=50000;two.walls.push_back(w);}auto parts=bcpproduct::splitPlanPanels(two,2);if(parts[0].xmin>=-100||parts[1].xmax<=52100)return 6;
if(parts.size()!=2||parts[0].plan.walls.size()!=2||parts[1].plan.walls.size()!=2)return 4;
if(argc==2){std::ifstream in{std::filesystem::path(argv[1])};auto source=readCache(in);source.majorWallThickness=240;source.partitionWallThickness=120;source.partitionAxes=true;auto panels=bcpproduct::splitPlanPanels(source,2);for(std::size_t i=0;i<panels.size();++i){const auto r=analyse(panels[i].plan);std::ofstream file(std::filesystem::path(L"插件/evidence/current-floor-"+std::to_wstring(i+1)+L".json"));file<<toJson(panels[i].plan,r);std::cout<<"panel="<<i+1<<" walls="<<panels[i].plan.walls.size()<<" openings="<<panels[i].plan.openings.size()<<" contours="<<r.topology.outerBodyContours.size()<<" axes="<<r.axes.size()<<" issues="<<r.issues.size()<<'\n';}}
std::cout<<"project-input checks=4\n";return 0;}
