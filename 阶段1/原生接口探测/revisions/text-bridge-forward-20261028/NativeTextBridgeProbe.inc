// Include inside the existing anonymous namespace AFTER NativeStyleAudit.inc,
// NativeObjectFamilyProbe.inc and AxisNativeTraceOwner. No additional headers.
// Stage-1 public TTextReplace selection protocol; never write private DXF or
// claim unreadable Tianzheng business text from command status or changed bounds.

std::string textBridgeCommon(AcDbObjectId id) {
    auto entity = open<AcDbEntity>(id); AcString layer, plot;
    check(entity->layer(layer), "text bridge layer getter");
    const auto plotStatus = entity->plotStyleName(plot);
    const auto* text = AcDbText::cast(entity.get());
    std::ostringstream out;
    out << "{\"sameDatabase\":" << styleAuditBool(entity->database() == id.database())
        << ",\"owner\":" << styleAuditReference(entity->ownerId())
        << ",\"ownerIsModel\":" << styleAuditBool(entity->ownerId() == modelId(*id.database()))
        << ",\"layer\":" << quote(layer.kACharPtr()) << ",\"layerId\":" << styleAuditReference(entity->layerId())
        << ",\"color\":" << styleAuditColor(entity->color()) << ",\"transparency\":" << styleAuditTransparency(entity->transparency())
        << ",\"linetype\":" << styleAuditReference(entity->linetypeId()) << ",\"linetypeScale\":" << number(entity->linetypeScale())
        << ",\"lineWeight\":" << static_cast<int>(entity->lineWeight()) << ",\"visibility\":" << static_cast<int>(entity->visibility())
        << ",\"plotStyleNameStatus\":" << plotStatus << ",\"plotStyleName\":" << (plotStatus == Acad::eOk ? quote(plot.kACharPtr()) : "null")
        << ",\"material\":" << styleAuditReference(entity->materialId())
        << ",\"textStyle\":" << (text ? styleAuditReference(text->textStyle()) : "null")
        << ",\"textStyleDefinition\":" << (text ? styleAuditTextStyle(text->textStyle()) : "null")
        << ",\"fontObservation\":" << quote(text ? L"native text-style definition; actual rendered font not established"
            : L"Unknown: this native entity has no AcDbText text-style getter") << '}';
    return out.str();
}

std::string textBridgeNativeText(AcDbObjectId id, bool* readable = nullptr, std::wstring* actual = nullptr) {
    if (readable) *readable = false;
    if (actual) actual->clear();
    auto entity = open<AcDbEntity>(id); const auto* text = AcDbText::cast(entity.get());
    if (!text) return "{\"readable\":false,\"getter\":null,\"status\":null,\"value\":null,\"position\":null,\"height\":null}";
    AcString value; const auto status = text->textString(value);
    if (status == Acad::eOk) { if (readable) *readable = true; if (actual) *actual = value.kACharPtr(); }
    std::ostringstream out;
    out << "{\"readable\":" << styleAuditBool(status == Acad::eOk) << ",\"getter\":\"AcDbText::textString\",\"status\":" << status
        << ",\"value\":" << (status == Acad::eOk ? quote(value.kACharPtr()) : "null")
        << ",\"position\":" << point(text->position()) << ",\"height\":" << number(text->height()) << '}';
    return out.str();
}

std::string textBridgeState(AcDbObjectId id, bool auditDxf = false) {
    // Each helper closes its native object before the public EntGet audit.
    const auto geometry = familyCompactEntity(id), common = textBridgeCommon(id), native = textBridgeNativeText(id);
    return "{\"geometry\":" + geometry + ",\"common\":" + common + ",\"nativeText\":" + native
        + ",\"publicDxf\":" + (auditDxf ? publicAxisDxf(id) : "null") + '}';
}

std::string textBridgeStates(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) { if (i) out << ','; out << textBridgeState(ids[i]); }
    out << ']'; return out.str();
}

