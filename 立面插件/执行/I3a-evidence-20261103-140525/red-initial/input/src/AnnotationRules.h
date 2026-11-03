#ifndef BCE_ANNOTATION_RULES_H
#define BCE_ANNOTATION_RULES_H
#include "Recognition.h"
namespace bce {
[[nodiscard]] Result<AnnotationPlan> planAnnotations(const FeatureModel&, const ReferenceStandard&, const ProjectConfig&);
}
#endif
