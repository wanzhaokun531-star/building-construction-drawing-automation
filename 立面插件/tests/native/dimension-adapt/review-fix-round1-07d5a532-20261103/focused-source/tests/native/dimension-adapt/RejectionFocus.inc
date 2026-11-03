// N8a fix round 1: real formal source binding and rejection-harness controls.
// No geometry, resource binding, paper adaptation, or roundtrip work runs here.
std::string formal_source_sha_rejection(const Config& config,Evidence& evidence) {
    auto healthy=config;validate_identity(healthy);
    evidence.step("healthyFormalIdentity","passed","Unchanged NativeEvidence.validate_identity accepted the bound original module/INI/scheme/reference");
    const auto badPath=config.output+L"\\rejection-focus-bad-reference.dwg";
    std::size_t copiedBytes{};
    {
        File input(CreateFileW(config.reference.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        require(input.valid(),"bad source control copy input open failed");auto output=create_new(badPath);std::vector<char> buffer(65536);
        for(;;){DWORD read{};require(ReadFile(input.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr)!=FALSE,"bad source control copy read failed");if(!read)break;write_flushed(output.get(),std::string(buffer.data(),read));copiedBytes+=read;}
        write_flushed(output.get(),"\nN8A_FIX_ROUND1_INTENTIONAL_BAD_SOURCE_SHA\n");
    }
    const auto actualBadSHA=sha256(badPath);require(actualBadSHA!=config.referenceSHA256,"bad source control did not change bytes");
    auto bad=config;bad.reference=badPath;
    evidence.checkpoint("before-original-formal-validate-identity-with-real-bad-source-copy");
    const auto rejection=expect_reject("source_sha_mismatch","NativeEvidence.validate_identity.reference_source_SHA",[&]{validate_identity(bad);},"reference source SHA mismatch");
    require(sha256(config.reference)==config.referenceSHA256&&sha256(config.scheme)==config.schemeSHA256,"original source changed during focused binding test");
    const auto result=object({{"name",quote(std::string("source_sha_mismatch_formal_entry"))},{"formalFunction",quote(std::string("unchanged bce::native::validate_identity"))},
        {"healthyIdentityPassed","true"},{"badReferencePath",quote(badPath)},{"badReferenceSHA256",quote(actualBadSHA)},
        {"copiedOriginalBytes",std::to_string(copiedBytes)},{"alteration",quote(std::string("Appended explicit rejection-control marker to independently created output copy; no CAD open/save"))},
        {"originalReferencePath",quote(config.reference)},{"originalReferenceSHA256",quote(config.referenceSHA256)},
        {"actualFormalRejection",rejection},{"rejected","true"},{"candidateReturned","false"},{"candidateSaved","false"}});
    report_file(config,L"formal-source-sha-rejection.json",result);evidence.step("formalBadSourceSHARejected","passed","Actual bad output copy reached original validate_identity and raised reference source SHA mismatch");return result;
}
bool rejection_focus(const Config& config,Evidence& evidence) {
    auto* working=acdbHostApplicationServices()->workingDatabase();auto* current=acDocManager->curDocument();auto* active=acDocManager->mdiActiveDocument();const auto globalsBefore=ambient_globals();
    bool normalReturnRejected=false,wrongStageRejected=false;std::string normalDiagnostic,wrongDiagnostic;
    try {n8a::expected_rejection("source_sha_mismatch",[]{},"source_sha_mismatch");}
    catch(const std::runtime_error& failure){normalDiagnostic=failure.what();normalReturnRejected=normalDiagnostic=="fault_was_not_rejected:source_sha_mismatch";}
    require(normalReturnRejected,"focused helper incorrectly accepted normal return with identical name/expected");
    const auto genuine=n8a::expected_rejection("formal_source_control",[]{throw std::runtime_error("reference source SHA mismatch");},"reference source SHA mismatch");
    require(genuine=="reference source SHA mismatch","focused helper lost expected operation exception");
    try {n8a::expected_rejection("formal_source_control",[]{throw std::runtime_error("module SHA mismatch");},"reference source SHA mismatch");}
    catch(const std::runtime_error& failure){wrongDiagnostic=failure.what();wrongStageRejected=wrongDiagnostic.find("fault failed before intended stage")==0;}
    require(wrongStageRejected,"focused helper incorrectly accepted wrong-stage exception");
    evidence.step("rejectionHelperControls","passed","Normal return rejected with name==expected; expected exception accepted; wrong stage rejected");
    const auto actual=formal_source_sha_rejection(config,evidence);const auto globalsAfter=ambient_globals();
    const bool pointers=working==acdbHostApplicationServices()->workingDatabase()&&current==acDocManager->curDocument()&&active==acDocManager->mdiActiveDocument();
    require(pointers&&globalsBefore==globalsAfter,"focused source binding changed ambient context/globals");
    require(sha256(config.reference)==config.referenceSHA256&&sha256(config.scheme)==config.schemeSHA256,"focused original source hashes changed");
    report_file(config,L"rejection-focused.json",object({{"contract",quote(std::string("bce.dimension-rejection-focused.v1"))},{"caseName",quote(config.caseName)},{"moduleSHA256",quote(config.moduleSHA256)},
        {"normalReturnNameEqualsExpectedRejected",truth(normalReturnRejected)},{"normalReturnDiagnostic",quote(normalDiagnostic)},
        {"genuineExpectedExceptionAccepted","true"},{"wrongStageExceptionRejected",truth(wrongStageRejected)},{"wrongStageDiagnostic",quote(wrongDiagnostic)},
        {"formalBadSourceControl",actual},{"currentActiveWorkingPreserved",truth(pointers)},{"globalsBefore",globalsBefore},{"globalsAfter",globalsAfter},
        {"originalSourceSHAsPreserved","true"},{"fullDimensionMatrixRerun","false"},{"old03f2EvidenceRewritten","false"},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"}}));
    return true;
}
