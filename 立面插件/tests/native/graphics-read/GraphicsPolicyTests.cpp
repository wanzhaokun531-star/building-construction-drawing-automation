#include "GraphicsPolicy.h"
#include <iostream>
int main() {
    using bce::graphics::Coverage;
    int failures = 0, checks = 0;
    auto expect = [&](bool value) { ++checks; if (!value) ++failures; };
    Coverage good; good.callbacks = 1; good.drawRequests = good.drawsEntered = 3;
    expect(good.requestComplete());
    auto reject = [&](Coverage c) { expect(!c.requestComplete()); };
    auto c = good; --c.drawsEntered; reject(c);
    c = good; c.transformsRecorded = false; reject(c);
    c = good; c.viewportRequired = 1; reject(c);
    c = good; c.unknown = 1; reject(c);
    c = good; c.budgetExceeded = true; reject(c);
    c = good; c.textNotTruncated = false; reject(c);
    c = good; c.callbacks = 0; reject(c);
    for (int i = 0; i < 3; ++i) {
        c = good;
        if (i == 0) c.sourceUnchanged = false;
        if (i == 1) c.stackBalanced = false;
        if (i == 2) c.hardFailure = true;
        reject(c);
        bool threw = false;
        try { c.requireSafe(); } catch (const std::runtime_error&) { threw = true; }
        expect(threw);
    }
    std::cout << "CHECKS=" << checks << " FAILED=" << failures << '\n';
    return failures ? 1 : 0;
}
