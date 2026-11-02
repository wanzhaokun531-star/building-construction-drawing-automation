#pragma once
#include <cstdint>
#include <type_traits>
// AutoCAD owning thread only; worker-thread calls return invalid request.
// Caller owns buffers. UTF-8 report has no SDK pointers or cross-module STL
// objects. 0 success; 1 no current result; 2 insufficient buffer; 3 invalid
// request; 4 stale sequence.
struct BceAnalysisReportV1 {
  std::uint32_t size;
  std::uint32_t version;
  std::uint64_t sequence;
  std::uint64_t revision;
  std::uint64_t requiredBytes;
  char documentIdentity[80];
};
using BceCopyAnalysisV1 = int(__cdecl *)(BceAnalysisReportV1 *, char *,
                                         std::uint64_t) noexcept;

static_assert(std::is_standard_layout<BceAnalysisReportV1>::value &&
                  std::is_trivially_copyable<BceAnalysisReportV1>::value,
              "ABI metadata must remain POD");
