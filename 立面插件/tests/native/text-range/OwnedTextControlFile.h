// N9a fixed-owned-path adaptation of existing N7 OwnedControlFile; sharing/read/hash contracts unchanged.
#pragma once
#include "NativeEvidence.h"
#include <functional>
#include <cstdint>
namespace bce::text_range {
struct FileIdentity {
    std::uint64_t volume{},index{},bytes{},created{},lastWrite{},lastAccess{};
    DWORD attributes{};
    bool sameStable(const FileIdentity& rhs) const noexcept {
        return volume==rhs.volume&&index==rhs.index&&bytes==rhs.bytes&&created==rhs.created&&lastWrite==rhs.lastWrite&&attributes==rhs.attributes;
    }
};
struct OwnedFileRead {std::string sha;FileIdentity identity;std::string evidence;};
using FileObserver=std::function<void(const std::string&,const bce::native::Fields&)>;
OwnedFileRead readOwnedTextControl(const std::wstring& session,const std::string& expectedSHA,
    std::uint64_t expectedSize,const FileObserver& observer);
}
