#pragma once
#include <string>
namespace bcpproduct {
struct ModelCounts {int objects{},walls{},openings{};};
bool sameWindowsFolder(const std::wstring& a,const std::wstring& b);
std::wstring decodeMessage(const std::string& text);
std::string cacheTextHex(const std::string& text);
std::string countDifference(ModelCounts actual,ModelCounts expected);
}
