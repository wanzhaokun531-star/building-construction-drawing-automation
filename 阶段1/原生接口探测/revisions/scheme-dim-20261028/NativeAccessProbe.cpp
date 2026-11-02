#include <Windows.h>
#include <aced.h>
#include <dbapserv.h>
#include <dbcurve.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbtrans.h>
#include <rxmember.h>
#include <rxprop.h>
#include <rxvalue.h>
#include <rxregsvc.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
HMODULE moduleHandle{};
void check(Acad::ErrorStatus status, const char* operation) {
    if (status != Acad::eOk) throw std::runtime_error(std::string(operation) + ": " + std::to_string(status));
}
std::wstring fullPath(const std::wstring& input) {
    std::vector<wchar_t> buffer(32768);
    const auto length = GetFullPathNameW(input.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (!length || length >= buffer.size()) throw std::runtime_error("invalid full path");
    return std::wstring(buffer.data(), length);
}
std::wstring parent(const std::wstring& path) { return path.substr(0, path.find_last_of(L"\\/")); }
bool below(const std::wstring& path, const std::wstring& directory) {
    const auto prefix = fullPath(directory) + L"\\";
    return path.size() > prefix.size() && !_wcsnicmp(path.c_str(), prefix.c_str(), prefix.size());
}
std::string utf8(const std::wstring& input) {
    if (input.empty()) return {};
    const auto length = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (!length) throw std::runtime_error("UTF-8 conversion failed");
    std::string value(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), &value[0], length, nullptr, nullptr);
    return value;
}
std::string quote(const std::wstring& value) {
    std::ostringstream out; out << '"';
    for (const unsigned char character : utf8(value)) {
        if (character == '"' || character == '\\') out << '\\' << character;
        else if (character < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(character) << std::dec;
        else out << character;
    }
    out << '"'; return out.str();
}
std::string number(double value) {
    if (!std::isfinite(value)) return "null";
    std::ostringstream out; out << std::setprecision(17) << value; return out.str();
}
std::string point(const AcGePoint3d& value) { return "[" + number(value.x) + "," + number(value.y) + "," + number(value.z) + "]"; }
std::string points(const AcGePoint3dArray& values) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < values.length(); ++i) { if (i) out << ','; out << point(values[i]); }
    out << ']'; return out.str();
}
std::string integers(const AcDbIntArray& values) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < values.length(); ++i) { if (i) out << ','; out << values[i]; }
    out << ']'; return out.str();
}
struct CloseObject { void operator()(AcDbObject* object) const { if (object) object->close(); } };
template<class T> using Opened = std::unique_ptr<T, CloseObject>;
template<class T> Opened<T> open(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
    T* object{}; check(acdbOpenObject(object, id, mode), "open object"); return Opened<T>(object);
}
class WorkingDatabaseScope {
public:
    explicit WorkingDatabaseScope(AcDbDatabase& db) : services_(acdbHostApplicationServices()), previous_(services_->workingDatabase()) { services_->setWorkingDatabase(&db); }
    ~WorkingDatabaseScope() { services_->setWorkingDatabase(previous_); }
    WorkingDatabaseScope(const WorkingDatabaseScope&) = delete;
    WorkingDatabaseScope& operator=(const WorkingDatabaseScope&) = delete;
private:
    AcDbHostApplicationServices* services_;
    AcDbDatabase* previous_;
};
AcDbObjectId modelId(AcDbDatabase& db) {
    AcDbBlockTable* raw{}; check(db.getBlockTable(raw, AcDb::kForRead), "get block table");
    Opened<AcDbBlockTable> table(raw); AcDbObjectId id;
    check(table->getAt(ACDB_MODEL_SPACE, id), "get model space"); return id;
}
int modelCount(AcDbDatabase& db) {
    auto model = open<AcDbBlockTableRecord>(modelId(db));
    AcDbBlockTableRecordIterator* raw{}; check(model->newIterator(raw), "create model iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw); int count = 0;
    for (; !iterator->done(); iterator->step()) ++count;
    return count;
}
AcDbObjectIdArray modelIds(AcDbDatabase& db) {
    auto model = open<AcDbBlockTableRecord>(modelId(db));
    AcDbBlockTableRecordIterator* raw{}; check(model->newIterator(raw), "create clone model iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw); AcDbObjectIdArray ids;
    for (; !iterator->done(); iterator->step()) {
        AcDbObjectId id; check(iterator->getEntityId(id), "get clone entity id"); ids.append(id);
    }
    return ids;
}
class TransactionScope {
public:
    explicit TransactionScope(AcDbDatabase& db) : manager_(db.transactionManager()) {
        if (!manager_) throw std::runtime_error("native exercise transaction manager absent");
        transaction_ = manager_->startTransaction();
        if (!transaction_) throw std::runtime_error("native exercise transaction start failed");
    }
    ~TransactionScope() { if (active_) manager_->abortTransaction(); }
    AcTransaction* get() const { return transaction_; }
    Acad::ErrorStatus finish(bool commit) {
        const auto status = commit ? manager_->endTransaction() : manager_->abortTransaction();
        if (status == Acad::eOk) active_ = false;
        return status;
    }
    TransactionScope(const TransactionScope&) = delete;
    TransactionScope& operator=(const TransactionScope&) = delete;
private:
    AcDbTransactionManager* manager_{};
    AcTransaction* transaction_{};
    bool active_{true};
};
std::wstring handleOf(AcDbObjectId id) {
    const auto handle = id.handle(); wchar_t text[32]{}; handle.getIntoAsciiBuffer(text); return text;
}
std::string propertyText(const AcRxValue& value, int& formatStatus) {
    const auto required = value.toString(nullptr, 0);
    formatStatus = required;
    if (required < 0 || required > 1048576) return "null";
    std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1, 0);
    formatStatus = value.toString(buffer.data(), buffer.size());
    if (formatStatus < 0) return "null";
    return quote(buffer.data());
}
std::string memberJson(AcRxMember& member, const AcDbEntity& entity) {
    std::ostringstream out;
    out << "{\"name\":" << quote(member.name()) << ",\"valueType\":" << quote(member.type().name())
        << ",\"kind\":" << quote(member.isA()->name());
    if (const auto* property = AcRxPropertyBase::cast(&member)) out << ",\"readOnly\":" << (property->isReadOnly(&entity) ? "true" : "false");
    else out << ",\"readOnly\":null";
    const auto* scalar = AcRxProperty::cast(&member);
    const auto* collection = AcRxCollectionProperty::cast(&member);
    if (scalar) {
        AcRxValue value; const auto status = scalar->getValue(&entity, value);
        int formatStatus = -1;
        const auto text = status == Acad::eOk ? propertyText(value, formatStatus) : "null";
        out << ",\"getStatus\":" << status << ",\"valueText\":" << text
            << ",\"valueTextStatus\":" << formatStatus << ",\"actualValueType\":" << quote(value.type().name());
    } else out << ",\"getStatus\":null,\"valueText\":null,\"valueTextStatus\":null,\"actualValueType\":null";
    if (collection) {
        int count = -1; const auto status = collection->tryGetCount(&entity, count);
        out << ",\"collectionCountStatus\":" << status << ",\"collectionCount\":"
            << ((status == Acad::eOk && count >= 0) ? std::to_string(count) : "null");
    } else out << ",\"collectionCountStatus\":null,\"collectionCount\":null";
    out << '}'; return out.str();
}
std::string curveJson(const AcDbEntity& entity) {
    const auto* curve = AcDbCurve::cast(&entity);
    if (!curve) return "null";
    AcGePoint3d start, end; double endParam = 0, length = 0;
    const auto startStatus = curve->getStartPoint(start);
    const auto endStatus = curve->getEndPoint(end);
    const auto parameterStatus = curve->getEndParam(endParam);
    std::string distanceStatus = "null", distance = "null";
    if (parameterStatus == Acad::eOk) {
        const auto status = curve->getDistAtParam(endParam, length);
        distanceStatus = std::to_string(status);
        if (status == Acad::eOk) distance = number(length);
    }
    std::ostringstream out;
    out << "{\"startStatus\":" << startStatus << ",\"endStatus\":" << endStatus
        << ",\"endParamStatus\":" << parameterStatus << ",\"distanceStatus\":" << distanceStatus
        << ",\"start\":" << (startStatus == Acad::eOk ? point(start) : "null")
        << ",\"end\":" << (endStatus == Acad::eOk ? point(end) : "null")
        << ",\"endParam\":" << (parameterStatus == Acad::eOk ? number(endParam) : "null")
        << ",\"length\":" << distance << '}';
    return out.str();
}
std::string scanEntity(AcDbObjectId id) {
    auto entity = open<AcDbEntity>(id);
    AcString layer; check(entity->layer(layer), "get layer");
    AcDbExtents bounds; const auto extentStatus = entity->getGeomExtents(bounds);
    auto* engine = AcRxMemberQueryEngine::theEngine();
    std::unique_ptr<AcRxMemberIterator> iterator(engine ? engine->newMemberIterator(entity.get()) : nullptr);
    std::ostringstream members; int count = 0;
    if (iterator) {
        for (; !iterator->done(); iterator->next()) {
            auto* member = iterator->current();
            if (!member) throw std::runtime_error("native member iterator returned a null member");
            if (count) members << ',';
            members << memberJson(*member, *entity); ++count;
            if (count > 10000) throw std::runtime_error("native member enumeration exceeded finite probe limit");
        }
    }
    AcGePoint3dArray gripPoints, stretchPoints; AcDbIntArray osnapModes, geomIds;
    const auto gripStatus = entity->getGripPoints(gripPoints, osnapModes, geomIds);
    const auto stretchStatus = entity->getStretchPoints(stretchPoints);
    std::ostringstream out;
    out << "{\"handle\":" << quote(handleOf(id)) << ",\"type\":" << quote(entity->isA()->name())
        << ",\"layer\":" << quote(layer.kACharPtr()) << ",\"proxy\":" << (entity->isAProxy() ? "true" : "false")
        << ",\"extentStatus\":" << extentStatus << ",\"min\":" << (extentStatus == Acad::eOk ? point(bounds.minPoint()) : "null")
        << ",\"max\":" << (extentStatus == Acad::eOk ? point(bounds.maxPoint()) : "null")
        << ",\"curve\":" << curveJson(*entity)
        << ",\"memberIteratorAvailable\":" << (iterator ? "true" : "false") << ",\"memberCount\":" << count
        << ",\"members\":[" << members.str() << "],\"grip\":{\"status\":" << gripStatus
        << ",\"points\":" << points(gripPoints) << ",\"osnapModes\":" << integers(osnapModes)
        << ",\"geomIds\":" << integers(geomIds) << "},\"stretch\":{\"status\":" << stretchStatus
        << ",\"points\":" << points(stretchPoints) << "}}";
    return out.str();
}
struct Paths { std::wstring root, scheme, reference, output; };
Paths paths() {
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(moduleHandle, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) throw std::runtime_error("module path unavailable");
    std::wstring ini(buffer.data(), length); const auto extension = ini.find_last_of(L'.');
    ini = ini.substr(0, extension) + L".ini";
    const auto read = [&](const wchar_t* key) {
        std::vector<wchar_t> value(32768);
        GetPrivateProfileStringW(L"probe", key, L"", value.data(), static_cast<DWORD>(value.size()), ini.c_str());
        if (!value[0]) throw std::runtime_error("missing native probe INI value");
        return fullPath(value.data());
    };
    Paths p{read(L"root"), read(L"scheme"), read(L"reference"), read(L"output")};
    const auto snapshot = parent(p.root) + L"\\阶段0\\输入快照";
    if (!below(p.scheme, snapshot) || !below(p.reference, snapshot) || !below(p.output, p.root + L"\\runs"))
        throw std::runtime_error("native probe path outside isolated input/output directories");
    const auto attributes = GetFileAttributesW(p.output.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("output directory absent");
    return p;
}
std::string scanSource(const wchar_t* source, const std::wstring& path, const std::vector<const wchar_t*>& handles) {
    AcDbDatabase db(false, true);
    const auto readStatus = db.readDwgFile(path.c_str(), AcDbDatabase::kForReadAndAllShare, false);
    check(readStatus, "read native scan source");
    const auto closeStatus = db.closeInput(true); check(closeStatus, "close native scan source input");
    db.disableUndoRecording(false); WorkingDatabaseScope working(db);
    std::ostringstream out;
    out << "{\"source\":" << quote(source) << ",\"path\":" << quote(path) << ",\"readStatus\":" << readStatus
        << ",\"closeInputStatus\":" << closeStatus << ",\"modelCount\":" << modelCount(db) << ",\"samples\":[";
    for (std::size_t i = 0; i < handles.size(); ++i) {
        AcDbObjectId id; check(db.getAcDbObjectId(id, false, AcDbHandle(handles[i])), "find native scan sample");
        if (i) out << ','; out << scanEntity(id);
    }
    out << "]}"; return out.str();
}
std::string nativeControl() {
    AcDbDatabase db(true, true); db.disableUndoRecording(false); WorkingDatabaseScope working(db);
    AcDbObjectId id;
    {
        auto model = open<AcDbBlockTableRecord>(modelId(db), AcDb::kForWrite);
        auto line = std::make_unique<AcDbLine>(AcGePoint3d(0, 0, 0), AcGePoint3d(1000, 0, 0));
        line->setDatabaseDefaults(&db); check(model->appendAcDbEntity(id, line.get()), "append temporary native control");
        auto* resident = line.release(); check(resident->close(), "close temporary native control");
    }
    return scanEntity(id);
}
void writeNewReport(const std::wstring& path, const std::string& text) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) throw std::runtime_error("native scan output already exists; select a new run directory");
    FILE* file{}; if (_wfopen_s(&file, path.c_str(), L"wbx") || !file) throw std::runtime_error("cannot create new native scan report");
    const auto written = fwrite(text.data(), 1, text.size(), file); const auto closeStatus = fclose(file);
    if (written != text.size() || closeStatus) throw std::runtime_error("native scan report write incomplete");
}
void nativeScan() {
    try {
        const auto p = paths();
        const auto scheme = scanSource(L"scheme", p.scheme, {L"CD83",L"CD84",L"CD89",L"CDC0"});
        const auto reference = scanSource(L"reference", p.reference, {L"19A0",L"199F",L"19F3",L"17DF"});
        const auto control = nativeControl();
        writeNewReport(p.output + L"\\native-access.json", "{\"contract\":\"stage1.native-access.v1\",\"api\":\"ObjectARX native; no COM\",\"sdkMajor\":23,\"sdkMinor\":1,\"mscVer\":"
            + std::to_string(_MSC_VER) + ",\"sources\":[" + scheme + "," + reference + "],\"nativeControl\":" + control + "}");
        acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_SCAN_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& error) { acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_SCAN_FAILED: %hs\n", error.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_SCAN_FAILED: unknown C++ exception\n"); }
}
void requireNewFile(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) throw std::runtime_error("native exercise file already exists; select a new run directory");
    const auto error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw std::runtime_error("cannot establish new native exercise file path");
}
struct ExerciseFixture {
    const wchar_t* id;
    const wchar_t* sourceHandle;
    const wchar_t* type;
    int index;
    AcGeVector3d offset;
    const wchar_t* mode{L"grip"};
};
std::string exerciseCase(const Paths& p, const ExerciseFixture& fixture) {
    const auto output = fullPath(p.output + L"\\" + fixture.id + L".dwg");
    if (!below(output, p.output)) throw std::runtime_error("native exercise output outside run directory");
    std::string cloneStatus = "null", moveStatus = "null", moveOpenStatus = "null", moveTransactionStatus = "null";
    std::string selectionQueryStatus = "null", selectionPoint = "null";
    std::string saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string before = "null", after = "null", saved = "null";
    int sourceCount = 0, mappedCount = 0, reopenedModelCount = 0;
    std::wstring targetHandle;
    try {
        requireNewFile(output);
        AcDbDatabase source(false, true);
        check(source.readDwgFile(p.reference.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read native exercise reference");
        check(source.closeInput(true), "close native exercise reference input");
        AcDbObjectId sourceSample;
        check(source.getAcDbObjectId(sourceSample, false, AcDbHandle(fixture.sourceHandle)), "find native exercise source sample");
        {
            auto entity = open<AcDbEntity>(sourceSample);
            if (_wcsicmp(entity->isA()->name(), fixture.type)) throw std::runtime_error("native exercise source type does not match fixture");
        }
        auto target = std::make_unique<AcDbDatabase>(true, true); target->disableUndoRecording(false);
        {
            WorkingDatabaseScope working(*target);
            const auto ids = modelIds(source); sourceCount = ids.length();
            AcDbIdMapping mapping; check(mapping.setDestDb(target.get()), "set native clone destination");
            const auto cloneResult = source.wblockCloneObjects(ids, modelId(*target), mapping, AcDb::kDrcMangleName, false);
            cloneStatus = std::to_string(cloneResult); check(cloneResult, "clone native exercise cohort");
            for (int i = 0; i < ids.length(); ++i) {
                AcDbIdPair pair; pair.setKey(ids[i]);
                if (mapping.compute(pair) && pair.isCloned() && !pair.value().isNull()) ++mappedCount;
            }
            if (mappedCount != sourceCount) throw std::runtime_error("native exercise did not clone all top-level entities");
            AcDbIdPair selected; selected.setKey(sourceSample);
            if (!mapping.compute(selected) || !selected.isCloned() || selected.value().isNull()) throw std::runtime_error("native exercise sample clone mapping absent");
            const auto targetSample = selected.value(); targetHandle = handleOf(targetSample);
            before = scanEntity(targetSample);
            {
                TransactionScope transaction(*target); AcDbObject* object{};
                const auto openResult = transaction.get()->getObject(object, targetSample, AcDb::kForWrite);
                moveOpenStatus = std::to_string(openResult); check(openResult, "open native grip sample for write");
                auto* entity = AcDbEntity::cast(object);
                if (!entity) throw std::runtime_error("native grip sample is not an entity");
                const bool stretchMode = !_wcsicmp(fixture.mode, L"stretch");
                if (!stretchMode && _wcsicmp(fixture.mode, L"grip")) throw std::runtime_error("unsupported native exercise mode");
                AcGePoint3dArray selectable; AcDbIntArray modes, geometries;
                const auto queryResult = stretchMode
                    ? entity->getStretchPoints(selectable)
                    : entity->getGripPoints(selectable, modes, geometries);
                selectionQueryStatus = std::to_string(queryResult); check(queryResult, "query current native exercise points");
                if (fixture.index < 0 || fixture.index >= selectable.length()) throw std::runtime_error("native exercise index is not in the current point array");
                selectionPoint = point(selectable[fixture.index]);
                AcDbIntArray indices; indices.append(fixture.index);
                const auto moveResult = stretchMode
                    ? entity->moveStretchPointsAt(indices, fixture.offset)
                    : entity->moveGripPointsAt(indices, fixture.offset);
                moveStatus = std::to_string(moveResult);
                const auto finishResult = transaction.finish(moveResult == Acad::eOk);
                moveTransactionStatus = std::to_string(finishResult); check(finishResult, "finish native grip transaction");
            }
            after = scanEntity(targetSample);
            requireNewFile(output);
            const auto saveResult = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT);
            saveStatus = std::to_string(saveResult); check(saveResult, "save native grip exercise");
        }
        // No target database or target object pointer is retained for the persisted observation.
        target.reset();
        AcDbDatabase reopened(false, true);
        const auto reopenResult = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        reopenStatus = std::to_string(reopenResult); check(reopenResult, "reopen native grip exercise");
        const auto closeResult = reopened.closeInput(true);
        closeInputStatus = std::to_string(closeResult); check(closeResult, "close reopened native grip input");
        {
            WorkingDatabaseScope working(reopened); reopenedModelCount = modelCount(reopened);
            AcDbObjectId savedSample; check(reopened.getAcDbObjectId(savedSample, false, AcDbHandle(targetHandle.c_str())), "find persisted native grip sample");
            saved = scanEntity(savedSample);
        }
    } catch (const std::exception& failure) {
        const std::string text(failure.what()); error = quote(std::wstring(text.begin(), text.end()));
    } catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream out;
    out << "{\"id\":" << quote(fixture.id) << ",\"sourceHandle\":" << quote(fixture.sourceHandle)
        << ",\"type\":" << quote(fixture.type) << ",\"index\":" << fixture.index
        << ",\"mode\":" << quote(fixture.mode)
        << ",\"offset\":[" << number(fixture.offset.x) << ',' << number(fixture.offset.y) << ',' << number(fixture.offset.z)
        << "],\"cloneStatus\":" << cloneStatus << ",\"sourceCount\":" << sourceCount << ",\"mappedCount\":" << mappedCount
        << ",\"moveStatus\":" << moveStatus << ",\"moveOpenStatus\":" << moveOpenStatus
        << ",\"selectionQueryStatus\":" << selectionQueryStatus << ",\"selectionPoint\":" << selectionPoint
        << ",\"moveTransactionStatus\":" << moveTransactionStatus << ",\"saveStatus\":" << saveStatus
        << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeInputStatus
        << ",\"reopenedModelCount\":" << reopenedModelCount << ",\"targetHandle\":" << quote(targetHandle)
        << ",\"before\":" << before << ",\"after\":" << after << ",\"saved\":" << saved
        << ",\"outputDWG\":" << quote(output) << ",\"error\":" << error
        << ",\"semanticStatus\":\"selected native point only; business meaning and association not established\"}";
    return out.str();
}
void nativeExercise() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\native-exercise.json"); requireNewFile(report);
        const ExerciseFixture dimension{L"dimension-grip-0", L"19A0", L"TDbDimension2", 0, AcGeVector3d(0, -200, 0)};
        const ExerciseFixture axis{L"axis-grip-1", L"199F", L"TDbAxisLabelSet", 1, AcGeVector3d(0, 200, 0)};
        const ExerciseFixture dimensionGrip3{L"dimension-measure-grip-3", L"19A0", L"TDbDimension2", 3, AcGeVector3d(500, 0, 0)};
        const auto dimensionResult = exerciseCase(p, dimension); const auto axisResult = exerciseCase(p, axis);
        const auto dimensionGrip3Result = exerciseCase(p, dimensionGrip3);
        writeNewReport(report, "{\"contract\":\"stage1.native-exercise.v1\",\"api\":\"ObjectARX native; no COM\",\"cases\":[" + dimensionResult + "," + axisResult + "," + dimensionGrip3Result + "]}");
        acutPrintf(L"\nSTAGE1_NATIVE_EXERCISE_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& error) { acutPrintf(L"\nSTAGE1_NATIVE_EXERCISE_FAILED: %hs\n", error.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_EXERCISE_FAILED: unknown C++ exception\n"); }
}
void nativeAxisAlternatives() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\native-axis-alt.json"); requireNewFile(report);
        const ExerciseFixture grip{L"axis-grip-2", L"199F", L"TDbAxisLabelSet", 2, AcGeVector3d(0, 200, 0), L"grip"};
        const ExerciseFixture stretch{L"axis-stretch-1", L"199F", L"TDbAxisLabelSet", 1, AcGeVector3d(0, 200, 0), L"stretch"};
        const auto gripResult = exerciseCase(p, grip); const auto stretchResult = exerciseCase(p, stretch);
        writeNewReport(report, "{\"contract\":\"stage1.native-axis-alt.v1\",\"api\":\"ObjectARX native; no COM\",\"cases\":[" + gripResult + "," + stretchResult + "]}");
        acutPrintf(L"\nSTAGE1_NATIVE_AXIS_ALTERNATIVES_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& error) { acutPrintf(L"\nSTAGE1_NATIVE_AXIS_ALTERNATIVES_FAILED: %hs\n", error.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_AXIS_ALTERNATIVES_FAILED: unknown C++ exception\n"); }
}
struct SchemeMapping {
    std::wstring sourceHandle, targetHandle, type;
};
std::string schemeMapJson(const std::vector<SchemeMapping>& values) {
    std::ostringstream out; out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << "{\"sourceHandle\":" << quote(values[i].sourceHandle) << ",\"targetHandle\":" << quote(values[i].targetHandle)
            << ",\"type\":" << quote(values[i].type) << '}';
    }
    out << ']'; return out.str();
}
std::string scanEntityArray(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) { if (i) out << ','; out << scanEntity(ids[i]); }
    out << ']'; return out.str();
}
struct SchemeGripStep {
    int index{};
    std::string queryStatus{"null"}, moveStatus{"null"}, transactionStatus{"null"};
    std::string currentPoint{"null"}, targetPoint{"null"}, offset{"null"}, after{"null"};
    bool succeeded{};
};
SchemeGripStep moveSchemeGripTo(AcDbDatabase& db, AcDbObjectId id, int index, const AcGePoint3d& destination) {
    SchemeGripStep step; step.index = index; step.targetPoint = point(destination);
    {
        TransactionScope transaction(db); AcDbObject* object{};
        check(transaction.get()->getObject(object, id, AcDb::kForWrite), "open scheme dimension grip");
        auto* entity = AcDbEntity::cast(object);
        if (!entity) throw std::runtime_error("scheme grip target is not an entity");
        AcGePoint3dArray grips; AcDbIntArray snap, geom;
        const auto query = entity->getGripPoints(grips, snap, geom); step.queryStatus = std::to_string(query);
        if (query == Acad::eOk && index >= 0 && index < grips.length()) {
            const auto delta = destination - grips[index];
            step.currentPoint = point(grips[index]); step.offset = point(AcGePoint3d(delta.x, delta.y, delta.z));
            AcDbIntArray indices; indices.append(index);
            const auto moved = entity->moveGripPointsAt(indices, delta); step.moveStatus = std::to_string(moved);
            const auto completed = transaction.finish(moved == Acad::eOk); step.transactionStatus = std::to_string(completed);
            step.succeeded = moved == Acad::eOk && completed == Acad::eOk;
        } else {
            const auto aborted = transaction.finish(false); step.transactionStatus = std::to_string(aborted);
        }
    }
    step.after = scanEntity(id); return step;
}
std::string schemeStepJson(const SchemeGripStep& step) {
    std::ostringstream out;
    out << "{\"gripIndex\":" << step.index << ",\"queryStatus\":" << step.queryStatus
        << ",\"moveStatus\":" << step.moveStatus << ",\"transactionStatus\":" << step.transactionStatus
        << ",\"currentPoint\":" << step.currentPoint << ",\"targetPoint\":" << step.targetPoint
        << ",\"offset\":" << step.offset << ",\"after\":" << step.after << '}';
    return out.str();
}
AcGePoint3d currentSchemeGrip(AcDbObjectId id, int index) {
    auto entity = open<AcDbEntity>(id); AcGePoint3dArray grips; AcDbIntArray snap, geom;
    check(entity->getGripPoints(grips, snap, geom), "read current scheme grip");
    if (index < 0 || index >= grips.length()) throw std::runtime_error("scheme grip index absent in current array");
    return grips[index];
}
std::string schemeCase(const Paths& p, bool addTemplate) {
    const wchar_t* caseId = addTemplate ? L"scheme-added-dimension" : L"scheme-existing-dimension";
    const auto output = fullPath(p.output + L"\\" + caseId + L".dwg");
    if (!below(output, p.output)) throw std::runtime_error("scheme verification output outside run");
    std::string loadCopyStatus = "null", copyInputCloseStatus = "null", templateCloneStatus = "null", moveStatus = "null", saveStatus = "null";
    std::string reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string sourceGeometry = "[]", savedOriginalGeometry = "[]", before = "null", after = "null", saved = "null", wallControl = "null";
    std::string minimumDimensionY = "null", proposedDimensionLineY = "null";
    std::vector<SchemeMapping> sourceMap; std::vector<SchemeGripStep> steps;
    int sourceCount = 0, mappedCount = 0, reopenedModelCount = 0, addedObjectCount = 0;
    std::wstring selectedTargetHandle;
    try {
        requireNewFile(output);
        AcDbDatabase source(false, true);
        check(source.readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read frozen scheme");
        check(source.closeInput(true), "close frozen scheme input");
        AcDbObjectIdArray sourceIds;
        AcGePoint3d wallStart, wallEnd, left, right; double wallLength = 0, minimumY = std::numeric_limits<double>::infinity();
        {
            WorkingDatabaseScope working(source); sourceIds = modelIds(source); sourceCount = sourceIds.length();
            sourceGeometry = scanEntityArray(sourceIds);
            if (addTemplate) {
                AcDbObjectId wallId; check(source.getAcDbObjectId(wallId, false, AcDbHandle(L"CD83")), "find scheme wall control");
                auto wall = open<AcDbEntity>(wallId); const auto* curve = AcDbCurve::cast(wall.get());
                if (!curve || _wcsicmp(wall->isA()->name(), L"TDbWall")) throw std::runtime_error("scheme wall control is not a native curve wall");
                double endParameter = 0;
                check(curve->getStartPoint(wallStart), "get scheme wall start"); check(curve->getEndPoint(wallEnd), "get scheme wall end");
                check(curve->getEndParam(endParameter), "get scheme wall end parameter"); check(curve->getDistAtParam(endParameter, wallLength), "get scheme wall length");
                left = wallStart; right = wallEnd; if (left.x > right.x) std::swap(left, right);
                wallControl = "{\"sourceHandle\":\"CD83\",\"start\":" + point(wallStart) + ",\"end\":" + point(wallEnd)
                    + ",\"sortedPoints\":[" + point(left) + "," + point(right) + "],\"length\":" + number(wallLength) + "}";
                for (int i = 0; i < sourceIds.length(); ++i) {
                    auto entity = open<AcDbEntity>(sourceIds[i]);
                    if (!_wcsicmp(entity->isA()->name(), L"TDbDimension2")) {
                        AcDbExtents extents; check(entity->getGeomExtents(extents), "get scheme dimension placement bound");
                        minimumY = std::min(minimumY, extents.minPoint().y);
                    }
                }
                if (!std::isfinite(minimumY)) throw std::runtime_error("scheme has no dimension placement bound");
                minimumDimensionY = number(minimumY); proposedDimensionLineY = number(minimumY - 1000.0);
            }
        }
        auto target = std::make_unique<AcDbDatabase>(false, true);
        const auto copyReadStatus = target->readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        loadCopyStatus = std::to_string(copyReadStatus); check(copyReadStatus, "read separate scheme working database");
        const auto copyCloseStatus = target->closeInput(true);
        copyInputCloseStatus = std::to_string(copyCloseStatus); check(copyCloseStatus, "close separate scheme working input");
        target->disableUndoRecording(false);
        {
            WorkingDatabaseScope working(*target);
            AcDbObjectId selected;
            for (int i = 0; i < sourceIds.length(); ++i) {
                const auto sourceHandle = handleOf(sourceIds[i]); AcDbObjectId targetId;
                check(target->getAcDbObjectId(targetId, false, AcDbHandle(sourceHandle.c_str())), "locate source entity in separate scheme working copy");
                auto targetEntity = open<AcDbEntity>(targetId);
                ++mappedCount; auto original = open<AcDbEntity>(sourceIds[i]);
                if (_wcsicmp(original->isA()->name(), targetEntity->isA()->name())) throw std::runtime_error("scheme working copy changed an original native type");
                SchemeMapping item{sourceHandle, handleOf(targetId), original->isA()->name()};
                if (!addTemplate && !_wcsicmp(item.sourceHandle.c_str(), L"CDE6")) selected = targetId;
                sourceMap.push_back(std::move(item));
            }
            if (mappedCount != sourceCount) throw std::runtime_error("scheme working copy handle mapping is incomplete");
            if (addTemplate) {
                AcDbDatabase reference(false, true);
                check(reference.readDwgFile(p.reference.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read reference dimension template");
                check(reference.closeInput(true), "close reference dimension template input");
                AcDbObjectId templateId; check(reference.getAcDbObjectId(templateId, false, AcDbHandle(L"19A0")), "find single dimension template");
                { auto entity = open<AcDbEntity>(templateId); if (_wcsicmp(entity->isA()->name(), L"TDbDimension2")) throw std::runtime_error("reference template is not native Tianzheng dimension"); }
                AcDbObjectIdArray oneTemplate; oneTemplate.append(templateId); AcDbIdMapping templateMapping;
                check(templateMapping.setDestDb(target.get()), "set single template destination");
                const auto templateResult = reference.wblockCloneObjects(oneTemplate, modelId(*target), templateMapping, AcDb::kDrcMangleName, false);
                templateCloneStatus = std::to_string(templateResult); check(templateResult, "clone single reference dimension template");
                addedObjectCount = modelCount(*target) - sourceCount;
                if (addedObjectCount != 1) throw std::runtime_error("single template clone imported extra model entities; no entities removed");
                AcDbIdPair pair; pair.setKey(templateId);
                if (!templateMapping.compute(pair) || !pair.isCloned() || pair.value().isNull()) throw std::runtime_error("single dimension template mapping absent");
                selected = pair.value();
            }
            if (selected.isNull()) throw std::runtime_error("selected scheme dimension absent");
            selectedTargetHandle = handleOf(selected); before = scanEntity(selected);
            if (addTemplate) {
                steps.push_back(moveSchemeGripTo(*target, selected, 2, left));
                if (!steps.back().succeeded) throw std::runtime_error("scheme dimension left measurement point update failed");
                steps.push_back(moveSchemeGripTo(*target, selected, 3, right));
                if (!steps.back().succeeded) throw std::runtime_error("scheme dimension right measurement point update failed");
                const auto current = currentSchemeGrip(selected, 0);
                steps.push_back(moveSchemeGripTo(*target, selected, 0, AcGePoint3d(current.x, minimumY - 1000.0, current.z)));
                if (!steps.back().succeeded) throw std::runtime_error("scheme dimension line placement failed");
            } else {
                const auto current = currentSchemeGrip(selected, 0);
                steps.push_back(moveSchemeGripTo(*target, selected, 0, AcGePoint3d(current.x, current.y - 200.0, current.z)));
                moveStatus = steps.back().moveStatus;
                if (!steps.back().succeeded) throw std::runtime_error("existing scheme dimension line movement failed");
            }
            after = scanEntity(selected); requireNewFile(output);
            const auto saveResult = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT);
            saveStatus = std::to_string(saveResult); check(saveResult, "save scheme verification drawing");
        }
        target.reset();
        AcDbDatabase reopened(false, true);
        const auto reopenedStatus = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        reopenStatus = std::to_string(reopenedStatus); check(reopenedStatus, "reopen scheme verification drawing");
        const auto closedStatus = reopened.closeInput(true); closeInputStatus = std::to_string(closedStatus); check(closedStatus, "close reopened scheme input");
        {
            WorkingDatabaseScope working(reopened); reopenedModelCount = modelCount(reopened); AcDbObjectIdArray originals;
            for (const auto& item : sourceMap) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(item.targetHandle.c_str())), "find original scheme entity after reopen"); originals.append(id);
            }
            savedOriginalGeometry = scanEntityArray(originals);
            AcDbObjectId selected; check(reopened.getAcDbObjectId(selected, false, AcDbHandle(selectedTargetHandle.c_str())), "find selected scheme dimension after reopen");
            saved = scanEntity(selected);
        }
    } catch (const std::exception& failure) { const std::string text(failure.what()); error = quote(std::wstring(text.begin(), text.end())); }
    catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream stepArray;
    for (std::size_t i = 0; i < steps.size(); ++i) { if (i) stepArray << ','; stepArray << schemeStepJson(steps[i]); }
    std::ostringstream out;
    out << "{\"id\":" << quote(caseId) << ",\"sourceCount\":" << sourceCount << ",\"mappedCount\":" << mappedCount
        << ",\"copyMethod\":\"readDwgFile separate working database\",\"loadCopyStatus\":" << loadCopyStatus
        << ",\"copyInputCloseStatus\":" << copyInputCloseStatus << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus
        << ",\"closeInputStatus\":" << closeInputStatus << ",\"error\":" << error << ",\"reopenedModelCount\":" << reopenedModelCount
        << ",\"outputDWG\":" << quote(output) << ",\"sourceMap\":" << schemeMapJson(sourceMap)
        << ",\"sourceGeometry\":" << sourceGeometry << ",\"savedOriginalGeometry\":" << savedOriginalGeometry
        << ",\"allowedSourceHandles\":" << (addTemplate ? "[]" : "[\"CDE6\"]")
        << ",\"before\":" << before << ",\"after\":" << after << ",\"saved\":" << saved
        << ",\"targetHandle\":" << quote(selectedTargetHandle) << ",\"sourceHandle\":" << (addTemplate ? "null" : "\"CDE6\"")
        << ",\"moveStatus\":" << moveStatus << ",\"templateHandle\":" << (addTemplate ? "\"19A0\"" : "null")
        << ",\"templateCloneStatus\":" << templateCloneStatus << ",\"addedObjectCount\":" << addedObjectCount
        << ",\"wallControl\":" << wallControl << ",\"steps\":[" << stepArray.str() << "]"
        << ",\"placement\":{\"sourceDimensionMinY\":" << minimumDimensionY << ",\"trialOffsetY\":-1000,\"targetY\":" << proposedDimensionLineY
        << ",\"basis\":\"trial fixture: 1000 model units below the minimum Y of source scheme dimension bounds; not a production spacing rule\"}}";
    return out.str();
}
void nativeSchemeVerification() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\scheme-native.json"); requireNewFile(report);
        const auto existing = schemeCase(p, false); const auto added = schemeCase(p, true);
        writeNewReport(report, "{\"contract\":\"stage1.scheme-native.v1\",\"api\":\"ObjectARX native; no COM\",\"schemeInputPath\":"
            + quote(p.scheme) + ",\"cases\":[" + existing + "," + added + "]}");
        acutPrintf(L"\nSTAGE1_SCHEME_NATIVE_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& error) { acutPrintf(L"\nSTAGE1_SCHEME_NATIVE_FAILED: %hs\n", error.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_SCHEME_NATIVE_FAILED: unknown C++ exception\n"); }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { moduleHandle = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    if (message == AcRx::kInitAppMsg) {
        acrxDynamicLinker->unlockApplication(id); acrxDynamicLinker->registerAppMDIAware(id);
        const auto scanStatus = acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1NATIVESCAN", L"S1NATIVESCAN", ACRX_CMD_MODAL, nativeScan);
        const auto exerciseStatus = scanStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1NATIVEEXERCISE", L"S1NATIVEEXERCISE", ACRX_CMD_MODAL, nativeExercise)
            : scanStatus;
        const auto axisStatus = exerciseStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISALT", L"S1AXISALT", ACRX_CMD_MODAL, nativeAxisAlternatives)
            : exerciseStatus;
        const auto schemeStatus = axisStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1SCHEMEVERIFY", L"S1SCHEMEVERIFY", ACRX_CMD_MODAL, nativeSchemeVerification)
            : axisStatus;
        if (scanStatus != Acad::eOk || exerciseStatus != Acad::eOk || axisStatus != Acad::eOk || schemeStatus != Acad::eOk) {
            acedRegCmds->removeGroup(L"STAGE1_NATIVE_ACCESS");
            acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_REGISTRATION_FAILED: scan=%d exercise=%d axis=%d scheme=%d\n", scanStatus, exerciseStatus, axisStatus, schemeStatus);
            return AcRx::kRetError;
        }
        acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_LOADED SDK=23.1 MSC=%d\n", _MSC_VER);
    } else if (message == AcRx::kUnloadAppMsg) {
        acedRegCmds->removeGroup(L"STAGE1_NATIVE_ACCESS"); acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_UNLOADED\n");
    }
    return AcRx::kRetOK;
}
