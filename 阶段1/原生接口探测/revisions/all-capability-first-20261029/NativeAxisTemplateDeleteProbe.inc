// Include inside NativeAccessProbe.cpp's anonymous namespace after Family and
// AxisNativeTraceOwner. No new SDK headers/libraries. A single documented
// TDelLabel operation only: no batch pruning, renumbering or axis movement.

std::vector<double> axisTemplateDeleteXs(const AcGePoint3dArray& points) {
    std::vector<double> values, result;
    for (int i = 0; i < points.length(); ++i) values.push_back(points[i].x);
    std::sort(values.begin(), values.end());
    for (const auto value : values)
        if (result.empty() || std::fabs(value-result.back()) > 1e-6) result.push_back(value);
    return result;
}

std::string axisTemplateDeleteNumbers(const std::vector<double>& values) {
    std::ostringstream out; out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) { if (i) out << ','; out << number(values[i]); }
    out << ']'; return out.str();
}

bool axisTemplateDeleteHasPoint(const AcGePoint3dArray& values, const AcGePoint3d& point) {
    for (int i = 0; i < values.length(); ++i) if (values[i].distanceTo(point) <= 1e-6) return true;
    return false;
}

bool axisTemplateDeleteSamePoints(const AcGePoint3dArray& a, const AcGePoint3dArray& b) {
    if (a.length() != b.length()) return false;
    for (int i = 0; i < a.length(); ++i) if (a[i].distanceTo(b[i]) > 1e-6) return false;
    return true;
}

struct AxisTemplateDeleteObservation {
    Acad::ErrorStatus readStatus{Acad::eNotApplicable}, gripStatus{Acad::eNotApplicable}, stretchStatus{Acad::eNotApplicable};
    std::string entity{"null"};
    AcGePoint3dArray grips, stretch, topCandidates, bottomCandidates;
    std::vector<double> unionXs, gripXs;
    double topY{std::numeric_limits<double>::quiet_NaN()}, bottomY{std::numeric_limits<double>::quiet_NaN()};
    bool available() const { return readStatus == Acad::eOk && gripStatus == Acad::eOk && stretchStatus == Acad::eOk && !grips.isEmpty() && !stretch.isEmpty(); }
    std::string json() const {
        if (!available()) return "null";
        std::ostringstream out;
        out << "{\"unionXs\":" << axisTemplateDeleteNumbers(unionXs) << ",\"gripXs\":" << axisTemplateDeleteNumbers(gripXs)
            << ",\"gripPointCount\":" << grips.length() << ",\"stretchPointCount\":" << stretch.length()
            << ",\"topGripY\":" << number(topY) << ",\"bottomGripY\":" << number(bottomY)
            << ",\"visibleTopCircleCandidates\":" << points(topCandidates) << ",\"visibleBottomCircleCandidates\":" << points(bottomCandidates) << '}';
        return out.str();
    }
};

AxisTemplateDeleteObservation axisTemplateDeleteObserve(AcDbObjectId id) {
    AxisTemplateDeleteObservation observed;
    {
        AcDbEntity* raw{}; observed.readStatus = acdbOpenObject(raw, id, AcDb::kForRead); Opened<AcDbEntity> entity(raw);
        if (observed.readStatus != Acad::eOk || !entity) return observed;
        AcDbIntArray snaps, geometry;
        observed.gripStatus = entity->getGripPoints(observed.grips, snaps, geometry);
        observed.stretchStatus = entity->getStretchPoints(observed.stretch);
    }
    observed.entity = familyCompactEntity(id);
    if (!observed.available()) return observed;
    observed.unionXs = axisTemplateDeleteXs(observed.stretch); observed.gripXs = axisTemplateDeleteXs(observed.grips);
    observed.topY = observed.bottomY = observed.grips[0].y;
    for (int i = 1; i < observed.grips.length(); ++i) {
        observed.topY = (std::max)(observed.topY, observed.grips[i].y);
        observed.bottomY = (std::min)(observed.bottomY, observed.grips[i].y);
    }
    for (int i = 0; i < observed.grips.length(); ++i) {
        if (std::fabs(observed.grips[i].y-observed.topY) <= 1e-6) observed.topCandidates.append(observed.grips[i]);
        if (std::fabs(observed.grips[i].y-observed.bottomY) <= 1e-6) observed.bottomCandidates.append(observed.grips[i]);
    }
    return observed;
}

