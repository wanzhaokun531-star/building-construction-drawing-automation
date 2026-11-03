#include "PrintedCommandAcceptance.h"
#include <iostream>
int main() {
  unsigned failed = 0, total = 0;
  // Synthetic observations only. No claim of native command execution or SDK success.
  constexpr int success = 17;
  for (const char* command : {"ISSUES after CONFIG", "ISSUES after switch", "INFO"}) {
    const auto check = [&](const char* factor, int status, bool readOnly,
                           bool restored, bool text, bool expected) {
      ++total;
      const bool actual = bce::unitconfig_test::acceptPrintedCommand(
          status, success, readOnly, restored, text);
      const bool passed = actual == expected;
      if (!passed) ++failed;
      std::cout << command << " / " << factor << ": "
                << (passed ? "PASS" : "FAIL") << '\n';
    };
    check("healthy", success, true, true, true, true);
    check("badStatus with expected text", success - 1, true, true, true, false);
    check("write with expected text", success, false, true, true, false);
    check("restore failure with expected text", success, true, false, true, false);
    check("wrong text", success, true, true, false, false);
  }
  std::cout << "TOTAL=" << total << " FAILED=" << failed << '\n';
  return failed == 0 ? 0 : 1;
}
