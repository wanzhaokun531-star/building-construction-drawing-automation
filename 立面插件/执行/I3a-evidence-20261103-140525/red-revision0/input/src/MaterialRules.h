#ifndef BCE_MATERIAL_RULES_H
#define BCE_MATERIAL_RULES_H
#include "Recognition.h"
namespace bce {
[[nodiscard]] Result<MaterialPlan> planMaterials(const FeatureModel&, const ReferenceStandard&, const ProjectConfig&);
[[nodiscard]] bool materialContains(const MaterialRegionPlan&, Point2);
}
#endif
