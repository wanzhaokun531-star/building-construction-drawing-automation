#pragma once
#include <cstddef>
#include <stdexcept>
namespace bce::graphics {
struct Coverage {
    std::size_t callbacks{}, drawRequests{}, drawsEntered{}, unknown{}, viewportRequired{};
    bool stackBalanced{true}, transformsRecorded{true}, textNotTruncated{true};
    bool sourceUnchanged{true}, hardFailure{}, budgetExceeded{};
    bool requestComplete() const noexcept {
        return callbacks > 0 && drawRequests == drawsEntered && unknown == 0 &&
               viewportRequired == 0 && stackBalanced && transformsRecorded && textNotTruncated &&
               sourceUnchanged && !hardFailure && !budgetExceeded;
    }
    void requireSafe() const {
        if (hardFailure || !sourceUnchanged || !stackBalanced)
            throw std::runtime_error("hard collector/source failure");
    }
};
}
