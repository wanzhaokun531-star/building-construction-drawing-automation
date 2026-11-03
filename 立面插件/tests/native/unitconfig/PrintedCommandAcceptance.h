#pragma once
namespace bce::unitconfig_test {
// Host-neutral acceptance of actual captured observations; no native API simulation.
inline bool acceptPrintedCommand(int status, int successStatus, bool readOnly,
                                 bool restored, bool expectedText) noexcept {
  return status == successStatus && readOnly && restored && expectedText;
}
}