std::string axisTemplateDeleteTrace(const AxisNativeTraceData& t) {
    std::ostringstream out;
    out << "{\"editorAdded\":" << (t.editorAdded ? "true" : "false") << ",\"editorRemoved\":" << (t.editorRemoved ? "true" : "false")
        << ",\"inputAddStatus\":" << (t.inputAddReturned ? std::to_string(t.inputAddStatus) : "null")
        << ",\"inputRemoveStatus\":" << (t.inputRemoveReturned ? std::to_string(t.inputRemoveStatus) : "null")
        << ",\"traceDropped\":" << (t.traceDropped ? "true" : "false") << ",\"retained\":" << (t.retained ? "true" : "false")
        << ",\"moduleLockAttempted\":" << (t.moduleLockAttempted ? "true" : "false") << ",\"moduleLockSucceeded\":" << (t.moduleLockSucceeded ? "true" : "false") << '}';
    return out.str();
}

std::string axisTemplateDeleteCase(const Paths& p) {
    const auto control = fullPath(p.output + L"\\control.dwg"), output = fullPath(p.output + L"\\axis-template-delete-generated.dwg");
    std::string error{"null"}, cloneStatus{"null"}, sourceRead{"null"}, sourceClose{"null"}, commandStatus{"null"}, zoomStatus{"null"};
    std::string saveStatus{"null"}, firstReaderStatus{"null"}, firstReaderClose{"null"}, reopenStatus{"null"}, closeStatus{"null"};
    std::string originalBefore{"[]"}, originalAfter{"[]"}, originalSaved{"[]"}, addedBefore{"[]"}, addedAfter{"[]"}, addedSaved{"[]"};
    std::string sourceTemplate{"null"}, sourceAfterClone{"null"}, resources{"[]"}, cloneMapping{"null"};
    AxisTemplateDeleteObservation before, after, saved;
    auto trace = std::make_shared<AxisNativeTraceData>();
    AcGePoint3d center; AcGePoint3dArray windowWorld, windowUser; AcDbIntArray transforms;
    double targetX = std::numeric_limits<double>::quiet_NaN(), observedRadius = std::numeric_limits<double>::quiet_NaN();
    int initialCount = -1, cloneCount = -1, finalCount = -1, firstCount = -1, reopenedCount = -1, transactionsBefore = -1, transactionsAfter = -1;
    bool visibleRemoved{}, storedRemoved{}, readerDestroyed{}, commandAttempted{};
    std::wstring primaryHandle; std::vector<std::wstring> originalHandles;
    try {
        if (!below(control, p.output) || !below(output, p.output)) throw std::runtime_error("axis-template-delete output outside isolated run");
        requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("axis-template-delete requires document command context");
        auto* document = acDocManager->curDocument(); auto* database = document ? document->database() : nullptr;
        const auto guard = [&] {
            if (!document || !database || acDocManager->isApplicationContext() || acDocManager->curDocument() != document
                || acDocManager->mdiActiveDocument() != document || document->database() != database
                || acdbHostApplicationServices()->workingDatabase() != database || !document->fileName()
                || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
                throw std::runtime_error("axis-template-delete current/MDI/working control guard failed");
            const ACHAR* filename{}; check(database->getFilename(filename), "axis-template-delete current database filename");
            if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("axis-template-delete current database filename differs from owned control");
            if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
                throw std::runtime_error("axis-template-delete requires zero active transactions");
        };
        guard(); const auto originals = modelIds(*database); initialCount = originals.length();
        if (initialCount != 213) throw std::runtime_error("axis-template-delete must start with the complete scheme213 control");
        originalBefore = familyCompactArray(originals);
        for (int i = 0; i < originals.length(); ++i) originalHandles.push_back(handleOf(originals[i]));
        AcDbObjectId primary;
        {
            auto reference = std::make_unique<AcDbDatabase>(false, true);
            const auto read = reference->readDwgFile(p.reference.c_str(), AcDbDatabase::kForReadAndAllShare, false);
            sourceRead = std::to_string(read); check(read, "read frozen axis-template-delete reference");
            const auto closed = reference->closeInput(true); sourceClose = std::to_string(closed); check(closed, "close frozen axis-template-delete reference input");
            AcDbObjectId source; check(reference->getAcDbObjectId(source, false, AcDbHandle(L"199F")), "find fixed native199F template");
            {
                WorkingDatabaseScope sourceWorking(*reference); auto entity = open<AcDbEntity>(source);
                if (entity->isAProxy() || _wcsicmp(entity->isA()->name(), L"TDbAxisLabelSet")) throw std::runtime_error("199F is not a native axis-label collection");
                entity.reset(); sourceTemplate = familyCompactEntity(source);
            }
            AcDbObjectIdArray one; one.append(source); AcDbIdMapping mapping;
            check(mapping.setDestDb(database), "set axis-template-delete clone destination");
            const auto cloned = reference->wblockCloneObjects(one, modelId(*database), mapping, AcDb::kDrcMangleName, false);
            cloneStatus = std::to_string(cloned); check(cloned, "clone exactly one native199F primary");
            AcDbIdPair pair; pair.setKey(source);
            if (!mapping.compute(pair) || !pair.isCloned() || pair.value().isNull() || pair.value().database() != database)
                throw std::runtime_error("axis-template-delete actual primary mapping unavailable");
            primary = pair.value(); primaryHandle = handleOf(primary);
            for (int i = 0; i < originals.length(); ++i) if (primary == originals[i]) throw std::runtime_error("axis-template-delete clone reused an original entity");
            cloneMapping = "{\"sourceHandle\":\"199F\",\"targetHandle\":" + quote(primaryHandle) + ",\"isCloned\":true,\"type\":\"TDbAxisLabelSet\"}";
            resources = symbolResourceMapping(mapping);
            { WorkingDatabaseScope sourceWorking(*reference); sourceAfterClone = familyCompactEntity(source); }
        } // The source side database and every source object are closed before commands.
        guard(); cloneCount = modelCount(*database); addedBefore = familyCompactArray(familyAdditions(*database, originals));
        if (cloneCount != 214) throw std::runtime_error("axis-template-delete expected one added collection without unexpected model entities");
        before = axisTemplateDeleteObserve(primary);
        if (!before.available() || before.unionXs.size() != 9 || before.grips.length() != 41 || before.stretch.length() != 36)
            throw std::runtime_error("axis-template-delete public template point baseline differs from the bound fixture");
        targetX = before.unionXs[1]; bool found{};
        for (int i = 0; i < before.grips.length(); ++i) {
            const auto& candidate = before.grips[i];
            if (std::fabs(candidate.x-targetX) <= 1e-6 && (!found || candidate.y > center.y)) { center = candidate; found = true; }
        }
        if (!found || std::fabs(center.y-before.topY) > 1e-6) throw std::runtime_error("second actual stretch-X station has no visible top-circle candidate");
        {
            auto entity = open<AcDbEntity>(primary); AcDbExtents bounds; check(entity->getGeomExtents(bounds), "axis-template-delete actual bounds");
            if (_wcsicmp(entity->isA()->name(), L"TDbAxisLabelSet")) throw std::runtime_error("axis-template-delete clone has an unexpected native type");
            observedRadius = bounds.maxPoint().y-center.y;
        }
        if (std::fabs(observedRadius-400) > 1e-6 || !axisTemplateDeleteHasPoint(before.grips, AcGePoint3d(center.x, center.y-observedRadius, center.z)))
            throw std::runtime_error("axis-template-delete400 circle pattern not supported by actual bounds/grips");
        for (const auto x : before.unionXs)
            if (std::fabs(x-targetX) > 1e-6 && std::fabs(x-targetX) <= 900) throw std::runtime_error("axis-template-delete450 window overlaps another public X station");
        windowWorld.append(AcGePoint3d(center.x-450, center.y-450, center.z));
        windowWorld.append(AcGePoint3d(center.x+450, center.y+450, center.z));
        guard(); const int zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_E", RTNONE); zoomStatus = std::to_string(zoomed);
        if (zoomed != RTNORM) throw std::runtime_error("axis-template-delete diagnostic zoom failed");
        guard();
        for (int i = 0; i < windowWorld.length(); ++i) {
            ads_point world{windowWorld[i].x, windowWorld[i].y, windowWorld[i].z}, user{}; resbuf from{}, to{};
            from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
            const int status = acedTrans(world, &from, &to, 0, user); transforms.append(status);
            if (status != RTNORM) throw std::runtime_error("axis-template-delete WCS-to-UCS window conversion failed");
            windowUser.append(AcGePoint3d(user[0], user[1], user[2]));
        }
        guard(); transactionsBefore = database->transactionManager()->numActiveTransactions();
        try {
            ads_point first{windowUser[0].x, windowUser[0].y, windowUser[0].z}, second{windowUser[1].x, windowUser[1].y, windowUser[1].z};
            AxisNativeTraceOwner owner(document, trace);
            if (!trace->editorAdded || !trace->inputAddReturned || trace->inputAddStatus != Acad::eOk || trace->traceDropped)
                throw std::runtime_error("axis-template-delete observers unavailable; documented command not attempted");
            // All native objects are closed; no transaction spans this complete
            // documented command. Empty ends repeated windows; N forbids reorder.
            commandAttempted = true;
            const int status = acedCommandS(RTSTR, L"TDelLabel", RT3DPOINT, first, RT3DPOINT, second, RTSTR, L"", RTSTR, L"N", RTNONE);
            commandStatus = std::to_string(status);
            if (status != RTNORM) throw std::runtime_error("TDelLabel did not return RTNORM; actual state retained");
        } catch (const std::exception& failure) { error = familyError(failure); }
          catch (...) { error = quote(L"unknown TDelLabel command exception"); }
        if (trace->retained && error == "null") error = quote(L"axis-template-delete callbacks retained by existing safe unload guard");
        guard(); transactionsAfter = database->transactionManager()->numActiveTransactions();
        after = axisTemplateDeleteObserve(primary); originalAfter = familyCompactArray(originals);
        finalCount = modelCount(*database); addedAfter = familyCompactArray(familyAdditions(*database, originals));
        if (after.available()) {
            visibleRemoved = axisTemplateDeleteHasPoint(before.grips, center) && !axisTemplateDeleteHasPoint(after.grips, center)
                && !axisTemplateDeleteSamePoints(before.grips, after.grips);
            std::vector<double> expected;
            for (const auto x : before.unionXs) if (std::fabs(x-targetX) > 1e-6) expected.push_back(x);
            storedRemoved = before.unionXs.size() == 9 && after.unionXs.size() == 8 && expected.size() == after.unionXs.size();
            if (storedRemoved) for (std::size_t i = 0; i < expected.size(); ++i)
                if (std::fabs(expected[i]-after.unionXs[i]) > 1e-6) storedRemoved = false;
            for (int i = 0; i < after.stretch.length(); ++i) if (std::fabs(after.stretch[i].x-targetX) <= 1e-6) storedRemoved = false;
        }
        // Partial is an observed semantic outcome, not an operational exception.
        // Never repair a no-op, hide-only result or unexpected deletion here.
        requireNewFile(output); const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT);
        saveStatus = std::to_string(stored); check(stored, "save isolated axis-template-delete result");
        {
            auto reader = std::make_unique<AcDbDatabase>(false, true);
            const auto loaded = reader->readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
            firstReaderStatus = std::to_string(loaded); check(loaded, "first independent axis-template-delete read");
            const auto closed = reader->closeInput(true); firstReaderClose = std::to_string(closed); check(closed, "close first axis-template-delete input");
            { WorkingDatabaseScope working(*reader); firstCount = modelCount(*reader); }
            reader.reset(); readerDestroyed = true;
        }
        auto reader = std::make_unique<AcDbDatabase>(false, true);
        const auto loaded = reader->readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        reopenStatus = std::to_string(loaded); check(loaded, "final new-reader axis-template-delete reopen");
        const auto closed = reader->closeInput(true); closeStatus = std::to_string(closed); check(closed, "close final axis-template-delete input");
        {
            WorkingDatabaseScope working(*reader); reopenedCount = modelCount(*reader); AcDbObjectIdArray originalCopies;
            for (const auto& handle : originalHandles) {
                AcDbObjectId id; check(reader->getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find persisted protected original"); originalCopies.append(id);
            }
            originalSaved = familyCompactArray(originalCopies); addedSaved = familyCompactArray(familyAdditions(*reader, originalCopies));
            AcDbObjectId savedId; const auto lookup = reader->getAcDbObjectId(savedId, false, AcDbHandle(primaryHandle.c_str()));
            if (lookup == Acad::eOk) saved = axisTemplateDeleteObserve(savedId); else saved.readStatus = lookup;
        }
    } catch (const std::exception& failure) { if (error == "null") error = familyError(failure); }
      catch (...) { if (error == "null") error = quote(L"unknown axis-template-delete native exception"); }
    std::ostringstream out;
    out << "{\"contract\":\"stage1.axis-template-delete-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandS\",\"commandName\":\"TDelLabel\",\"entrypoint\":\"S1AXISTEMPLATEDELETE\""
        << ",\"operation\":\"delete-one-visible-top-interior-label-no-renumber\",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
        << ",\"schemeInputPath\":" << quote(p.scheme) << ",\"referenceInputPath\":" << quote(p.reference) << ",\"primaryHandle\":" << quote(primaryHandle)
        << ",\"cloneMapping\":" << cloneMapping << ",\"resourceMapping\":" << resources << ",\"sourceTemplate\":" << sourceTemplate << ",\"sourceTemplateAfterClone\":" << sourceAfterClone
        << ",\"initialModelCount\":" << initialCount << ",\"postCloneModelCount\":" << cloneCount << ",\"finalModelCount\":" << finalCount << ",\"firstReaderModelCount\":" << firstCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"originalBefore\":" << originalBefore << ",\"originalAfter\":" << originalAfter << ",\"originalSaved\":" << originalSaved
        << ",\"before\":" << before.entity << ",\"after\":" << after.entity << ",\"saved\":" << saved.entity << ",\"beforeReadStatus\":" << before.readStatus << ",\"afterReadStatus\":" << after.readStatus << ",\"savedReadStatus\":" << saved.readStatus
        << ",\"observedBefore\":" << before.json() << ",\"observedAfter\":" << after.json() << ",\"observedSaved\":" << saved.json()
        << ",\"addedAfterClone\":" << addedBefore << ",\"addedAfterCommand\":" << addedAfter << ",\"savedAdded\":" << addedSaved
        << ",\"selectionWindow\":{\"targetStationX\":" << number(targetX) << ",\"centerWCS\":" << point(center) << ",\"observedRadius\":" << number(observedRadius)
        << ",\"halfSize\":450,\"worldPoints\":" << points(windowWorld) << ",\"userPoints\":" << points(windowUser) << ",\"transformStatuses\":" << integers(transforms) << '}'
        << ",\"sourceReadStatus\":" << sourceRead << ",\"sourceCloseInputStatus\":" << sourceClose << ",\"cloneStatus\":" << cloneStatus << ",\"zoomStatus\":" << zoomStatus << ",\"commandStatus\":" << commandStatus
        << ",\"saveStatus\":" << saveStatus << ",\"firstReaderStatus\":" << firstReaderStatus << ",\"firstReaderCloseInputStatus\":" << firstReaderClose << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus
        << ",\"readerDestroyedBeforeFinalRead\":" << (readerDestroyed ? "true" : "false") << ",\"hostDatabaseDestroyed\":false,\"activeTransactionsBeforeCommand\":" << transactionsBefore << ",\"activeTransactionsAfterCommand\":" << transactionsAfter
        << ",\"commandAttempted\":" << (commandAttempted ? "true" : "false") << ",\"requestedReorderAnswer\":\"N\",\"suppliedReorderAnswer\":" << (commandAttempted ? "\"N\"" : "null")
        << ",\"reorderAnswerObservation\":\"N supplied only when commandAttempted; shared input trace may omit normal-status keyword text, so no unobserved readback is synthesized\""
        << ",\"inputTraceLimitations\":\"shared owner does not record GetCorner; actual SSGet input geometry is retained as an alternative window observation\""
        << ",\"additionsScope\":\"all new Model_Space entities; cloned symbol resources mapped separately\",\"batchDeletion\":false,\"otherAxisMovementAttempted\":false,\"commandTrace\":" << axisTraceArray(trace->commands) << ",\"inputTrace\":" << axisTraceArray(trace->inputs) << ",\"trace\":" << axisTemplateDeleteTrace(*trace)
        << ",\"result\":{\"visibleMarkerRemoved\":" << (visibleRemoved ? "true" : "false") << ",\"storedStationXsReduced\":" << (storedRemoved ? "true" : "false")
        << ",\"status\":" << quote(visibleRemoved && storedRemoved ? L"Pass" : visibleRemoved ? L"Partial" : L"Fail") << '}'
        << ",\"scope\":\"one template deletion; unionXs derives public stretch points across both sides, not a private internal axisCount; no2/3-axis generation, renumbering, other-axis movement or rendered-label approval\",\"error\":" << error << '}';
    return out.str();
}

void nativeAxisTemplateDeleteProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\axis-template-delete-native.json"); requireNewFile(report);
        writeNewReport(report, axisTemplateDeleteCase(p)); acutPrintf(L"\nSTAGE1_AXIS_TEMPLATE_DELETE_COMPLETED; VISIBLE_AND_STORED_STATIONS_REQUIRE_SEPARATE_VERIFICATION\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_AXIS_TEMPLATE_DELETE_FAILED: %hs\n", failure.what()); }
      catch (...) { acutPrintf(L"\nSTAGE1_AXIS_TEMPLATE_DELETE_FAILED: unknown C++ exception\n"); }
}
