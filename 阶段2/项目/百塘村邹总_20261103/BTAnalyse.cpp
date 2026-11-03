#include "PlanRules.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int wmain(int argc,wchar_t** argv){try{if(argc!=3)return 2;std::ifstream f{std::filesystem::path(argv[1])};auto p=bcp::readCache(f);p.majorWallThickness=240;p.partitionWallThickness=120;p.partitionAxes=true;auto a=bcp::analyse(p);std::ofstream o{std::filesystem::path(argv[2])};o<<bcp::toJson(p,a);std::cout<<"axes="<<a.axes.size()<<" issues="<<a.issues.size()<<" complete="<<a.dimensionMeasurementsComplete<<"\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