std::string textBridgeTrace(const AxisNativeTraceData& trace) {
    std::ostringstream out;
    out << "{\"editorAvailable\":" << styleAuditBool(trace.editorAvailable) << ",\"managerAvailable\":" << styleAuditBool(trace.managerAvailable)
        << ",\"editorAdded\":" << styleAuditBool(trace.editorAdded) << ",\"editorRemoved\":" << styleAuditBool(trace.editorRemoved)
        << ",\"inputAddAttempted\":" << styleAuditBool(trace.inputAddAttempted) << ",\"inputAddReturned\":" << styleAuditBool(trace.inputAddReturned)
        << ",\"inputAddStatus\":" << (trace.inputAddReturned ? std::to_string(trace.inputAddStatus) : "null")
        << ",\"inputRemoveAttempted\":" << styleAuditBool(trace.inputRemoveAttempted) << ",\"inputRemoveReturned\":" << styleAuditBool(trace.inputRemoveReturned)
        << ",\"inputRemoveStatus\":" << (trace.inputRemoveReturned ? std::to_string(trace.inputRemoveStatus) : "null")
        << ",\"traceDropped\":" << styleAuditBool(trace.traceDropped) << ",\"retained\":" << styleAuditBool(trace.retained)
        << ",\"moduleLockAttempted\":" << styleAuditBool(trace.moduleLockAttempted) << ",\"moduleLockSucceeded\":" << styleAuditBool(trace.moduleLockSucceeded) << '}';
    return out.str();
}

AcGePoint3d textBridgePick(AcDbObjectId id) {
    auto entity = open<AcDbEntity>(id); AcDbExtents bounds;
    check(entity->getGeomExtents(bounds), "text bridge actual pick extents");
    const auto lo = bounds.minPoint(), hi = bounds.maxPoint();
    if (!(hi.x > lo.x && hi.y > lo.y)) throw std::runtime_error("text bridge text has no finite two-dimensional pick extent");
    return AcGePoint3d((lo.x + hi.x) / 2.0, (lo.y + hi.y) / 2.0, (lo.z + hi.z) / 2.0);
}

int textBridgeToUcs(const AcGePoint3d& world, AcGePoint3d& user) {
    ads_point input{world.x, world.y, world.z}, output{}; resbuf from{}, to{};
    from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
    const int status = acedTrans(input, &from, &to, 0, output);
    if (status == RTNORM) user = AcGePoint3d(output[0], output[1], output[2]);
    return status;
}

struct TextBridgePair {
    std::wstring id, expected, targetHandle, donorHandle;
    AcDbObjectId target, donor;
    AcGePoint3d wanted, donorPick, targetPick, donorUcs, targetUcs;
    AcGeVector3d translation{0, 0, 0};
    Acad::ErrorStatus appendTarget{Acad::eNotApplicable}, moved{Acad::eNotApplicable}, appendDonor{Acad::eNotApplicable};
    bool transient{}, sameType{}, attempted{}, closedBefore{}, geometryChanged{}, readableAfter{}, readableSaved{};
    int transactionsBefore{-1}, transactionsAfter{-1};
    std::string commandStatus{"null"}, commandError{"null"}, targetBefore{"null"}, targetAfter{"null"}, targetSaved{"null"};
    std::string donorBefore{"null"}, donorAfter{"null"}, donorSaved{"null"}, beforeGeometry, afterGeometry;
    AcDbIntArray conversionStatuses;
    std::wstring actualAfter, actualSaved;
    std::shared_ptr<AxisNativeTraceData> trace{std::make_shared<AxisNativeTraceData>()};
};

