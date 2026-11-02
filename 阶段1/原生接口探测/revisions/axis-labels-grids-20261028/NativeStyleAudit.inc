// Included by NativeAccessProbe.cpp inside its anonymous namespace.
// The including translation unit must include dbAnnotationScale.h at file scope.
// This audit opens frozen input databases for reading and writes only a new JSON report.

const char* styleAuditBool(bool value) { return value ? "true" : "false"; }
std::string styleAuditText(const ACHAR* value) { return value ? quote(value) : "null"; }
std::string styleAuditError(const std::exception& failure) {
    const std::string message(failure.what());
    return quote(std::wstring(message.begin(), message.end()));
}

std::string styleAuditReference(AcDbObjectId id) {
    if (id.isNull()) return "null";
    AcDbObject* raw{};
    const auto status = acdbOpenObject(raw, id, AcDb::kForRead);
    Opened<AcDbObject> object(raw);
    std::ostringstream out;
    out << "{\"handle\":" << quote(handleOf(id)) << ",\"openStatus\":" << status;
    if (status != Acad::eOk || !object) {
        out << ",\"name\":null,\"type\":null,\"nameStatus\":null}";
        return out.str();
    }
    out << ",\"type\":" << quote(object->isA()->name());
    const auto* symbol = AcDbSymbolTableRecord::cast(object.get());
    if (symbol) {
        AcString name;
        const auto nameStatus = symbol->getName(name);
        out << ",\"nameStatus\":" << nameStatus << ",\"name\":"
            << (nameStatus == Acad::eOk ? quote(name.kACharPtr()) : "null");
    } else {
        // Dictionary-resident objects need not have a symbol-table name.
        out << ",\"nameStatus\":null,\"name\":null";
    }
    out << '}';
    return out.str();
}

std::string styleAuditIdentityFields(const AcDbSymbolTableRecord& record) {
    AcString name;
    const auto nameStatus = record.getName(name);
    std::ostringstream out;
    out << "\"handle\":" << quote(handleOf(record.objectId()))
        << ",\"type\":" << quote(record.isA()->name())
        << ",\"nameStatus\":" << nameStatus << ",\"name\":"
        << (nameStatus == Acad::eOk ? quote(name.kACharPtr()) : "null")
        << ",\"dependent\":" << styleAuditBool(record.isDependent())
        << ",\"resolved\":" << styleAuditBool(record.isResolved())
        << ",\"renamable\":" << styleAuditBool(record.isRenamable());
    return out.str();
}

std::string styleAuditColor(const AcCmColor& color) {
    std::ostringstream out;
    out << "{\"colorMethod\":" << static_cast<int>(color.colorMethod())
        << ",\"colorIndex\":" << color.colorIndex()
        << ",\"colorValue\":" << color.color()
        << ",\"red\":" << static_cast<unsigned int>(color.red())
        << ",\"green\":" << static_cast<unsigned int>(color.green())
        << ",\"blue\":" << static_cast<unsigned int>(color.blue())
        << ",\"isByColor\":" << styleAuditBool(color.isByColor())
        << ",\"isByACI\":" << styleAuditBool(color.isByACI())
        << ",\"isByLayer\":" << styleAuditBool(color.isByLayer())
        << ",\"isByBlock\":" << styleAuditBool(color.isByBlock())
        << ",\"isByPen\":" << styleAuditBool(color.isByPen())
        << ",\"isForeground\":" << styleAuditBool(color.isForeground())
        << ",\"isNone\":" << styleAuditBool(color.isNone())
        << ",\"hasColorName\":" << styleAuditBool(color.hasColorName())
        << ",\"hasBookName\":" << styleAuditBool(color.hasBookName())
        << ",\"colorName\":" << styleAuditText(color.colorName())
        << ",\"bookName\":" << styleAuditText(color.bookName()) << '}';
    return out.str();
}

std::string styleAuditTransparency(const AcCmTransparency& transparency) {
    std::ostringstream out;
    out << "{\"serialized\":" << transparency.serializeOut()
        << ",\"alpha\":" << static_cast<unsigned int>(transparency.alpha())
        << ",\"alphaPercent\":" << number(transparency.alphaPercent())
        << ",\"isByAlpha\":" << styleAuditBool(transparency.isByAlpha())
        << ",\"isByLayer\":" << styleAuditBool(transparency.isByLayer())
        << ",\"isByBlock\":" << styleAuditBool(transparency.isByBlock())
        << ",\"isInvalid\":" << styleAuditBool(transparency.isInvalid())
        << ",\"isClear\":" << styleAuditBool(transparency.isClear())
        << ",\"isSolid\":" << styleAuditBool(transparency.isSolid()) << '}';
    return out.str();
}

