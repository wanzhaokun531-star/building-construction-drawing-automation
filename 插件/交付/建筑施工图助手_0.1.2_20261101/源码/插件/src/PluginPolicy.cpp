#include "PluginPolicy.h"
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <Windows.h>
namespace bcpproduct {
namespace {std::wstring folded(std::filesystem::path path){auto value=std::filesystem::weakly_canonical(path).wstring();std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return value;}}
std::string dimensionGroup(const std::string& row,const std::string& side,const std::string& sourceWall){
 if(row=="opening-wall-chain"||row=="opening-end-face")return "third:"+side;
 if(row=="internal-opening-chain"||row=="local-opening-chain"){if(sourceWall.empty())throw std::invalid_argument("internal dimension wall is absent");return row+":"+sourceWall;}
 return row+":"+side;
}
void publishNewFile(const std::filesystem::path& ready,const std::filesystem::path& output){
 if(!std::filesystem::is_regular_file(ready)||folded(ready)==folded(output))throw std::runtime_error("invalid prepared output");
 // No REPLACE_EXISTING: Windows atomically refuses a competing destination.
 if(!MoveFileExW(ready.c_str(),output.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("cannot publish new DWG without overwriting; Windows error="+std::to_string(GetLastError()));
}
void validateOutput(const std::filesystem::path& source,const std::filesystem::path& output){
 if(source.empty()||!std::filesystem::is_regular_file(source))throw std::invalid_argument("saved input DWG is required");
 if(output.empty()||folded(source)==folded(output))throw std::invalid_argument("output must not replace the input drawing");
 auto extension=output.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
 if(extension!=L".dwg"||std::filesystem::exists(output))throw std::invalid_argument("choose a new DWG filename; existing files are never overwritten");
 if(!std::filesystem::is_directory(output.parent_path()))throw std::invalid_argument("output parent directory is absent");
}
}