void textBridgeCreatePair(AcDbDatabase& db, AcDbObjectId sourceId, TextBridgePair& pair) {
    std::unique_ptr<AcDbEntity> clone;
    {
        auto source = open<AcDbEntity>(sourceId);
        std::unique_ptr<AcRxObject> object(source->clone());
        auto* entity = object ? AcDbEntity::cast(object.get()) : nullptr;
        if (!entity) throw std::runtime_error("CDC0 native clone returned no AcDbEntity");
        pair.transient = entity->objectId().isNull() && entity->database() == nullptr;
        pair.sameType = entity->isA() == source->isA() && !entity->isAProxy();
        if (!pair.transient || !pair.sameType) throw std::runtime_error("CDC0 clone is not a transient native TDbText");
        AcDbExtents bounds; check(source->getGeomExtents(bounds), "read original native text clone placement extent");
        pair.translation = pair.wanted - bounds.minPoint();
        object.release(); clone.reset(entity);
    }
    {
        auto model = open<AcDbBlockTableRecord>(modelId(db), AcDb::kForWrite);
        pair.appendTarget = model->appendAcDbEntity(pair.target, clone.get()); check(pair.appendTarget, "append unchanged-type native text clone");
        auto* resident = clone.release(); check(resident->close(), "close database-owned native text clone");
    }
    {
        auto resident = open<AcDbEntity>(pair.target, AcDb::kForWrite);
        pair.moved = resident->transformBy(AcGeMatrix3d::translation(pair.translation)); check(pair.moved, "place native text clone in diagnosis area");
    }
    pair.targetHandle = handleOf(pair.target);
    auto donor = std::make_unique<AcDbText>(); donor->setDatabaseDefaults(&db);
    // Existing scheme resource only; actual font definition is reported, not assumed rendered.
    { auto styles = open<AcDbTextStyleTable>(db.textStyleTableId()); AcDbObjectId style;
      check(styles->getAt(L"黑体", style), "find existing scheme text style for readable donor legend");
      check(donor->setTextStyle(style), "set target-owned donor text-style reference"); }
    { auto source = open<AcDbEntity>(sourceId); check(donor->setLayer(source->layerId()), "set donor to existing source text layer"); }
    check(donor->setTextString(pair.expected.c_str()), "set actual native donor text");
    check(donor->setHeight(350), "set diagnostic donor text height");
    check(donor->setPosition(pair.wanted + AcGeVector3d(0, -2000, 0)), "set diagnostic donor legend position");
    {
        auto model = open<AcDbBlockTableRecord>(modelId(db), AcDb::kForWrite);
        pair.appendDonor = model->appendAcDbEntity(pair.donor, donor.get()); check(pair.appendDonor, "append native text donor");
        auto* resident = donor.release(); check(resident->close(), "close database-owned native text donor");
    }
    pair.donorHandle = handleOf(pair.donor);
    pair.targetBefore = textBridgeState(pair.target, true); pair.donorBefore = textBridgeState(pair.donor);
    pair.beforeGeometry = familyCompactEntity(pair.target);
    pair.donorPick = textBridgePick(pair.donor); pair.targetPick = textBridgePick(pair.target);
}

