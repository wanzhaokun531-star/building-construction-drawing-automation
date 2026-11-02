// Include AFTER NativeTextBridgeProbe.inc in the same anonymous namespace.
// Reuses its public native clone, text, state, coordinate and trace helpers.
// These fixed strings are test fixtures, never project drawing/elevation values.

template<class Guard>
void symbolTextRoundtrip(AcApDocument* document, AcDbDatabase& database, TextBridgePair& pair, const wchar_t* expectedType, Guard& guard) {
    const int sourceConverted = textBridgeToUcs(pair.donorPick, pair.donorUcs), destinationConverted = textBridgeToUcs(pair.targetPick, pair.targetUcs);
    pair.conversionStatuses.append(sourceConverted); pair.conversionStatuses.append(destinationConverted);
    if (sourceConverted != RTNORM || destinationConverted != RTNORM) throw std::runtime_error("symbol text forward point conversion failed");
    guard(); pair.transactionsBefore = database.transactionManager()->numActiveTransactions(); pair.closedBefore = true;
    const int result = textBridgeInvoke(document, pair.donorUcs, pair.targetUcs, pair.trace, pair.attempted, pair.commandStatus, pair.commandError);
    if (pair.trace->retained || textBridgeHasUnbalancedCommandTrace(*pair.trace)) throw std::runtime_error("symbol text forward trace retained or unbalanced; further commands withheld");
    guard(); pair.transactionsAfter = database.transactionManager()->numActiveTransactions();
    pair.forwardCompleted = textBridgeCommandCompleted(result, *pair.trace, pair.donorHandle, pair.targetHandle);
    pair.donorForwardAfter = textBridgeState(pair.donor);
    if (pair.sentinel == pair.expected) throw std::runtime_error("symbol text sentinel must be independent of expected content");
    { auto donor = open<AcDbText>(pair.donor, AcDb::kForWrite); pair.sentinelSetStatus = donor->setTextString(pair.sentinel.c_str()); check(pair.sentinelSetStatus, "set independent symbol-text donor sentinel"); }
    bool readable{}; std::wstring actual;
    textBridgeNativeText(pair.donor, &readable, &actual); pair.sentinelConfirmed = readable && actual == pair.sentinel; pair.donorSentinel = textBridgeState(pair.donor);
    if (!pair.sentinelConfirmed) throw std::runtime_error("symbol-text sentinel getter did not confirm the new nonexpected value");
    guard(); const int regenerated = acedCommandS(RTSTR, L"_.REGEN", RTNONE); pair.reverseRegenStatus = std::to_string(regenerated); guard();
    if (regenerated != RTNORM) throw std::runtime_error("symbol text reverse regeneration failed");
    textBridgeNativeText(pair.donor, &readable, &actual); pair.sentinelConfirmed = readable && actual == pair.sentinel;
    pair.donorSentinel = textBridgeState(pair.donor); pair.targetForwardAfter = textBridgeState(pair.target, true);
    if (!pair.sentinelConfirmed) throw std::runtime_error("symbol-text sentinel did not survive regeneration; reverse withheld");
    pair.reverseSourcePick = textBridgePick(pair.target); pair.reverseDonorPick = textBridgePick(pair.donor);
    const int a = textBridgeToUcs(pair.reverseSourcePick, pair.reverseSourceUcs), b = textBridgeToUcs(pair.reverseDonorPick, pair.reverseDonorUcs);
    pair.reverseConversionStatuses.append(a); pair.reverseConversionStatuses.append(b);
    if (a != RTNORM || b != RTNORM) throw std::runtime_error("symbol text reverse point conversion failed");
    guard(); pair.reverseTransactionsBefore = database.transactionManager()->numActiveTransactions(); pair.reverseClosedBefore = true;
    const int reverse = textBridgeInvoke(document, pair.reverseSourceUcs, pair.reverseDonorUcs, pair.reverseTrace, pair.reverseAttempted, pair.reverseStatus, pair.reverseError);
    if (pair.reverseTrace->retained || textBridgeHasUnbalancedCommandTrace(*pair.reverseTrace)) throw std::runtime_error("symbol text reverse trace retained or unbalanced; further commands withheld");
    guard(); pair.reverseTransactionsAfter = database.transactionManager()->numActiveTransactions();
    pair.reverseCompleted = textBridgeCommandCompleted(reverse, *pair.reverseTrace, pair.targetHandle, pair.donorHandle);
    pair.reverseReadback = textBridgeNativeText(pair.donor, &pair.readbackReadable, &pair.readbackValue);
    pair.targetAfter = textBridgeState(pair.target, true); pair.donorAfter = textBridgeState(pair.donor);
    pair.afterGeometry = familyCompactEntity(pair.target); pair.geometryChanged = pair.beforeGeometry != pair.afterGeometry;
    textBridgeNativeText(pair.target, &pair.readableAfter, &pair.actualAfter);
    { auto target = open<AcDbEntity>(pair.target); pair.targetTypeAfter = !_wcsicmp(target->isA()->name(), expectedType) && !target->isAProxy(); }
}