std::string styleAuditValue(bool value) { return styleAuditBool(value); }
std::string styleAuditValue(int value) { return std::to_string(value); }
std::string styleAuditValue(double value) { return number(value); }
std::string styleAuditValue(ACHAR value) { return quote(std::wstring(1, value)); }
std::string styleAuditValue(const ACHAR* value) { return styleAuditText(value); }
std::string styleAuditValue(AcDbObjectId value) { return styleAuditReference(value); }
std::string styleAuditValue(const AcCmColor& value) { return styleAuditColor(value); }
std::string styleAuditValue(AcDb::LineWeight value) { return std::to_string(static_cast<int>(value)); }

void styleAuditDimField(std::ostringstream& out, bool& first, const char* name, const std::string& value) {
    if (!first) out << ',';
    first = false;
    out << '"' << name << "\":" << value;
}

template<class T> std::string styleAuditDimValues(const T& record) {
    // Exhaustive getter list from this machine's ObjectARX 2020 inc/dbdimvar.h.
    // Overloads above preserve ObjectId/color/string/character/bool/numeric types.
    std::ostringstream out;
    out << '{';
    bool first = true;
#define STAGE1_STYLE_DIM_GETTER(getter) styleAuditDimField(out, first, #getter, styleAuditValue(record.getter()))
    STAGE1_STYLE_DIM_GETTER(dimadec);
    STAGE1_STYLE_DIM_GETTER(dimalt);
    STAGE1_STYLE_DIM_GETTER(dimaltd);
    STAGE1_STYLE_DIM_GETTER(dimaltf);
    STAGE1_STYLE_DIM_GETTER(dimaltrnd);
    STAGE1_STYLE_DIM_GETTER(dimalttd);
    STAGE1_STYLE_DIM_GETTER(dimalttz);
    STAGE1_STYLE_DIM_GETTER(dimaltu);
    STAGE1_STYLE_DIM_GETTER(dimaltz);
    STAGE1_STYLE_DIM_GETTER(dimapost);
    STAGE1_STYLE_DIM_GETTER(dimarcsym);
    STAGE1_STYLE_DIM_GETTER(dimasz);
    STAGE1_STYLE_DIM_GETTER(dimatfit);
    STAGE1_STYLE_DIM_GETTER(dimaunit);
    STAGE1_STYLE_DIM_GETTER(dimazin);
    STAGE1_STYLE_DIM_GETTER(dimblk);
    STAGE1_STYLE_DIM_GETTER(dimblk1);
    STAGE1_STYLE_DIM_GETTER(dimblk2);
    STAGE1_STYLE_DIM_GETTER(dimcen);
    STAGE1_STYLE_DIM_GETTER(dimclrd);
    STAGE1_STYLE_DIM_GETTER(dimclre);
    STAGE1_STYLE_DIM_GETTER(dimclrt);
    STAGE1_STYLE_DIM_GETTER(dimdec);
    STAGE1_STYLE_DIM_GETTER(dimdle);
    STAGE1_STYLE_DIM_GETTER(dimdli);
    STAGE1_STYLE_DIM_GETTER(dimdsep);
    STAGE1_STYLE_DIM_GETTER(dimexe);
    STAGE1_STYLE_DIM_GETTER(dimexo);
    STAGE1_STYLE_DIM_GETTER(dimfrac);
    STAGE1_STYLE_DIM_GETTER(dimgap);
    STAGE1_STYLE_DIM_GETTER(dimjogang);
    STAGE1_STYLE_DIM_GETTER(dimjust);
    STAGE1_STYLE_DIM_GETTER(dimldrblk);
    STAGE1_STYLE_DIM_GETTER(dimlfac);
    STAGE1_STYLE_DIM_GETTER(dimlim);
    STAGE1_STYLE_DIM_GETTER(dimltex1);
    STAGE1_STYLE_DIM_GETTER(dimltex2);
    STAGE1_STYLE_DIM_GETTER(dimltype);
    STAGE1_STYLE_DIM_GETTER(dimlunit);
    STAGE1_STYLE_DIM_GETTER(dimlwd);
    STAGE1_STYLE_DIM_GETTER(dimlwe);
    STAGE1_STYLE_DIM_GETTER(dimpost);
    STAGE1_STYLE_DIM_GETTER(dimrnd);
    STAGE1_STYLE_DIM_GETTER(dimsah);
    STAGE1_STYLE_DIM_GETTER(dimscale);
    STAGE1_STYLE_DIM_GETTER(dimsd1);
    STAGE1_STYLE_DIM_GETTER(dimsd2);
    STAGE1_STYLE_DIM_GETTER(dimse1);
    STAGE1_STYLE_DIM_GETTER(dimse2);
    STAGE1_STYLE_DIM_GETTER(dimsoxd);
    STAGE1_STYLE_DIM_GETTER(dimtad);
    STAGE1_STYLE_DIM_GETTER(dimtdec);
    STAGE1_STYLE_DIM_GETTER(dimtfac);
    STAGE1_STYLE_DIM_GETTER(dimtfill);
    STAGE1_STYLE_DIM_GETTER(dimtfillclr);
    STAGE1_STYLE_DIM_GETTER(dimtih);
    STAGE1_STYLE_DIM_GETTER(dimtix);
    STAGE1_STYLE_DIM_GETTER(dimtm);
    STAGE1_STYLE_DIM_GETTER(dimtmove);
    STAGE1_STYLE_DIM_GETTER(dimtofl);
    STAGE1_STYLE_DIM_GETTER(dimtoh);
    STAGE1_STYLE_DIM_GETTER(dimtol);
    STAGE1_STYLE_DIM_GETTER(dimtolj);
    STAGE1_STYLE_DIM_GETTER(dimtp);
    STAGE1_STYLE_DIM_GETTER(dimtsz);
    STAGE1_STYLE_DIM_GETTER(dimtvp);
    STAGE1_STYLE_DIM_GETTER(dimtxsty);
    STAGE1_STYLE_DIM_GETTER(dimtxt);
    STAGE1_STYLE_DIM_GETTER(dimtzin);
    STAGE1_STYLE_DIM_GETTER(dimupt);
    STAGE1_STYLE_DIM_GETTER(dimzin);
    STAGE1_STYLE_DIM_GETTER(dimfxlenOn);
    STAGE1_STYLE_DIM_GETTER(dimfxlen);
    STAGE1_STYLE_DIM_GETTER(dimtxtdirection);
    STAGE1_STYLE_DIM_GETTER(dimmzf);
    STAGE1_STYLE_DIM_GETTER(dimmzs);
    STAGE1_STYLE_DIM_GETTER(dimaltmzf);
    STAGE1_STYLE_DIM_GETTER(dimaltmzs);
#undef STAGE1_STYLE_DIM_GETTER
    out << '}';
    return out.str();
}

