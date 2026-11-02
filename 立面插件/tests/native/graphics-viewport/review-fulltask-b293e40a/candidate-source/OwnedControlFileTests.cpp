#include "OwnedControlFile.h"
#include <iostream>
using namespace bce::native;
using namespace bce::viewport;
int main(){
    unsigned checks{},failed{};auto check=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::cout<<"FAIL "<<name<<"\n";}};
    try {
        wchar_t cwd[32768]{};require(GetCurrentDirectoryW(32768,cwd)>0,"test cwd");
        const auto session=std::wstring(cwd)+L"\\owned-test-"+std::to_wstring(GetCurrentProcessId());
        require(CreateDirectoryW(session.c_str(),nullptr)!=FALSE,"create owned test directory");
        const auto job=session+L"\\viewport-native-job";require(CreateDirectoryW(job.c_str(),nullptr)!=FALSE,"create job");
        const auto path=job+L"\\control.dwg";
        File writer(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));require(writer.valid(),"writer fixture");write_flushed(writer.get(),"abc");
        const std::string abc="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";
        bool strictFailed=false;try{sha256(path);}catch(const std::exception&){strictFailed=true;}check(strictFailed,"strict shared-source helper rejects existing writer (fixture only)");
        std::vector<std::string> observed;
        const FileObserver observer=[&](const std::string& name,const Fields&){observed.push_back(name);};
        try{const auto value=readOwnedControl(session,abc,3,observer);check(value.sha==abc&&value.identity.bytes==3,"owned reader hashes file with existing writer");}
        catch(const std::exception& e){std::cout<<"OBSERVED "<<e.what()<<"\n";check(false,"owned reader hashes file with existing writer");}
        writer=File();
        bool wrong=false;try{readOwnedControl(session,std::string(64,'0'),3,observer);}catch(const std::exception&){wrong=true;}check(wrong,"wrong prepared SHA rejected");
        bool size=false;try{readOwnedControl(session,abc,4,observer);}catch(const std::exception&){size=true;}check(size,"wrong prepared size rejected");
        bool changed=false;
        try{readOwnedControl(session,abc,3,[&](const std::string& event,const Fields&){if(event=="ownedFile.beforeAfterSnapshot"){
            File change(CreateFileW(path.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));require(change.valid(),"timestamp mutation handle");FILETIME time{100,200};require(SetFileTime(change.get(),nullptr,nullptr,&time)!=FALSE,"timestamp mutation");}});}
        catch(const std::exception&){changed=true;}check(changed,"metadata mutation during hash rejected");
        bool missing=false;try{readOwnedControl(session+L"-missing",abc,3,observer);}catch(const std::exception&){missing=true;}check(missing,"missing owned path rejected");
    }catch(const std::exception& e){std::cout<<"SETUP_FAILURE "<<e.what()<<"\n";return 2;}
    std::cout<<"OWNED_FILE_CHECKS="<<checks<<" FAILED="<<failed<<"\n";return failed?1:0;
}
