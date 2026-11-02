// Include after NativeAxisTemplateDeleteProbe.inc, Family and native trace
// helpers, inside the parent anonymous namespace. No additional SDK headers.
// Synthetic stage1 capability: public template stations and real scheme wall X
// controls only. Template Y range/numbering are retained and not business output.

bool axisPruneSameXs(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (std::fabs(a[i]-b[i]) > 1e-6) return false;
    return true;
}

bool axisPruneHasX(const std::vector<double>& values, double x) {
    for (const auto value : values) if (std::fabs(value-x) <= 1e-6) return true;
    return false;
}

bool axisPrunePointBag(const AcGePoint3dArray& a, const AcGePoint3dArray& b) {
    if (a.length() != b.length()) return false;
    std::vector<bool> used(b.length(), false);
    for (int i = 0; i < a.length(); ++i) {
        bool found{};
        for (int j = 0; j < b.length(); ++j) if (!used[j] && a[i].distanceTo(b[j]) <= 1e-6) { used[j] = true; found = true; break; }
        if (!found) return false;
    }
    return true;
}

AcGePoint3dArray axisPruneWithoutX(const AcGePoint3dArray& points, double x) {
    AcGePoint3dArray result;
    for (int i = 0; i < points.length(); ++i) if (std::fabs(points[i].x-x) > 1e-6) result.append(points[i]);
    return result;
}

bool axisPruneBothSides(const AxisTemplateDeleteObservation& observed, const std::vector<double>& wanted) {
    return observed.available() && axisPruneSameXs(observed.unionXs, wanted)
        && axisPruneSameXs(axisTemplateDeleteXs(observed.topCandidates), wanted)
        && axisPruneSameXs(axisTemplateDeleteXs(observed.bottomCandidates), wanted);
}

struct AxisPruneWallControl {
    AcDbObjectId id; std::wstring handle; double x{}; std::string entity;
    std::string json() const { return "{\"handle\":" + quote(handle) + ",\"centerX\":" + number(x) + ",\"entity\":" + entity + '}'; }
};

bool axisPruneReadWall(AcDbObjectId id, AxisPruneWallControl& control) {
    auto entity = open<AcDbEntity>(id);
    if (entity->isAProxy() || _wcsicmp(entity->isA()->name(), L"TDbWall")) return false;
    auto* curve = AcDbCurve::cast(entity.get()); if (!curve) return false;
    AcGePoint3d a, b; AcDbExtents bounds;
    if (curve->getStartPoint(a) != Acad::eOk || curve->getEndPoint(b) != Acad::eOk || entity->getGeomExtents(bounds) != Acad::eOk
        || std::fabs(a.x-b.x) > 1e-6 || std::fabs(a.y-b.y) <= 1e-6 || std::fabs(bounds.maxPoint().x-bounds.minPoint().x-200) > 1e-6) return false;
    control.id = id; control.handle = handleOf(id); control.x = (bounds.minPoint().x+bounds.maxPoint().x)/2;
    entity.reset(); control.entity = familyCompactEntity(id); return true;
}