std::string styleAuditFontFile(AcDbDatabase& db, const AcString& fileName,
                              Acad::ErrorStatus getterStatus, bool bigFont) {
    const std::wstring requested(fileName.kACharPtr());
    std::ostringstream out;
    out << "{\"requested\":" << (getterStatus == Acad::eOk ? quote(requested) : "null")
        << ",\"getterStatus\":" << getterStatus;
    if (getterStatus != Acad::eOk || requested.empty()) {
        out << ",\"state\":" << quote(getterStatus == Acad::eOk ? L"NotReferenced" : L"Unknown")
            << ",\"findFileStatus\":null,\"hint\":null,\"resolvedPath\":null,\"fileExists\":null"
            << ",\"fontInstallationVerified\":false}";
        return out.str();
    }
    auto hint = AcDbHostApplicationServices::kFontFile;
    const auto extensionPosition = requested.find_last_of(L'.');
    const auto extension = extensionPosition == std::wstring::npos ? std::wstring() : requested.substr(extensionPosition);
    if (bigFont || !_wcsicmp(extension.c_str(), L".shx")) hint = AcDbHostApplicationServices::kCompiledShapeFile;
    else if (!_wcsicmp(extension.c_str(), L".ttf") || !_wcsicmp(extension.c_str(), L".ttc") || !_wcsicmp(extension.c_str(), L".otf"))
        hint = AcDbHostApplicationServices::kTrueTypeFontFile;
    AcString resolved;
    const auto status = acdbHostApplicationServices()->findFile(resolved, requested.c_str(), &db, hint);
    const bool hasPath = status == Acad::eOk && resolved.kACharPtr()[0] != 0;
    const auto attributes = hasPath ? GetFileAttributesW(resolved.kACharPtr()) : INVALID_FILE_ATTRIBUTES;
    const bool exists = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    out << ",\"state\":" << quote(exists ? L"Found" : L"Unknown")
        << ",\"findFileStatus\":" << status << ",\"hint\":" << static_cast<int>(hint)
        << ",\"resolvedPath\":" << (hasPath ? quote(resolved.kACharPtr()) : "null")
        << ",\"fileExists\":" << styleAuditBool(exists)
        << ",\"fontInstallationVerified\":false}";
    return out.str();
}

