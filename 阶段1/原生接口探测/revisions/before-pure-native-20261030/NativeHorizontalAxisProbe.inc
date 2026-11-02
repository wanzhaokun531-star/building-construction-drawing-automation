// Reuse the exact family clone/persistence helper for the other reference axis orientation.
void nativeHorizontalAxisProbe() {
    try {
        const auto p=paths();const auto report=fullPath(p.output+L"\\horizontal-axis-native.json");requireNewFile(report);
        const ObjectFamilyFixture fixture{L"H01",L"reference",L"19A3",L"TDbAxisLabelSet"};
        const auto body=familyFixtureCase(p,fixture);
        const std::string result="{\"contract\":\"stage1.object-family-native.v1\",\"api\":\"ObjectARX native; no COM\",\"schemeInputPath\":"+quote(p.scheme)+",\"referenceInputPath\":"+quote(p.reference)+",\"fixtures\":["+body+"],\"scope\":\"reference19A3 axis-label template carrier clone, rigid translation and persistence only; letter numbering/four-sided generation not established\"}";
        writeNewReport(report,result);acutPrintf(L"\nSTAGE1_HORIZONTAL_AXIS_TEMPLATE_COMPLETED\n");
    }catch(const std::exception& e){acutPrintf(L"\nSTAGE1_HORIZONTAL_AXIS_TEMPLATE_FAILED: %hs\n",e.what());}
}
