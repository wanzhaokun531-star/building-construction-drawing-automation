// Include after NativePlotProbe.inc inside the existing anonymous namespace.
// No new SDK headers/libraries. The host copies one fixed saved fixture to the
// new run/control.dwg and binds SHA256 in its receipt. This native code checks
// the owned current DB, actual fixture handles/classes/counts and containment;
// verify_native_preview.py independently hashes both saved input and control.
// No DWG saves, view changes, production rules or shared plot settings changes.

struct NativePreviewEntity { const wchar_t* handle; const wchar_t* type; };
struct NativePreviewFixture {
    const wchar_t* id;
    const wchar_t* sourceDWG;
    const wchar_t* sourceReport;
    const wchar_t* sourceHash;
    const wchar_t* reportHash;
    int modelCount;
    double window[4];
    const NativePreviewEntity* objects;
    std::size_t objectCount;
};

std::string nativePreviewManifest(NativePlotContext& context, const NativePreviewFixture& fixture, bool& contained) {
    const auto members = modelIds(*context.database); const auto owner = modelId(*context.database);
    contained = true; std::ostringstream out; out << '[';
    for (std::size_t i = 0; i < fixture.objectCount; ++i) {
        if (i) out << ',';
        const auto& wanted = fixture.objects[i]; AcDbObjectId id;
        const auto lookup = context.database->getAcDbObjectId(id, false, AcDbHandle(wanted.handle));
        AcDbEntity* raw{}; NativePlotStatus opened, extent;
        if (lookup == Acad::eOk) opened.set(acdbOpenObject(raw, id, AcDb::kForRead));
        Opened<AcDbEntity> entity(raw); AcDbExtents bounds;
        bool enumerated = false;
        for (int n = 0; n < members.length(); ++n) if (members[n] == id) { enumerated = true; break; }
        const bool available = lookup == Acad::eOk && opened.ok() && entity;
        const bool sameDatabase = available && entity->database() == context.database;
        const bool ownerIsModel = available && entity->ownerId() == owner;
        if (available) extent.set(entity->getGeomExtents(bounds));
        bool inside = false;
        if (extent.ok()) {
            const auto lo = bounds.minPoint(), hi = bounds.maxPoint();
            inside = std::isfinite(lo.x) && std::isfinite(lo.y) && std::isfinite(hi.x) && std::isfinite(hi.y)
                && lo.x >= fixture.window[0]-1e-6 && lo.y >= fixture.window[1]-1e-6
                && hi.x <= fixture.window[2]+1e-6 && hi.y <= fixture.window[3]+1e-6;
        }
        const bool valid = available && sameDatabase && ownerIsModel && enumerated && !entity->isAProxy()
            && !_wcsicmp(entity->isA()->name(), wanted.type) && inside;
        contained = contained && valid;
        out << "{\"handle\":" << quote(wanted.handle) << ",\"expectedType\":" << quote(wanted.type)
            << ",\"lookupStatus\":" << lookup << ",\"openStatus\":" << opened.json()
            << ",\"type\":" << (available ? quote(entity->isA()->name()) : "null")
            << ",\"proxy\":" << (available ? plotBool(entity->isAProxy() != 0) : "null")
            << ",\"sameDatabase\":" << plotBool(sameDatabase) << ",\"ownerIsModel\":" << plotBool(ownerIsModel)
            << ",\"enumeratedInModel\":" << plotBool(enumerated) << ",\"extentStatus\":" << extent.json()
            << ",\"min\":" << (extent.ok() ? point(bounds.minPoint()) : "null")
            << ",\"max\":" << (extent.ok() ? point(bounds.maxPoint()) : "null") << ",\"insideWindow\":" << plotBool(inside) << '}';
    }
    out << ']'; return out.str();
}

