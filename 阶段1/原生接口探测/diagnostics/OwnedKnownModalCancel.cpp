// Cancel one exact known native test editor on a verified private CAD process.
// No COM/CUA, process suspension/termination, activation, or business generation.
#include <Windows.h>
#include <TlHelp32.h>
#include <wct.h>
#include <iostream>
#include <string>
#include <cstdlib>
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct DesktopProof {DWORD pid{};bool found{};};
BOOL CALLBACK ownedWindow(HWND hwnd,LPARAM data){auto& p=*reinterpret_cast<DesktopProof*>(data);DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);if(pid==p.pid)p.found=true;return TRUE;}
int wmain(int argc,wchar_t** argv){
    if(argc!=4)return 2;const auto pid=static_cast<DWORD>(wcstoul(argv[1],nullptr,10));const auto created=_wcstoui64(argv[2],nullptr,10);const std::wstring desktop=argv[3];
    if(!pid||!created||desktop.rfind(L"CodexS1_",0)!=0||desktop.size()!=40)return 3;
    Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid)};if(!process.value)return 4;
    wchar_t image[32768]{};DWORD size=32768;FILETIME c{},e{},k{},u{};if(!QueryFullProcessImageNameW(process.value,0,image,&size)||_wcsicmp(image,L"D:\\CAD2020\\AutoCAD 2020\\acad.exe")||!GetProcessTimes(process.value,&c,&e,&k,&u))return 5;
    ULARGE_INTEGER stamp{};stamp.LowPart=c.dwLowDateTime;stamp.HighPart=c.dwHighDateTime;if(stamp.QuadPart!=created)return 6;
    HDESK desk=OpenDesktopW(desktop.c_str(),0,FALSE,DESKTOP_READOBJECTS|DESKTOP_ENUMERATE);if(!desk)return 7;DesktopProof proof{pid};EnumDesktopWindows(desk,ownedWindow,reinterpret_cast<LPARAM>(&proof));CloseDesktop(desk);if(!proof.found)return 8;

    struct Match {DWORD pid;HWND dialog{};int count{};};Match match{pid};
    HDESK targetDesk=OpenDesktopW(desktop.c_str(),0,FALSE,DESKTOP_READOBJECTS|DESKTOP_ENUMERATE);if(!targetDesk)return 11;
    const auto find=[](HWND hwnd,LPARAM data)->BOOL{auto& m=*reinterpret_cast<Match*>(data);DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);wchar_t title[256]{},name[128]{};GetWindowTextW(hwnd,title,256);GetClassNameW(hwnd,name,128);if(pid==m.pid&&IsWindowVisible(hwnd)&&!wcscmp(name,L"#32770")&&!wcscmp(title,L"编辑索引文字")){m.dialog=hwnd;++m.count;}return TRUE;};
    EnumDesktopWindows(targetDesk,find,reinterpret_cast<LPARAM>(&match));if(match.count!=1){CloseDesktop(targetDesk);return 12;}
    const auto previousDesktop=GetThreadDesktop(GetCurrentThreadId());if(!SetThreadDesktop(targetDesk)){std::cerr<<"SetThreadDesktop error="<<GetLastError()<<"\n";CloseDesktop(targetDesk);return 16;}
    HWND cancel=GetDlgItem(match.dialog,2);wchar_t label[64]{},className[64]{};DWORD_PTR textResult{};if(!SendMessageTimeoutW(cancel,WM_GETTEXT,64,reinterpret_cast<LPARAM>(label),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&textResult)){std::cerr<<"WM_GETTEXT error="<<GetLastError()<<"\n";SetThreadDesktop(previousDesktop);CloseDesktop(targetDesk);return 15;}GetClassNameW(cancel,className,64);DWORD owner{};const auto thread=GetWindowThreadProcessId(cancel,&owner);DWORD dialogPid{};const auto dialogThread=GetWindowThreadProcessId(match.dialog,&dialogPid);
    if(!cancel||!IsWindowVisible(cancel)||!IsWindowEnabled(cancel)||owner!=pid||dialogPid!=pid||thread!=dialogThread||wcscmp(className,L"Button")||wcscmp(label,L"取消"))return 13;
    const bool posted=PostMessageW(cancel,BM_CLICK,0,0)!=FALSE;SetThreadDesktop(previousDesktop);CloseDesktop(targetDesk);
    std::cout<<"{\"contract\":\"stage1.owned-known-modal-cancel.v1\",\"pid\":"<<pid<<",\"identityVerified\":true,\"actualDialog\":"<<reinterpret_cast<unsigned long long>(match.dialog)<<",\"actualCancel\":"<<reinterpret_cast<unsigned long long>(cancel)<<",\"actualCancelId\":2,\"posted\":"<<(posted?"true":"false")<<",\"scope\":\"Only exact own private standard index editor Cancel; no CAD business writes or process termination\"}\n";return posted?0:14;
}