void nativeSymbolTextBridgeProbe() {
    try {
        const auto p = paths(); const auto control = fullPath(p.output + L"\\control.dwg");
        const auto output = fullPath(p.output + L"\\symbol-text-bridge-generated.dwg"), report = fullPath(p.output + L"\\symbol-text-bridge-native.json");
        if (!below(control, p.output) || !below(output, p.output) || !below(report, p.output)) throw std::runtime_error("symbol text paths outside isolated run");
        requireNewFile(output); requireNewFile(report);
        std::string error{"null"}, before{"[]"}, after{"[]"}, savedOriginal{"[]"}, added{"[]"}, savedAdded{"[]"};
        std::string zoomStatus{"null"}, regenStatus{"null"}, saveStatus{"null"}, reopenStatus{"null"}, closeStatus{"null"};
        int initialCount = -1, afterCount = -1, reopenedCount = -1, extraCount = -1;
        const wchar_t* sourceHandles[]{ L"CFCE", L"CE8F" }; const wchar_t* types[]{ L"TDbDrawingName", L"TDbSymbElevation" };
        std::vector<TextBridgePair> pairs(2); std::vector<std::string> sourceStates(2, "null"), caseErrors(2, "null");
        pairs[0].id = L"drawing-name"; pairs[0].expected = L"S1_TEST_NAME"; pairs[0].sentinel = L"S1_SYMBOL_UNREAD_NAME"; pairs[0].wanted = AcGePoint3d(40000, 28000, 0);
        pairs[1].id = L"elevation"; pairs[1].expected = L"+0.123"; pairs[1].sentinel = L"S1_SYMBOL_UNREAD_ELEV"; pairs[1].wanted = AcGePoint3d(56000, 28000, 0);
        try {
            if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("symbol text bridge requires modal document context");
            auto* document = acDocManager->curDocument(); auto* database = document ? document->database() : nullptr;
            const auto guard = [&] {
                if (!document || !database || acDocManager->isApplicationContext() || acDocManager->curDocument() != document || acDocManager->mdiActiveDocument() != document
                    || document->database() != database || acdbHostApplicationServices()->workingDatabase() != database || !document->fileName()
                    || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str())) throw std::runtime_error("symbol text bridge current/MDI/working control guard failed");
                const ACHAR* name{}; check(database->getFilename(name), "symbol text bridge native filename");
                if (!name || _wcsicmp(fullPath(name).c_str(), control.c_str())) throw std::runtime_error("symbol text bridge native filename guard failed");
                if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0) throw std::runtime_error("symbol text bridge forbids active transactions across commands");
            };
            guard(); const auto originalIds = modelIds(*database); initialCount = originalIds.length();
            if (initialCount != 213) throw std::runtime_error("symbol text bridge requires the complete frozen scheme213 control");
            before = textBridgeStates(originalIds); std::vector<std::wstring> originalHandles;
            for (int i = 0; i < originalIds.length(); ++i) originalHandles.push_back(handleOf(originalIds[i]));
            for (int i = 0; i < 2; ++i) {
                AcDbObjectId sourceId; check(database->getAcDbObjectId(sourceId, false, AcDbHandle(sourceHandles[i])), "find actual composite text source");
                { auto source = open<AcDbEntity>(sourceId); if (_wcsicmp(source->isA()->name(), types[i]) || source->isAProxy()) throw std::runtime_error("actual source is not the required native drawing-name/elevation class"); }
                sourceStates[i] = textBridgeState(sourceId, true); textBridgeCreatePair(*database, sourceId, pairs[i]);
            }
            // All original/template/table/clone/donor objects are closed before
            // view or replacement commands. The view affects this owned copy only.
            AcGePoint3d worldA(39000, 25000, 0), worldB(68000, 33000, 0), userA, userB;
            if (textBridgeToUcs(worldA, userA) != RTNORM || textBridgeToUcs(worldB, userB) != RTNORM) throw std::runtime_error("symbol text diagnostic view conversion failed");
            ads_point a{userA.x, userA.y, userA.z}, b{userB.x, userB.y, userB.z};
            guard(); const int zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_W", RT3DPOINT, a, RT3DPOINT, b, RTNONE); zoomStatus = std::to_string(zoomed); guard();
            const int regenerated = acedCommandS(RTSTR, L"_.REGEN", RTNONE); regenStatus = std::to_string(regenerated); guard();
            if (zoomed != RTNORM || regenerated != RTNORM) throw std::runtime_error("symbol text diagnostic view commands failed");
            for (int i = 0; i < 2; ++i) {
                try { symbolTextRoundtrip(document, *database, pairs[i], types[i], guard); }
                catch (const std::exception& failure) { caseErrors[i] = styleAuditError(failure); }
                catch (...) { caseErrors[i] = quote(L"unknown compound-symbol text roundtrip exception"); }
                // An unsupported but fully ended operation remains a per-class
                // failure. Retained callbacks or incomplete commands stop all use.
                if (pairs[i].trace->retained || pairs[i].reverseTrace->retained || textBridgeHasUnbalancedCommandTrace(*pairs[i].trace)
                    || textBridgeHasUnbalancedCommandTrace(*pairs[i].reverseTrace)) throw std::runtime_error("compound-symbol command state is unsafe; further host operations withheld");
                guard();
            }
            after = textBridgeStates(originalIds); const auto additions = familyAdditions(*database, originalIds);
            afterCount = modelCount(*database); extraCount = additions.length()-4; added = textBridgeStates(additions); std::vector<std::wstring> newHandles;
            for (int i = 0; i < additions.length(); ++i) newHandles.push_back(handleOf(additions[i]));
            requireNewFile(output); const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save compound-symbol bridge output");
            AcDbDatabase reopened(false, true); const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loaded); check(loaded, "reopen compound-symbol bridge output");
            const auto closed = reopened.closeInput(true); closeStatus = std::to_string(closed); check(closed, "close compound-symbol reopened input");
            {
                WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray originals, additionsSaved;
                for (const auto& handle : originalHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find saved compound-symbol original"); originals.append(id); }
                for (const auto& handle : newHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find saved compound-symbol addition"); additionsSaved.append(id); }
                savedOriginal = textBridgeStates(originals); savedAdded = textBridgeStates(additionsSaved);
                for (int i = 0; i < 2; ++i) {
                    auto& pair = pairs[i]; AcDbObjectId target, donor;
                    check(reopened.getAcDbObjectId(target, false, AcDbHandle(pair.targetHandle.c_str())), "find saved native compound-symbol target");
                    check(reopened.getAcDbObjectId(donor, false, AcDbHandle(pair.donorHandle.c_str())), "find saved native compound-symbol donor");
                    pair.targetSaved = textBridgeState(target, true); pair.donorSaved = textBridgeState(donor);
                    textBridgeNativeText(target, &pair.readableSaved, &pair.actualSaved); textBridgeNativeText(donor, &pair.donorSavedReadable, &pair.donorSavedValue);
                    { auto object = open<AcDbEntity>(target); pair.targetTypeSaved = !_wcsicmp(object->isA()->name(), types[i]) && !object->isAProxy(); }
                }
            }
        } catch (const std::exception& failure) { error = styleAuditError(failure); }
          catch (...) { error = quote(L"unknown native compound-symbol text bridge exception"); }
        bool allMatched = true; std::ostringstream serialized; serialized << '[';
        for (int i = 0; i < 2; ++i) {
            auto& pair = pairs[i];
            const bool directMismatch = (pair.readableAfter && pair.actualAfter != pair.expected) || (pair.readableSaved && pair.actualSaved != pair.expected);
            pair.roundtripMatched = caseErrors[i] == "null" && pair.forwardCompleted && pair.sentinelConfirmed && pair.reverseCompleted && pair.readbackReadable
                && pair.readbackValue == pair.expected && pair.donorSavedReadable && pair.donorSavedValue == pair.expected && pair.targetTypeAfter && pair.targetTypeSaved && !directMismatch;
            allMatched = allMatched && pair.roundtripMatched; if (i) serialized << ',';
            const auto body = textBridgePairJson(pair);
            serialized << "{\"sourceHandle\":" << quote(sourceHandles[i]) << ",\"expectedNativeType\":" << quote(types[i]) << ",\"sourceOriginal\":" << sourceStates[i]
                << ",\"caseError\":" << caseErrors[i] << ",\"contentVerification\":" << quote(pair.roundtripMatched ? L"command-bridge roundtrip verified" : L"not verified") << ',' << body.substr(1);
        }
        serialized << ']'; std::ostringstream out;
        out << "{\"contract\":\"stage1.symbol-text-bridge-native.v1\",\"api\":\"ObjectARX native; no COM\",\"error\":" << error
            << ",\"schemeInputPath\":" << quote(p.scheme) << ",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
            << ",\"initialModelCount\":" << initialCount << ",\"modelCountAfter\":" << afterCount << ",\"reopenedModelCount\":" << reopenedCount << ",\"extraModelEntityCount\":" << extraCount
            << ",\"originalBefore\":" << before << ",\"originalAfter\":" << after << ",\"originalSaved\":" << savedOriginal << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded << ",\"pairs\":" << serialized.str()
            << ",\"zoomStatus\":" << zoomStatus << ",\"regenStatus\":" << regenStatus << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus
            << ",\"businessTextVerification\":" << quote(allMatched ? L"command-bridge roundtrip verified" : L"not verified")
            << ",\"fixtureValuesAreProjectParameters\":false,\"renderedVerification\":\"pending\",\"fontVerification\":\"pending rendered verification\""
            << ",\"scope\":\"two real composite native classes only; exact sentinel-protected text roundtrip does not establish elevation format/units, independent drawing-name scale-field access, associations or phase-2 readiness\"}";
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_SYMBOL_TEXT_BRIDGE_COMPLETED; PER_CLASS_CONTENT_ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_SYMBOL_TEXT_BRIDGE_FAILED: %hs\n", failure.what()); }
      catch (...) { acutPrintf(L"\nSTAGE1_SYMBOL_TEXT_BRIDGE_FAILED: unknown C++ exception\n"); }
}
