// N9a fixed-owned-path adaptation of existing N7 OwnedControlFile; sharing/read/hash contracts unchanged.
#include "OwnedTextControlFile.h"
#include <wincrypt.h>
#include <array>
#include <iomanip>
#include <sstream>
namespace bce::text_range {
using namespace bce::native;
namespace {
std::uint64_t join(DWORD high,DWORD low){return (static_cast<std::uint64_t>(high)<<32)|low;}
FileIdentity identity(HANDLE handle){
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info)){const auto e=GetLastError();require(false,"owned GetFileInformationByHandle failed error="+std::to_string(e));}
    return {info.dwVolumeSerialNumber,join(info.nFileIndexHigh,info.nFileIndexLow),join(info.nFileSizeHigh,info.nFileSizeLow),join(info.ftCreationTime.dwHighDateTime,info.ftCreationTime.dwLowDateTime),join(info.ftLastWriteTime.dwHighDateTime,info.ftLastWriteTime.dwLowDateTime),join(info.ftLastAccessTime.dwHighDateTime,info.ftLastAccessTime.dwLowDateTime),info.dwFileAttributes};
}
std::string metadata(const FileIdentity& v){return object({{"volume",std::to_string(v.volume)},{"fileIndex",std::to_string(v.index)},{"bytes",std::to_string(v.bytes)},{"created",std::to_string(v.created)},{"lastWrite",std::to_string(v.lastWrite)},{"lastAccess",std::to_string(v.lastAccess)},{"attributes",std::to_string(v.attributes)}});}
std::wstring finalPath(HANDLE file){
    std::wstring path(32768,L'\0');const auto size=GetFinalPathNameByHandleW(file,&path[0],static_cast<DWORD>(path.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!size){const auto e=GetLastError();require(false,"owned final path failed error="+std::to_string(e));}
    require(size<path.size(),"owned final path truncated");path.resize(size);
    if(path.compare(0,4,L"\\\\?\\")==0)path.erase(0,4);return path;
}
}
OwnedFileRead readOwnedTextControl(const std::wstring& session,const std::string& expectedSHA,std::uint64_t expectedSize,const FileObserver& observe) {
    const auto job=session+L"\\text-range-native-job";const auto path=job+L"\\control.dwg";
    const auto attrs=GetFileAttributesW(job.c_str());
    if(attrs==INVALID_FILE_ATTRIBUTES){const auto e=GetLastError();observe("ownedFile.directoryFailure",{{"path",quote(job)},{"error",std::to_string(e)}});require(false,"owned control directory unavailable error="+std::to_string(e));}
    require((attrs&FILE_ATTRIBUTE_DIRECTORY)!=0&&(attrs&FILE_ATTRIBUTE_REPARSE_POINT)==0,"owned control parent is not plain directory");
    // Diagnostic reproduction of the old sharing request; no read/write is attempted.
    {File strict(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
     const auto error=strict.valid()?ERROR_SUCCESS:GetLastError();observe("ownedFile.strictOpenObservation",{{"path",quote(path)},{"success",strict.valid()?"true":"false"},{"error",std::to_string(error)},{"access",std::to_string(GENERIC_READ)},{"share",std::to_string(FILE_SHARE_READ)}});}
    constexpr DWORD sharing=FILE_SHARE_READ|FILE_SHARE_WRITE;
    File file(CreateFileW(path.c_str(),GENERIC_READ,sharing,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    const auto openError=file.valid()?ERROR_SUCCESS:GetLastError();
    observe("ownedFile.open",{{"path",quote(path)},{"success",file.valid()?"true":"false"},{"error",std::to_string(openError)},{"access",std::to_string(GENERIC_READ)},{"share",std::to_string(sharing)}});
    require(file.valid(),"owned control read-only open failed error="+std::to_string(openError)+" path="+utf8(path));
    const auto actualPath=finalPath(file.get());require(_wcsicmp(path.c_str(),actualPath.c_str())==0,"owned control final path differs");
    const auto before=identity(file.get());require((before.attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))==0,"owned control is not a plain file");
    require(before.bytes==expectedSize&&expectedSize>0,"owned control size differs from prepared target");
    observe("ownedFile.beforeRead",{{"path",quote(actualPath)},{"metadata",metadata(before)}});
    HCRYPTPROV provider{};if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){const auto e=GetLastError();require(false,"owned SHA provider failed error="+std::to_string(e));}
    struct Provider {HCRYPTPROV value;~Provider(){CryptReleaseContext(value,0);}} providerScope{provider};
    HCRYPTHASH hash{};if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){const auto e=GetLastError();require(false,"owned SHA initialization failed error="+std::to_string(e));}
    struct Hash {HCRYPTHASH value;~Hash(){CryptDestroyHash(value);}} hashScope{hash};
    std::array<BYTE,65536> buffer{};std::uint64_t readBytes{};DWORD reads{};
    for(;;){DWORD count{};if(!ReadFile(file.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr)){
        const auto e=GetLastError();observe("ownedFile.readFailure",{{"error",std::to_string(e)},{"readBytes",std::to_string(readBytes)},{"readCalls",std::to_string(reads)}});require(false,"owned control ReadFile failed error="+std::to_string(e));}
        ++reads;if(count==0)break;readBytes+=count;require(readBytes<=expectedSize,"owned control grew during hash");
        if(!CryptHashData(hash,buffer.data(),count,0)){const auto e=GetLastError();require(false,"owned SHA update failed error="+std::to_string(e));}}
    std::array<BYTE,32> digest{};DWORD digestSize=static_cast<DWORD>(digest.size());
    if(!CryptGetHashParam(hash,HP_HASHVAL,digest.data(),&digestSize,0)){const auto e=GetLastError();require(false,"owned SHA finalize failed error="+std::to_string(e));}
    require(digestSize==digest.size(),"owned SHA size mismatch");std::ostringstream hex;for(auto b:digest)hex<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(b);
    observe("ownedFile.beforeAfterSnapshot",{{"readBytes",std::to_string(readBytes)},{"readCalls",std::to_string(reads)}});
    const auto after=identity(file.get());const auto sha=hex.str();const bool stable=before.sameStable(after)&&readBytes==before.bytes;
    const auto evidence=object({{"path",quote(actualPath)},{"before",metadata(before)},{"after",metadata(after)},{"readBytes",std::to_string(readBytes)},{"readCalls",std::to_string(reads)},{"sha256",quote(sha)},{"stableIdentitySizeCreationWriteTime",stable?"true":"false"},{"accessTimeCompared","false"},{"matchesPreparedSHA",sha==expectedSHA?"true":"false"}});
    observe("ownedFile.result",{{"observation",evidence}});
    require(stable,"owned control identity/size/creation/write time changed while hashing");require(sha==expectedSHA,"owned control SHA mismatch");
    return {sha,before,evidence};
}
}
