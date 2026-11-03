#include "ProjectFacts.h"

namespace bce {
Result<ProjectFacts> confirmProjectFacts(const ProjectFactConfirmation&) { return {}; }
FactSubsetCheck checkProjectFacts(const ProjectFacts&, const std::optional<ProjectFactObservation>&) { return {}; }
Result<double> projectFactElevationMeters(const ProjectFacts&, FactPoint3) { return {}; }
}
