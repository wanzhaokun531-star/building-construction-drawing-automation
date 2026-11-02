// Include inside NativeAccessProbe.cpp's anonymous namespace AFTER its trace
// helpers (AxisNativeTraceData/AxisNativeTraceOwner). No extra SDK headers/libs.
// Stage-1 documented command fixture only; never edit the frozen input DWGs.

std::string dimensionBridgeError(const std::exception& failure) {
    const std::string message(failure.what()); return quote(std::wstring(message.begin(), message.end()));
}

struct DimensionBridgeMeasurements {
    Acad::ErrorStatus queryStatus{Acad::eNotApplicable};
    AcGePoint3dArray points;
    std::vector<double> segments;
    std::string json(double baselineY, double drawingZ) const {
        std::ostringstream out;
        out << "{\"queryStatus\":" << queryStatus << ",\"baselineY\":" << number(baselineY) << ",\"drawingZ\":" << number(drawingZ)
            << ",\"points\":" << ::points(points) << ",\"segments\":[";
        for (size_t i = 0; i < segments.size(); ++i) { if (i) out << ','; out << number(segments[i]); }
        out << "],\"method\":\"actual native grip points on the observed wall baseline, sorted and deduplicated by coordinate\"}"; return out.str();
    }
};

DimensionBridgeMeasurements dimensionBridgeMeasurements(AcDbObjectId id, double baselineY, double drawingZ) {
    DimensionBridgeMeasurements result; AcGePoint3dArray raw; AcDbIntArray snaps, geometry;
    { auto entity = open<AcDbEntity>(id); result.queryStatus = entity->getGripPoints(raw, snaps, geometry); }
    if (result.queryStatus != Acad::eOk) return result;
    std::vector<AcGePoint3d> candidates;
    for (int i = 0; i < raw.length(); ++i)
        if (std::fabs(raw[i].y-baselineY) <= 1e-6 && std::fabs(raw[i].z-drawingZ) <= 1e-6) candidates.push_back(raw[i]);
    std::sort(candidates.begin(), candidates.end(), [](const AcGePoint3d& a, const AcGePoint3d& b) { return a.x < b.x; });
    for (const auto& candidate : candidates)
        if (!result.points.length() || candidate.distanceTo(result.points[result.points.length()-1]) > 1e-6) result.points.append(candidate);
    for (int i = 1; i < result.points.length(); ++i) result.segments.push_back(result.points[i].x-result.points[i-1].x);
    return result;
}

std::string dimensionBridgeTraceJson(const AxisNativeTraceData& trace) {
    std::ostringstream out;
    out << "{\"editorAvailable\":" << (trace.editorAvailable ? "true" : "false") << ",\"managerAvailable\":" << (trace.managerAvailable ? "true" : "false")
        << ",\"editorAdded\":" << (trace.editorAdded ? "true" : "false") << ",\"editorRemoved\":" << (trace.editorRemoved ? "true" : "false")
        << ",\"inputAddAttempted\":" << (trace.inputAddAttempted ? "true" : "false") << ",\"inputAddStatus\":" << (trace.inputAddReturned ? std::to_string(trace.inputAddStatus) : "null")
        << ",\"inputRemoveAttempted\":" << (trace.inputRemoveAttempted ? "true" : "false") << ",\"inputRemoveStatus\":" << (trace.inputRemoveReturned ? std::to_string(trace.inputRemoveStatus) : "null")
        << ",\"traceDropped\":" << (trace.traceDropped ? "true" : "false") << ",\"retained\":" << (trace.retained ? "true" : "false")
        << ",\"moduleLockAttempted\":" << (trace.moduleLockAttempted ? "true" : "false") << ",\"moduleLockSucceeded\":" << (trace.moduleLockSucceeded ? "true" : "false") << '}';
    return out.str();
}

