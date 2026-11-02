#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "RuleModel.h"
#include <dbmain.h>
#include <memory>
namespace bce {
struct ReferenceMaterialSeed {
    AcDbDatabase* database{}; // Borrowed for filename only; prototypes/dependencies load from SHA-verified file bytes.
    std::string referenceSha256, prototypeId, handle;
    double mmPerSourceUnit{}, sourcePaperScale{};
    std::string unitConfirmationSource, scaleConfirmationSource, prototypeConfirmationSource;
    Point2 referenceAnchor;
    std::string phaseConfirmationSource;
};
struct MaterialPlacement {
    std::string regionIdentity, viewIdentity;
    double targetPaperScale{};
    Point2 targetAnchor;
    std::string scaleConfirmationSource, phaseConfirmationSource;
};
struct MaterialPreparationOptions { std::vector<MaterialPlacement> placements; };
class PreparedMaterialBatch final {
public:
    ~PreparedMaterialBatch();
    PreparedMaterialBatch(PreparedMaterialBatch&&) noexcept;
    PreparedMaterialBatch& operator=(PreparedMaterialBatch&&) noexcept;
    PreparedMaterialBatch(const PreparedMaterialBatch&) = delete;
    PreparedMaterialBatch& operator=(const PreparedMaterialBatch&) = delete;
    AcDbDatabase& database() const;
    const AcDbObjectIdArray& roots() const;
    bool intact() const noexcept;
    // Only an independently owned empty destination is accepted. Never a current-document writer.
    Result<std::vector<AcDbObjectId>> cloneToEmpty(AcDbDatabase&) const;
private:
    struct State;
    std::unique_ptr<State> state_;
    explicit PreparedMaterialBatch(std::unique_ptr<State>);
    friend Result<PreparedMaterialBatch> prepareMaterials(const MaterialPlan&, const std::vector<ReferenceMaterialSeed>&, const MaterialPreparationOptions&);
};
[[nodiscard]] Result<PreparedMaterialBatch> prepareMaterials(const MaterialPlan&, const std::vector<ReferenceMaterialSeed>&, const MaterialPreparationOptions&);
}
