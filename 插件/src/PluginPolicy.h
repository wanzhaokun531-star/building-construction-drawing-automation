#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace bcpproduct {
// The input is the saved DWG, including its saved database variables. Never
// auto-save the live document or reset DBMOD. Entity/field/unknown bits block.
inline bool canGenerateSavedDrawing(int dbmod) noexcept {
 constexpr int savedInputOnlyChanges = 4 | 8 | 16;
 return dbmod >= 0 && (dbmod & ~savedInputOnlyChanges) == 0;
}
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
