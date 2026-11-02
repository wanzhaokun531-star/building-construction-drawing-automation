#include "ViewportProbe.h"
#include "ViewportTrace.h"
#include <aced.h>
#include <rxregsvc.h>
#include <memory>
#include <exception>
#include <cstdint>
namespace {
using namespace bce::native;
HMODULE module{};
Config pendingConfig;
std::unique_ptr<Evidence> pendingEvidence;
unsigned prepareCalls{},captureCalls{};
void failPending(const std::string& reason) noexcept {
    bce::viewport::trace("handler.failPending",{{"reason",quote(reason)},{"pendingEvidence",pendingEvidence?"true":"false"},{"prepareCalls",std::to_string(prepareCalls)},{"captureCalls",std::to_string(captureCalls)}});
    bce::viewport::cleanup();
    if(pendingEvidence){try{pendingEvidence->error(reason);pendingEvidence->finish(pendingConfig,"failed");}catch(...){}}
    pendingEvidence.reset();
}
void prepareRun() noexcept {
    ++prepareCalls;
    bce::viewport::trace("handler.prepare.enter",{{"prepareCalls",std::to_string(prepareCalls)},{"captureCalls",std::to_string(captureCalls)},{"pendingEvidence",pendingEvidence?"true":"false"}});
    try {
        require(!pendingEvidence,"N7 prepare already active");
        pendingConfig=load_config(module);pendingEvidence=std::make_unique<Evidence>(pendingConfig);
        pendingEvidence->checkpoint("validate-N7-identity");validate_identity(pendingConfig);
        require(pendingConfig.caseName==L"PublicGraphicsViewport","unsupported N7 case");
        bce::viewport::prepare(pendingConfig,*pendingEvidence);
        require(bce::viewport::traceHealthy(),"command trace logging failed");
        bce::viewport::trace("handler.prepare.return",{{"pendingEvidence",pendingEvidence?"true":"false"}});
    } catch(const std::exception& ex){failPending(ex.what());}catch(...){failPending("unknown prepare failure");}
}
void run() noexcept {
    ++captureCalls;
    bce::viewport::trace("handler.capture.enter",{{"prepareCalls",std::to_string(prepareCalls)},{"captureCalls",std::to_string(captureCalls)},{"pendingEvidence",pendingEvidence?"true":"false"}});
    if(!pendingEvidence)return;
    auto& config=pendingConfig;auto& evidence=pendingEvidence;
    try {
        validate_identity(config);
        const auto result=bce::viewport::capture(config,*evidence);
        require(bce::viewport::traceHealthy(),"command trace logging failed");
        unsigned failed{};
        auto check=[&](bool value,const std::string& name){evidence->step(name,value?"passed":"failed",value?"observed":"required observation missing");if(!value){++failed;evidence->error(name);}};
        check(result.prepared,"owned-target-prepared");check(result.naturalRegen,"host-natural-regen-executed");
        check(result.sourceProtected,"reference-source-protected");check(result.ambientProtected,"non-target-ambient-protected");
        check(result.workingDbRestored,"working-database-restored");check(result.targetOnly,"overrule-only-owned-targets");
        check(result.registrationRemoved,"own-overrule-unregistered");check(result.globalOverruleRestored,"global-overrule-state-restored");
        check(result.failureCleanup,"failure-cleanup-contract");
        for(std::size_t i=0;i<result.samples.size();++i){
            const auto& sample=result.samples[i];const auto prefix="sample-"+std::to_string(i)+"-";
            check(sample.worldCalls+sample.viewportCalls>0,prefix+"actual-host-draw");
            check(sample.hostContext&&sample.contextCalls>0,prefix+"real-host-context");
            check(sample.worldCalls+sample.viewportCalls>0&&sample.forwarded==sample.worldCalls+sample.viewportCalls,prefix+"original-draw-forwarded");
            check(sample.stable,prefix+"two-regens-stable");
            if(i<7){const auto value=bce::graphics::validateControl(i,sample.observation);check(value.valuesMatch,prefix+"fixed-control-values");}
            else check(sample.viewportCalls>0&&sample.hostViewport&&sample.finiteViewport,prefix+"real-finite-viewport");
        }
        check(sha256(config.reference)==config.referenceSHA256&&sha256(config.scheme)==config.schemeSHA256,"source-disk-sha-unchanged");
        {auto report=create_new(config.output+L"\\graphics-viewport.json");
        write_flushed(report.get(),object({{"contract",quote(std::string("bce.graphics-viewport.v1"))},{"status",quote(std::string(failed?"failed":"partial"))},{"failedAssertions",std::to_string(failed)},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"},{"records",array(result.records)}}));}
        evidence->capability(object({{"actualGlyphCoverage","false"},{"mtextLayoutVerified","false"},{"clipAppliedToCollectedPrimitives","false"},{"trueTypeBackendVerified","false"}}));
        evidence->finish(config,failed?"failed":"partial");
        require(bce::viewport::cleanup(),"N7 cleanup failed");evidence.reset();
        bce::viewport::trace("handler.capture.return");
    } catch(const std::exception& ex){failPending(ex.what());}
    catch(...){failPending("unknown N7 failure");}
}
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* id){
    try {
        if(message==AcRx::kInitAppMsg){
            const auto config=load_config(module);bce::viewport::initializeTrace(config);
            bce::viewport::trace("module.init",{{"prepareHandler",std::to_string(reinterpret_cast<std::uintptr_t>(&prepareRun))},{"captureHandler",std::to_string(reinterpret_cast<std::uintptr_t>(&run))}});
            acrxDynamicLinker->unlockApplication(id);acrxRegisterAppMDIAware(id);
            const auto first=acedRegCmds->addCommand(L"BCE_VIEWPORT_CASES",L"BCE_TEST_RUN",L"BCE_TEST_RUN",ACRX_CMD_MODAL,prepareRun);
            bce::viewport::trace("module.prepareRegistered",{{"status",std::to_string(first)}});if(first!=Acad::eOk){bce::viewport::closeTrace();return AcRx::kRetError;}
            const auto second=acedRegCmds->addCommand(L"BCE_VIEWPORT_CASES",L"BCE_VIEWPORT_CAPTURE",L"BCE_VIEWPORT_CAPTURE",ACRX_CMD_MODAL,run);
            bce::viewport::trace("module.captureRegistered",{{"status",std::to_string(second)}});
            if(second!=Acad::eOk){acedRegCmds->removeGroup(L"BCE_VIEWPORT_CASES");bce::viewport::closeTrace();return AcRx::kRetError;}return AcRx::kRetOK;
        }
        bce::viewport::trace("module.message",{{"message",std::to_string(message)}});
        if(message==AcRx::kUnloadAppMsg){if(pendingEvidence)failPending("unload before capture completion");if(!bce::viewport::cleanup())return AcRx::kRetError;const auto status=acedRegCmds->removeGroup(L"BCE_VIEWPORT_CASES");bce::viewport::trace("module.commandsRemoved",{{"status",std::to_string(status)}});bce::viewport::closeTrace();return status==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;}return AcRx::kRetOK;
    }catch(...){bce::viewport::trace("module.exception");bce::viewport::closeTrace();return AcRx::kRetError;}
}
