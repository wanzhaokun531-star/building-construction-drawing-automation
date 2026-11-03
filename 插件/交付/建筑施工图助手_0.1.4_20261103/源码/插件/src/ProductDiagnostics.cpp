#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ProductDiagnostics.h"
#include <filesystem>
namespace bcpproduct {
std::string cacheTextHex(const std::string& text){static constexpr char digits[]="0123456789ABCDEF";std::string out;out.reserve(text.size()*2);for(unsigned char c:text){out+=digits[c>>4];out+=digits[c&15];}return out;}
bool sameWindowsFolder(const std::wstring& a,const std::wstring& b){const auto normal=[](const std::wstring& p){auto value=std::filesystem::path(p).lexically_normal().make_preferred().wstring();while(value.size()>3&&(value.back()==L'\\'||value.back()==L'/'))value.pop_back();return value;};return _wcsicmp(normal(a).c_str(),normal(b).c_str())==0;}
std::wstring decodeMessage(const std::string& text){
 if(text.empty())return {};
 UINT page=CP_UTF8;DWORD flags=MB_ERR_INVALID_CHARS;int n=MultiByteToWideChar(page,flags,text.data(),static_cast<int>(text.size()),nullptr,0);
 if(!n){page=CP_ACP;flags=0;n=MultiByteToWideChar(page,flags,text.data(),static_cast<int>(text.size()),nullptr,0);}
 if(!n)return L"错误信息无法解码，请查看诊断日志。";
 std::wstring result(n,L'\0');MultiByteToWideChar(page,flags,text.data(),static_cast<int>(text.size()),&result[0],n);return result;
}
std::string countDifference(ModelCounts actual,ModelCounts expected){
 std::string result;
 const auto append=[&](const char* name,int got,int want){if(got==want)return;if(!result.empty())result+=u8"；";result+=name;result+=u8"：实际 "+std::to_string(got)+u8"，已确认方案 "+std::to_string(want);};
 append(u8"模型对象",actual.objects,expected.objects);append(u8"天正墙体",actual.walls,expected.walls);append(u8"天正门窗",actual.openings,expected.openings);return result;
}
}
