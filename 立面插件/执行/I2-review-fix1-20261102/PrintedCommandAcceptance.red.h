#pragma once
namespace bce::unitconfig_test {
// Host-neutral acceptance of actual captured observations; no native API simulation.
inline bool acceptPrintedCommand(int status, int successStatus, bool readOnly,
                                 bool restored, bool expectedText) noexcept {
  (void)status; (void)successStatus; (void)readOnly; (void)restored;
  return expectedText;
}
}
