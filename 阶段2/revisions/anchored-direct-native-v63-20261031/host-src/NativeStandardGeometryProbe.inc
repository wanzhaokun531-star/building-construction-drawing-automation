// Included after NativeObjectFamilyProbe.inc inside the probe's anonymous namespace.
// Explicitly synthetic ObjectARX mechanics: these are not Tianzheng replacements,
// project dimensions, approved construction details, or production drawing styles.

struct StandardOffsetCurves {
    AcDbVoidPtrArray values;
    ~StandardOffsetCurves() {
        for (int i = 0; i < values.length(); ++i) delete static_cast<AcDbCurve*>(values[i]);
    }
    StandardOffsetCurves() = default;
    StandardOffsetCurves(const StandardOffsetCurves&) = delete;
    StandardOffsetCurves& operator=(const StandardOffsetCurves&) = delete;
};

struct StandardResbufCloser {
    void operator()(resbuf* value) const { if (value) acutRelRb(value); }
};
using StandardResbuf = std::unique_ptr<resbuf, StandardResbufCloser>;

struct StandardFixtureIds {
    AcDbObjectId rectangle, outer, hatch, text, mtext, leader, block;
    AcDbObjectIdArray attributes;
};

bool standardSamePoint(const AcGePoint3d& a, const AcGePoint3d& b) {
    return std::abs(a.x - b.x) <= 1e-6 && std::abs(a.y - b.y) <= 1e-6 && std::abs(a.z - b.z) <= 1e-6;
}

void standardDefaults(AcDbEntity& entity, AcDbDatabase& database) {
    entity.setDatabaseDefaults(&database);
    AcDbLayerTable* raw{}; check(database.getLayerTable(raw, AcDb::kForRead), "standard fixture layer table");
    Opened<AcDbLayerTable> layers(raw); AcDbObjectId zero;
    check(layers->getAt(L"0", zero), "standard fixture target layer 0");
    check(entity.setLayer(zero), "standard fixture target-owned layer");
}

template<class T>
AcDbObjectId standardAppend(AcDbObjectId owner, std::unique_ptr<T> entity) {
    auto record = open<AcDbBlockTableRecord>(owner, AcDb::kForWrite); AcDbObjectId id;
    check(record->appendAcDbEntity(id, entity.get()), "append synthetic native entity");
    auto* resident = entity.release();
    check(resident->close(), "close synthetic native entity after database ownership transfer");
    return id;
}

std::unique_ptr<AcDbPolyline> standardRectangle(AcDbDatabase& database, double x, double y, double width, double depth) {
    auto polyline = std::make_unique<AcDbPolyline>(4); standardDefaults(*polyline, database);
    check(polyline->addVertexAt(0, AcGePoint2d(x, y)), "synthetic rectangle vertex 0");
    check(polyline->addVertexAt(1, AcGePoint2d(x + width, y)), "synthetic rectangle vertex 1");
    check(polyline->addVertexAt(2, AcGePoint2d(x + width, y + depth)), "synthetic rectangle vertex 2");
    check(polyline->addVertexAt(3, AcGePoint2d(x, y + depth)), "synthetic rectangle vertex 3");
    polyline->setClosed(Adesk::kTrue); return polyline;
}

AcDbObjectId standardOuterOffset(AcDbDatabase& database, AcDbObjectId rectangle) {
    std::unique_ptr<AcDbCurve> outer; double largestArea = 0;
    {
        auto source = open<AcDbPolyline>(rectangle);
        for (double distance : {-800.0, 800.0}) {
            StandardOffsetCurves generated;
            check(source->getOffsetCurves(distance, generated.values), "public native offset curve generation");
            if (generated.values.isEmpty()) throw std::runtime_error("native offset returned no curves");
            for (int i = 0; i < generated.values.length(); ++i) {
                auto* curve = static_cast<AcDbCurve*>(generated.values[i]);
                const auto* polyline = AcDbPolyline::cast(curve);
                if (!polyline || !polyline->isClosed() || polyline->numVerts() != 4)
                    throw std::runtime_error("rectangular offset is not a closed four-vertex native polyline");
                double area = 0; check(curve->getArea(area), "read actual offset curve area");
                if (std::abs(area) > largestArea) {
                    largestArea = std::abs(area); outer.reset(curve); generated.values[i] = nullptr;
                }
            }
        }
    }
    if (!outer || std::abs(largestArea - 42560000.0) > 1e-4)
        throw std::runtime_error("native outward offset did not produce the explicit synthetic area");
    standardDefaults(*outer, database);
    return standardAppend(modelId(database), std::move(outer));
}

