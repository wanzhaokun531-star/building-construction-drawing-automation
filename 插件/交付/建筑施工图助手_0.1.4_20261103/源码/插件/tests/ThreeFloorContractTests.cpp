#include "../src/ThreeFloorContract.h"
#include "../src/GeometryComparison.h"
#include <fstream>
#include <sstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("layout and area paths required");
 if(bcpproduct::threeParametersMatch({{"height",1800}},{{"height",2000}})||bcpproduct::threeParametersMatch({{"thickness",120}},{{"thickness",100}})||bcpproduct::threeParametersMatch({{"height",1800}},{}))throw std::runtime_error("changed opening/wall parameter was accepted");
 if(!bcpproduct::samePublicGeometry("[32031.701528206282]","[32031.70152820628]"))throw std::runtime_error("DWG floating-point roundtrip incorrectly rejected");
 if(bcpproduct::samePublicGeometry("[32031.70]","[32031.71]")||bcpproduct::samePublicGeometry("{\"text\":\"1.00\"}","{\"text\":\"1.01\"}")||bcpproduct::samePublicGeometry("{\"color\":3}","{\"color\":25}"))throw std::runtime_error("actual geometry/property change accepted");
 std::ifstream a(argv[1]),b(argv[2]);auto layout=bcpproduct::readThreeLayout(a);auto areas=bcpproduct::readThreeAreas(b);
 if(layout.walls.size()!=126||layout.openings.size()!=47||areas.size()!=30)throw std::runtime_error("source associations incomplete");
 std::istringstream bad("BCPAREAPOLY1 POLY X 1 1 3 WALL 8 0");bool rejected=false;try{bcpproduct::readThreeAreas(bad);}catch(...){rejected=true;}if(!rejected)throw std::runtime_error("invalid edge accepted");
 bcpproduct::ThreeAreaPolygon p;p.coefficient=1;p.points={{{{"W",0,0},{"W",1,0}}},{{{"W",3,0},{"W",1,0}}},{{{"W",3,0},{"W",4,0}}},{{{"W",0,0},{"W",4,0}}}};
 std::map<std::string,std::array<double,6>> bounds{{"W",{0,0,0,4000,3000,0}}};if(std::abs(bcpproduct::evaluateThreeArea(p,bounds)-12)>1e-9)throw std::runtime_error("area evaluation");
 bounds["W"][3]=5000;if(std::abs(bcpproduct::evaluateThreeArea(p,bounds)-15)>1e-9)throw std::runtime_error("area did not follow geometry");
 for(auto i:{0,3})bounds["W"][i]+=1e7;for(auto i:{1,4})bounds["W"][i]-=1e7;if(std::abs(bcpproduct::evaluateThreeArea(p,bounds)-15)>1e-9)throw std::runtime_error("area translation");
 std::cout<<"Three-floor contracts and geometry-dependent area checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what();return 1;}}
