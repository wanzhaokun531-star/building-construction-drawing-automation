#include "../src/ApprovedLayoutContract.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
int main(){int tests=0;for(int n:{1,2}){std::ifstream in(std::filesystem::path(L"插件/resources/approved-layout")/(L"floor-"+std::to_wstring(n)+L".tsv"));auto c=bcpproduct::readApprovedLayout(in);if(c.floor!=n||c.groups.size()!=10||c.keep.empty()||c.clone.empty())return 1;++tests;std::ifstream raw(std::filesystem::path(L"插件/resources/approved-layout")/(L"floor-"+std::to_wstring(n)+L".tsv"));std::string text((std::istreambuf_iterator<char>(raw)),{});text+="UNEXPECTED";std::istringstream wrong(text);try{bcpproduct::readApprovedLayout(wrong);return 2;}catch(const std::runtime_error&){++tests;}}
 for(auto text:{"BCPAPPROVED1 0 00", "BAD 2 hash"}){std::istringstream in(text);try{bcpproduct::readApprovedLayout(in);return 3;}catch(const std::runtime_error&){++tests;}}
 std::cout<<tests<<" approved contract tests passed\n";
}