void standardRegisterIssueApp(AcDbDatabase& database) {
    AcDbRegAppTable* raw{}; check(database.getRegAppTable(raw, AcDb::kForWrite), "open target native RegApp table");
    Opened<AcDbRegAppTable> table(raw); AcDbObjectId existing;
    const auto found = table->getAt(L"STAGE1_CAPABILITY_PROBE", existing);
    if (found == Acad::eOk) return;
    if (found != Acad::eKeyNotFound) check(found, "query synthetic issue RegApp");
    auto record = std::make_unique<AcDbRegAppTableRecord>();
    check(record->setName(L"STAGE1_CAPABILITY_PROBE"), "set synthetic issue RegApp name");
    AcDbObjectId added; check(table->add(added, record.get()), "register native issue XData app in target");
    auto* resident = record.release(); check(resident->close(), "close target-owned RegApp record");
}

void standardSetIssueXData(AcDbObjectId marker) {
    StandardResbuf data(acutBuildList(AcDb::kDxfRegAppName, L"STAGE1_CAPABILITY_PROBE",
        AcDb::kDxfXdAsciiString, L"S1_SLOPE", RTNONE));
    if (!data) throw std::runtime_error("cannot allocate synthetic issue XData");
    auto object = open<AcDbMText>(marker, AcDb::kForWrite);
    check(object->setXData(data.get()), "attach native issue identifier to actual marker");
}

std::wstring standardIssueXData(AcDbObjectId marker) {
    auto object = open<AcDbMText>(marker);
    StandardResbuf data(object->xData(L"STAGE1_CAPABILITY_PROBE"));
    if (!data) throw std::runtime_error("actual marker has no synthetic issue XData");
    for (auto* item = data.get(); item; item = item->rbnext) {
        if (item->restype == AcDb::kDxfXdAsciiString && item->resval.rstring) return item->resval.rstring;
    }
    throw std::runtime_error("actual marker XData has no issue identifier string");
}

std::string standardAttributes(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) {
        if (i) out << ',';
        auto attribute = open<AcDbAttribute>(ids[i]); AcString tag, value;
        check(attribute->tag(tag), "read actual native attribute tag");
        check(attribute->textString(value), "read actual native attribute value");
        out << "{\"handle\":" << quote(handleOf(ids[i])) << ",\"tag\":" << quote(tag.kACharPtr())
            << ",\"value\":" << quote(value.kACharPtr()) << ",\"owner\":" << quote(handleOf(attribute->ownerId()))
            << ",\"position\":" << point(attribute->position()) << '}';
    }
    out << ']'; return out.str();
}