std::string styleAuditTextStyle(AcDbObjectId id) {
    auto record = open<AcDbTextStyleTableRecord>(id);
    AcString fileName, bigFontFileName, typeface;
    const auto fileStatus = record->fileName(fileName);
    const auto bigFontStatus = record->bigFontFileName(bigFontFileName);
    bool bold = false, italic = false;
    Charset charset{};
    Autodesk::AutoCAD::PAL::FontUtils::FontPitch pitch{};
    Autodesk::AutoCAD::PAL::FontUtils::FontFamily family{};
    const auto fontStatus = record->font(typeface, bold, italic, charset, pitch, family);
    std::ostringstream out;
    out << '{' << styleAuditIdentityFields(*record)
        << ",\"fileNameStatus\":" << fileStatus << ",\"fileName\":"
        << (fileStatus == Acad::eOk ? quote(fileName.kACharPtr()) : "null")
        << ",\"bigFontFileNameStatus\":" << bigFontStatus << ",\"bigFontFileName\":"
        << (bigFontStatus == Acad::eOk ? quote(bigFontFileName.kACharPtr()) : "null")
        << ",\"font\":{\"status\":" << fontStatus
        << ",\"typeface\":" << (fontStatus == Acad::eOk ? quote(typeface.kACharPtr()) : "null")
        << ",\"bold\":" << (fontStatus == Acad::eOk ? styleAuditBool(bold) : "null")
        << ",\"italic\":" << (fontStatus == Acad::eOk ? styleAuditBool(italic) : "null")
        << ",\"charset\":" << (fontStatus == Acad::eOk ? std::to_string(static_cast<int>(charset)) : "null")
        << ",\"pitch\":" << (fontStatus == Acad::eOk ? std::to_string(static_cast<int>(pitch)) : "null")
        << ",\"family\":" << (fontStatus == Acad::eOk ? std::to_string(static_cast<int>(family)) : "null")
        << ",\"fontInstallationVerified\":false}"
        << ",\"textSize\":" << number(record->textSize())
        << ",\"xScale\":" << number(record->xScale())
        << ",\"obliquingAngle\":" << number(record->obliquingAngle())
        << ",\"flagBits\":" << static_cast<unsigned int>(record->flagBits())
        << ",\"priorSize\":" << number(record->priorSize())
        << ",\"isVertical\":" << styleAuditBool(record->isVertical() != 0)
        << ",\"isShapeFile\":" << styleAuditBool(record->isShapeFile() != 0)
        << ",\"fontFiles\":{\"primary\":" << styleAuditFontFile(*record->database(), fileName, fileStatus, false)
        << ",\"bigFont\":" << styleAuditFontFile(*record->database(), bigFontFileName, bigFontStatus, true) << "}}";
    return out.str();
}

