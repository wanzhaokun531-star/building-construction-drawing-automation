#pragma once
#include "ElevationModel.h"
#include <string>
#include <vector>

namespace bce {
struct ViewTranslation {
    EntityKey target;
    Point2 displacement;
};
struct LayoutPlan {
    std::string viewIdentity;
    std::vector<ViewTranslation> translations;
    std::vector<ResourceDefinition> requiredResources;
};
struct CommitReceipt {
    std::string documentIdentity;
    std::vector<EntityKey> createdEntities;
    std::vector<std::string> createdResourceHandles;
    bool committed{false};
};
enum class CommitStage { resourcesCreated, entitiesAppended, targetsTranslated };
// Host-neutral progress/cancellation boundary. Exceptions are converted to failed Results.
class CommitObserver {
public:
    virtual ~CommitObserver() = default;
    virtual bool continueCommit(CommitStage stage) = 0;
};
}