std::string textBridgePairJson(const TextBridgePair& pair) {
    std::ostringstream out;
    out << "{\"id\":" << quote(pair.id) << ",\"expectedText\":" << quote(pair.expected) << ",\"targetMinWcs\":" << point(pair.wanted)
        << ",\"translation\":[" << number(pair.translation.x) << ',' << number(pair.translation.y) << ',' << number(pair.translation.z) << ']'
        << ",\"cloneWasTransient\":" << styleAuditBool(pair.transient) << ",\"cloneSameNativeType\":" << styleAuditBool(pair.sameType)
        << ",\"appendTargetStatus\":" << pair.appendTarget << ",\"transformStatus\":" << pair.moved << ",\"appendDonorStatus\":" << pair.appendDonor
        << ",\"allObjectsClosedBeforeCommand\":" << styleAuditBool(pair.closedBefore) << ",\"activeTransactionsBeforeCommand\":" << pair.transactionsBefore
        << ",\"activeTransactionsAfterCommand\":" << pair.transactionsAfter << ",\"targetBefore\":" << pair.targetBefore << ",\"targetAfter\":" << pair.targetAfter
        << ",\"targetSaved\":" << pair.targetSaved << ",\"donorBefore\":" << pair.donorBefore << ",\"donorAfter\":" << pair.donorAfter << ",\"donorSaved\":" << pair.donorSaved
        << ",\"commandName\":\"TTextReplace\",\"commandAttempted\":" << styleAuditBool(pair.attempted) << ",\"commandStatus\":" << pair.commandStatus
        << ",\"commandError\":" << pair.commandError << ",\"tokens\":[\"TTextReplace\",\"donor point\",\"\",\"target point\",\"\"]"
        << ",\"donorPickWcs\":" << point(pair.donorPick) << ",\"targetPickWcs\":" << point(pair.targetPick)
        << ",\"donorPickUcs\":" << point(pair.donorUcs) << ",\"targetPickUcs\":" << point(pair.targetUcs)
        << ",\"coordinateTransformStatuses\":" << integers(pair.conversionStatuses)
        << ",\"geometryChanged\":" << styleAuditBool(pair.geometryChanged) << ",\"commandTrace\":" << axisTraceArray(pair.trace->commands)
        << ",\"inputTrace\":" << axisTraceArray(pair.trace->inputs) << ",\"trace\":" << textBridgeTrace(*pair.trace) << '}';
    return out.str();
}

void nativeTextBridgeProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\text-bridge-native.json");
        const auto control = fullPath(p.output + L"\\control.dwg"), output = fullPath(p.output + L"\\text-bridge-generated.dwg");
        if (!below(report, p.output) || !below(control, p.output) || !below(output, p.output)) throw std::runtime_error("text bridge path outside isolated run");
        requireNewFile(report); requireNewFile(output);
        std::string error{"null"}, source{"null"}, before{"[]"}, after{"[]"}, savedOriginal{"[]"}, added{"[]"}, savedAdded{"[]"};
        std::string saveStatus{"null"}, reopenStatus{"null"}, closeStatus{"null"}, zoomStatus{"null"}, regenStatus{"null"};
        std::wstring business = L"pending rendered verification";
        int initialCount = -1, afterCount = -1, reopenedCount = -1, extraCount = -1;
        std::vector<TextBridgePair> pairs(2);
        pairs[0].id = L"code"; pairs[0].expected = L"C09235"; pairs[0].wanted = AcGePoint3d(40000, 20000, 0);
        pairs[1].id = L"pending"; pairs[1].expected = L"待确认"; pairs[1].wanted = AcGePoint3d(44000, 20000, 0);
        try {
            if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("text bridge requires modal document context");
            auto* document = acDocManager->curDocument(); auto* database = document ? document->database() : nullptr;
            const auto guard = [&] {
                if (!document || !database || acDocManager->isApplicationContext() || acDocManager->curDocument() != document
                    || acDocManager->mdiActiveDocument() != document || document->database() != database
                    || acdbHostApplicationServices()->workingDatabase() != database || !document->fileName()
                    || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str())) throw std::runtime_error("text bridge current/MDI/working control guard failed");
                const ACHAR* filename{}; check(database->getFilename(filename), "text bridge native database filename");
                if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("text bridge native filename guard failed");
                if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
                    throw std::runtime_error("text bridge forbids active transactions across native commands");
            };
            guard(); const auto originalIds = modelIds(*database); initialCount = originalIds.length();
            if (initialCount != 213) throw std::runtime_error("text bridge requires the complete frozen scheme213 control");
            before = textBridgeStates(originalIds); std::vector<std::wstring> originalHandles;
            for (int i = 0; i < originalIds.length(); ++i) originalHandles.push_back(handleOf(originalIds[i]));
            AcDbObjectId sourceId; check(database->getAcDbObjectId(sourceId, false, AcDbHandle(L"CDC0")), "find original native CDC0 text");
            { auto entity = open<AcDbEntity>(sourceId);
              if (_wcsicmp(entity->isA()->name(), L"TDbText") || entity->isAProxy()) throw std::runtime_error("CDC0 is not native TDbText"); }
            source = textBridgeState(sourceId, true);
            for (auto& pair : pairs) textBridgeCreatePair(*database, sourceId, pair);
            // All source, clone, donor, table and model objects are now closed.
            // View-only commands on this owned control help point selection of the new objects.
            AcGePoint3d viewFirst, viewLast;
            if (textBridgeToUcs(AcGePoint3d(39000, 17000, 0), viewFirst) != RTNORM
                || textBridgeToUcs(AcGePoint3d(47000, 22000, 0), viewLast) != RTNORM) throw std::runtime_error("text bridge diagnostic view conversion failed");
            ads_point viewA{viewFirst.x, viewFirst.y, viewFirst.z}, viewB{viewLast.x, viewLast.y, viewLast.z};
            guard(); const int zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_W", RT3DPOINT, viewA, RT3DPOINT, viewB, RTNONE);
            zoomStatus = std::to_string(zoomed); guard();
            const int regenerated = acedCommandS(RTSTR, L"_.REGEN", RTNONE); regenStatus = std::to_string(regenerated); guard();
            if (zoomed != RTNORM || regenerated != RTNORM) throw std::runtime_error("text bridge diagnostic view commands failed");
            for (auto& pair : pairs) {
                const int a = textBridgeToUcs(pair.donorPick, pair.donorUcs), b = textBridgeToUcs(pair.targetPick, pair.targetUcs);
                pair.conversionStatuses.append(a); pair.conversionStatuses.append(b);
                if (a != RTNORM || b != RTNORM) throw std::runtime_error("text bridge point conversion failed");
                guard(); pair.transactionsBefore = database->transactionManager()->numActiveTransactions(); pair.closedBefore = true;
                ads_point donor{pair.donorUcs.x, pair.donorUcs.y, pair.donorUcs.z}, target{pair.targetUcs.x, pair.targetUcs.y, pair.targetUcs.z};
                int result = RTERROR;
                {
                    AxisNativeTraceOwner trace(document, pair.trace); pair.attempted = true;
                    result = acedCommandS(RTSTR, L"TTextReplace", RT3DPOINT, donor, RTSTR, L"", RT3DPOINT, target, RTSTR, L"", RTNONE);
                }
                pair.commandStatus = std::to_string(result);
                if (result != RTNORM) pair.commandError = quote(L"TTextReplace returned non-RTNORM; actual objects retained without a success inference");
                for (const auto& event : pair.trace->commands) {
                    if (event.find("\"event\":\"commandCancelled\"") != std::string::npos
                        || event.find("\"event\":\"commandFailed\"") != std::string::npos
                        || event.find("\"event\":\"unknownCommand\"") != std::string::npos)
                        pair.commandError = quote(L"actual command trace reports cancellation/failure/unknown command; objects retained for diagnosis");
                }
                if (pair.trace->retained) throw std::runtime_error("text bridge trace retained after failed detach; module must remain loaded");
                guard(); pair.transactionsAfter = database->transactionManager()->numActiveTransactions();
                pair.targetAfter = textBridgeState(pair.target, true); pair.donorAfter = textBridgeState(pair.donor);
                pair.afterGeometry = familyCompactEntity(pair.target); pair.geometryChanged = pair.beforeGeometry != pair.afterGeometry;
                textBridgeNativeText(pair.target, &pair.readableAfter, &pair.actualAfter);
            }
            guard(); after = textBridgeStates(originalIds); const auto additions = familyAdditions(*database, originalIds);
            afterCount = modelCount(*database); extraCount = additions.length() - 4; added = textBridgeStates(additions);
            std::vector<std::wstring> addedHandles;
            for (int i = 0; i < additions.length(); ++i) addedHandles.push_back(handleOf(additions[i]));
            requireNewFile(output); const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT);
            saveStatus = std::to_string(stored); check(stored, "save text bridge diagnosis output");
            // An independent saved-file database is used; the owned host control remains alive.
            AcDbDatabase reopened(false, true);
            const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
            reopenStatus = std::to_string(loaded); check(loaded, "reopen saved text bridge in independent database");
            const auto closed = reopened.closeInput(true); closeStatus = std::to_string(closed); check(closed, "close text bridge persisted input");
            {
                WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray originals, additionsSaved;
                for (const auto& handle : originalHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find persisted original text bridge object"); originals.append(id); }
                for (const auto& handle : addedHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find persisted text bridge addition"); additionsSaved.append(id); }
                savedOriginal = textBridgeStates(originals); savedAdded = textBridgeStates(additionsSaved);
                for (auto& pair : pairs) {
                    AcDbObjectId targetId, donorId;
                    check(reopened.getAcDbObjectId(targetId, false, AcDbHandle(pair.targetHandle.c_str())), "find persisted native TDbText target");
                    check(reopened.getAcDbObjectId(donorId, false, AcDbHandle(pair.donorHandle.c_str())), "find persisted donor legend");
                    pair.targetSaved = textBridgeState(targetId, true); pair.donorSaved = textBridgeState(donorId);
                    textBridgeNativeText(targetId, &pair.readableSaved, &pair.actualSaved);
                }
            }
            bool allReadable = true, allMatched = true, knownMismatch = false;
            for (const auto& pair : pairs) {
                allReadable = allReadable && pair.readableAfter && pair.readableSaved;
                allMatched = allMatched && pair.actualAfter == pair.expected && pair.actualSaved == pair.expected;
                knownMismatch = knownMismatch || (pair.readableAfter && pair.actualAfter != pair.expected)
                    || (pair.readableSaved && pair.actualSaved != pair.expected);
            }
            if (knownMismatch) business = L"native text getter mismatch";
            else if (allReadable && allMatched) business = L"native text getter verified";
        } catch (const std::exception& failure) { error = styleAuditError(failure); }
        catch (...) { error = quote(L"unknown text bridge C++ exception"); }
        std::ostringstream pairJson; pairJson << '[';
        for (std::size_t i = 0; i < pairs.size(); ++i) { if (i) pairJson << ','; pairJson << textBridgePairJson(pairs[i]); }
        pairJson << ']'; std::ostringstream out;
        out << "{\"contract\":\"stage1.text-bridge-native.v1\",\"api\":\"ObjectARX native; no COM\",\"error\":" << error
            << ",\"schemeInputPath\":" << quote(p.scheme) << ",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
            << ",\"sourceTarget\":" << source << ",\"originalBefore\":" << before << ",\"originalAfter\":" << after << ",\"originalSaved\":" << savedOriginal
            << ",\"initialModelCount\":" << initialCount << ",\"modelCountAfter\":" << afterCount << ",\"reopenedModelCount\":" << reopenedCount
            << ",\"extraModelEntityCount\":" << extraCount << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded << ",\"pairs\":" << pairJson.str()
            << ",\"zoomStatus\":" << zoomStatus << ",\"regenStatus\":" << regenStatus << ",\"saveStatus\":" << saveStatus
            << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus << ",\"businessTextVerification\":" << quote(business)
            << ",\"renderedVerification\":\"not performed by this probe\",\"persistenceMethod\":\"saved then independent AcDbDatabase read; owned host control remains alive\""
            << ",\"helpProtocol\":\"C:/Tangent/TArchT20V8/SYS/TArch.chm :: 12textable/天正文字工具.htm :: TTextReplace\""
            << ",\"scope\":\"two isolated same-database TDbText clones and public donor/target selection cycles; unreadable target content remains pending rendered verification\"}";
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_TEXT_BRIDGE_COMPLETED; STRUCTURE_AND_RENDERED_ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_TEXT_BRIDGE_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_TEXT_BRIDGE_FAILED: unknown C++ exception\n"); }
}
