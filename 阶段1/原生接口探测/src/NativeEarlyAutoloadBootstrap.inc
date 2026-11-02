// Opt-in /ld startup only. Public SDK variables in the authorized test profile.
struct NativeEarlyControlState {bool active{};short originalDemand{},originalAutoload{};std::wstring profile,session;};
NativeEarlyControlState nativeEarlyControls;
void nativeEarlyAutoloadBootstrap(){
 const auto ini=quietModuleIni();if(quietIni(ini,L"worker",L"earlyNativeAutoload")!=L"1")return;
 const auto p=paths();QuietWorkerState attestation;attestation.nonce=quietIni(ini,L"worker",L"nonce");attestation.session=fullPath(quietIni(ini,L"worker",L"session"));attestation.desktop=L"CodexS1_"+attestation.nonce;
 if(attestation.nonce.size()!=32||!quietSafeToken(attestation.nonce)||!below(attestation.session,p.root+L"\\runs"))throw std::runtime_error("early native startup binding invalid");attestation.guard();
 const auto expected=quietIni(ini,L"worker",L"expectedProfile"),actual=hostGetText(L"CPROFILE");
 if(expected.rfind(L"CodexS1Native_",0)!=0||expected!=actual||hostGetShort(L"SECURELOAD")!=1)throw std::runtime_error("early native startup refuses non-test profile or insecure loader");
 const auto demandBefore=hostGetShort(L"DEMANDLOAD"),autoBefore=hostGetShort(L"APPAUTOLOAD");
 writeNewReport(attestation.session+L"\\early-native-autoload-before.json","{\"profile\":"+quote(actual)+",\"demandloadBefore\":"+std::to_string(demandBefore)+",\"appautoloadBefore\":"+std::to_string(autoBefore)+",\"secureload\":1,\"desktop\":"+quote(attestation.desktop)+",\"api\":\"acedGetVar/acedSetVar only\"}");
 resbuf zero{};zero.restype=RTSHORT;zero.resval.rint=0;
 if(acedSetVar(L"DEMANDLOAD",&zero)!=RTNORM||acedSetVar(L"APPAUTOLOAD",&zero)!=RTNORM||hostGetShort(L"DEMANDLOAD")!=0||hostGetShort(L"APPAUTOLOAD")!=0||hostGetShort(L"SECURELOAD")!=1||hostGetText(L"CPROFILE")!=expected)throw std::runtime_error("early native SDK loading controls did not apply");
 nativeEarlyControls.originalDemand=demandBefore;nativeEarlyControls.originalAutoload=autoBefore;nativeEarlyControls.profile=actual;nativeEarlyControls.session=attestation.session;nativeEarlyControls.active=true;
 writeNewReport(attestation.session+L"\\early-native-autoload-applied.json","{\"contract\":\"stage1.early-native-autoload.v1\",\"profile\":"+quote(actual)+",\"demandloadBefore\":"+std::to_string(demandBefore)+",\"appautoloadBefore\":"+std::to_string(autoBefore)+",\"demandloadAfter\":0,\"appautoloadAfter\":0,\"secureload\":1,\"globalApplicationRegistrationChanged\":false,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"desktop\":"+quote(attestation.desktop)+"}");
}
void nativePostStartupLoadControls(){
 if(!nativeEarlyControls.active)return;quietRequireIsolatedWorker();
 if(hostGetText(L"CPROFILE")!=nativeEarlyControls.profile||hostGetShort(L"SECURELOAD")!=1||quietWorker->session!=nativeEarlyControls.session)throw std::runtime_error("post-startup native profile binding changed");
 resbuf restored{};restored.restype=RTSHORT;restored.resval.rint=nativeEarlyControls.originalDemand;
 if(acedSetVar(L"DEMANDLOAD",&restored)!=RTNORM||hostGetShort(L"DEMANDLOAD")!=nativeEarlyControls.originalDemand||hostGetShort(L"APPAUTOLOAD")!=0)throw std::runtime_error("post-startup object demand-loading restoration failed");
 writeNewReport(nativeEarlyControls.session+L"\\post-startup-native-load-controls.json","{\"contract\":\"stage1.post-startup-native-load-controls.v1\",\"profile\":"+quote(nativeEarlyControls.profile)+",\"demandloadRestored\":"+std::to_string(nativeEarlyControls.originalDemand)+",\"appautoload\":0,\"secureload\":1,\"reason\":\"registry startup phase ended; re-enable object dependency loading before DWG reads\",\"globalApplicationRegistrationChanged\":false}");
}