void standardCreateBlock(AcDbDatabase& database, StandardFixtureIds& ids) {
    AcDbObjectId definition;
    {
        AcDbBlockTable* raw{}; check(database.getBlockTable(raw, AcDb::kForWrite), "open target block table for synthetic fixture");
        Opened<AcDbBlockTable> table(raw);
        if (table->has(L"S1_CAPABILITY_FIXTURE_BLOCK")) throw std::runtime_error("synthetic block name already exists in frozen input");
        auto record = std::make_unique<AcDbBlockTableRecord>();
        check(record->setName(L"S1_CAPABILITY_FIXTURE_BLOCK"), "set explicit synthetic block name");
        check(table->add(definition, record.get()), "add synthetic block definition");
        auto* resident = record.release(); check(resident->close(), "close target-owned synthetic block definition");
    }
    standardAppend(definition, standardRectangle(database, 0, 0, 4000, 2000));
    AcDbObjectIdArray definitions;
    const wchar_t* tags[] = {L"SHEET_NO", L"ISSUE_CODE"};
    const wchar_t* values[] = {L"S1-A001", L"待确认"};
    for (int i = 0; i < 2; ++i) {
        auto attribute = std::make_unique<AcDbAttributeDefinition>(); standardDefaults(*attribute, database);
        check(attribute->setTag(tags[i]), "set synthetic attribute definition tag");
        check(attribute->setPrompt(tags[i]), "set synthetic attribute definition prompt");
        check(attribute->setTextString(values[i]), "set synthetic attribute definition value");
        check(attribute->setPosition(AcGePoint3d(250, 1200 - i * 500, 0)), "set local attribute definition position");
        check(attribute->setHeight(350), "set synthetic attribute definition height");
        definitions.append(standardAppend(definition, std::move(attribute)));
    }
    auto insert = std::make_unique<AcDbBlockReference>(); standardDefaults(*insert, database);
    check(insert->setBlockTableRecord(definition), "set actual synthetic INSERT definition");
    check(insert->setPosition(AcGePoint3d(50000, 28000, 0)), "set synthetic INSERT position");
    ids.block = standardAppend(modelId(database), std::move(insert));
    for (int i = 0; i < definitions.length(); ++i) {
        auto block = open<AcDbBlockReference>(ids.block, AcDb::kForWrite);
        auto source = open<AcDbAttributeDefinition>(definitions[i]);
        auto attribute = std::make_unique<AcDbAttribute>(); standardDefaults(*attribute, database);
        check(attribute->setAttributeFromBlock(source.get(), block->blockTransform()), "instantiate native attribute in INSERT coordinates");
        AcDbObjectId id; check(block->appendAttribute(id, attribute.get()), "append native attribute to actual INSERT owner");
        auto* resident = attribute.release(); check(resident->close(), "close target-owned native attribute");
        ids.attributes.append(id);
    }
}

int standardMoveBlockOnce(const StandardFixtureIds& ids) {
    std::vector<AcGePoint3d> before;
    for (int i = 0; i < ids.attributes.length(); ++i) before.push_back(open<AcDbAttribute>(ids.attributes[i])->position());
    const AcGeVector3d delta(600, 900, 0); AcGeMatrix3d matrix; matrix.setToTranslation(delta);
    {
        auto block = open<AcDbBlockReference>(ids.block, AcDb::kForWrite);
        check(block->transformBy(matrix), "move synthetic INSERT exactly once");
    }
    int fixed = 0;
    for (int i = 0; i < ids.attributes.length(); ++i) {
        auto attribute = open<AcDbAttribute>(ids.attributes[i], AcDb::kForWrite);
        const auto expected = before[static_cast<std::size_t>(i)] + delta;
        if (!standardSamePoint(attribute->position(), expected)) {
            if (!standardSamePoint(attribute->position(), before[static_cast<std::size_t>(i)]))
                throw std::runtime_error("attribute moved by an unexpected partial or duplicate transform");
            check(attribute->transformBy(matrix), "move only attribute not already moved by INSERT"); ++fixed;
        }
        if (!standardSamePoint(attribute->position(), expected)) throw std::runtime_error("native attribute final position does not equal one block move");
        if (attribute->ownerId() != ids.block) throw std::runtime_error("native attribute owner is not its actual INSERT");
    }
    return fixed;
}

