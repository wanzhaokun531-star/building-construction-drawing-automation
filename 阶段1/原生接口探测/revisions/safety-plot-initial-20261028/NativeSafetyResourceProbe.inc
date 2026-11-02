// Include after NativeStyleAudit.inc, inside the existing anonymous namespace.
// Required FILE-SCOPE headers in NativeAccessProbe.cpp (never include here):
// AcDbAssocDependency.h, AcDbAssocNetwork.h. Uses the existing 2020 SDK helpers.
// No commands, document/UI mutation, private serialization, or source-file saves.

std::wstring nsrOutput(const Paths& p, const wchar_t* name) {
    const auto result = fullPath(p.output + L"\\" + name);
    if (!below(result, p.output)) throw std::runtime_error("safety probe output escaped isolated run");
    requireNewFile(result); return result;
}
void nsrReadFrozen(AcDbDatabase& db, const std::wstring& path) {
    check(db.readDwgFile(path.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read safety frozen input");
    check(db.closeInput(true), "close safety frozen input");
}
AcDbObjectId nsrFind(AcDbDatabase& db, const wchar_t* handle) {
    AcDbObjectId id; check(db.getAcDbObjectId(id, false, AcDbHandle(handle)), "find safety fixture"); return id;
}
std::string nsrReference(AcDbObjectId id, const AcDbDatabase& expected) {
    if (id.isNull()) return "null";
    if (!id.isValid()) return "{\"valid\":false,\"handle\":null,\"sameDatabase\":false}";
    auto result = styleAuditReference(id);
    result.pop_back();
    return result + ",\"valid\":true,\"erased\":" + styleAuditBool(id.isErased())
        + ",\"sameDatabase\":" + styleAuditBool(id.database() == &expected) + '}';
}
std::string nsrReferences(const AcDbObjectIdArray& ids, const AcDbDatabase& db) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) { if (i) out << ','; out << nsrReference(ids[i], db); }
    out << ']'; return out.str();
}
std::string nsrGeometry(const AcDbEntity& entity) {
    AcString layer; check(entity.layer(layer), "read safety geometry layer");
    AcDbExtents bounds; const auto extentStatus = entity.getGeomExtents(bounds);
    AcGePoint3dArray grip, stretch; AcDbIntArray snap, geom;
    const auto gripStatus = entity.getGripPoints(grip, snap, geom);
    const auto stretchStatus = entity.getStretchPoints(stretch);
    std::ostringstream out;
    out << "{\"handle\":" << quote(handleOf(entity.objectId())) << ",\"type\":" << quote(entity.isA()->name())
        << ",\"proxy\":" << styleAuditBool(entity.isAProxy() != 0) << ",\"layer\":" << quote(layer.kACharPtr())
        << ",\"colorIndex\":" << entity.colorIndex() << ",\"extentStatus\":" << extentStatus
        << ",\"min\":" << (extentStatus == Acad::eOk ? point(bounds.minPoint()) : "null")
        << ",\"max\":" << (extentStatus == Acad::eOk ? point(bounds.maxPoint()) : "null")
        << ",\"curve\":" << curveJson(entity) << ",\"grip\":{\"status\":" << gripStatus << ",\"points\":" << points(grip)
        << "},\"stretch\":{\"status\":" << stretchStatus << ",\"points\":" << points(stretch) << "}}";
    return out.str();
}
std::string nsrGeometryId(AcDbObjectId id) { const auto entity = open<AcDbEntity>(id); return nsrGeometry(*entity); }
std::string nsrGeometryArray(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) { if (i) out << ','; out << nsrGeometryId(ids[i]); }
    out << ']'; return out.str();
}
std::string nsrRelation(const Paths& p) {
    try {
        AcDbDatabase db(false, true);
        const auto read = db.readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false); check(read, "read relationship input");
        const auto closed = db.closeInput(true); check(closed, "close relationship input"); WorkingDatabaseScope working(db);
        const auto ids = modelIds(db);
        const auto top = AcDbAssocNetwork::getInstanceFromDatabase(&db, false);
        std::string actions = "[]";
        if (!top.isNull()) { auto network = open<AcDbAssocNetwork>(top); actions = nsrReferences(network->getActions(), db); }
        std::ostringstream records; records << '[';
        for (int i = 0; i < ids.length(); ++i) {
            auto entity = open<AcDbEntity>(ids[i]);
            AcDbObjectIdArray dependencies;
            const auto status = AcDbAssocDependency::getDependenciesOnObject(entity.get(), true, true, dependencies);
            const auto network = AcDbAssocNetwork::getInstanceFromObject(ids[i], false);
            bool observed = !network.isNull() || dependencies.length() > 0;
            std::ostringstream reactors; reactors << '[';
            const auto* values = entity->reactors();
            if (values) for (int n = 0; n < values->length(); ++n) {
                if (n) reactors << ',';
                const bool persistent = acdbIsPersistentReactor((*values)[n]); observed = observed || persistent;
                reactors << "{\"persistent\":" << styleAuditBool(persistent) << ",\"object\":"
                    << (persistent ? nsrReference(acdbPersistentReactorObjectId((*values)[n]), db) : "null") << '}';
            }
            reactors << ']'; std::ostringstream deps; deps << '[';
            for (int n = 0; n < dependencies.length(); ++n) {
                if (n) deps << ',';
                auto dependency = open<AcDbAssocDependency>(dependencies[n]);
                deps << "{\"object\":" << nsrReference(dependencies[n], db)
                    << ",\"dependentOnObject\":" << nsrReference(dependency->dependentOnObject(), db)
                    << ",\"owningAction\":" << nsrReference(dependency->owningAction(), db)
                    << ",\"dependentOnObjectStatus\":" << dependency->dependentOnObjectStatus()
                    << ",\"readDependency\":" << styleAuditBool(dependency->isReadDependency())
                    << ",\"writeDependency\":" << styleAuditBool(dependency->isWriteDependency()) << '}';
            }
            deps << ']';
            if (i) records << ',';
            records << "{\"handle\":" << quote(handleOf(ids[i])) << ",\"type\":" << quote(entity->isA()->name())
                << ",\"owner\":" << nsrReference(entity->ownerId(), db)
                << ",\"extensionDictionary\":" << nsrReference(entity->extensionDictionary(), db)
                << ",\"objectNetwork\":" << nsrReference(network, db) << ",\"reactors\":" << reactors.str()
                << ",\"dependencyQueryStatus\":" << status << ",\"dependencies\":" << deps.str()
                << ",\"observation\":" << quote(status != Acad::eOk ? L"QueryUnavailable" : observed ? L"PublicStructureObserved" : L"NoPublicStructureObserved") << '}';
        }
        records << ']'; std::ostringstream out;
        out << "{\"error\":null,\"readStatus\":" << read << ",\"closeInputStatus\":" << closed << ",\"sourceCount\":" << ids.length()
            << ",\"createNetworkIfAbsent\":false,\"businessAssociationStatus\":\"Unknown\",\"records\":" << records.str()
            << ",\"topNetwork\":" << nsrReference(top, db) << ",\"topNetworkActions\":" << actions
            << ",\"scope\":\"public structure only; zero reactors or dependencies does not establish absent or preserved Tianzheng hosting\"}";
        return out.str();
    } catch (const std::exception& failure) { return "{\"error\":" + styleAuditError(failure) + '}'; }
}
AcDbObjectId nsrTextStyle(AcDbDatabase& db, const wchar_t* name) {
    auto table = open<AcDbTextStyleTable>(db.textStyleTableId()); AcDbObjectId id;
    check(table->getAt(name, id), "find conflicting text style"); return id;
}
AcDbObjectIdArray nsrResourceIds(AcDbDatabase& db) {
    AcDbObjectIdArray ids;
    for (const auto tableId : {db.textStyleTableId(), db.linetypeTableId(), db.dimStyleTableId(), db.layerTableId()}) {
        auto table = open<AcDbSymbolTable>(tableId); AcDbSymbolTableIterator* raw{};
        check(table->newIterator(raw), "create safety resource iterator"); std::unique_ptr<AcDbSymbolTableIterator> iterator(raw);
        for (; !iterator->done(); iterator->step()) { AcDbObjectId id; check(iterator->getRecordId(id), "read safety resource id"); ids.append(id); }
    }
    return ids;
}
std::vector<std::wstring> nsrHandles(const AcDbObjectIdArray& ids) {
    std::vector<std::wstring> result;
    for (int i = 0; i < ids.length(); ++i) result.push_back(handleOf(ids[i])); return result;
}
AcDbObjectIdArray nsrResolve(AcDbDatabase& db, const std::vector<std::wstring>& handles) {
    AcDbObjectIdArray result;
    for (const auto& handle : handles) result.append(nsrFind(db, handle.c_str())); return result;
}
std::string nsrResourceArray(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) {
        std::string record;
        { auto object = open<AcDbObject>(ids[i]);
          if (object->isKindOf(AcDbTextStyleTableRecord::desc())) record = styleAuditTextStyle(ids[i]);
          else if (object->isKindOf(AcDbLinetypeTableRecord::desc())) record = styleAuditLinetype(ids[i]);
          else if (object->isKindOf(AcDbDimStyleTableRecord::desc())) record = styleAuditDimStyle(ids[i]);
          else if (object->isKindOf(AcDbLayerTableRecord::desc())) record = styleAuditLayer(ids[i]);
          else throw std::runtime_error("unexpected safety resource record"); }
        if (i) out << ','; out << record;
    }
    out << ']'; return out.str();
}
std::string nsrStyleConflict(const Paths& p) {
    try {
        const auto output = nsrOutput(p, L"native-safety-style-conflict.dwg");
        AcDbDatabase reference(false, true); nsrReadFrozen(reference, p.reference);
        auto target = std::make_unique<AcDbDatabase>(false, true); nsrReadFrozen(*target, p.scheme);
        const auto referenceId = nsrTextStyle(reference, L"_TCH_WINDOW");
        const auto schemeId = nsrTextStyle(*target, L"_TCH_WINDOW");
        std::string schemeStyle, referenceStyle, before, after, cloned, saved, savedResources;
        std::wstring clonedHandle; std::vector<std::wstring> originals;
        int countBefore = 0, countAfter = 0, countSaved = 0; Acad::ErrorStatus cloneStatus = Acad::eNotApplicable, saveStatus = Acad::eNotApplicable;
        {
            WorkingDatabaseScope working(reference); referenceStyle = styleAuditTextStyle(referenceId);
        }
        {
            WorkingDatabaseScope working(*target);
            schemeStyle = styleAuditTextStyle(schemeId); const auto resources = nsrResourceIds(*target); originals = nsrHandles(resources);
            before = nsrResourceArray(resources); countBefore = modelCount(*target);
            AcDbObjectIdArray one; one.append(referenceId); AcDbIdMapping mapping;
            check(mapping.setDestDb(target.get()), "set style clone destination");
            cloneStatus = reference.wblockCloneObjects(one, target->textStyleTableId(), mapping, AcDb::kDrcMangleName, false);
            check(cloneStatus, "clone one native text style record");
            AcDbIdPair pair; pair.setKey(referenceId);
            if (!mapping.compute(pair) || !pair.isCloned() || pair.value().isNull()) throw std::runtime_error("style clone primary mapping absent");
            clonedHandle = handleOf(pair.value()); cloned = styleAuditTextStyle(pair.value());
            after = nsrResourceArray(resources); countAfter = modelCount(*target);
            requireNewFile(output); saveStatus = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); check(saveStatus, "save style conflict fixture");
        }
        // Persisted observations must not use the target database or its object pointers.
        target.reset(); AcDbDatabase reopened(false, true);
        const auto reopenStatus = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); check(reopenStatus, "reopen style conflict fixture");
        const auto closeStatus = reopened.closeInput(true); check(closeStatus, "close style conflict persisted input");
        { WorkingDatabaseScope working(reopened); saved = styleAuditTextStyle(nsrFind(reopened, clonedHandle.c_str()));
          savedResources = nsrResourceArray(nsrResolve(reopened, originals)); countSaved = modelCount(reopened); }
        std::ostringstream out;
        out << "{\"error\":null,\"schemeStyle\":" << schemeStyle << ",\"referenceStyle\":" << referenceStyle
            << ",\"clonedStyle\":" << cloned << ",\"savedStyle\":" << saved
            << ",\"mapping\":{\"sourceHandle\":" << quote(handleOf(referenceId)) << ",\"targetHandle\":" << quote(clonedHandle)
            << ",\"isCloned\":true,\"duplicatePolicy\":\"kDrcMangleName\"},\"cloneStatus\":" << cloneStatus
            << ",\"originalResourcesBefore\":" << before << ",\"originalResourcesAfter\":" << after << ",\"originalResourcesSaved\":" << savedResources
            << ",\"modelCountBefore\":" << countBefore << ",\"modelCountAfter\":" << countAfter << ",\"reopenedModelCount\":" << countSaved
            << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus
            << ",\"outputDWG\":" << quote(output) << ",\"businessPrimaryBindingVerified\":false}";
        return out.str();
    } catch (const std::exception& failure) { return "{\"error\":" + styleAuditError(failure) + '}'; }
}
struct NsrAbortObservation {
    std::string before, during, after;
    Acad::ErrorStatus move = Acad::eNotApplicable, aborted = Acad::eNotApplicable;
};
NsrAbortObservation nsrTransformAbort(AcDbDatabase& db, AcDbObjectId id, const AcGeVector3d& delta) {
    NsrAbortObservation result; result.before = nsrGeometryId(id);
    {
        TransactionScope transaction(db); AcDbObject* object{};
        check(transaction.get()->getObject(object, id, AcDb::kForWrite), "open native transform rollback fixture");
        auto* entity = AcDbEntity::cast(object); if (!entity) throw std::runtime_error("transform rollback fixture is not an entity");
        result.move = entity->transformBy(AcGeMatrix3d::translation(delta));
        // Observe through the transaction-owned pointer; do not reopen/close it independently.
        result.during = nsrGeometry(*entity); result.aborted = transaction.finish(false);
        check(result.aborted, "abort observed native geometry transform");
    }
    result.after = nsrGeometryId(id); return result;
}
std::string nsrAbortTrialJson(const wchar_t* name, const NsrAbortObservation& observation, const AcGeVector3d& offset,
                            const std::string& saved, const std::wstring& output, Acad::ErrorStatus save,
                            Acad::ErrorStatus reopened, Acad::ErrorStatus closed) {
    std::ostringstream out;
    out << "{\"id\":" << quote(name) << ",\"moveStatus\":" << observation.move << ",\"abortStatus\":" << observation.aborted
        << ",\"offset\":[" << number(offset.x) << ',' << number(offset.y) << ',' << number(offset.z)
        << "],\"before\":" << observation.before << ",\"during\":" << observation.during << ",\"after\":" << observation.after
        << ",\"saved\":" << saved << ",\"saveStatus\":" << save << ",\"reopenStatus\":" << reopened << ",\"closeInputStatus\":" << closed
        << ",\"outputDWG\":" << quote(output) << ",\"trigger\":\"explicit abort after an observed native transform\"}";
    return out.str();
}
std::string nsrGeometryRollback(const Paths& p) {
    try {
        const AcGeVector3d delta(37, 61, 0);
        const auto dimensionOutput = nsrOutput(p, L"native-safety-dimension-abort.dwg");
        const auto lineOutput = nsrOutput(p, L"native-safety-line-abort.dwg");
        std::string before, after, savedOriginal, dimensionSaved, lineSaved;
        NsrAbortObservation dimension, line; std::vector<std::wstring> handles;
        auto target = std::make_unique<AcDbDatabase>(false, true); nsrReadFrozen(*target, p.scheme); target->disableUndoRecording(false);
        Acad::ErrorStatus savedDimension = Acad::eNotApplicable, savedLine = Acad::eNotApplicable;
        {
            WorkingDatabaseScope working(*target); const auto ids = modelIds(*target); handles = nsrHandles(ids); before = nsrGeometryArray(ids);
            const auto selected = nsrFind(*target, L"CDE6");
            { auto entity = open<AcDbEntity>(selected); if (_wcsicmp(entity->isA()->name(), L"TDbDimension2")) throw std::runtime_error("CDE6 rollback fixture is not native dimension"); }
            dimension = nsrTransformAbort(*target, selected, delta); after = nsrGeometryArray(ids);
            requireNewFile(dimensionOutput); savedDimension = target->saveAs(dimensionOutput.c_str(), false, AcDb::kDHL_CURRENT); check(savedDimension, "save dimension abort fixture");
        }
        target.reset(); AcDbDatabase persisted(false, true);
        const auto reopenedDimension = persisted.readDwgFile(dimensionOutput.c_str(), AcDbDatabase::kForReadAndAllShare, false); check(reopenedDimension, "reopen dimension abort fixture");
        const auto closedDimension = persisted.closeInput(true); check(closedDimension, "close dimension abort persisted input");
        { WorkingDatabaseScope working(persisted); savedOriginal = nsrGeometryArray(nsrResolve(persisted, handles)); dimensionSaved = nsrGeometryId(nsrFind(persisted, L"CDE6")); }
        auto control = std::make_unique<AcDbDatabase>(true, true); control->disableUndoRecording(false); std::wstring lineHandle;
        {
            WorkingDatabaseScope working(*control); AcDbObjectId id;
            { auto model = open<AcDbBlockTableRecord>(modelId(*control), AcDb::kForWrite);
              auto entity = std::make_unique<AcDbLine>(AcGePoint3d(0, 0, 0), AcGePoint3d(1000, 0, 0)); entity->setDatabaseDefaults(control.get());
              check(model->appendAcDbEntity(id, entity.get()), "append native line abort control"); auto* resident = entity.release(); check(resident->close(), "close native line abort control"); }
            lineHandle = handleOf(id); line = nsrTransformAbort(*control, id, delta);
            requireNewFile(lineOutput); savedLine = control->saveAs(lineOutput.c_str(), false, AcDb::kDHL_CURRENT); check(savedLine, "save native line abort control");
        }
        control.reset(); AcDbDatabase linePersisted(false, true);
        const auto reopenedLine = linePersisted.readDwgFile(lineOutput.c_str(), AcDbDatabase::kForReadAndAllShare, false); check(reopenedLine, "reopen native line abort control");
        const auto closedLine = linePersisted.closeInput(true); check(closedLine, "close native line persisted input");
        { WorkingDatabaseScope working(linePersisted); lineSaved = nsrGeometryId(nsrFind(linePersisted, lineHandle.c_str())); }
        return "{\"error\":null,\"originalBefore\":" + before + ",\"originalAfter\":" + after + ",\"originalSaved\":" + savedOriginal
            + ",\"trials\":[" + nsrAbortTrialJson(L"dimension", dimension, delta, dimensionSaved, dimensionOutput, savedDimension, reopenedDimension, closedDimension)
            + ',' + nsrAbortTrialJson(L"native-line", line, delta, lineSaved, lineOutput, savedLine, reopenedLine, closedLine) + "]}";
    } catch (const std::exception& failure) { return "{\"error\":" + styleAuditError(failure) + '}'; }
}
int nsrCountRecord(AcDbBlockTableRecord& model) {
    AcDbBlockTableRecordIterator* raw{}; check(model.newIterator(raw), "create during-abort model iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw); int count = 0;
    for (; !iterator->done(); iterator->step()) ++count; return count;
}
bool nsrActiveObject(AcDbObjectId id) {
    return !id.isNull() && id.isValid() && !id.isErased();
}
std::string nsrReadSentinel(const std::wstring& path) {
    FILE* raw{}; if (_wfopen_s(&raw, path.c_str(), L"rb") || !raw) throw std::runtime_error("cannot reopen owned sentinel");
    struct FileClose { void operator()(FILE* file) const { if (file) fclose(file); } };
    std::unique_ptr<FILE, FileClose> file(raw); char buffer[128]{}; const auto count = fread(buffer, 1, sizeof(buffer), file.get());
    if (ferror(file.get()) || !feof(file.get())) throw std::runtime_error("sentinel read incomplete or unexpectedly large");
    return std::string(buffer, count);
}
std::string nsrCreationRollback(const Paths& p) {
    try {
        const auto output = nsrOutput(p, L"native-safety-object-abort.dwg");
        const auto failureParent = fullPath(p.output + L"\\native-safety-absent-parent");
        const auto failureOutput = fullPath(failureParent + L"\\not-published.dwg");
        if (!below(failureOutput, p.output) || GetFileAttributesW(failureParent.c_str()) != INVALID_FILE_ATTRIBUTES)
            throw std::runtime_error("save failure fixture requires a fresh absent child directory");
        const auto absenceError = GetLastError();
        if (absenceError != ERROR_FILE_NOT_FOUND && absenceError != ERROR_PATH_NOT_FOUND) throw std::runtime_error("cannot establish absent save parent");
        const auto sentinelPath = nsrOutput(p, L"native-safety-sentinel.bin");
        writeNewReport(sentinelPath, "stage1-native-safety-sentinel-v1\n"); const auto sentinelBefore = nsrReadSentinel(sentinelPath);
        bool rejected = false; try { requireNewFile(sentinelPath); } catch (const std::exception&) { rejected = true; }
        const auto sentinelAfter = nsrReadSentinel(sentinelPath);
        auto target = std::make_unique<AcDbDatabase>(false, true); nsrReadFrozen(*target, p.scheme); target->disableUndoRecording(false);
        std::string originalBefore, originalAfter, originalSaved, existingBefore, existingDuring, existingAfter, newDuring;
        std::vector<std::wstring> originals; std::wstring newHandle; AcDbObjectId newId;
        const AcGeVector3d delta(13, 17, 0); int beforeCount = 0, duringCount = 0, afterCount = 0, savedCount = 0;
        bool activeAfter = true, activeSaved = true;
        Acad::ErrorStatus moved = Acad::eNotApplicable, appended = Acad::eNotApplicable, registered = Acad::eNotApplicable;
        Acad::ErrorStatus aborted = Acad::eNotApplicable, failedSave = Acad::eNotApplicable, saved = Acad::eNotApplicable;
        {
            WorkingDatabaseScope working(*target); const auto ids = modelIds(*target); originals = nsrHandles(ids); beforeCount = ids.length();
            originalBefore = nsrGeometryArray(ids); const auto existing = nsrFind(*target, L"CDE6"); existingBefore = nsrGeometryId(existing);
            {
                TransactionScope transaction(*target); AcDbObject* object{};
                check(transaction.get()->getObject(object, existing, AcDb::kForWrite), "open existing mixed-abort object");
                auto* entity = AcDbEntity::cast(object); if (!entity) throw std::runtime_error("mixed-abort existing object is not an entity");
                moved = entity->transformBy(AcGeMatrix3d::translation(delta)); existingDuring = nsrGeometry(*entity);
                AcDbObject* modelObject{}; check(transaction.get()->getObject(modelObject, modelId(*target), AcDb::kForWrite), "open mixed-abort model record");
                auto* model = AcDbBlockTableRecord::cast(modelObject); if (!model) throw std::runtime_error("mixed-abort owner is not model record");
                auto line = std::make_unique<AcDbLine>(AcGePoint3d(0, 0, 0), AcGePoint3d(100, 25, 0)); line->setDatabaseDefaults(target.get());
                appended = model->appendAcDbEntity(newId, line.get()); check(appended, "append mixed-abort new LINE");
                // append transferred ownership to the database; registration transfers closing to the transaction.
                Opened<AcDbLine> unregistered(line.release());
                registered = target->transactionManager()->addNewlyCreatedDBRObject(unregistered.get(), true);
                check(registered, "register mixed-abort new LINE"); auto* resident = unregistered.release();
                newHandle = handleOf(newId); newDuring = nsrGeometry(*resident); duringCount = nsrCountRecord(*model);
                aborted = transaction.finish(false); check(aborted, "abort mixed existing/new-object transaction");
            }
            activeAfter = nsrActiveObject(newId); afterCount = modelCount(*target); existingAfter = nsrGeometryId(existing); originalAfter = nsrGeometryArray(ids);
            requireNewFile(failureOutput); failedSave = target->saveAs(failureOutput.c_str(), false, AcDb::kDHL_CURRENT);
            // The failed destination is never promoted or retried at a different location as a success.
            requireNewFile(output); saved = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); check(saved, "save mixed-abort observation drawing");
        }
        target.reset(); AcDbDatabase reopened(false, true);
        const auto reopenedStatus = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); check(reopenedStatus, "reopen mixed-abort observation");
        const auto closedStatus = reopened.closeInput(true); check(closedStatus, "close mixed-abort persisted input");
        {
            WorkingDatabaseScope working(reopened); savedCount = modelCount(reopened); originalSaved = nsrGeometryArray(nsrResolve(reopened, originals));
            AcDbObjectId persistedNew; const auto status = reopened.getAcDbObjectId(persistedNew, false, AcDbHandle(newHandle.c_str()));
            activeSaved = status == Acad::eOk && nsrActiveObject(persistedNew);
        }
        const bool failureExists = GetFileAttributesW(failureOutput.c_str()) != INVALID_FILE_ATTRIBUTES;
        std::ostringstream out;
        out << "{\"error\":null,\"moveStatus\":" << moved << ",\"appendStatus\":" << appended << ",\"registerStatus\":" << registered
            << ",\"abortStatus\":" << aborted << ",\"offset\":[13,17,0],\"existingBefore\":" << existingBefore
            << ",\"existingDuring\":" << existingDuring << ",\"existingAfter\":" << existingAfter << ",\"newEntityDuring\":" << newDuring
            << ",\"newEntityHandle\":" << quote(newHandle) << ",\"newEntityActiveAfter\":" << styleAuditBool(activeAfter)
            << ",\"newEntityActiveSaved\":" << styleAuditBool(activeSaved) << ",\"modelCountBefore\":" << beforeCount
            << ",\"modelCountDuring\":" << duringCount << ",\"modelCountAfter\":" << afterCount << ",\"reopenedModelCount\":" << savedCount
            << ",\"originalBefore\":" << originalBefore << ",\"originalAfter\":" << originalAfter << ",\"originalSaved\":" << originalSaved
            << ",\"saveFailureStatus\":" << failedSave << ",\"saveFailureDWG\":" << quote(failureOutput) << ",\"failedOutputPublished\":" << styleAuditBool(failureExists)
            << ",\"sentinel\":{\"path\":" << quote(sentinelPath) << ",\"guardRejected\":" << styleAuditBool(rejected)
            << ",\"before\":" << quote(std::wstring(sentinelBefore.begin(), sentinelBefore.end()))
            << ",\"after\":" << quote(std::wstring(sentinelAfter.begin(), sentinelAfter.end())) << "},\"saveStatus\":" << saved
            << ",\"reopenStatus\":" << reopenedStatus << ",\"closeInputStatus\":" << closedStatus << ",\"outputDWG\":" << quote(output) << '}';
        return out.str();
    } catch (const std::exception& failure) { return "{\"error\":" + styleAuditError(failure) + '}'; }
}
void nativeSafetyResourceProbe() {
    try {
        const auto p = paths(); const auto report = nsrOutput(p, L"native-safety-resources.json");
        const auto relation = nsrRelation(p);
        const auto style = nsrStyleConflict(p);
        const auto geometry = nsrGeometryRollback(p);
        const auto creation = nsrCreationRollback(p);
        writeNewReport(report, "{\"contract\":\"stage1.native-safety-resources.v1\",\"api\":\"ObjectARX native; no COM\",\"businessAssociationStatus\":\"Unknown\","
            "\"schemeInputPath\":" + quote(p.scheme) + ",\"referenceInputPath\":" + quote(p.reference) + ",\"relation\":" + relation
            + ",\"styleConflict\":" + style + ",\"geometryRollback\":" + geometry + ",\"creationRollback\":" + creation + '}');
        acutPrintf(L"\nSTAGE1_NATIVE_SAFETY_RESOURCES_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_NATIVE_SAFETY_RESOURCES_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_SAFETY_RESOURCES_FAILED: unknown C++ exception\n"); }
}
