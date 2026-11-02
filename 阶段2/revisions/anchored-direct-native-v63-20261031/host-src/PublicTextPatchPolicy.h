#pragma once
#include <string>
#include <vector>
#include <limits>
namespace stage1 {
struct PublicTextToken { int code{}; std::wstring value; bool valid{}; };
inline std::size_t publicSymbolSlot(const std::vector<PublicTextToken>& values,const std::wstring& dxf,const std::wstring& subclass,const std::wstring& expected){
 const auto absent=std::numeric_limits<std::size_t>::max();std::size_t selected=absent;int roots=0,ones=0;bool type=false,inBody=false;
 for(std::size_t i=0;i<values.size();++i){const auto& item=values[i];if(item.code==0){++roots;type=item.valid&&item.value==dxf;}if(item.code==100)inBody=item.valid&&item.value==subclass;if(item.code==1){++ones;if(inBody&&item.valid&&item.value==expected)selected=i;}}
 return roots==1&&type&&ones==1?selected:absent;
}
inline std::size_t publicTextSlot(const std::vector<PublicTextToken>& values,const std::wstring& expected) {
 const auto absent=std::numeric_limits<std::size_t>::max();std::size_t selected=absent;int count=0;bool type=false,inText=false;
 for(std::size_t i=0;i<values.size();++i){
  const auto& item=values[i];if(item.code==0)type=item.valid&&item.value==L"TCH_TEXT";
  if(item.code==100)inText=item.valid&&item.value==L"TDbText";
  if(item.code==1){++count;if(inText&&item.valid&&item.value==expected)selected=i;}
 }
 return type&&count==1?selected:absent;
}
}
