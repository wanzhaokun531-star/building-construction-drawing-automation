#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <sstream>
#include "../build/Payload.h"
namespace fs=std::filesystem;
namespace {
const wchar_t* title=L"建筑施工图助手 0.1.1 安装程序";
struct Algorithm { BCRYPT_ALG_HANDLE h{};~Algorithm(){if(h)BCryptCloseAlgorithmProvider(h,0);} };
struct Hash { BCRYPT_HASH_HANDLE h{};~Hash(){if(h)BCryptDestroyHash(h);} };
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
std::string digest(const std::vector<unsigned char>& bytes){
 Algorithm a;Hash h;require(BCryptOpenAlgorithmProvider(&a.h,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"SHA256 provider failed");
 require(BCryptCreateHash(a.h,&h.h,nullptr,0,nullptr,0,0)==0,"SHA256 allocation failed");
 require(bytes.size()<=0xFFFFFFFFull&&BCryptHashData(h.h,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)==0,"SHA256 input failed");
 unsigned char result[32]{};require(BCryptFinishHash(h.h,result,32,0)==0,"SHA256 failed");std::string out;const char* hex="0123456789ABCDEF";for(auto b:result){out+=hex[b>>4];out+=hex[b&15];}return out;
}
std::vector<unsigned char> fileBytes(const fs::path& p){std::ifstream in(p,std::ios::binary);require(static_cast<bool>(in),"Cannot read installed file");return {std::istreambuf_iterator<char>(in),{}};}
std::vector<unsigned char> resource(int id){auto module=GetModuleHandleW(nullptr);auto r=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);require(r!=nullptr,"Installer resource missing");auto h=LoadResource(module,r);const auto n=SizeofResource(module,r);const auto* data=static_cast<const unsigned char*>(LockResource(h));require(data&&n,"Installer resource empty");return {data,data+n};}
fs::path relative(const wchar_t* name){fs::path p(name);require(!p.empty()&&!p.is_absolute()&&!p.has_root_path(),"Invalid payload path");for(const auto& part:p)require(part!=L".."&&part!=L"."&&part.wstring().find(L':')==std::wstring::npos,"Invalid payload path");return p;}
bool installedMatches(const fs::path& root){try{for(const auto& f:payload)if(digest(fileBytes(root/relative(f.name)))!=f.sha)return false;return true;}catch(...){return false;}}
void cleanOwnedStage(const fs::path& stage)noexcept{
 // Only exact payload files in this newly-created staging directory are removed.
 // Unknown files or non-empty directories are preserved; no recursive deletion.
 try{std::vector<fs::path> dirs;for(const auto& f:payload){const auto p=stage/relative(f.name);DeleteFileW(p.c_str());for(auto d=p.parent_path();d!=stage&&d!=d.parent_path();d=d.parent_path())dirs.push_back(d);}std::sort(dirs.begin(),dirs.end(),[](const auto& a,const auto& b){return a.native().size()>b.native().size();});dirs.erase(std::unique(dirs.begin(),dirs.end()),dirs.end());for(const auto& d:dirs)RemoveDirectoryW(d.c_str());RemoveDirectoryW(stage.c_str());}catch(...){}
}
bool install(const fs::path& parent){
 require(parent.is_absolute(),"Install parent must be absolute");const auto target=parent/L"BuildingPlan2020.bundle";
 if(fs::exists(target)){require(installedMatches(target),u8"检测到已有但内容不同的插件目录，未覆盖。请先备份并移走旧目录后重试。");return true;}
 std::vector<std::vector<unsigned char>> data;for(const auto& f:payload){auto bytes=resource(f.id);require(digest(bytes)==f.sha,"Installer payload integrity check failed");data.push_back(std::move(bytes));}
 fs::create_directories(parent);unsigned long long nonce{};require(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0,"Cannot create staging identity");
 const auto stage=parent/(L".BCP-stage-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(nonce));require(fs::create_directory(stage),"Cannot create owned staging directory");
 try{for(std::size_t i=0;i<std::size(payload);++i){const auto path=stage/relative(payload[i].name);fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);out.exceptions(std::ios::failbit|std::ios::badbit);out.write(reinterpret_cast<const char*>(data[i].data()),static_cast<std::streamsize>(data[i].size()));out.close();}require(installedMatches(stage),"Written payload check failed");require(MoveFileExW(stage.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=0,u8"安装目录被占用或同时出现旧版本，未覆盖。请关闭CAD并检查目录。");}catch(...){cleanOwnedStage(stage);throw;}return false;
}
std::wstring wide(const std::string& s){const int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);if(!n)return L"安装失败，请检查目录权限。";std::wstring w(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&w[0],n);w.resize(n-1);return w;}
std::string jsonQuoted(const std::string& s){std::string out="\"";for(char c:s){if(c=='\\'||c=='"')out+='\\';if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else out+=c;}return out+'"';}
fs::path userParent(){wchar_t path[32768]{};const auto n=GetEnvironmentVariableW(L"APPDATA",path,32768);require(n>0&&n<32768,"Cannot find current user APPDATA");return fs::path(path)/L"Autodesk/ApplicationPlugins";}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_COMMAND&&LOWORD(wp)==101){EnableWindow(GetDlgItem(hwnd,101),FALSE);try{const bool existed=install(userParent());MessageBoxW(hwnd,existed?L"此版本已经安装，现有文件未改动。\n重新打开 AutoCAD 2020，输入 BCP_BUILD。":L"安装完成。\n重新打开 AutoCAD 2020，输入 BCP_BUILD。\n仍需已安装兼容天正组件；其他组件路径按使用说明配置。",title,MB_OK|MB_ICONINFORMATION);}catch(const std::exception& e){MessageBoxW(hwnd,wide(e.what()).c_str(),title,MB_OK|MB_ICONERROR);}EnableWindow(GetDlgItem(hwnd,101),TRUE);return 0;}
 if((msg==WM_COMMAND&&LOWORD(wp)==102)||msg==WM_CLOSE){DestroyWindow(hwnd);return 0;}if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);
}
int testInstall(const fs::path& root){
 try{require(root.is_absolute()&&fs::is_directory(root),"Test directory required");const auto marker=fileBytes(root/L".bcp-setup-test");require(std::string(marker.begin(),marker.end())=="BCPSETUPTEST1","Owned test marker required");}catch(...){return 3;}
 bool existed=false;std::string error;try{existed=install(root);}catch(const std::exception& e){error=e.what();}
 std::ofstream out(root/L"test-result.json",std::ios::binary);out<<"{\"ok\":"<<(error.empty()?"true":"false")<<",\"alreadyInstalled\":"<<(existed?"true":"false")<<",\"error\":"<<(error.empty()?"null":jsonQuoted(error))<<'}';return error.empty()?0:2;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 3;
 if(argc!=1){const bool test=argc==3&&std::wstring(argv[1])==L"--test-install";const fs::path root=test?fs::path(argv[2]):fs::path{};LocalFree(argv);return test?testInstall(root):3;}LocalFree(argv);
 try{const auto path=userParent()/L"BuildingPlan2020.bundle";WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=instance;cls.lpszClassName=L"BCPSetup011";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);require(RegisterClassW(&cls)!=0,"Cannot register installer window");
 auto window=CreateWindowExW(0,cls.lpszClassName,title,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,690,285,nullptr,nullptr,instance,nullptr);require(window!=nullptr,"Cannot create installer window");
 const auto text=std::wstring(L"安装建筑施工图助手 0.1.1（AutoCAD 2020 x64）\n\n安装到当前用户，无需管理员权限：\n")+path.wstring()+L"\n\n不修改CAD安全设置，不启动CAD；保留已有但不同的版本。";
 auto label=CreateWindowW(L"STATIC",text.c_str(),WS_CHILD|WS_VISIBLE,22,18,640,170,window,nullptr,instance,nullptr);SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
 for(const auto id:{101,102}){auto b=CreateWindowW(L"BUTTON",id==101?L"开始安装":L"关闭",WS_CHILD|WS_VISIBLE|WS_TABSTOP|(id==101?BS_DEFPUSHBUTTON:0),id==101?420:545,204,110,30,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);SendMessageW(b,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);}ShowWindow(window,show);UpdateWindow(window);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return 0;
 }catch(const std::exception& e){MessageBoxW(nullptr,wide(e.what()).c_str(),title,MB_OK|MB_ICONERROR);return 2;}
}
