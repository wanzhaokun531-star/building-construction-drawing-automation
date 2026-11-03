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
struct MaterialDatabaseDeleter { void operator()(AcDbDatabase*) const noexcept; };
struct OwnedMaterialDatabase {
    std::unique_ptr<AcDbDatabase,MaterialDatabaseDeleter> database;
    AcDbObjectIdArray roots;
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
    Result<OwnedMaterialDatabase> cloneIntoNewOwnedDb() const;
private:
    struct State;
    std::unique_ptr<State> state_;
    explicit PreparedMaterialTiles(std::unique_ptr<State>);
    friend Result<PreparedMaterialTiles> prepareMaterialTiles(const MaterialPlan&,const std::vector<ReferenceMaterialSeed>&,const MaterialPreparationOptions&,const MaterialTileLimits&);
};
[[nodiscard]] Result<PreparedMaterialTiles> prepareMaterialTiles(const MaterialPlan&,const std::vector<ReferenceMaterialSeed>&,const MaterialPreparationOptions&,const MaterialTileLimits& = {});
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
namespace material_tiles_diagnostics {
enum class OwnedFault { AfterFirstTile, AfterSecondTile, AfterExportClone, BeforeExportPublish };
enum class ResourceConflict { LayerColor, DictionaryKey, XrecordPayload, XrecordPolicy, ReusedOwnedKey, ReusedOwnedPayload, ReusedOwnedOwner, ReferenceAlias };
struct OwnedProbe {bool noResult{},destroyedAll{},workingRestored{},preflightUnchanged{};std::size_t created{},destroyed{};std::string issue,bindings;std::vector<std::pair<std::string,bool>> typedChecks;};
OwnedProbe ownershipFault(const MaterialPlan&,const std::vector<ReferenceMaterialSeed>&,const MaterialPreparationOptions&,OwnedFault);
OwnedProbe resourceConflict(const PreparedMaterialTiles&,ResourceConflict);
OwnedProbe typedXrecordValues(const PreparedMaterialTiles&);
}
#endif
}