std::string standardPolylineState(AcDbObjectId id) {
    auto polyline = open<AcDbPolyline>(id); double area = 0;
    check(polyline->getArea(area), "read persisted synthetic polyline area"); AcGePoint3dArray vertices;
    for (unsigned int i = 0; i < polyline->numVerts(); ++i) { AcGePoint3d p; check(polyline->getPointAt(i, p), "read actual synthetic polyline vertex"); vertices.append(p); }
    std::ostringstream out;
    out << "{\"handle\":" << quote(handleOf(id)) << ",\"closed\":" << (polyline->isClosed() ? "true" : "false")
        << ",\"vertices\":" << polyline->numVerts() << ",\"area\":" << number(std::abs(area)) << ",\"points\":" << points(vertices) << '}';
    return out.str();
}

std::string standardState(const StandardFixtureIds& ids) {
    std::ostringstream out;
    out << "{\"rectangle\":" << standardPolylineState(ids.rectangle) << ",\"outer\":" << standardPolylineState(ids.outer);
    {
        auto hatch = open<AcDbHatch>(ids.hatch); AcDbObjectIdArray boundaries;
        check(hatch->getAssocObjIds(boundaries), "read actual native hatch boundary references");
        double area = 0; check(hatch->getArea(area), "read actual evaluated synthetic hatch area");
        if (std::abs(area - 18560000.0) > 1e-4) throw std::runtime_error("synthetic hatch did not retain the actual inner hole area");
        out << ",\"hatch\":{\"handle\":" << quote(handleOf(ids.hatch)) << ",\"associative\":" << (hatch->associative() ? "true" : "false")
            << ",\"loops\":" << hatch->numLoops() << ",\"area\":" << number(area) << ",\"boundaries\":[";
        for (int i = 0; i < boundaries.length(); ++i) { if (i) out << ','; out << quote(handleOf(boundaries[i])); }
        out << "]}";
    }
    {
        auto text = open<AcDbText>(ids.text); AcString value; check(text->textString(value), "read actual native rotated text");
        out << ",\"text\":{\"handle\":" << quote(handleOf(ids.text)) << ",\"value\":" << quote(value.kACharPtr())
            << ",\"height\":" << number(text->height()) << ",\"rotation\":" << number(text->rotation()) << ",\"position\":" << point(text->position()) << '}';
    }
    {
        auto text = open<AcDbMText>(ids.mtext); AcString value; check(text->contents(value), "read actual native pending MText");
        out << ",\"mtext\":{\"handle\":" << quote(handleOf(ids.mtext)) << ",\"value\":" << quote(value.kACharPtr())
            << ",\"position\":" << point(text->location()) << '}';
    }
    {
        auto leader = open<AcDbLeader>(ids.leader); AcGePoint3dArray vertices;
        for (int i = 0; i < leader->numVertices(); ++i) vertices.append(leader->vertexAt(i));
        const auto annotation = leader->annotationObjId();
        out << ",\"leader\":{\"handle\":" << quote(handleOf(ids.leader)) << ",\"vertices\":" << leader->numVertices()
            << ",\"annotation\":" << (annotation.isNull() ? "null" : quote(handleOf(annotation))) << ",\"points\":" << points(vertices) << '}';
    }
    {
        auto block = open<AcDbBlockReference>(ids.block);
        out << ",\"block\":{\"handle\":" << quote(handleOf(ids.block)) << ",\"definition\":" << quote(handleOf(block->blockTableRecord()))
            << ",\"position\":" << point(block->position()) << '}';
    }
    out << ",\"attributes\":" << standardAttributes(ids.attributes)
        << ",\"issue\":{\"id\":\"S1_SLOPE\",\"marker\":" << quote(handleOf(ids.mtext))
        << ",\"nativeXData\":" << quote(standardIssueXData(ids.mtext)) << "}}";
    return out.str();
}

void nativeStandardGeometryProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\standard-geometry-native.json"); requireNewFile(report);
        const auto output = fullPath(p.output + L"\\standard-geometry-generated.dwg");
        std::string error = "null", saveStatus = "null", reopenStatus = "null", closeInputStatus = "null";
        std::string originalBefore = "[]", originalAfter = "[]", originalSaved = "[]", attributesBeforeMove = "[]", after = "{}", saved = "{}";
        int initialCount = 0, generatedCount = 0, reopenedCount = 0, attributeMoveFixups = 0;
        try {
            if (!below(output, p.output)) throw std::runtime_error("standard geometry output outside isolated run");
            requireNewFile(output); auto target = std::make_unique<AcDbDatabase>(false, true);
            check(target->readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read complete frozen scheme for synthetic fixture");
            check(target->closeInput(true), "close synthetic fixture frozen scheme input");
            target->disableUndoRecording(false); StandardFixtureIds ids; std::vector<std::wstring> originals;
            {
                WorkingDatabaseScope working(*target); const auto originalIds = modelIds(*target); initialCount = originalIds.length();
                if (initialCount != 213) throw std::runtime_error("synthetic fixture must retain all 213 original scheme entities");
                originalBefore = familyCompactArray(originalIds);
                for (int i = 0; i < originalIds.length(); ++i) originals.push_back(handleOf(originalIds[i]));
                ids.rectangle = standardAppend(modelId(*target), standardRectangle(*target, 50000, 20000, 6000, 4000));
                ids.outer = standardOuterOffset(*target, ids.rectangle);
                auto hatch = std::make_unique<AcDbHatch>(); standardDefaults(*hatch, *target);
                check(hatch->setPattern(AcDbHatch::kPreDefined, L"SOLID"), "set explicit synthetic SOLID pattern");
                check(hatch->setHatchStyle(AcDbHatch::kNormal), "set synthetic hatch island style");
                ids.hatch = standardAppend(modelId(*target), std::move(hatch));
                {
                    auto resident = open<AcDbHatch>(ids.hatch, AcDb::kForWrite); check(resident->setAssociative(true), "enable real hatch association after posting");
                    AcDbObjectIdArray outer; outer.append(ids.outer); AcDbObjectIdArray inner; inner.append(ids.rectangle);
                    check(resident->appendLoop(AcDbHatch::kExternal, outer), "associate actual outer boundary entity");
                    check(resident->appendLoop(AcDbHatch::kDefault, inner), "associate actual inner hole boundary entity");
                }
                {
                    auto resident = open<AcDbHatch>(ids.hatch, AcDb::kForWrite);
                    check(resident->evaluateHatch(), "evaluate posted associative synthetic hatch");
                }
                auto text = std::make_unique<AcDbText>(); standardDefaults(*text, *target);
                check(text->setTextString(L"C09235"), "set explicit non-round code text fixture");
                check(text->setHeight(350), "set synthetic native text height");
                check(text->setRotation(3.14159265358979323846 / 2.0), "set synthetic native text rotation");
                check(text->setPosition(AcGePoint3d(59000, 20000, 0)), "set native text fixture position");
                ids.text = standardAppend(modelId(*target), std::move(text));
                auto marker = std::make_unique<AcDbMText>(); standardDefaults(*marker, *target);
                check(marker->setLocation(AcGePoint3d(61000, 23000, 0)), "set pending marker fixture position");
                check(marker->setTextHeight(350), "set pending marker fixture height"); check(marker->setWidth(5000), "set pending marker fixture width");
                marker->setContents(L"待确认\\PS1_SLOPE");
                AcString markerReadback; check(marker->contents(markerReadback), "read native pending contents after setting");
                const std::wstring actualMarker(markerReadback.kACharPtr());
                if (actualMarker.find(L"待确认") == std::wstring::npos || actualMarker.find(L"S1_SLOPE") == std::wstring::npos)
                    throw std::runtime_error("native MText did not retain explicit pending text");
                ids.mtext = standardAppend(modelId(*target), std::move(marker));
                standardRegisterIssueApp(*target); standardSetIssueXData(ids.mtext);
                auto leader = std::make_unique<AcDbLeader>(); standardDefaults(*leader, *target);
                for (const auto& vertex : {AcGePoint3d(56000, 24000, 0), AcGePoint3d(59000, 23000, 0), AcGePoint3d(61000, 23000, 0)})
                    if (!leader->appendVertex(vertex)) throw std::runtime_error("native leader rejected explicit synthetic vertex");
                ids.leader = standardAppend(modelId(*target), std::move(leader));
                {
                    auto resident = open<AcDbLeader>(ids.leader, AcDb::kForWrite);
                    check(resident->attachAnnotation(ids.mtext), "attach actual native MText annotation to leader");
                    check(resident->evaluateLeader(), "evaluate native leader after real annotation attachment");
                }
                standardCreateBlock(*target, ids); attributesBeforeMove = standardAttributes(ids.attributes);
                attributeMoveFixups = standardMoveBlockOnce(ids);
                after = standardState(ids); originalAfter = familyCompactArray(originalIds); generatedCount = modelCount(*target);
                requireNewFile(output); const auto stored = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT);
                saveStatus = std::to_string(stored); check(stored, "save explicit synthetic native capability fixture");
            }
            // Only handles survive this boundary; no target object pointer is reused.
            std::vector<std::wstring> generatedHandles;
            for (const auto& id : {ids.rectangle, ids.outer, ids.hatch, ids.text, ids.mtext, ids.leader, ids.block}) generatedHandles.push_back(handleOf(id));
            std::vector<std::wstring> attributeHandles;
            for (int i = 0; i < ids.attributes.length(); ++i) attributeHandles.push_back(handleOf(ids.attributes[i]));
            target.reset(); AcDbDatabase reopened(false, true);
            const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
            reopenStatus = std::to_string(loaded); check(loaded, "reopen synthetic capability fixture in new native database");
            const auto closed = reopened.closeInput(true); closeInputStatus = std::to_string(closed); check(closed, "close reopened synthetic input");
            {
                WorkingDatabaseScope working(reopened); AcDbObjectIdArray originalsReopened;
                for (const auto& handle : originals) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find unchanged original after synthetic reopen"); originalsReopened.append(id); }
                StandardFixtureIds persisted; AcDbObjectId* fields[] = {&persisted.rectangle, &persisted.outer, &persisted.hatch, &persisted.text, &persisted.mtext, &persisted.leader, &persisted.block};
                for (std::size_t i = 0; i < generatedHandles.size(); ++i) check(reopened.getAcDbObjectId(*fields[i], false, AcDbHandle(generatedHandles[i].c_str())), "find actual generated entity after new database reopen");
                for (const auto& handle : attributeHandles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find actual INSERT attribute after reopen"); persisted.attributes.append(id); }
                saved = standardState(persisted); originalSaved = familyCompactArray(originalsReopened); reopenedCount = modelCount(reopened);
            }
        } catch (const std::exception& failure) { error = familyError(failure); }
        catch (...) { error = quote(L"unknown synthetic standard-geometry C++ exception"); }
        std::ostringstream out;
        out << "{\"contract\":\"stage1.standard-geometry-native.v1\",\"api\":\"ObjectARX native; no COM\",\"error\":" << error
            << ",\"outputDWG\":" << quote(output) << ",\"schemeInputPath\":" << quote(p.scheme)
            << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeInputStatus
            << ",\"initialSchemeCount\":" << initialCount << ",\"generatedModelCount\":" << generatedCount << ",\"reopenedModelCount\":" << reopenedCount
            << ",\"attributesBeforeMove\":" << attributesBeforeMove << ",\"attributeMoveFixups\":" << attributeMoveFixups
            << ",\"after\":" << after << ",\"saved\":" << saved << ",\"originalBefore\":" << originalBefore
            << ",\"originalAfter\":" << originalAfter << ",\"originalSaved\":" << originalSaved
            << ",\"scope\":\"explicit synthetic native geometry, association, attribute and issue-marker mechanics only; no Tianzheng replacement, project parameters or final drawing styles\"}";
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_STANDARD_GEOMETRY_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_STANDARD_GEOMETRY_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_STANDARD_GEOMETRY_FAILED: unknown C++ exception\n"); }
}
