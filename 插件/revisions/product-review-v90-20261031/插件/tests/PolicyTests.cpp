#include "../src/PluginPolicy.h"
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
 std::cout<<"policy checks="<<checks<<"\n";return checks==10?0:1;
}
