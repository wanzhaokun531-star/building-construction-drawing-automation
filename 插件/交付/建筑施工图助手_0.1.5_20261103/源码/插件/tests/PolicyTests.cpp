#include "../src/PluginPolicy.h"
#include "../../阶段2/src/DoorCaptionLayout.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;const auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root);const auto source=root/L"input.dwg",output=root/L"result.dwg";std::ofstream(source)<<"source";int checks=0;
 bcpproduct::validateOutput(source,output);++checks;
 for(const auto& pair:std::vector<std::pair<std::filesystem::path,std::filesystem::path>>{{source,source},{source,root/L"INPUT.DWG"},{root/L"missing.dwg",output},{source,root/L"bad.txt"}}){try{bcpproduct::validateOutput(pair.first,pair.second);throw std::logic_error("bad path accepted");}catch(const std::invalid_argument&){++checks;}}
 std::ofstream(output)<<"existing";try{bcpproduct::validateOutput(source,output);return 3;}catch(const std::invalid_argument&){++checks;}
 if(bcpproduct::dimensionGroup("opening-wall-chain","top","CD88")!="third:top"||bcpproduct::dimensionGroup("opening-end-face","top","")!="third:top"||bcpproduct::dimensionGroup("internal-opening-chain","left","CDB7")!="internal-opening-chain:CDB7")return 4;++checks;
 const auto candidate=root/L"ready.dwg",published=root/L"published.dwg";std::ofstream(candidate)<<"generated";bcpproduct::publishNewFile(candidate,published);if(std::filesystem::exists(candidate)||!std::filesystem::exists(published))return 5;++checks;
 std::ofstream(candidate)<<"new candidate";try{bcpproduct::publishNewFile(candidate,published);return 6;}catch(const std::runtime_error&){++checks;}
 std::ifstream old(published);std::string contents;std::getline(old,contents);if(contents!="generated"||!std::filesystem::exists(candidate))return 7;++checks;
 bool jobAlive=true;int actions=0;
 const auto recovery=bcpproduct::recoverFailure([&]{actions|=1;std::ofstream file;file.exceptions(std::ios::failbit|std::ios::badbit);file.open(root);},[&]{actions|=2;jobAlive=false;},[&]{actions|=4;});
 if(!recovery.diagnosticFailed||recovery.cleanupFailed||jobAlive||actions!=7)return 8;++checks;
 bool released=false;const auto printFailure=bcpproduct::recoverFailure([]{},[&]{released=true;},[]{throw std::runtime_error("console unavailable");});if(!released||!printFailure.reportFailed)return 9;++checks;
 bcpdoor::Stroke arc;arc.arc=true;arc.cx=0;arc.cy=0;arc.radius=900;arc.angle=0;arc.sweep=1.5707963267948966;
 if(bcpdoor::hits({250,50,500,150},arc))return 10;++checks;
 if(!bcpdoor::hits({600,600,700,700},arc))return 11;++checks;
 if(bcpdoor::hits({-910,-20,-880,20},arc))return 12;++checks;
 bcpdoor::Stroke line;line.x1=0;line.y1=0;line.x2=0;line.y2=900;if(!bcpdoor::hits({-80,100,80,500},line)||bcpdoor::hits({100,100,400,500},line))return 13;++checks;
 const auto candidates= bcpdoor::candidates({0,0},{0,900},-1,300,700,{ -900,0,0,900 });if(candidates.empty()||candidates.front().x>=0||candidates.front().y>=0)return 14;++checks;
 arc.angle=1.5707963267948966;arc.sweep=-1.5707963267948966;if(!bcpdoor::hits({600,600,700,700},arc)||bcpdoor::hits({-910,-20,-880,20},arc))return 15;++checks;
 arc.angle=0;arc.sweep=1.54;if(bcpdoor::hits({-1,898,1,902},arc)||!bcpdoor::hits({898,-1,902,1},arc))return 16;++checks;
 bool kept=false;auto safe=bcpdoor::safeCandidates(std::vector<bcpdoor::Point>{{0,0}},bcpdoor::Point{200,0},[](bcpdoor::Point p){return p.x>100;},kept);if(!kept||safe.size()!=1||safe[0].x!=200)return 17;++checks;
 safe=bcpdoor::safeCandidates(std::vector<bcpdoor::Point>{{150,0}},bcpdoor::Point{200,0},[](bcpdoor::Point p){return p.x>100;},kept);if(kept||safe.size()!=1||safe[0].x!=150)return 18;++checks;
 safe=bcpdoor::safeCandidates(std::vector<bcpdoor::Point>{{0,0}},bcpdoor::Point{0,0},[](bcpdoor::Point){return false;},kept);if(!safe.empty())return 19;++checks;
 arc.angle=0;arc.sweep=6.283185307179586;if(!bcpdoor::hits({-910,-20,-880,20},arc))return 20;++checks;
 // Generation reads the saved DWG. Unsaved entities/fields must still block it.
 for(int state:{0,4,8,16,20,24,28}){if(!bcpproduct::canGenerateSavedDrawing(state)){std::cerr<<"saved drawing rejected: DBMOD="<<state<<"\n";return 21;}++checks;}
 for(int state:{1,2,5,17,21,29,32,36,48,64,-1}){if(bcpproduct::canGenerateSavedDrawing(state)){std::cerr<<"unsaved content accepted: DBMOD="<<state<<"\n";return 22;}++checks;}
 std::cout<<"policy checks="<<checks<<"\n";return checks==41?0:1;
}
