// Included inside NativeAccessProbe.cpp's anonymous namespace.
// Uses its native path guards, object/transaction RAII, scanEntity and clone helpers.
// This is a stage-1 template/transform probe, not a construction drawing generator.

struct ObjectFamilyFixture {
    const wchar_t* id;
    const wchar_t* source;
    const wchar_t* handle;
    const wchar_t* type;
};

std::string familyError(const std::exception& failure) {
    const std::string message(failure.what());
    return quote(std::wstring(message.begin(), message.end()));
}

std::string familyCompactEntity(AcDbObjectId id) {
    auto entity = open<AcDbEntity>(id);
    AcString layer; check(entity->layer(layer), "family compact layer");
    AcDbExtents bounds; const auto extentStatus = entity->getGeomExtents(bounds);
    AcGePoint3dArray grips, stretch; AcDbIntArray snaps, geometry;
    const auto gripStatus = entity->getGripPoints(grips, snaps, geometry);
    const auto stretchStatus = entity->getStretchPoints(stretch);
    std::ostringstream out;
    out << "{\"handle\":" << quote(handleOf(id)) << ",\"type\":" << quote(entity->isA()->name())
        << ",\"layer\":" << quote(layer.kACharPtr()) << ",\"proxy\":" << (entity->isAProxy() ? "true" : "false")
        << ",\"extentStatus\":" << extentStatus << ",\"min\":" << (extentStatus == Acad::eOk ? point(bounds.minPoint()) : "null")
        << ",\"max\":" << (extentStatus == Acad::eOk ? point(bounds.maxPoint()) : "null")
        << ",\"curve\":" << curveJson(*entity) << ",\"grip\":{\"status\":" << gripStatus << ",\"points\":" << points(grips)
        << ",\"osnapModes\":" << integers(snaps) << ",\"geomIds\":" << integers(geometry)
        << "},\"stretch\":{\"status\":" << stretchStatus << ",\"points\":" << points(stretch) << "}}";
    return out.str();
}

std::string familyCompactArray(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) { if (i) out << ','; out << familyCompactEntity(ids[i]); }
    out << ']'; return out.str();
}

std::string familyClassInfo(AcDbObjectId id) {
    auto entity = open<AcDbEntity>(id);
    std::vector<const AcRxClass*> visited; std::ostringstream chain; chain << '['; bool complete = true;
    for (const auto* type = entity->isA(); type; type = type->myParent()) {
        if (visited.size() >= 256 || std::find(visited.begin(), visited.end(), type) != visited.end()) { complete = false; break; }
        if (!visited.empty()) chain << ',';
        chain << quote(type->name()); visited.push_back(type);
    }
    chain << ']'; const auto* type = entity->isA(); std::ostringstream out;
    out << "{\"parentChain\":" << chain.str() << ",\"parentChainComplete\":" << (complete ? "true" : "false")
        << ",\"dxfName\":" << (type->dxfName() ? quote(type->dxfName()) : "null")
        << ",\"appName\":" << (type->appName() ? quote(type->appName()) : "null")
        << ",\"casts\":{\"AcDbEntity\":" << (AcDbEntity::cast(entity.get()) ? "true" : "false")
        << ",\"AcDbCurve\":" << (AcDbCurve::cast(entity.get()) ? "true" : "false")
        << ",\"AcDbLine\":" << (AcDbLine::cast(entity.get()) ? "true" : "false")
        << ",\"AcDbText\":" << (AcDbText::cast(entity.get()) ? "true" : "false")
        << ",\"AcDbBlockReference\":" << (AcDbBlockReference::cast(entity.get()) ? "true" : "false") << "}}";
    return out.str();
}

AcDbObjectIdArray familyAdditions(AcDbDatabase& database, const AcDbObjectIdArray& originals) {
    const auto current = modelIds(database); AcDbObjectIdArray result;
    for (int i = 0; i < current.length(); ++i) {
        bool original = false;
        for (int j = 0; j < originals.length(); ++j) if (current[i] == originals[j]) { original = true; break; }
        if (!original) result.append(current[i]);
    }
    return result;
}

