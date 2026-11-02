#include "PublicTextPatchPolicy.h"
#include <iostream>
int main(){
 using stage1::PublicTextToken;const auto absent=std::numeric_limits<std::size_t>::max();int failed=0;
 std::vector<PublicTextToken> good{{0,L"TCH_ELEVATION",true},{100,L"AcDbEntity",true},{100,L"TDbEntity",true},{100,L"TDbSymbWithText",true},{7,L"_TCH_DIM",true},{100,L"TDbSymbElevation",true},{1,L"-0.030",true}};
 auto select=[](const std::vector<PublicTextToken>& v){return stage1::publicSymbolSlot(v,L"TCH_ELEVATION",L"TDbSymbElevation",L"-0.030");};
 if(select(good)!=6)++failed;auto dupe=good;dupe.push_back({1,L"-0.030",true});if(select(dupe)!=absent)++failed;
 auto wrong=good;wrong[0].value=L"TCH_TEXT";if(select(wrong)!=absent)++failed;wrong=good;wrong[5].value=L"TDbSymbSection";if(select(wrong)!=absent)++failed;
 wrong=good;wrong[6].value=L"-0.031";if(select(wrong)!=absent)++failed;wrong=good;wrong[6].valid=false;if(select(wrong)!=absent)++failed;
 std::cout<<"compiler="<<_MSC_VER<<" assertions=6 failed="<<failed<<"\n";return failed?1:0;
}
