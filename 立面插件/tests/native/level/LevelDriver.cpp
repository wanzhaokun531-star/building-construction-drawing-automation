#include "NativeEvidence.h"
#include "LevelProbe.h"
#include <aced.h>
#include <acutads.h>
#include <rxregsvc.h>
#include <memory>
#include <exception>

namespace {
HMODULE moduleHandle{}; // Borrowed process module; never FreeLibrary from this module.
void run_case() noexcept {
    bce::native::Config config;
    std::unique_ptr<bce::native::Evidence> evidence;
    try {
        config = bce::native::load_config(moduleHandle);
        evidence = std::make_unique<bce::native::Evidence>(config);
        evidence->checkpoint("before-identity-validation");
        bce::native::validate_identity(config);
        evidence->step("identityAndSourceBinding", "passed", "module/INI SHA, local session paths, isolated desktop, bound source SHA validated");
        bce::native::require(config.caseName == L"PublicElevationY", "UNSUPPORTED_CASE: only PublicElevationY is registered");
        evidence->checkpoint("before-public-elevation-y-observations");
        const bool passed = bce::native::assert_elevation_y(config, *evidence);
        evidence->checkpoint("before-final-source-hash-check");
        bce::native::require(bce::native::sha256(config.scheme) == config.schemeSHA256, "scheme source SHA changed");
        bce::native::require(bce::native::sha256(config.reference) == config.referenceSHA256, "reference source SHA changed");
        evidence->finish(config, passed ? "partial" : "failed");
        acutPrintf(passed ? L"\nBCE_NATIVE_CASE_PARTIAL\n" : L"\nBCE_NATIVE_CASE_FAILED; inspect native-case.json\n");
    } catch (const std::exception& failure) {
        try { if (evidence) { evidence->error(failure.what()); evidence->finish(config, "failed"); } } catch (...) {}
        acutPrintf(L"\nBCE_NATIVE_CASE_FAILED; inspect exclusive report/checkpoint if created\n");
    } catch (...) {
        try { if (evidence) { evidence->error("Unknown C++ exception at command ABI boundary"); evidence->finish(config, "failed"); } } catch (...) {}
        acutPrintf(L"\nBCE_NATIVE_CASE_FAILED_UNKNOWN\n");
    }
}
}
BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { moduleHandle = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    try {
        if (message == AcRx::kInitAppMsg) {
            acrxDynamicLinker->unlockApplication(id);
            acrxRegisterAppMDIAware(id);
            return acedRegCmds->addCommand(L"BCE_LEVEL_CASES", L"BCE_TEST_RUN", L"BCE_TEST_RUN", ACRX_CMD_MODAL, run_case)
                == Acad::eOk ? AcRx::kRetOK : AcRx::kRetError;
        }
        if (message == AcRx::kUnloadAppMsg)
            return acedRegCmds->removeGroup(L"BCE_LEVEL_CASES") == Acad::eOk ? AcRx::kRetOK : AcRx::kRetError;
        return AcRx::kRetOK;
    } catch (...) { return AcRx::kRetError; }
}
