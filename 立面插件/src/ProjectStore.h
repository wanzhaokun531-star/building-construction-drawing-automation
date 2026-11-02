#pragma once
#include "ProjectStoreCodec.h"
#include "SceneReader.h"
class AcDbDatabase;
namespace bce {
inline constexpr wchar_t projectStoreDictionary[] = L"BCE_ELEVATION_PROJECT_8D5BB4A2";
inline constexpr wchar_t projectStoreRecord[] = L"PROJECT_ARCHIVE";
enum class StoreStage { beforeDictionary, dictionaryReady, beforeRecord, recordWritten };
class StoreObserver {
public:
    virtual ~StoreObserver() = default;
    virtual bool proceed(StoreStage) = 0;
};
struct StoreRequest {
    std::string expectedDocumentIdentity;
    std::uint64_t expectedRevision{0};
    ProjectArchive archive;
};
struct StoreReceipt { bool committed{false}; std::string persistentProjectId; };
// Read-only, including a SideDB reopened specifically by a test. Missing returns
// PROJECT_NOT_FOUND. Archives always remain unbound.
[[nodiscard]] Result<ProjectArchive> readProjectStore(AcDbDatabase&);
// Exact raw rbChain bytes decoded from our bounded ASCII-hex envelope.
[[nodiscard]] Result<std::string> readProjectStoreBytes(AcDbDatabase&);
// Standalone modal command only. Refuses caller transactions and caller Undo
// groups; never saves/closes/reopens. Future whole-build integration is separate.
[[nodiscard]] Result<StoreReceipt> writeProjectStore(NativeSceneContext&,
    const StoreRequest&, StoreObserver* observer = nullptr);
}