std::string styleAuditLinetype(AcDbObjectId id) {
    auto record = open<AcDbLinetypeTableRecord>(id);
    AcString comments;
    const auto commentsStatus = record->comments(comments);
    const auto dashCount = record->numDashes();
    if (dashCount < 0 || dashCount > 1000000) throw std::runtime_error("invalid linetype dash count");
    std::ostringstream out;
    out << '{' << styleAuditIdentityFields(*record)
        << ",\"commentsStatus\":" << commentsStatus << ",\"comments\":"
        << (commentsStatus == Acad::eOk ? quote(comments.kACharPtr()) : "null")
        << ",\"patternLength\":" << number(record->patternLength())
        << ",\"isScaledToFit\":" << styleAuditBool(record->isScaledToFit())
        << ",\"numDashes\":" << dashCount << ",\"dashes\":[";
    for (int index = 0; index < dashCount; ++index) {
        if (index) out << ',';
        const auto offset = record->shapeOffsetAt(index);
        AcString text;
        const auto textStatus = record->textAt(index, text);
        out << "{\"index\":" << index << ",\"dashLength\":" << number(record->dashLengthAt(index))
            << ",\"shapeStyle\":" << styleAuditReference(record->shapeStyleAt(index))
            << ",\"shapeNumber\":" << record->shapeNumberAt(index)
            << ",\"shapeOffset\":[" << number(offset.x) << ',' << number(offset.y) << ']'
            << ",\"shapeScale\":" << number(record->shapeScaleAt(index))
            << ",\"shapeRotation\":" << number(record->shapeRotationAt(index))
            << ",\"shapeIsUcsOriented\":" << styleAuditBool(record->shapeIsUcsOrientedAt(index))
            << ",\"shapeIsUpright\":" << styleAuditBool(record->shapeIsUprightAt(index))
            << ",\"textAtStatus\":" << textStatus << ",\"text\":"
            << (textStatus == Acad::eOk ? quote(text.kACharPtr()) : "null") << '}';
    }
    out << "]}";
    return out.str();
}

std::string styleAuditDimStyle(AcDbObjectId id) {
    auto record = open<AcDbDimStyleTableRecord>(id);
    std::ostringstream out;
    out << '{' << styleAuditIdentityFields(*record)
        << ",\"values\":" << styleAuditDimValues(*record)
        << ",\"getterCount\":78,\"deprecated\":{\"dimfit\":" << record->dimfit()
        << ",\"dimunit\":" << record->dimunit() << '}'
        << ",\"isModifiedForRecompute\":" << styleAuditBool(record->isModifiedForRecompute()) << '}';
    return out.str();
}

std::string styleAuditLayer(AcDbObjectId id) {
    auto record = open<AcDbLayerTableRecord>(id);
    AcString description, plotStyleName;
    const auto descriptionStatus = record->description(description);
    const auto plotStyleNameStatus = record->plotStyleName(plotStyleName);
    std::ostringstream out;
    out << '{' << styleAuditIdentityFields(*record)
        << ",\"isOff\":" << styleAuditBool(record->isOff())
        << ",\"isFrozen\":" << styleAuditBool(record->isFrozen())
        << ",\"isHidden\":" << styleAuditBool(record->isHidden())
        << ",\"isLocked\":" << styleAuditBool(record->isLocked())
        << ",\"VPDFLT\":" << styleAuditBool(record->VPDFLT())
        << ",\"isPlottable\":" << styleAuditBool(record->isPlottable())
        << ",\"isInUse\":" << styleAuditBool(record->isInUse())
        << ",\"isReconciled\":" << styleAuditBool(record->isReconciled())
        << ",\"hasAnyOverrides\":" << styleAuditBool(record->hasAnyOverrides())
        << ",\"color\":" << styleAuditColor(record->color())
        << ",\"transparency\":" << styleAuditTransparency(record->transparency())
        << ",\"linetype\":" << styleAuditReference(record->linetypeObjectId())
        << ",\"material\":" << styleAuditReference(record->materialId())
        << ",\"lineWeight\":" << static_cast<int>(record->lineWeight())
        << ",\"plotStyleNameStatus\":" << plotStyleNameStatus << ",\"plotStyleName\":"
        << (plotStyleNameStatus == Acad::eOk ? quote(plotStyleName.kACharPtr()) : "null")
        << ",\"plotStyle\":" << styleAuditReference(record->plotStyleNameId())
        << ",\"descriptionStatus\":" << descriptionStatus << ",\"description\":"
        << (descriptionStatus == Acad::eOk ? quote(description.kACharPtr()) : "null") << '}';
    return out.str();
}

