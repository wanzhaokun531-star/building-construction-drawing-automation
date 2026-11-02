#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace bcpproduct {
struct FailureRecovery {bool diagnosticFailed=false,cleanupFailed=false,reportFailed=false;};
template<class Record,class Cleanup,class Report>
FailureRecovery recoverFailure(Record&& record,Cleanup&& cleanup,Report&& report) noexcept {
 FailureRecovery result;
 try{record();}catch(...){result.diagnosticFailed=true;}
 try{cleanup();}catch(...){result.cleanupFailed=true;}
 try{report();}catch(...){result.reportFailed=true;}
 return result;
}
void validateOutput(const std::filesystem::path& source,const std::filesystem::path& output);
void publishNewFile(const std::filesystem::path& ready,const std::filesystem::path& output);
std::string dimensionGroup(const std::string& row,const std::string& side,const std::string& sourceWall);
}
