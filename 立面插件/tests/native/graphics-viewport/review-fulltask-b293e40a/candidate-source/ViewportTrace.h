#pragma once
#include "NativeEvidence.h"
namespace bce::viewport {
void initializeTrace(const bce::native::Config& config);
void trace(const char* event,bce::native::Fields fields={}) noexcept;
bool traceHealthy() noexcept;
void closeTrace() noexcept;
}
