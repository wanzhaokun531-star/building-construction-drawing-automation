#pragma once
#include "ProjectStore.h"
namespace bce {
// A units-only archive never confers a geometry or generation qualification.
bool isUnitOnlyProject(const StoredProject&) noexcept;
Result<StoreReceipt> configureProjectUnits(NativeSceneContext&, double);
}
