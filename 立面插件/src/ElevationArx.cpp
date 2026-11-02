#include "AnalysisCommands.h"
#include <rxregsvc.h>
extern "C" __declspec(dllexport) AcRx::AppRetCode
    acrxEntryPoint(AcRx::AppMsgCode message, void *id) noexcept {
  try {
    if (message == AcRx::kInitAppMsg) {
      acrxDynamicLinker->unlockApplication(id);
      acrxRegisterAppMDIAware(id);
      return bce::commands::initialize() ? AcRx::kRetOK : AcRx::kRetError;
    }
    if (message == AcRx::kUnloadAppMsg)
      return bce::commands::shutdown() ? AcRx::kRetOK : AcRx::kRetError;
    return AcRx::kRetOK;
  } catch (...) {
    return AcRx::kRetError;
  }
}