std::string styleAuditAnnotationScale(AcDbDatabase& db) {
    // cannoscale() returns a caller-owned, non-database-resident copy.
    std::unique_ptr<AcDbAnnotationScale> scale(db.cannoscale());
    if (!scale) return "null";
    AcString name;
    double paperUnits = 0, drawingUnits = 0, factor = 0;
    bool temporary = false;
    const auto nameStatus = scale->getName(name);
    const auto paperStatus = scale->getPaperUnits(paperUnits);
    const auto drawingStatus = scale->getDrawingUnits(drawingUnits);
    const auto factorStatus = scale->getScale(factor);
    const auto temporaryStatus = scale->getIsTemporaryScale(temporary);
    std::ostringstream out;
    out << "{\"nameStatus\":" << nameStatus << ",\"name\":"
        << (nameStatus == Acad::eOk ? quote(name.kACharPtr()) : "null")
        << ",\"type\":" << quote(scale->isA()->name())
        << ",\"collectionName\":" << quote(scale->collectionName().kACharPtr())
        << ",\"paperUnitsStatus\":" << paperStatus << ",\"paperUnits\":"
        << (paperStatus == Acad::eOk ? number(paperUnits) : "null")
        << ",\"drawingUnitsStatus\":" << drawingStatus << ",\"drawingUnits\":"
        << (drawingStatus == Acad::eOk ? number(drawingUnits) : "null")
        << ",\"scaleStatus\":" << factorStatus << ",\"scale\":"
        << (factorStatus == Acad::eOk ? number(factor) : "null")
        << ",\"temporaryStatus\":" << temporaryStatus << ",\"isTemporary\":"
        << (temporaryStatus == Acad::eOk ? styleAuditBool(temporary) : "null") << '}';
    // uniqueIdentifier() is intentionally omitted: it is not a DWG handle.
    return out.str();
}

std::string styleAuditDatabaseSettings(AcDbDatabase& db) {
    std::ostringstream out;
    out << "{\"ltscale\":" << number(db.ltscale())
        << ",\"celtscale\":" << number(db.celtscale())
        << ",\"psltscale\":" << styleAuditBool(db.psltscale())
        << ",\"msltscale\":" << styleAuditBool(db.msltscale())
        << ",\"cmlscale\":" << number(db.cmlscale())
        << ",\"mleaderscale\":" << number(db.mleaderscale())
        << ",\"cannoscale\":" << styleAuditAnnotationScale(db)
        << ",\"annotationScaleListEnumerated\":false"
        << ",\"annoAllVisible\":" << styleAuditBool(db.annoAllVisible())
        << ",\"annotativeDwg\":" << styleAuditBool(db.annotativeDwg())
        << ",\"insunits\":" << static_cast<int>(db.insunits())
        << ",\"measurement\":" << static_cast<int>(db.measurement())
        << ",\"lunits\":" << db.lunits() << ",\"luprec\":" << db.luprec()
        << ",\"aunits\":" << db.aunits() << ",\"auprec\":" << db.auprec()
        << ",\"textsize\":" << number(db.textsize())
        << ",\"celweight\":" << static_cast<int>(db.celweight())
        << ",\"textstyle\":" << styleAuditReference(db.textstyle())
        << ",\"dimstyle\":" << styleAuditReference(db.dimstyle())
        << ",\"celtype\":" << styleAuditReference(db.celtype())
        << ",\"clayer\":" << styleAuditReference(db.clayer())
        << ",\"dimensionVariables\":" << styleAuditDimValues(db) << '}';
    return out.str();
}

struct StyleAuditTableResult {
    std::string records{"[]"};
    std::string error{"null"};
    int count{0};
    bool complete{false};
};

StyleAuditTableResult styleAuditTable(AcDbObjectId tableId, std::string (*reader)(AcDbObjectId)) {
    StyleAuditTableResult result;
    std::ostringstream records;
    records << '[';
    try {
        auto table = open<AcDbSymbolTable>(tableId);
        AcDbSymbolTableIterator* raw{};
        check(table->newIterator(raw), "create style audit symbol iterator");
        std::unique_ptr<AcDbSymbolTableIterator> iterator(raw);
        bool recordFailure = false;
        for (; !iterator->done(); iterator->step()) {
            AcDbObjectId id;
            check(iterator->getRecordId(id), "get style audit record id");
            if (result.count) records << ',';
            try {
                records << reader(id);
            } catch (const std::exception& failure) {
                recordFailure = true;
                records << "{\"handle\":" << quote(handleOf(id)) << ",\"name\":null,\"type\":null,\"error\":"
                    << styleAuditError(failure) << '}';
            }
            ++result.count;
        }
        result.complete = !recordFailure;
        if (recordFailure) result.error = quote(L"One or more symbol records could not be fully read");
    } catch (const std::exception& failure) {
        result.error = styleAuditError(failure);
    }
    records << ']';
    result.records = records.str();
    return result;
}