std::string dimensionCombineCase(const Paths& p) {
    const auto control = fullPath(p.output + L"\\control.dwg"), output = fullPath(p.output + L"\\dimension-combine-generated.dwg");
    std::string error{"null"}, commandStatus{"null"}, saveStatus{"null"}, reopenStatus{"null"}, closeStatus{"null"};
    std::string before{"null"}, after{"null"}, saved{"null"}, originals{"[]"}, originalAfter{"[]"}, originalSaved{"[]"};
    std::string wallControl{"null"}, openingControl{"null"}, beforeMeasurements{"null"}, afterMeasurements{"null"}, savedMeasurements{"null"};
    AcGePoint3dArray windowWorld, windowUser; AcDbIntArray transformStatuses;
    auto trace = std::make_shared<AxisNativeTraceData>(); std::vector<SchemeMapping> mappings;
    int initialCount = -1, finalCount = -1, sourceCount = -1, reopenedCount = -1; bool actualReached{};
    try {
        if (!below(control, p.output) || !below(output, p.output)) throw std::runtime_error("dimension-combine output guard failed"); requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("dimension-combine requires document command context");
        auto* document = acDocManager->curDocument(); auto* database = document ? document->database() : nullptr;
        const auto guard = [&] {
            if (!document || !database || acDocManager->isApplicationContext() || acDocManager->curDocument() != document || acDocManager->mdiActiveDocument() != document
                || document->database() != database || acdbHostApplicationServices()->workingDatabase() != database || !document->fileName()
                || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str())) throw std::runtime_error("dimension-combine current/MDI/working control guard failed");
            const ACHAR* filename{}; check(database->getFilename(filename), "dimension-combine native database filename");
            if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("dimension-combine native filename guard failed");
            if (database->transactionManager() && database->transactionManager()->numActiveTransactions() != 0)
                throw std::runtime_error("dimension-combine requires no active transaction across its document command");
        };
        guard(); const auto allIds = modelIds(*database); initialCount = allIds.length();
        if (initialCount != 214) throw std::runtime_error("dimension-combine requires the validated scheme213 plus one subdivided dimension fixture");
        AcDbObjectId dimensionId; check(database->getAcDbObjectId(dimensionId, false, AcDbHandle(L"D0D3")), "find dimension-combine target");
        AcDbObjectIdArray protectedIds;
        for (int i = 0; i < allIds.length(); ++i) {
            if (allIds[i] == dimensionId) continue;
            protectedIds.append(allIds[i]); auto entity = open<AcDbEntity>(allIds[i]); const auto handle = handleOf(allIds[i]);
            mappings.push_back(SchemeMapping{handle, handle, entity->isA()->name()});
        }
        sourceCount = protectedIds.length(); if (sourceCount != 213) throw std::runtime_error("dimension-combine original scheme coverage incomplete");
        originals = scanEntityArray(protectedIds); before = scanEntity(dimensionId);
        AcGePoint3d wallStart, wallEnd, lineStart, lineEnd, openingLeft, openingRight;
        {
            AcDbObjectId wallId; check(database->getAcDbObjectId(wallId, false, AcDbHandle(L"CD83")), "find dimension-combine wall control");
            auto entity = open<AcDbEntity>(wallId); const auto* curve = AcDbCurve::cast(entity.get());
            if (!curve || _wcsicmp(entity->isA()->name(), L"TDbWall")) throw std::runtime_error("dimension-combine wall control is not a native wall curve");
            check(curve->getStartPoint(wallStart), "dimension-combine wall start"); check(curve->getEndPoint(wallEnd), "dimension-combine wall end");
            if (std::fabs(wallStart.y-wallEnd.y) > 1e-6 || std::fabs(wallStart.z-wallEnd.z) > 1e-6 || std::fabs(std::fabs(wallStart.x-wallEnd.x)-3600) > 1e-6)
                throw std::runtime_error("dimension-combine wall control is not the expected horizontal 3600 fixture");
            wallControl = "{\"sourceHandle\":\"CD83\",\"type\":\"TDbWall\",\"start\":" + point(wallStart) + ",\"end\":" + point(wallEnd) + '}';
        }
        {
            AcDbObjectId openingId; check(database->getAcDbObjectId(openingId, false, AcDbHandle(L"CD84")), "find dimension-combine opening control");
            auto entity = open<AcDbEntity>(openingId);
            if (_wcsicmp(entity->isA()->name(), L"TDbOpening")) throw std::runtime_error("dimension-combine opening control native type mismatch");
            AcGePoint3dArray grips; AcDbIntArray snaps, geometry; const auto queried = entity->getGripPoints(grips, snaps, geometry); check(queried, "read dimension-combine opening side grips");
            int left = -1, right = -1; const double xmin = (std::min)(wallStart.x, wallEnd.x), xmax = (std::max)(wallStart.x, wallEnd.x);
            for (int i = 0; i < grips.length(); ++i) {
                if (std::fabs(grips[i].y-wallStart.y) > 1e-6 || grips[i].x < xmin-1e-6 || grips[i].x > xmax+1e-6) continue;
                if (left < 0 || grips[i].x < grips[left].x) left = i;
                if (right < 0 || grips[i].x > grips[right].x) right = i;
            }
            if (left < 0 || right < 0 || left == right || std::fabs(grips[right].x-grips[left].x-2400) > 1e-6)
                throw std::runtime_error("dimension-combine actual 2400 opening side grips were not found");
            openingLeft = grips[left]; openingRight = grips[right];
            openingControl = "{\"sourceHandle\":\"CD84\",\"type\":\"TDbOpening\",\"gripQueryStatus\":" + std::to_string(queried)
                + ",\"sideIndices\":[" + std::to_string(left) + ',' + std::to_string(right) + "],\"sidePoints\":[" + point(openingLeft) + ',' + point(openingRight) + "]}";
        }
        {
            auto entity = open<AcDbEntity>(dimensionId); const auto* curve = AcDbCurve::cast(entity.get());
            if (!curve || _wcsicmp(entity->isA()->name(), L"TDbDimension2")) throw std::runtime_error("dimension-combine target native dimension curve unavailable");
            check(curve->getStartPoint(lineStart), "dimension-combine dimension-line start"); check(curve->getEndPoint(lineEnd), "dimension-combine dimension-line end");
            if (std::fabs(lineStart.y-lineEnd.y) > 1e-6 || std::fabs(lineStart.z-wallStart.z) > 1e-6 || std::fabs(lineEnd.z-wallStart.z) > 1e-6)
                throw std::runtime_error("dimension-combine dimension line is not horizontal in the wall drawing plane");
        }
        const auto measuredBefore = dimensionBridgeMeasurements(dimensionId, wallStart.y, wallStart.z); beforeMeasurements = measuredBefore.json(wallStart.y, wallStart.z);
        if (measuredBefore.queryStatus != Acad::eOk || measuredBefore.points.length() != 4 || measuredBefore.segments.size() != 3
            || std::fabs(measuredBefore.segments[0]-600) > 1e-6 || std::fabs(measuredBefore.segments[1]-2400) > 1e-6 || std::fabs(measuredBefore.segments[2]-600) > 1e-6)
            throw std::runtime_error("dimension-combine baseline is not the observed native 600/2400/600 subdivision");
        windowWorld.append(AcGePoint3d(openingLeft.x-20, lineStart.y-100, wallStart.z)); windowWorld.append(AcGePoint3d(openingLeft.x+20, lineStart.y+100, wallStart.z));
        for (int i = 0; i < windowWorld.length(); ++i) {
            ads_point world{windowWorld[i].x, windowWorld[i].y, windowWorld[i].z}, user{}; resbuf from{}, to{};
            from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
            const int status = acedTrans(world, &from, &to, 0, user); transformStatuses.append(status);
            if (status != RTNORM) throw std::runtime_error("dimension-combine WCS-to-UCS window conversion failed");
            windowUser.append(AcGePoint3d(user[0], user[1], user[2]));
        }
        guard();
        try {
            ads_point first{windowUser[0].x, windowUser[0].y, windowUser[0].z}, second{windowUser[1].x, windowUser[1].y, windowUser[1].z};
            {
                AxisNativeTraceOwner owner(document, trace);
                // No database object or transaction is held open during this
                // complete documented command: two box corners, Enter to exit.
                const int status = acedCommandS(RTSTR, L"TConbineDim", RT3DPOINT, first, RT3DPOINT, second, RTSTR, L"", RTNONE);
                commandStatus = std::to_string(status);
                if (status != RTNORM) throw std::runtime_error("TConbineDim did not return RTNORM; actual result retained");
            }
        } catch (const std::exception& failure) { error = dimensionBridgeError(failure); }
          catch (...) { error = quote(L"unknown dimension-combine command exception"); }
        if (trace->retained && error == "null") error = quote(L"dimension-combine native trace removal failed; existing module unload guard retains callbacks");
        guard(); after = scanEntity(dimensionId); originalAfter = scanEntityArray(protectedIds); finalCount = modelCount(*database);
        const auto measuredAfter = dimensionBridgeMeasurements(dimensionId, wallStart.y, wallStart.z); afterMeasurements = measuredAfter.json(wallStart.y, wallStart.z);
        const double leftX = (std::min)(wallStart.x, wallEnd.x), rightX = (std::max)(wallStart.x, wallEnd.x);
        actualReached = measuredAfter.queryStatus == Acad::eOk && measuredAfter.points.length() == 3
            && std::fabs(measuredAfter.points[0].x-leftX) <= 1e-6 && std::fabs(measuredAfter.points[1].x-openingRight.x) <= 1e-6
            && std::fabs(measuredAfter.points[2].x-rightX) <= 1e-6;
        if (!actualReached && error == "null") error = quote(L"TConbineDim returned but the actual native first interior measurement was not removed");
        requireNewFile(output); const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save dimension-combine output");
        AcDbDatabase reopened(false, true); const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        reopenStatus = std::to_string(loaded); check(loaded, "reopen dimension-combine output"); const auto closed = reopened.closeInput(true); closeStatus = std::to_string(closed); check(closed, "close reopened dimension-combine input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray originalCopies;
            for (const auto& mapping : mappings) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(mapping.targetHandle.c_str())), "find saved dimension-combine original"); originalCopies.append(id); }
            originalSaved = scanEntityArray(originalCopies); AcDbObjectId savedId; check(reopened.getAcDbObjectId(savedId, false, AcDbHandle(L"D0D3")), "find saved dimension-combine target");
            saved = scanEntity(savedId); const auto measuredSaved = dimensionBridgeMeasurements(savedId, wallStart.y, wallStart.z); savedMeasurements = measuredSaved.json(wallStart.y, wallStart.z);
        }
    } catch (const std::exception& failure) { if (error == "null") error = dimensionBridgeError(failure); }
      catch (...) { if (error == "null") error = quote(L"unknown dimension-combine native exception"); }
    std::ostringstream out;
    out << "{\"contract\":\"stage1.dimension-bridge-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandS\",\"commandName\":\"TConbineDim\""
        << ",\"operation\":\"combine-first-interior-measurement\",\"dimensionHandle\":\"D0D3\",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
        << ",\"initialModelCount\":" << initialCount << ",\"finalModelCount\":" << finalCount << ",\"sourceCount\":" << sourceCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"sourceMap\":" << schemeMapJson(mappings) << ",\"sourceGeometry\":" << originals << ",\"afterOriginalGeometry\":" << originalAfter << ",\"savedOriginalGeometry\":" << originalSaved
        << ",\"before\":" << before << ",\"after\":" << after << ",\"saved\":" << saved << ",\"measurementBefore\":" << beforeMeasurements
        << ",\"measurementAfter\":" << afterMeasurements << ",\"measurementSaved\":" << savedMeasurements << ",\"actualReached\":" << (actualReached ? "true" : "false")
        << ",\"wallControl\":" << wallControl << ",\"openingControl\":" << openingControl << ",\"expectedBeforeSegments\":[600,2400,600],\"expectedAfterSegments\":[3000,600]"
        << ",\"selectionWindow\":{\"worldPoints\":" << points(windowWorld) << ",\"userPoints\":" << points(windowUser) << ",\"transformStatuses\":" << integers(transformStatuses) << '}'
        << ",\"commandStatus\":" << commandStatus << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus
        << ",\"commandTrace\":" << axisTraceArray(trace->commands) << ",\"inputTrace\":" << axisTraceArray(trace->inputs) << ",\"trace\":" << dimensionBridgeTraceJson(*trace)
        << ",\"visualVerificationStatus\":\"pending independent native PDF inspection; RTNORM and expected segments are not rendered-label proof\",\"error\":" << error << '}'; return out.str();
}

void nativeDimensionCombineProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\dimension-bridge-native.json"); requireNewFile(report);
        writeNewReport(report, dimensionCombineCase(p)); acutPrintf(L"\nSTAGE1_DIMENSION_COMBINE_COMPLETED; GEOMETRY_AND_LABEL_VERIFICATION_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_DIMENSION_COMBINE_FAILED: %hs\n", failure.what()); }
      catch (...) { acutPrintf(L"\nSTAGE1_DIMENSION_COMBINE_FAILED: unknown C++ exception\n"); }
}
