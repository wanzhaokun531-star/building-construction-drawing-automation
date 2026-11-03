#pragma once
#include "NativeEvidence.h"
#include "GraphicsControlPolicy.h"
#include <array>
namespace bce::viewport {
struct Sample {
    std::size_t worldCalls{}, viewportCalls{}, forwarded{}, contextCalls{}, textCalls{};
    bool hostContext{}, hostViewport{}, finiteViewport{}, stable{};
    bce::graphics::ControlObservation observation;
};
struct Result {
    std::array<Sample,12> samples{};
    bool prepared{}, naturalRegen{}, sourceProtected{}, ambientProtected{}, workingDbRestored{};
    bool registrationRemoved{}, globalOverruleRestored{}, targetOnly{}, failureCleanup{};
    std::vector<std::pair<std::string,bool>> ownershipChecks;
    std::vector<std::string> records;
};
Result capture(const bce::native::Config&, bce::native::Evidence&);
void prepare(const bce::native::Config&, bce::native::Evidence&);
void acceptPreparedReplay(const bce::native::Config&);
bool cleanup() noexcept;
}