template<class Guard>
std::string axisPruneDeleteAttempt(AcApDocument* document, AcDbDatabase& database, AcDbObjectId primary,
    const AxisTemplateDeleteObservation& before, double x, bool top, Guard&& guard, AxisTemplateDeleteObservation& after, std::string& error) {
    std::string commandStatus{"null"}; bool attempted{}; int transactionsBefore = -1, transactionsAfter = -1;
    AcGePoint3d center; AcGePoint3dArray worldPoints, userPoints; AcDbIntArray transforms;
    auto trace = std::make_shared<AxisNativeTraceData>();
    try {
        const auto& candidates = top ? before.topCandidates : before.bottomCandidates; bool found{};
        for (int i = 0; i < candidates.length(); ++i) if (std::fabs(candidates[i].x-x) <= 1e-6) { center = candidates[i]; found = true; break; }
        if (!found) throw std::runtime_error("prune target has no visible circle on the selected side");
        for (int i = 0; i < candidates.length(); ++i)
            if (std::fabs(candidates[i].x-x) > 1e-6 && std::fabs(candidates[i].x-x) <= 900) throw std::runtime_error("prune450 window overlaps another visible circle on the same side");
        // Close X stations40 apart are safe only when their actual visible
        // circles are on opposite sides; never infer visibility from unionXs.
        worldPoints.append(AcGePoint3d(center.x-450, center.y-450, center.z)); worldPoints.append(AcGePoint3d(center.x+450, center.y+450, center.z));
        for (int i = 0; i < worldPoints.length(); ++i) {
            ads_point world{worldPoints[i].x, worldPoints[i].y, worldPoints[i].z}, user{}; resbuf from{}, to{};
            from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
            const int status = acedTrans(world, &from, &to, 0, user); transforms.append(status);
            if (status != RTNORM) throw std::runtime_error("prune window WCS-to-UCS conversion failed");
            userPoints.append(AcGePoint3d(user[0], user[1], user[2]));
        }
        guard(); transactionsBefore = database.transactionManager()->numActiveTransactions();
        try {
            ads_point first{userPoints[0].x, userPoints[0].y, userPoints[0].z}, second{userPoints[1].x, userPoints[1].y, userPoints[1].z};
            AxisNativeTraceOwner owner(document, trace);
            if (!trace->editorAdded || !trace->inputAddReturned || trace->inputAddStatus != Acad::eOk || trace->traceDropped)
                throw std::runtime_error("prune observers unavailable; no command attempted");
            attempted = true;
            const int status = acedCommandS(RTSTR, L"TDelLabel", RT3DPOINT, first, RT3DPOINT, second, RTSTR, L"", RTSTR, L"N", RTNONE);
            commandStatus = std::to_string(status);
            if (status != RTNORM) throw std::runtime_error("TDelLabel prune attempt did not return RTNORM");
        } catch (const std::exception& failure) { error = familyError(failure); }
          catch (...) { error = quote(L"unknown prune command exception"); }
        if (trace->retained && error == "null") error = quote(L"prune trace callbacks retained by existing safe unload guard");
        guard(); transactionsAfter = database.transactionManager()->numActiveTransactions(); after = axisTemplateDeleteObserve(primary);
    } catch (const std::exception& failure) { if (error == "null") error = familyError(failure); }
      catch (...) { if (error == "null") error = quote(L"unknown prune attempt exception"); }
    std::ostringstream out;
    out << "{\"side\":" << quote(top ? L"top" : L"bottom") << ",\"centerWCS\":" << point(center) << ",\"halfSize\":450,\"worldPoints\":" << points(worldPoints)
        << ",\"userPoints\":" << points(userPoints) << ",\"transformStatuses\":" << integers(transforms) << ",\"before\":" << before.entity << ",\"after\":" << after.entity
        << ",\"commandAttempted\":" << (attempted ? "true" : "false") << ",\"commandStatus\":" << commandStatus << ",\"suppliedReorderAnswer\":" << (attempted ? "\"N\"" : "null")
        << ",\"activeTransactionsBeforeCommand\":" << transactionsBefore << ",\"activeTransactionsAfterCommand\":" << transactionsAfter
        << ",\"commandTrace\":" << axisTraceArray(trace->commands) << ",\"inputTrace\":" << axisTraceArray(trace->inputs) << ",\"trace\":" << axisTemplateDeleteTrace(*trace) << ",\"error\":" << error << '}';
    return out.str();
}

