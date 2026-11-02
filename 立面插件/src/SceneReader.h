#pragma once
#include "ElevationModel.h"
#include <memory>
class AcDbDatabase;
class AcDbObjectId;
class AcDbSymbolTableRecord;

namespace bce {
class NativeSceneContext final {
public:
    NativeSceneContext();
    ~NativeSceneContext();
    NativeSceneContext(const NativeSceneContext&) = delete;
    NativeSceneContext& operator=(const NativeSceneContext&) = delete;
    NativeSceneContext(NativeSceneContext&&) = delete;
    NativeSceneContext& operator=(NativeSceneContext&&) = delete;
    struct Impl;
    Impl& native() noexcept;
    AcDbDatabase* currentDatabase() const noexcept;
    std::string documentIdentity() const;
    std::uint64_t revision() const noexcept;
private:
    std::unique_ptr<Impl> impl_;
};
struct SceneRequest {
    NativeSceneContext& context;
    std::vector<std::string> targetHandles;
    std::optional<double> confirmedMmPerInputUnit;
};
[[nodiscard]] Result<SceneSnapshot> readScene(const SceneRequest& request);
namespace native_scene {
std::wstring utf16(const std::string& value);
std::string utf8(const std::wstring& value);
std::string handle(const AcDbObjectId& id);
std::string symbolName(const AcDbObjectId& id);
ResourceDefinition resourceDefinition(const AcDbSymbolTableRecord& record);
std::vector<ResourceDefinition> readResources(AcDbDatabase& database);
AcDbObjectId objectId(AcDbDatabase& database, const std::string& handle);
}
}