std::string familyFixtureCase(const Paths& p, const ObjectFamilyFixture& fixture) {
    const auto output = fullPath(p.output + L"\\object-family-" + fixture.id + L".dwg");
    std::string cloneStatus = "null", transformStatus = "null", transformOpenStatus = "null", transactionStatus = "null";
    std::string saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string sourceReadStatus = "null", sourceCloseStatus = "null", schemeReadStatus = "null", schemeCloseStatus = "null";
    std::string sourceEntity = "null", classInfo = "null", before = "null", after = "null", saved = "null", resources = "[]";
    std::string originalBefore = "[]", originalAfter = "[]", originalSaved = "[]", added = "[]", savedAdded = "[]", addedAfterClone = "[]";
    int initialCount = 0, addedCount = 0, reopenedCount = 0;
    std::vector<std::wstring> originalHandles, addedHandles; std::wstring primaryHandle;
    try {
        if (!below(output, p.output)) throw std::runtime_error("object-family output outside isolated run");
        requireNewFile(output);
        const auto& sourcePath = !_wcsicmp(fixture.source, L"scheme") ? p.scheme : p.reference;
        AcDbDatabase source(false, true);
        const auto readSource = source.readDwgFile(sourcePath.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        sourceReadStatus = std::to_string(readSource); check(readSource, "read frozen family template input");
        const auto closeSource = source.closeInput(true); sourceCloseStatus = std::to_string(closeSource); check(closeSource, "close frozen family template input");
        AcDbObjectId templateId;
        check(source.getAcDbObjectId(templateId, false, AcDbHandle(fixture.handle)), "find fixed family source handle");
        {
            WorkingDatabaseScope working(source);
            sourceEntity = scanEntity(templateId); classInfo = familyClassInfo(templateId);
            auto entity = open<AcDbEntity>(templateId);
            if (_wcsicmp(entity->isA()->name(), fixture.type)) throw std::runtime_error("family source native type differs from fixed manifest");
            if (entity->isAProxy()) throw std::runtime_error("family source is a proxy; native implementation unavailable");
        }
        auto target = std::make_unique<AcDbDatabase>(false, true);
        const auto readScheme = target->readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        schemeReadStatus = std::to_string(readScheme); check(readScheme, "read independent complete family scheme work copy");
        const auto closeScheme = target->closeInput(true); schemeCloseStatus = std::to_string(closeScheme); check(closeScheme, "close independent family scheme input");
        target->disableUndoRecording(false);
        {
            WorkingDatabaseScope working(*target);
            const auto originals = modelIds(*target); initialCount = originals.length();
            if (initialCount != 213) throw std::runtime_error("family target must begin with the complete 213-entity frozen scheme");
            originalBefore = familyCompactArray(originals);
            for (int i = 0; i < originals.length(); ++i) originalHandles.push_back(handleOf(originals[i]));
            AcDbObjectIdArray one; one.append(templateId); AcDbIdMapping mapping;
            check(mapping.setDestDb(target.get()), "set native family clone destination");
            const auto cloned = source.wblockCloneObjects(one, modelId(*target), mapping, AcDb::kDrcMangleName, false);
            cloneStatus = std::to_string(cloned); check(cloned, "clone one family primary and its native dependencies");
            AcDbIdPair pair; pair.setKey(templateId);
            if (!mapping.compute(pair) || !pair.isCloned() || pair.value().isNull()) throw std::runtime_error("native family primary clone mapping unavailable");
            const auto primary = pair.value(); primaryHandle = handleOf(primary);
            for (int i = 0; i < originals.length(); ++i) if (primary == originals[i]) throw std::runtime_error("family clone unexpectedly reused an original scheme entity");
            before = familyCompactEntity(primary); resources = symbolResourceMapping(mapping);
            addedAfterClone = familyCompactArray(familyAdditions(*target, originals));
            {
                TransactionScope transaction(*target); AcDbObject* object{};
                const auto opened = transaction.get()->getObject(object, primary, AcDb::kForWrite); transformOpenStatus = std::to_string(opened);
                if (opened == Acad::eOk) {
                    auto* entity = AcDbEntity::cast(object);
                    if (!entity) throw std::runtime_error("family primary clone is not an entity");
                    const auto moved = entity->transformBy(AcGeMatrix3d::translation(AcGeVector3d(40000, 0, 0)));
                    transformStatus = std::to_string(moved);
                    const auto finished = transaction.finish(moved == Acad::eOk); transactionStatus = std::to_string(finished); check(finished, "finish native family transform transaction");
                    if (moved != Acad::eOk) error = quote(L"native transformBy returned a nonzero status; no replacement geometry was synthesized");
                } else {
                    const auto aborted = transaction.finish(false); transactionStatus = std::to_string(aborted); check(aborted, "abort unopened family transform transaction");
                    error = quote(L"primary clone could not be opened for native transformation");
                }
            }
            after = familyCompactEntity(primary); originalAfter = familyCompactArray(originals);
            const auto additions = familyAdditions(*target, originals); addedCount = additions.length(); added = familyCompactArray(additions);
            for (int i = 0; i < additions.length(); ++i) addedHandles.push_back(handleOf(additions[i]));
            requireNewFile(output);
            const auto stored = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save isolated family scheme result");
        }
        // The working target and all of its opened objects are gone before persistence is checked.
        target.reset(); AcDbDatabase reopened(false, true);
        const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        reopenStatus = std::to_string(loaded); check(loaded, "reopen saved family result in a new database");
        const auto closed = reopened.closeInput(true); closeInputStatus = std::to_string(closed); check(closed, "close reopened family input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened);
            AcDbObjectId primary; check(reopened.getAcDbObjectId(primary, false, AcDbHandle(primaryHandle.c_str())), "find persisted family primary");
            saved = familyCompactEntity(primary); AcDbObjectIdArray originals, additions;
            for (const auto& handle : originalHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find persisted original scheme entity"); originals.append(id); }
            for (const auto& handle : addedHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find persisted family addition"); additions.append(id); }
            originalSaved = familyCompactArray(originals); savedAdded = familyCompactArray(additions);
        }
    } catch (const std::exception& failure) { error = familyError(failure); }
    catch (...) { error = quote(L"unknown family-case C++ exception"); }
    std::ostringstream out;
    out << "{\"id\":" << quote(fixture.id) << ",\"source\":" << quote(fixture.source) << ",\"sourceHandle\":" << quote(fixture.handle)
        << ",\"expectedType\":" << quote(fixture.type) << ",\"initialSchemeCount\":" << initialCount << ",\"error\":" << error
        << ",\"cloneStatus\":" << cloneStatus << ",\"transformStatus\":" << transformStatus << ",\"transformOpenStatus\":" << transformOpenStatus
        << ",\"transformTransactionStatus\":" << transactionStatus << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeInputStatus
        << ",\"sourceReadStatus\":" << sourceReadStatus << ",\"sourceInputCloseStatus\":" << sourceCloseStatus << ",\"schemeReadStatus\":" << schemeReadStatus << ",\"schemeInputCloseStatus\":" << schemeCloseStatus
        << ",\"outputDWG\":" << quote(output) << ",\"classInfo\":" << classInfo << ",\"sourceEntity\":" << sourceEntity
        << ",\"before\":" << before << ",\"after\":" << after << ",\"saved\":" << saved << ",\"offset\":[40000,0,0]"
        << ",\"originalBefore\":" << originalBefore << ",\"originalAfter\":" << originalAfter << ",\"originalSaved\":" << originalSaved
        << ",\"addedAfterClone\":" << addedAfterClone << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded << ",\"addedCount\":" << addedCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"primaryHandle\":" << quote(primaryHandle) << ",\"resourceMapping\":" << resources << ",\"associationStatus\":\"not validated\"}";
    return out.str();
}

void nativeObjectFamilyProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\object-family-native.json"); requireNewFile(report);
        const ObjectFamilyFixture fixtures[] = {
            {L"V01",L"scheme",L"CD83",L"TDbWall"}, {L"V02",L"scheme",L"CD84",L"TDbOpening"},
            {L"V03",L"scheme",L"CD89",L"TDbOpening"}, {L"V04",L"scheme",L"CDC0",L"TDbText"},
            {L"V05",L"reference",L"19A0",L"TDbDimension2"}, {L"V06",L"reference",L"199F",L"TDbAxisLabelSet"},
            {L"V07",L"reference",L"19F3",L"TDbDrawingName"}, {L"V08",L"reference",L"17DF",L"AcDbBlockReference"},
            {L"V09",L"scheme",L"CD94",L"TDbGroupOpening"}, {L"V10",L"scheme",L"CF5F",L"TDbRectCornerStair"},
            {L"V11",L"reference",L"199E",L"TDbSymbElevation"}, {L"V12",L"reference",L"19E9",L"TDbSymbMultiLeader"},
            {L"V13",L"reference",L"1A15",L"TDbSymbArrow"}, {L"V14",L"reference",L"1A05",L"TDbSymbSection"},
            {L"V15",L"reference",L"1A26",L"TDbPipe"}, {L"V16",L"reference",L"1A29",L"TDbPipeFitting"},
            {L"V17",L"reference",L"19AE",L"TDbColumn"}, {L"V18",L"reference",L"1A1E",L"TDbSymbComposing"}
        };
        std::ostringstream out;
        out << "{\"contract\":\"stage1.object-family-native.v1\",\"api\":\"ObjectARX native; no COM\",\"schemeInputPath\":" << quote(p.scheme)
            << ",\"referenceInputPath\":" << quote(p.reference) << ",\"fixtures\":[";
        bool comma = false;
        for (const auto& fixture : fixtures) { if (comma) out << ','; comma = true; out << familyFixtureCase(p, fixture); }
        out << "],\"scope\":\"native class/RX inventory, single-primary template clone into full scheme, rigid translation and persistence only; arbitrary parameter creation, semantic fields, associations and visual equivalence are not established\"}";
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_OBJECT_FAMILY_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_OBJECT_FAMILY_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_OBJECT_FAMILY_FAILED: unknown C++ exception\n"); }
}