void nativeFixturePreview(const NativePreviewFixture& fixture) {
    try {
        const auto p = paths();
        const auto report = fullPath(p.output + L"\\" + fixture.id + L".json");
        const auto source = fullPath(p.root + L"\\runs\\" + fixture.sourceDWG);
        const auto sourceReport = fullPath(p.root + L"\\runs\\" + fixture.sourceReport);
        if (!below(report, p.output) || !below(source, p.root + L"\\runs") || !below(sourceReport, p.root + L"\\runs"))
            throw std::runtime_error("native preview paths escaped fixed input/output roots");
        requireNewFile(report);
        NativePlotContext context(p); NativePlotBackgroundScope background;
        std::string error{"null"}, before{"[]"}, after{"[]"}, globalsBefore{"null"}, globalsAfter{"null"};
        std::string layoutBefore{"null"}, layoutAfter{"null"}, current{"null"}, active{"null"}, working{"null"}, objects{"[]"}, page{"null"};
        int initialCount = -1, finalCount = -1; bool appContext = true, modelCurrent = false, contained = false;
        try {
            context.prepare(fixture.window);
            appContext = acDocManager->isApplicationContext(); modelCurrent = context.database->tilemode();
            current = quote(fullPath(context.document->fileName())); active = quote(fullPath(acDocManager->mdiActiveDocument()->fileName()));
            const ACHAR* databaseName{}; check(context.database->getFilename(databaseName), "preview working filename");
            working = databaseName ? quote(fullPath(databaseName)) : "null";
            const auto ids = modelIds(*context.database); initialCount = ids.length();
            before = plotGeometryJson(ids); globalsBefore = plotGlobalsJson();
            { auto layout = open<AcDbLayout>(context.layout); layoutBefore = plotSettingsJson(*layout); }
            if (initialCount != fixture.modelCount) throw std::runtime_error("native preview model count differs from the named saved fixture");
            objects = nativePreviewManifest(context, fixture, contained);
            if (!contained) throw std::runtime_error("native preview handle/class/owner/containment fixture check failed");
            // No object remains open across the AcPl page pipeline.
            context.guard(); background.activate(); page = nativePlotPage(context, fixture.id, 420, 297);
            context.guard(); const auto afterIds = modelIds(*context.database); finalCount = afterIds.length(); after = plotGeometryJson(afterIds);
            { auto layout = open<AcDbLayout>(context.layout); layoutAfter = plotSettingsJson(*layout); }
        } catch (const std::exception& failure) { error = plotError(failure); }
          catch (...) { error = quote(L"unknown native saved-fixture preview exception"); }
        background.restore();
        if (background.restorationFailed()) error = quote(L"preview BACKGROUNDPLOT restoration failed");
        globalsAfter = plotGlobalsJson();
        std::ostringstream out;
        out << "{\"contract\":\"stage1.native-preview.v1\",\"api\":\"ObjectARX native plot; no COM\",\"fixtureId\":" << quote(fixture.id)
            << ",\"controlDWG\":" << quote(context.control) << ",\"sourceDWG\":" << quote(source) << ",\"sourceReport\":" << quote(sourceReport)
            << ",\"expectedSourceSHA256\":" << quote(fixture.sourceHash) << ",\"expectedSourceReportSHA256\":" << quote(fixture.reportHash)
            << ",\"inputHashScope\":\"host receipt and independent verifier; native probe checks the owned DB manifest\""
            << ",\"currentDWG\":" << current << ",\"mdiActiveDWG\":" << active << ",\"workingDWG\":" << working
            << ",\"isApplicationContext\":" << plotBool(appContext) << ",\"modelLayoutCurrent\":" << plotBool(modelCurrent)
            << ",\"expectedModelCount\":" << fixture.modelCount << ",\"initialModelCount\":" << initialCount << ",\"finalModelCount\":" << finalCount
            << ",\"fixtureObjects\":" << objects << ",\"fullFixtureContained\":" << plotBool(contained)
            << ",\"windowWCS\":[" << number(fixture.window[0]) << ',' << number(fixture.window[1]) << ',' << number(fixture.window[2]) << ',' << number(fixture.window[3])
            << "],\"windowDCS\":" << context.windowDCS << ",\"windowTransformStatuses\":" << context.windowTransforms << ",\"viewCheck\":" << context.viewEvidence
            << ",\"globalsBefore\":" << globalsBefore << ",\"globalsAfter\":" << globalsAfter << ",\"backgroundOverride\":" << background.json()
            << ",\"layoutBefore\":" << layoutBefore << ",\"layoutAfter\":" << layoutAfter << ",\"originalBefore\":" << before << ",\"originalAfter\":" << after
            << ",\"page\":" << page << ",\"dwgSaved\":false,\"fullBuildingCoverage\":false"
            << ",\"visualReview\":\"pending independent rendering and inspection\",\"error\":" << error << '}';
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_NATIVE_PREVIEW_COMPLETED; PDF_RENDERING_AND_ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_NATIVE_PREVIEW_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_PREVIEW_FAILED: unknown C++ exception\n"); }
}

void nativeTextPreview() {
    const NativePreviewEntity objects[]{ {L"D0DA",L"TDbText"}, {L"D0DB",L"AcDbText"}, {L"D0DC",L"TDbText"}, {L"D0DD",L"AcDbText"} };
    const NativePreviewFixture fixture{L"nativeTextPreview",
        L"coverage-text-20261029-003350-351\\text-bridge-generated.dwg", L"coverage-text-20261029-003350-351\\text-bridge-native.json",
        L"D18096CD2294124B428F2CB52A82B3F4F699CAE96CE68E6B881110CBB816C008",
        L"D466CF9AC44F78392D5A0D394F8B7E165E8F71733B54E887EB4F4D65BAEC7A31",
        217, {39000,17000,47000,22000}, objects, sizeof(objects)/sizeof(objects[0])};
    nativeFixturePreview(fixture);
}

void nativeCombinePreview() {
    const NativePreviewEntity objects[]{ {L"D0D3",L"TDbDimension2"} };
    const NativePreviewFixture fixture{L"nativeCombinePreview",
        L"coverage-combine-20261028-234100-432\\dimension-combine-generated.dwg", L"coverage-combine-20261028-234100-432\\dimension-bridge-native.json",
        L"56861B0A037FB28201EC09440C7CF6F0A163C44CEA0369405C512280FDC53C27",
        L"5B3C6A50D4B39A2999F7471ECCF272B229B910FB8D425A53659FC32A342C5E1D",
        214, {12000,12000,36000,35000}, objects, sizeof(objects)/sizeof(objects[0])};
    nativeFixturePreview(fixture);
}

void nativeGeometryPreview() {
    const NativePreviewEntity objects[]{ {L"D0D8",L"AcDbPolyline"}, {L"D0D9",L"AcDbPolyline"}, {L"D0DA",L"AcDbHatch"},
        {L"D0DB",L"AcDbText"}, {L"D0DC",L"AcDbMText"}, {L"D0DE",L"AcDbLeader"}, {L"D0E5",L"AcDbBlockReference"} };
    const NativePreviewFixture fixture{L"nativeGeometryPreview",
        L"coverage-geometry-20261028-232845-683\\standard-geometry-generated.dwg", L"coverage-geometry-20261028-232845-683\\standard-geometry-native.json",
        L"03A36511806ACFF2FC5416B21FBFE1A57EC42D4D26B18CAA26FE7579E9E1F62F",
        L"E5AB4381E510D2DE138558D08A26DBAD99387383E758958507D2C1D92A3096C0",
        220, {48000,18000,65000,32000}, objects, sizeof(objects)/sizeof(objects[0])};
    nativeFixturePreview(fixture);
}
