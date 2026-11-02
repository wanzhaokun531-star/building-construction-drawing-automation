#pragma once
#include "NativeMaterials.h"
namespace bce {
struct MaterialTileLimits { std::size_t maxTiles{2048}; unsigned maxDepth{16}; std::size_t maxAttempts{4096}; };
struct MaterialTileRecord {
    std::string parentIdentity, viewIdentity;
    PolygonRegion geometry;
    double nativeArea{};
    int nativeLines{};
    unsigned depth{};
};
class PreparedMaterialTiles final {
public:
    ~PreparedMaterialTiles();
    PreparedMaterialTiles(PreparedMaterialTiles&&) noexcept;
    PreparedMaterialTiles& operator=(PreparedMaterialTiles&&) noexcept;
    PreparedMaterialTiles(const PreparedMaterialTiles&)=delete;
    PreparedMaterialTiles& operator=(const PreparedMaterialTiles&)=delete;
    AcDbDatabase& database() const;
    const AcDbObjectIdArray& roots() const;
    const std::vector<MaterialTileRecord>& tiles() const;
    const MaterialPlan& parentPlan() const;
    bool intact() const noexcept;
    Result<std::vector<AcDbObjectId>> cloneToEmpty(AcDbDatabase&) const;
private:
    struct State;
    std::unique_ptr<State> state_;
    explicit PreparedMaterialTiles(std::unique_ptr<State>);
    friend Result<PreparedMaterialTiles> prepareMaterialTiles(const MaterialPlan&,const std::vector<ReferenceMaterialSeed>&,const MaterialPreparationOptions&,const MaterialTileLimits&);
};
[[nodiscard]] Result<PreparedMaterialTiles> prepareMaterialTiles(const MaterialPlan&,const std::vector<ReferenceMaterialSeed>&,const MaterialPreparationOptions&,const MaterialTileLimits& = {});
}
