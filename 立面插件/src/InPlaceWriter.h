#pragma once
#include "PreparedBatch.h"
#include "SceneReader.h"
namespace bce {
class InPlaceWriter final {
public:
    explicit InPlaceWriter(NativeSceneContext& context, CommitObserver* observer = nullptr) noexcept
        : context_(context), observer_(observer) {}
    [[nodiscard]] Result<WriteScopePlan> beginWriteScope(const LayoutPlan& plan, const SceneSnapshot& snapshot);
    [[nodiscard]] Result<CommitReceipt> applyToCurrentDocument(
        PreparedBatch& batch, const LayoutPlan& plan, const SceneSnapshot& snapshot);
private:
    NativeSceneContext& context_;
    CommitObserver* observer_;
};
}
