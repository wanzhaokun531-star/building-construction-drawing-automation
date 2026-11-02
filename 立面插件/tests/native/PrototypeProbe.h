#pragma once
#include "NativeEvidence.h"

namespace bce::native {
// These are the native behavioral assertions, not a production generator API.
struct PrototypeAssertions {
    std::string type;
    int dimensionSegments{-1};
    bool cloneMapped{false};
    bool nativeClassAndOwner{false};
    bool rigidTranslation1234{false};
    bool restoreBaseline{false};
    bool saveReopen{false};
    bool textAndMeasurementPreserved{false};
    bool stylesReadBack{false};
    bool baselineGrip120{false};
    bool sharedGrip40{false};
    bool sourceUnchanged{false};
    bool implemented{false};
    bool observationComplete{false};
    bool executionFailed{false};
    bool hardObservationFailure{false};
    std::string details;
};
std::vector<PrototypeAssertions> probe_prototypes(const Config&, Evidence&);
bool assert_prototype_roundtrip(const Config&, Evidence&);
} // namespace bce::native
