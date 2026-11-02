#pragma once
#include <string>
#include <map>
#include <sstream>
#include <locale>
#include <stdexcept>
#include <cmath>
namespace bcpproduct {
using PublicProperties=std::map<std::wstring,std::wstring>;
inline std::wstring propertyTrim(const std::wstring& text){const auto a=text.find_first_not_of(L" \t\r\n");if(a==std::wstring::npos)return {};return text.substr(a,text.find_last_not_of(L" \t\r\n")-a+1);}
inline PublicProperties parseProperties(const std::wstring& text){PublicProperties fields;std::wistringstream stream(text);std::wstring line;while(std::getline(stream,line)){const auto at=line.find_first_of(L":：");if(at==std::wstring::npos)continue;auto key=propertyTrim(line.substr(0,at)),value=propertyTrim(line.substr(at+1));if(key.empty())continue;auto old=fields.find(key);if(old!=fields.end()&&old->second!=value)throw std::invalid_argument("conflicting public property");fields[key]=value;}return fields;}
inline double propertyNumber(const PublicProperties& fields,const std::wstring& key){auto it=fields.find(key);if(it==fields.end())throw std::invalid_argument("required public property missing");std::wistringstream input(it->second);input.imbue(std::locale::classic());double value{};std::wstring extra;if(!(input>>value)||!std::isfinite(value)||(input>>extra))throw std::invalid_argument("invalid public numeric property");return value;}
}
