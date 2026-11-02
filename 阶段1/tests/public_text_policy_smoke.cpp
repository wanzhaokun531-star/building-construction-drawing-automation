#include "PublicTextPatchPolicy.h"
#include <iostream>
int main(){
 using stage1::PublicTextToken;const auto absent=std::numeric_limits<std::size_t>::max();int failed=0;
 std::vector<PublicTextToken> good{{0,L"TCH_TEXT",true},{100,L"AcDbEntity",true},{100,L"TDbEntity",true},{100,L"TDbText",true},{1,L"courtyard",true}};
 if(stage1::publicTextSlot(good,L"courtyard")!=4)++failed;
 auto duplicate=good;duplicate.push_back({1,L"courtyard",true});if(stage1::publicTextSlot(duplicate,L"courtyard")!=absent)++failed;
 auto wrong=good;wrong[0].value=L"TEXT";if(stage1::publicTextSlot(wrong,L"courtyard")!=absent)++failed;
 auto missing=good;missing.erase(missing.begin()+3);if(stage1::publicTextSlot(missing,L"courtyard")!=absent)++failed;
 if(stage1::publicTextSlot(good,L"different")!=absent)++failed;
 auto nullValue=good;nullValue[4].valid=false;if(stage1::publicTextSlot(nullValue,L"courtyard")!=absent)++failed;
 std::cout<<"compiler="<<_MSC_VER<<" assertions=6 failed="<<failed<<"\n";return failed?1:0;
}
