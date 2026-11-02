// Read-only Windows diagnostics for one exact nonce-owned CAD process.
// No process suspension/termination, COM, activation, CAD calls, or window messages.
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
    HWCT session=OpenThreadWaitChainSession(0,nullptr);if(!session)return 9;
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0)};if(snapshot.value==INVALID_HANDLE_VALUE){CloseThreadWaitChainSession(session);return 10;}
    std::cout<<"{\"contract\":\"stage1.owned-wait-chain-readonly.v1\",\"pid\":"<<pid<<",\"identityVerified\":true,\"privilegeAdjusted\":false,\"threads\":[";
    THREADENTRY32 entry{};entry.dwSize=sizeof(entry);bool first=true;
    if(Thread32First(snapshot.value,&entry))do{if(entry.th32OwnerProcessID!=pid)continue;
        WAITCHAIN_NODE_INFO nodes[WCT_MAX_NODE_COUNT]{};DWORD count=WCT_MAX_NODE_COUNT;BOOL cycle{};SetLastError(0);const BOOL ok=GetThreadWaitChain(session,0,0,entry.th32ThreadID,&count,nodes,&cycle);const DWORD error=ok?0:GetLastError();
        if(!first)std::cout<<',';first=false;std::cout<<"{\"thread\":"<<entry.th32ThreadID<<",\"read\":"<<(ok?"true":"false")<<",\"error\":"<<error<<",\"cycle\":"<<(cycle?"true":"false")<<",\"nodes\":[";
        const DWORD available=count<WCT_MAX_NODE_COUNT?count:WCT_MAX_NODE_COUNT;
        for(DWORD i=0;i<available&&(ok||error==ERROR_MORE_DATA);++i){if(i)std::cout<<',';const auto& n=nodes[i];std::cout<<"{\"type\":"<<n.ObjectType<<",\"status\":"<<n.ObjectStatus;
            if(n.ObjectType==WctThreadType)std::cout<<",\"pid\":"<<n.ThreadObject.ProcessId<<",\"tid\":"<<n.ThreadObject.ThreadId<<",\"waitTime\":"<<n.ThreadObject.WaitTime;
            std::cout<<'}';}std::cout<<"]}";
    }while(Thread32Next(snapshot.value,&entry));
    std::cout<<"],\"mutationAttempted\":false}\n";CloseThreadWaitChainSession(session);return 0;
}
