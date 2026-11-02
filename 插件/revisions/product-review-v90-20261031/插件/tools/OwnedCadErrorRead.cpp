// Read-only diagnostic for an exact owned private CAD fatal error dialog.
// No clicks, key injection, trust changes, process control, COM or CUA.
#include <Windows.h>
#include <string>
#include <iostream>
#include <cstdlib>
std::string json(const std::wstring& w){std::string s;const int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);s.resize(n);if(n)WideCharToMultiByte(CP_UTF8,0,w.c_str(),static_cast<int>(w.size()),&s[0],n,nullptr,nullptr);std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';if(c=='\n'){out+="\\n";continue;}if(c=='\r'){out+="\\r";continue;}if(c=='\t'){out+="\\t";continue;}if(c>=32)out+=static_cast<char>(c);}return out+'"';}
struct Find{DWORD pid{};HWND dialog{};int count{};};
BOOL CALLBACK find(HWND h,LPARAM data){auto& f=*reinterpret_cast<Find*>(data);DWORD pid{};GetWindowThreadProcessId(h,&pid);wchar_t title[256]{},type[64]{};GetWindowTextW(h,title,256);GetClassNameW(h,type,64);if(pid==f.pid&&IsWindowVisible(h)&&!wcscmp(type,L"#32770")&&!wcscmp(title,L"AutoCAD 错误中断")){f.dialog=h;++f.count;}return TRUE;}
struct Read{DWORD pid{};int count{};bool first{true};};
BOOL CALLBACK child(HWND h,LPARAM data){auto& r=*reinterpret_cast<Read*>(data);if(++r.count>128)return FALSE;DWORD pid{};GetWindowThreadProcessId(h,&pid);if(pid!=r.pid)return FALSE;wchar_t text[4096]{},type[128]{};DWORD_PTR result{};GetClassNameW(h,type,128);const bool ok=SendMessageTimeoutW(h,WM_GETTEXT,4096,reinterpret_cast<LPARAM>(text),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&result)!=0;if(!r.first)std::cout<<',';r.first=false;std::cout<<"{\"class\":"<<json(type)<<",\"id\":"<<GetDlgCtrlID(h)<<",\"visible\":"<<(IsWindowVisible(h)?"true":"false")<<",\"readSucceeded\":"<<(ok?"true":"false")<<",\"text\":"<<json(text)<<'}';return TRUE;}
int wmain(int argc,wchar_t** argv){
 if(argc!=4)return 2;const auto pid=static_cast<DWORD>(wcstoul(argv[1],nullptr,10));const auto expected=_wcstoui64(argv[2],nullptr,10);const std::wstring name=argv[3];if(!pid||!expected||name.size()!=40||name.rfind(L"CodexS1_",0)!=0)return 3;
 HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return 4;wchar_t image[32768]{};DWORD size=32768;FILETIME c{},e{},k{},u{};const bool read=QueryFullProcessImageNameW(p,0,image,&size)&&GetProcessTimes(p,&c,&e,&k,&u);CloseHandle(p);ULARGE_INTEGER stamp{};stamp.LowPart=c.dwLowDateTime;stamp.HighPart=c.dwHighDateTime;if(!read||stamp.QuadPart!=expected||_wcsicmp(image,L"D:\\CAD2020\\AutoCAD 2020\\acad.exe"))return 5;
 HDESK desk=OpenDesktopW(name.c_str(),0,FALSE,DESKTOP_READOBJECTS|DESKTOP_ENUMERATE);if(!desk)return 6;Find f{pid};EnumDesktopWindows(desk,find,reinterpret_cast<LPARAM>(&f));if(f.count!=1){CloseDesktop(desk);return 7;}const auto previous=GetThreadDesktop(GetCurrentThreadId());if(!SetThreadDesktop(desk)){CloseDesktop(desk);return 8;}
 Read r{pid};std::cout<<"{\"contract\":\"bcp.owned-cad-error-readonly.v1\",\"pid\":"<<pid<<",\"desktop\":"<<json(name)<<",\"identityVerified\":true,\"controls\":[";EnumChildWindows(f.dialog,child,reinterpret_cast<LPARAM>(&r));std::cout<<"],\"action\":\"read only; no approval, click or trust change\"}\n";SetThreadDesktop(previous);CloseDesktop(desk);return 0;
}