std::string styleAuditSource(const wchar_t* role, const std::wstring& input) {
    AcDbDatabase sourceDB(false, true);
    const auto readStatus = sourceDB.readDwgFile(input.c_str(), AcDbDatabase::kForReadAndAllShare, false);
    std::string closeInputStatus = "null", error = "null", databaseSettings = "null";
    StyleAuditTableResult textStyles, linetypes, dimStyles, layers;
    try {
        check(readStatus, "read frozen style audit input");
        const auto closeStatus = sourceDB.closeInput(true);
        closeInputStatus = std::to_string(closeStatus);
        check(closeStatus, "close frozen style audit input");
        WorkingDatabaseScope working(sourceDB);
        textStyles = styleAuditTable(sourceDB.textStyleTableId(), styleAuditTextStyle);
        linetypes = styleAuditTable(sourceDB.linetypeTableId(), styleAuditLinetype);
        dimStyles = styleAuditTable(sourceDB.dimStyleTableId(), styleAuditDimStyle);
        layers = styleAuditTable(sourceDB.layerTableId(), styleAuditLayer);
        databaseSettings = styleAuditDatabaseSettings(sourceDB);
        if (!textStyles.complete || !linetypes.complete || !dimStyles.complete || !layers.complete)
            error = quote(L"One or more resource tables could not be fully enumerated");
    } catch (const std::exception& failure) {
        error = styleAuditError(failure);
    }
    std::ostringstream out;
    out << "{\"role\":" << quote(role) << ",\"path\":" << quote(input)
        << ",\"readOnly\":true,\"readStatus\":" << readStatus << ",\"closeInputStatus\":" << closeInputStatus
        << ",\"status\":" << quote(error == "null" ? L"Complete" : L"Incomplete")
        << ",\"error\":" << error << ",\"databaseSettings\":" << databaseSettings
        << ",\"textStyles\":" << textStyles.records << ",\"linetypes\":" << linetypes.records
        << ",\"dimStyles\":" << dimStyles.records << ",\"layers\":" << layers.records
        << ",\"counts\":{\"textStyles\":" << textStyles.count << ",\"linetypes\":" << linetypes.count
        << ",\"dimStyles\":" << dimStyles.count << ",\"layers\":" << layers.count << '}'
        << ",\"tableErrors\":{\"textStyles\":" << textStyles.error << ",\"linetypes\":" << linetypes.error
        << ",\"dimStyles\":" << dimStyles.error << ",\"layers\":" << layers.error << "}}";
    return out.str();
}

void nativeStyleAudit() {
    try {
        const auto p = paths();
        const auto report = fullPath(p.output + L"\\native-styles.json");
        requireNewFile(report);
        const auto scheme = styleAuditSource(L"scheme", p.scheme);
        const auto reference = styleAuditSource(L"reference", p.reference);
        writeNewReport(report,
            "{\"contract\":\"stage1.native-styles.v1\",\"api\":\"ObjectARX native; no COM\","
            "\"scope\":\"resourceTablesOnly\",\"entityOverridesVerified\":false,\"drawingWritten\":false,"
            "\"sdkDimensionGetterCount\":78,\"sources\":[" + scheme + "," + reference + "],"
            "\"limitations\":[\"Resource table definitions and database header defaults only\","
            "\"Entity local overrides, viewport overrides and rendering are not verified\","
            "\"TDbDimension2 is not assumed to derive from AcDbDimension\","
            "\"Resolved font paths do not establish actual glyph availability or rendered appearance\"]}");
        acutPrintf(L"\nSTAGE1_NATIVE_STYLES_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) {
        acutPrintf(L"\nSTAGE1_NATIVE_STYLES_FAILED: %hs\n", failure.what());
    } catch (...) {
        acutPrintf(L"\nSTAGE1_NATIVE_STYLES_FAILED: unknown C++ exception\n");
    }
}
