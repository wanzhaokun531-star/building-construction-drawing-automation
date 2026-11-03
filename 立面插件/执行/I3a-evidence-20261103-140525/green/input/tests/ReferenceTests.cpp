#include "TestSupport.h"
#include <limits>

namespace bce::test {
// Catches treating a label/INSUNITS=0 as mm, or accepting invalid scale values.
void undefinedUnitRequiresConfirmation() {
    UnitDeclaration declaration{0, false, std::nullopt, "mm"};
    const auto unknown = resolveUnits(declaration);
    require(!unknown.value && !unknown.canCommit() && hasCode(unknown, "UNIT_REQUIRED"),
        "unknown units must require explicit confirmation despite mm label");
    declaration.confirmedMmPerInputUnit = 1.0;
    const auto confirmed = resolveUnits(declaration);
    require(confirmed.canCommit() && confirmed.value->mmPerInputUnit == 1.0,
        "explicit 1.0 confirmation must resolve to millimeters");
    for (const auto invalid : {std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::quiet_NaN(), 0.0, -1.0}) {
        declaration.confirmedMmPerInputUnit = invalid;
        const auto result = resolveUnits(declaration);
        require(!result.value && !result.canCommit() && hasCode(result, "UNIT_REQUIRED"),
            "nonfinite/nonpositive confirmation must be rejected");
    }
    const auto untrusted = resolveUnits({4, false, std::nullopt, "mm"});
    require(!untrusted.canCommit() && hasCode(untrusted, "UNIT_REQUIRED"),
        "untrusted INSUNITS must not establish scale");
    Result<UnitScale> blocking{UnitScale{1.0}, {{"UNIT_CONFLICT", u8"单位冲突", {}, true}}};
    require(!blocking.canCommit(), "blocking issue must disable commit even with value");
    blocking.issues.front().blocking = false;
    require(blocking.canCommit(), "nonblocking issue must allow a valid value");
}

// Catches silent precedence of confirmation over trusted declarations, and mm/m mixup.
void knownUnitsConflictNeedsConfirmation() {
    const auto mm = resolveUnits({4, true, std::nullopt, {}});
    require(mm.canCommit() && mm.value->mmPerInputUnit == 1.0, "trusted mm must be 1");
    const auto meters = resolveUnits({6, true, std::nullopt, {}});
    require(meters.canCommit() && meters.value->mmPerInputUnit == 1000.0,
        "trusted meters must be 1000 mm/input unit");
    const auto matching = resolveUnits({6, true, 1000.0, {}});
    require(matching.canCommit(), "matching explicit confirmation must succeed");
    const auto conflict = resolveUnits({6, true, 1.0, {}});
    require(!conflict.value && !conflict.canCommit() && hasCode(conflict, "UNIT_CONFLICT"),
        "conflicting confirmation must not silently override trusted meters");
    const auto unsupported = resolveUnits({99, true, std::nullopt, {}});
    require(!unsupported.canCommit() && hasCode(unsupported, "UNIT_REQUIRED"),
        "unsupported declarations must not invent scale");
}
}