std::string axisTemplatePruneCase(const Paths& p, int count) {
    const auto control = fullPath(p.output + L"\\control.dwg"), output = fullPath(p.output + L"\\axis-template-prune-" + std::to_wstring(count) + L"-generated.dwg");
    std::string error{"null"}, sourceRead{"null"}, sourceClose{"null"}, cloneStatus{"null"}, zoomStatus{"null"}, saveStatus{"null"};
    std::string firstRead{"null"}, firstClose{"null"}, reopenStatus{"null"}, closeStatus{"null"};
    std::string originalBefore{"[]"}, originalAfter{"[]"}, originalSaved{"[]"}, sourceTemplate{"null"}, sourceAfterClone{"null"}, cloneMapping{"null"}, resources{"[]"};
    std::string addedClone{"[]"}, addedPrune{"[]"}, added{"[]"}, savedAdded{"[]"}, translation{"null"};
    AxisTemplateDeleteObservation before, pruned, translated, after, saved;
    std::vector<std::string> deleteSteps, stretchMoves; std::vector<double> keep, targetXs; std::vector<AxisPruneWallControl> controls;
    std::vector<std::wstring> originalHandles; std::wstring primaryHandle;
    int initialCount = -1, cloneCount = -1, finalCount = -1, firstCount = -1, reopenedCount = -1; bool pruneCompleted{}, positioned{}, readerDestroyed{};
    try {
        if ((count != 2 && count != 3) || !below(control, p.output) || !below(output, p.output)) throw std::runtime_error("prune fixture count/output guard failed");
        requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("prune requires document command context");
        auto* document = acDocManager->curDocument(); auto* database = document ? document->database() : nullptr;
        const auto guard = [&] {
            if (!document || !database || acDocManager->isApplicationContext() || acDocManager->curDocument() != document || acDocManager->mdiActiveDocument() != document
                || document->database() != database || acdbHostApplicationServices()->workingDatabase() != database || !document->fileName()
                || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str())) throw std::runtime_error("prune current/MDI/working owned-control guard failed");
            const ACHAR* filename{}; check(database->getFilename(filename), "prune current database filename");
            if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str()) || !database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
                throw std::runtime_error("prune filename or active-transaction guard failed");
        };
        guard(); const auto originals = modelIds(*database); initialCount = originals.length();
        if (initialCount != 213) throw std::runtime_error("prune requires complete fresh scheme213 control");
        originalBefore = familyCompactArray(originals);
        for (int i = 0; i < originals.length(); ++i) originalHandles.push_back(handleOf(originals[i]));
        AxisPruneWallControl left, right;
        AcDbObjectId leftId, rightId;
        check(database->getAcDbObjectId(leftId, false, AcDbHandle(L"CDB4")), "find prune left scheme wall");
        check(database->getAcDbObjectId(rightId, false, AcDbHandle(L"CDB0")), "find prune right scheme wall");
        if (!axisPruneReadWall(leftId, left) || !axisPruneReadWall(rightId, right) || left.x >= right.x) throw std::runtime_error("prune outer scheme controls are not ordered vertical200 walls");
        controls.push_back(left);
        if (count == 3) {
            const double midpoint = (left.x+right.x)/2; AxisPruneWallControl selected; bool found{};
            for (int i = 0; i < originals.length(); ++i) {
                AxisPruneWallControl candidate;
                if (!axisPruneReadWall(originals[i], candidate) || candidate.x <= left.x+1e-6 || candidate.x >= right.x-1e-6) continue;
                const double distance = std::fabs(candidate.x-midpoint), best = std::fabs(selected.x-midpoint);
                if (!found || distance < best-1e-6 || (std::fabs(distance-best) <= 1e-6 && (candidate.x < selected.x-1e-6
                    || (std::fabs(candidate.x-selected.x) <= 1e-6 && candidate.handle < selected.handle)))) { selected = candidate; found = true; }
            }
            if (!found) throw std::runtime_error("prune no actual interior vertical200 scheme wall available");
            controls.push_back(selected);
        }
        controls.push_back(right); for (const auto& c : controls) targetXs.push_back(c.x);
        AcDbObjectId primary;
        {
            auto reference = std::make_unique<AcDbDatabase>(false, true);
            const auto read = reference->readDwgFile(p.reference.c_str(), AcDbDatabase::kForReadAndAllShare, false); sourceRead = std::to_string(read); check(read, "read prune frozen reference");
            const auto closed = reference->closeInput(true); sourceClose = std::to_string(closed); check(closed, "close prune reference input");
            AcDbObjectId source; check(reference->getAcDbObjectId(source, false, AcDbHandle(L"199F")), "find prune199F source");
            { WorkingDatabaseScope working(*reference); auto entity = open<AcDbEntity>(source);
              if (entity->isAProxy() || _wcsicmp(entity->isA()->name(), L"TDbAxisLabelSet")) throw std::runtime_error("prune199F source native type mismatch");
              entity.reset(); sourceTemplate = familyCompactEntity(source); }
            AcDbObjectIdArray one; one.append(source); AcDbIdMapping mapping; check(mapping.setDestDb(database), "set prune clone destination");
            const auto cloned = reference->wblockCloneObjects(one, modelId(*database), mapping, AcDb::kDrcMangleName, false); cloneStatus = std::to_string(cloned); check(cloned, "clone one native prune template");
            AcDbIdPair pair; pair.setKey(source);
            if (!mapping.compute(pair) || !pair.isCloned() || pair.value().isNull() || pair.value().database() != database) throw std::runtime_error("prune real primary clone mapping unavailable");
            primary = pair.value(); primaryHandle = handleOf(primary); cloneMapping = "{\"sourceHandle\":\"199F\",\"targetHandle\":" + quote(primaryHandle) + ",\"isCloned\":true}";
            for (int i = 0; i < originals.length(); ++i) if (primary == originals[i]) throw std::runtime_error("prune clone reused original entity");
            resources = symbolResourceMapping(mapping); { WorkingDatabaseScope working(*reference); sourceAfterClone = familyCompactEntity(source); }
        }
        guard(); cloneCount = modelCount(*database); addedClone = familyCompactArray(familyAdditions(*database, originals)); before = axisTemplateDeleteObserve(primary);
        if (cloneCount != 214 || !before.available() || before.unionXs.size() != 9 || before.grips.length() != 41 || before.stretch.length() != 36)
            throw std::runtime_error("prune native template point/count baseline mismatch");
        keep.push_back(before.unionXs.front());
        if (count == 3) {
            if (std::fabs(before.unionXs[3]-41881.381329981) > 1e-6) throw std::runtime_error("prune approved middle template station differs from bound fixture");
            keep.push_back(before.unionXs[3]);
        }
        keep.push_back(before.unionXs.back());
        for (const auto x : keep) if (!axisPruneHasX(axisTemplateDeleteXs(before.topCandidates), x) || !axisPruneHasX(axisTemplateDeleteXs(before.bottomCandidates), x))
            throw std::runtime_error("prune retained station is not initially visible on both sides");
        guard(); const int zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_E", RTNONE); zoomStatus = std::to_string(zoomed);
        if (zoomed != RTNORM) throw std::runtime_error("prune diagnostic zoom failed");
        AxisTemplateDeleteObservation current = before;
        for (const auto x : before.unionXs) {
            if (axisPruneHasX(keep, x)) continue;
            const auto stageBefore = current; std::vector<std::string> attempts; bool deleted{};
            std::vector<double> wanted;
            for (const auto old : current.unionXs) if (std::fabs(old-x) > 1e-6) wanted.push_back(old);
            bool usedTop{}, usedBottom{};
            for (int trial = 0; trial < 2 && error == "null"; ++trial) {
                const bool hasTop = axisPruneHasX(axisTemplateDeleteXs(current.topCandidates), x), hasBottom = axisPruneHasX(axisTemplateDeleteXs(current.bottomCandidates), x);
                const bool top = hasTop && !usedTop;
                if (!top && (!hasBottom || usedBottom)) { error = quote(L"prune no remaining visible circle for a stored station; stopped"); break; }
                const auto attemptBefore = current; AxisTemplateDeleteObservation attemptAfter; std::string attemptError{"null"};
                guard(); attempts.push_back(axisPruneDeleteAttempt(document, *database, primary, current, x, top, guard, attemptAfter, attemptError));
                if (top) usedTop = true; else usedBottom = true;
                current = attemptAfter;
                if (attemptError != "null") { error = attemptError; break; }
                if (!current.available()) { error = quote(L"prune primary unavailable after command; stopped"); break; }
                const bool otherPoints = axisPrunePointBag(axisPruneWithoutX(attemptBefore.grips, x), axisPruneWithoutX(current.grips, x))
                    && axisPrunePointBag(axisPruneWithoutX(attemptBefore.stretch, x), axisPruneWithoutX(current.stretch, x));
                if (!otherPoints || familyCompactArray(originals) != originalBefore) { error = quote(L"prune changed another public point or protected original; stopped"); break; }
                if (axisPruneSameXs(current.unionXs, wanted)) { deleted = true; break; }
                const auto& previousCandidates = top ? attemptBefore.topCandidates : attemptBefore.bottomCandidates;
                const auto& remainingCandidates = top ? current.topCandidates : current.bottomCandidates;
                if (!axisPruneSameXs(current.unionXs, stageBefore.unionXs) || !axisPruneHasX(axisTemplateDeleteXs(previousCandidates), x)
                    || axisPruneHasX(axisTemplateDeleteXs(remainingCandidates), x)) { error = quote(L"prune no exact station drop or visible-side removal; stopped without retry"); break; }
            }
            if (deleted && (!axisPrunePointBag(axisPruneWithoutX(stageBefore.grips, x), current.grips)
                || !axisPrunePointBag(axisPruneWithoutX(stageBefore.stretch, x), current.stretch))) {
                deleted = false; error = quote(L"prune public station X disappeared but its complete point group was not removed; stopped");
            }
            if (!deleted && error == "null") error = quote(L"prune two-side attempts did not remove stored public station; stopped");
            deleteSteps.push_back("{\"targetX\":" + number(x) + ",\"before\":" + stageBefore.entity + ",\"after\":" + current.entity + ",\"attempts\":" + axisTraceArray(attempts) + '}');
            if (error != "null") break;
        }
        guard(); pruned = axisTemplateDeleteObserve(primary); addedPrune = familyCompactArray(familyAdditions(*database, originals));
        pruneCompleted = error == "null" && axisPruneBothSides(pruned, keep);
        if (!pruneCompleted && error == "null") error = quote(L"prune final kept stations/two-sided visible circles do not match contract");
        if (pruneCompleted) {
            const AcGeVector3d shift(targetXs.front()-keep.front(), 0, 0); std::string opened{"null"}, moved{"null"}, finished{"null"};
            try {
                guard();
                TransactionScope transaction(*database); AcDbObject* raw{}; const auto status = transaction.get()->getObject(raw, primary, AcDb::kForWrite); opened = std::to_string(status); check(status, "open pruned collection for translation");
                auto* entity = AcDbEntity::cast(raw); if (!entity) throw std::runtime_error("pruned primary is not an entity");
                const auto result = entity->transformBy(AcGeMatrix3d::translation(shift)); moved = std::to_string(result);
                const auto end = transaction.finish(result == Acad::eOk); finished = std::to_string(end); check(end, "finish prune translation transaction"); check(result, "translate pruned primary");
            } catch (const std::exception& failure) { error = familyError(failure); }
            guard(); translated = axisTemplateDeleteObserve(primary);
            translation = "{\"offset\":" + point(AcGePoint3d(shift.x, shift.y, shift.z)) + ",\"openStatus\":" + opened + ",\"moveStatus\":" + moved + ",\"transactionStatus\":" + finished + '}';
            std::vector<double> currentXs; for (const auto x : keep) currentXs.push_back(x+shift.x);
            std::vector<bool> handled(count, false); current = translated;
            for (int step = 0; step < count && error == "null"; ++step) {
                int index = -1;
                for (int i = 0; i < count && index < 0; ++i) if (!handled[i]) {
                    bool occupied{};
                    for (int j = 0; j < count; ++j) if (j != i && std::fabs(currentXs[j]-targetXs[i]) <= 1e-6) occupied = true;
                    if (!occupied) index = i;
                }
                if (index < 0) { error = quote(L"prune no collision-free target mapping order available; stopped"); break; }
                const auto moveBefore = current; const double oldX = currentXs[index], delta = targetXs[index]-oldX;
                AcDbIntArray indices; AcGePoint3dArray selected;
                for (int i = 0; i < current.stretch.length(); ++i) if (std::fabs(current.stretch[i].x-oldX) <= 1e-6) { indices.append(i); selected.append(current.stretch[i]); }
                std::string openStatus{"null"}, moveStatus{"null"}, transactionStatus{"null"}; bool attempted{};
                if (indices.length() != 4) error = quote(L"prune current station does not expose exactly four stretch points");
                else if (std::fabs(delta) > 1e-6) {
                    try {
                        guard();
                        TransactionScope transaction(*database); AcDbObject* raw{}; const auto status = transaction.get()->getObject(raw, primary, AcDb::kForWrite); openStatus = std::to_string(status); check(status, "open prune stretch group");
                        auto* entity = AcDbEntity::cast(raw); if (!entity) throw std::runtime_error("prune stretch primary is not an entity");
                        attempted = true; const auto result = entity->moveStretchPointsAt(indices, AcGeVector3d(delta, 0, 0)); moveStatus = std::to_string(result);
                        const auto end = transaction.finish(result == Acad::eOk); transactionStatus = std::to_string(end); check(end, "finish prune stretch transaction"); check(result, "move four prune stretch points");
                    } catch (const std::exception& failure) { error = familyError(failure); }
                }
                guard(); current = axisTemplateDeleteObserve(primary);
                stretchMoves.push_back("{\"mappingIndex\":" + std::to_string(index) + ",\"currentX\":" + number(oldX) + ",\"targetX\":" + number(targetXs[index])
                    + ",\"offset\":" + point(AcGePoint3d(delta, 0, 0)) + ",\"indices\":" + integers(indices) + ",\"moveAttempted\":" + (attempted ? "true" : "false")
                    + ",\"openStatus\":" + openStatus + ",\"moveStatus\":" + moveStatus + ",\"transactionStatus\":" + transactionStatus + ",\"before\":" + moveBefore.entity + ",\"after\":" + current.entity + '}');
                if (error == "null") {
                    AcGePoint3dArray wantedGrips, wantedStretch;
                    for (int i = 0; i < moveBefore.grips.length(); ++i) { auto p = moveBefore.grips[i]; if (std::fabs(p.x-oldX) <= 1e-6) p.x += delta; wantedGrips.append(p); }
                    for (int i = 0; i < moveBefore.stretch.length(); ++i) { auto p = moveBefore.stretch[i]; if (std::fabs(p.x-oldX) <= 1e-6) p.x += delta; wantedStretch.append(p); }
                    if (!current.available() || !axisPrunePointBag(wantedGrips, current.grips) || !axisPrunePointBag(wantedStretch, current.stretch))
                        error = quote(L"prune stretch changed a point outside the selected four-point station group; stopped");
                }
                currentXs[index] = targetXs[index]; std::vector<double> wanted = currentXs; std::sort(wanted.begin(), wanted.end());
                if (error == "null" && !axisPruneBothSides(current, wanted)) error = quote(L"prune native stretch did not reach the exact real-wall positions/two-sided circles");
                handled[index] = true;
            }
        }
        guard(); after = axisTemplateDeleteObserve(primary); positioned = error == "null" && pruneCompleted && axisPruneBothSides(after, targetXs);
        originalAfter = familyCompactArray(originals); finalCount = modelCount(*database); added = familyCompactArray(familyAdditions(*database, originals));
        if (error == "null" && originalAfter != originalBefore) error = quote(L"prune protected original compact records changed");
        requireNewFile(output); const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save isolated prune output including any recorded failure");
        {
            auto reader = std::make_unique<AcDbDatabase>(false, true); const auto read = reader->readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); firstRead = std::to_string(read); check(read, "first independent prune read");
            const auto close = reader->closeInput(true); firstClose = std::to_string(close); check(close, "close first prune input");
            { WorkingDatabaseScope working(*reader); firstCount = modelCount(*reader); } reader.reset(); readerDestroyed = true;
        }
        auto reader = std::make_unique<AcDbDatabase>(false, true); const auto read = reader->readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(read); check(read, "new reader prune reopen");
        const auto close = reader->closeInput(true); closeStatus = std::to_string(close); check(close, "close final prune input");
        {
            WorkingDatabaseScope working(*reader); reopenedCount = modelCount(*reader); AcDbObjectIdArray copies;
            for (const auto& handle : originalHandles) { AcDbObjectId id; check(reader->getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find persisted prune original"); copies.append(id); }
            originalSaved = familyCompactArray(copies); savedAdded = familyCompactArray(familyAdditions(*reader, copies));
            AcDbObjectId id; const auto lookup = reader->getAcDbObjectId(id, false, AcDbHandle(primaryHandle.c_str())); if (lookup == Acad::eOk) saved = axisTemplateDeleteObserve(id); else saved.readStatus = lookup;
        }
    } catch (const std::exception& failure) { if (error == "null") error = familyError(failure); }
      catch (...) { if (error == "null") error = quote(L"unknown prune fixture exception"); }
    std::vector<std::string> controlJson; for (const auto& c : controls) controlJson.push_back(c.json());
    std::ostringstream out;
    out << "{\"contract\":\"stage1.axis-template-prune-native.v1\",\"api\":\"ObjectARX native; no COM\",\"entrypoint\":" << quote(count == 2 ? L"S1AXISTEMPLATETWO" : L"S1AXISTEMPLATETHREE")
        << ",\"requestedStationCount\":" << count << ",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output) << ",\"schemeInputPath\":" << quote(p.scheme) << ",\"referenceInputPath\":" << quote(p.reference)
        << ",\"primaryHandle\":" << quote(primaryHandle) << ",\"cloneMapping\":" << cloneMapping << ",\"resourceMapping\":" << resources << ",\"sourceTemplate\":" << sourceTemplate << ",\"sourceTemplateAfterClone\":" << sourceAfterClone
        << ",\"before\":" << before.entity << ",\"afterPrune\":" << pruned.entity << ",\"afterTranslation\":" << translated.entity << ",\"after\":" << after.entity << ",\"saved\":" << saved.entity
        << ",\"sourceKeepXs\":" << axisTemplateDeleteNumbers(keep) << ",\"targetStationXs\":" << axisTemplateDeleteNumbers(targetXs) << ",\"targetWallControls\":" << axisTraceArray(controlJson)
        << ",\"middleWallSelectionRule\":\"fixture only: actual vertical200 wall center nearest CDB4/CDB0 midpoint; tie smaller X then handle; not project axis-selection business logic\""
        << ",\"deleteSteps\":" << axisTraceArray(deleteSteps) << ",\"translation\":" << translation << ",\"stretchMoves\":" << axisTraceArray(stretchMoves)
        << ",\"initialModelCount\":" << initialCount << ",\"postCloneModelCount\":" << cloneCount << ",\"finalModelCount\":" << finalCount << ",\"firstReaderModelCount\":" << firstCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"originalBefore\":" << originalBefore << ",\"originalAfter\":" << originalAfter << ",\"originalSaved\":" << originalSaved << ",\"addedAfterClone\":" << addedClone << ",\"addedAfterPrune\":" << addedPrune << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded
        << ",\"sourceReadStatus\":" << sourceRead << ",\"sourceCloseInputStatus\":" << sourceClose << ",\"cloneStatus\":" << cloneStatus << ",\"zoomStatus\":" << zoomStatus << ",\"saveStatus\":" << saveStatus
        << ",\"firstReaderStatus\":" << firstRead << ",\"firstReaderCloseInputStatus\":" << firstClose << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus
        << ",\"pruneCompleted\":" << (pruneCompleted ? "true" : "false") << ",\"positioningCompleted\":" << (positioned ? "true" : "false") << ",\"readerDestroyedBeforeFinalRead\":" << (readerDestroyed ? "true" : "false") << ",\"hostDatabaseDestroyed\":false,\"numberingAttempted\":false"
        << ",\"traceLimitations\":\"shared owner omits GetCorner and may omit normal-status keyword text; actual SSGet window and supplied N remain recorded, no fabricated readback\""
        << ",\"scope\":\"template pruning and real-scheme X positioning only; public station counts are not private internal axisCount; numbering, template Y range, full grid, business associations and rendered approval remain unverified\",\"error\":" << error << '}';
    return out.str();
}

void nativeAxisTemplatePruneProbe(int count) {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\axis-template-prune-native.json"); requireNewFile(report);
        writeNewReport(report, axisTemplatePruneCase(p, count)); acutPrintf(L"\nSTAGE1_AXIS_TEMPLATE_PRUNE_COMPLETED; STATIONS_POSITIONS_AND_FAILURES_REQUIRE_VERIFICATION\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_AXIS_TEMPLATE_PRUNE_FAILED: %hs\n", failure.what()); }
      catch (...) { acutPrintf(L"\nSTAGE1_AXIS_TEMPLATE_PRUNE_FAILED: unknown C++ exception\n"); }
}
void nativeAxisTemplateTwo() { nativeAxisTemplatePruneProbe(2); }
void nativeAxisTemplateThree() { nativeAxisTemplatePruneProbe(3); }
