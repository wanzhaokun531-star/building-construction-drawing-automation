#include <Windows.h>
#include <aced.h>
#include <acedads.h>
#include <acedinpt.h>
#include <adscodes.h>
#include <acedCmdNF.h>
#include <acutads.h>
#include <dbapserv.h>
#include <dbAnnotationScale.h>
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
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
HMODULE moduleHandle{};
void* nativeApplicationId{};
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
std::string symbolResourceMapping(const AcDbIdMapping& mapping) {
    AcDbIdMappingIter iterator(mapping); std::ostringstream out; out << '['; bool comma = false;
    for (iterator.start(); !iterator.done(); iterator.next()) {
        AcDbIdPair pair; if (!iterator.getMap(pair)) throw std::runtime_error("cannot inspect axis resource mapping");
        if (pair.key().isNull() || pair.value().isNull()) continue;
        auto original = open<AcDbObject>(pair.key()); auto target = open<AcDbObject>(pair.value());
        const auto* sourceRecord = AcDbSymbolTableRecord::cast(original.get());
        const auto* targetRecord = AcDbSymbolTableRecord::cast(target.get());
        if (!sourceRecord || !targetRecord) continue;
        AcString sourceName, targetName;
        check(sourceRecord->getName(sourceName), "get source axis resource name");
        check(targetRecord->getName(targetName), "get target axis resource name");
        if (comma) out << ','; comma = true;
        out << "{\"type\":" << quote(original->isA()->name()) << ",\"sourceName\":" << quote(sourceName.kACharPtr())
            << ",\"targetName\":" << quote(targetName.kACharPtr()) << ",\"sourceHandle\":" << quote(handleOf(pair.key()))
            << ",\"targetHandle\":" << quote(handleOf(pair.value())) << ",\"isCloned\":" << (pair.isCloned() ? "true" : "false") << '}';
    }
    out << ']'; return out.str();
}
struct AxisWallControl {
    std::wstring handle;
    AcGePoint3d start, end;
    double axisX{}, thickness{};
};
AxisWallControl readAxisWallControl(AcDbDatabase& db, const wchar_t* handle) {
    AcDbObjectId id; check(db.getAcDbObjectId(id, false, AcDbHandle(handle)), "find scheme axis wall control");
    auto entity = open<AcDbEntity>(id); const auto* curve = AcDbCurve::cast(entity.get());
    if (!curve || _wcsicmp(entity->isA()->name(), L"TDbWall")) throw std::runtime_error("axis wall control is not a native wall curve");
    AxisWallControl control; control.handle = handle;
    check(curve->getStartPoint(control.start), "axis wall start"); check(curve->getEndPoint(control.end), "axis wall end");
    AcDbExtents bounds; check(entity->getGeomExtents(bounds), "axis wall bounds");
    control.axisX = (control.start.x + control.end.x) / 2.0;
    control.thickness = bounds.maxPoint().x - bounds.minPoint().x;
    if (std::abs(control.start.x - control.end.x) > 1e-6 || std::abs(control.start.y - control.end.y) < 1e-6
        || std::abs(control.thickness - 200.0) > 1e-6) throw std::runtime_error("axis wall fixture is not a straight vertical 200-unit wall");
    return control;
}
std::string axisWallControlsJson(const std::vector<AxisWallControl>& controls) {
    std::ostringstream out; out << '[';
    for (std::size_t i = 0; i < controls.size(); ++i) {
        if (i) out << ','; const auto& control = controls[i];
        out << "{\"sourceHandle\":" << quote(control.handle) << ",\"start\":" << point(control.start)
            << ",\"end\":" << point(control.end) << ",\"axisX\":" << number(control.axisX)
            << ",\"observedPerpendicularThickness\":" << number(control.thickness) << '}';
    }
    out << ']'; return out.str();
}
std::string moveRightAxisStretchGroup(AcDbDatabase& db, AcDbObjectId id, double targetX) {
    std::string queryStatus = "null", moveStatus = "null", transactionStatus = "null";
    AcGePoint3dArray selectedPoints, requestedPoints; AcDbIntArray indices;
    AcGeVector3d offset(0, 0, 0);
    {
        TransactionScope transaction(db); AcDbObject* object{};
        check(transaction.get()->getObject(object, id, AcDb::kForWrite), "open scheme axis stretch group");
        auto* entity = AcDbEntity::cast(object);
        if (!entity) throw std::runtime_error("axis stretch target is not an entity");
        AcGePoint3dArray current; const auto query = entity->getStretchPoints(current); queryStatus = std::to_string(query);
        if (query == Acad::eOk && !current.isEmpty()) {
            double rightX = -std::numeric_limits<double>::infinity();
            for (int i = 0; i < current.length(); ++i) rightX = std::max(rightX, current[i].x);
            offset = AcGeVector3d(targetX - rightX, 0, 0);
            for (int i = 0; i < current.length(); ++i) {
                if (std::abs(current[i].x - rightX) <= 1e-6) {
                    indices.append(i); selectedPoints.append(current[i]); requestedPoints.append(current[i] + offset);
                }
            }
            const auto moved = entity->moveStretchPointsAt(indices, offset); moveStatus = std::to_string(moved);
            const auto finished = transaction.finish(moved == Acad::eOk); transactionStatus = std::to_string(finished);
        } else { transactionStatus = std::to_string(transaction.finish(false)); }
    }
    AcGePoint3dArray observed;
    { auto entity = open<AcDbEntity>(id); check(entity->getStretchPoints(observed), "query observed scheme axis stretch group"); }
    bool reached = !requestedPoints.isEmpty();
    for (int i = 0; i < requestedPoints.length(); ++i) {
        bool found = false;
        for (int j = 0; j < observed.length(); ++j) {
            if (std::abs(requestedPoints[i].x - observed[j].x) <= 1e-6 && std::abs(requestedPoints[i].y - observed[j].y) <= 1e-6
                && std::abs(requestedPoints[i].z - observed[j].z) <= 1e-6) { found = true; break; }
        }
        reached = reached && found;
    }
    std::ostringstream out;
    out << "{\"mode\":\"stretch\",\"indices\":" << integers(indices) << ",\"queryStatus\":" << queryStatus
        << ",\"moveStatus\":" << moveStatus << ",\"transactionStatus\":" << transactionStatus
        << ",\"currentPoint\":" << (selectedPoints.isEmpty() ? "null" : point(selectedPoints[0]))
        << ",\"targetPoint\":" << (requestedPoints.isEmpty() ? "null" : point(requestedPoints[0]))
        << ",\"currentPoints\":" << points(selectedPoints) << ",\"targetPoints\":" << points(requestedPoints)
        << ",\"offset\":[" << number(offset.x) << ',' << number(offset.y) << ',' << number(offset.z)
        << "],\"actualReached\":" << (reached ? "true" : "false") << ",\"after\":" << scanEntity(id) << '}';
    return out.str();
}
std::string schemeAxisCase(const Paths& p, bool adjust) {
    const wchar_t* name = adjust ? L"scheme-axis-adjust" : L"scheme-axis-transfer";
    const auto output = fullPath(p.output + L"\\" + name + L".dwg");
    if (!below(output, p.output)) throw std::runtime_error("axis output outside isolated scheme run");
    std::string loadCopyStatus = "null", copyCloseStatus = "null", templateStatus = "null", transformStatus = "null", transformTransactionStatus = "null";
    std::string saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null", resources = "[]";
    std::string sourceGeometry = "[]", savedOriginalGeometry = "[]", templateBefore = "null", before = "null", transformed = "null", after = "null", saved = "null", steps = "[]";
    std::vector<SchemeMapping> sourceMap; std::vector<AxisWallControl> wallControls;
    int sourceCount = 0, mappedCount = 0, addedObjectCount = 0, reopenedCount = 0;
    AcGeVector3d translation(0, 0, 0); std::wstring targetHandle;
    try {
        requireNewFile(output); AcDbDatabase source(false, true);
        check(source.readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read scheme axis baseline");
        check(source.closeInput(true), "close scheme axis baseline input");
        AcDbObjectIdArray originalIds;
        {
            WorkingDatabaseScope working(source); originalIds = modelIds(source); sourceCount = originalIds.length(); sourceGeometry = scanEntityArray(originalIds);
            wallControls.push_back(readAxisWallControl(source, L"CDB4")); wallControls.push_back(readAxisWallControl(source, L"CDB0"));
            if (wallControls[0].axisX > wallControls[1].axisX) std::swap(wallControls[0], wallControls[1]);
        }
        auto target = std::make_unique<AcDbDatabase>(false, true);
        const auto loaded = target->readDwgFile(p.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        loadCopyStatus = std::to_string(loaded); check(loaded, "read separate scheme axis work copy");
        const auto closed = target->closeInput(true); copyCloseStatus = std::to_string(closed); check(closed, "close scheme axis working input"); target->disableUndoRecording(false);
        {
            WorkingDatabaseScope working(*target);
            for (int i = 0; i < originalIds.length(); ++i) {
                const auto h = handleOf(originalIds[i]); AcDbObjectId copyId; check(target->getAcDbObjectId(copyId, false, AcDbHandle(h.c_str())), "map original scheme axis work copy");
                auto original = open<AcDbEntity>(originalIds[i]); auto copy = open<AcDbEntity>(copyId);
                if (_wcsicmp(original->isA()->name(), copy->isA()->name())) throw std::runtime_error("axis work copy changed original native type");
                sourceMap.push_back(SchemeMapping{h, handleOf(copyId), original->isA()->name()}); ++mappedCount;
            }
            AcDbDatabase reference(false, true);
            check(reference.readDwgFile(p.reference.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read axis template source"); check(reference.closeInput(true), "close axis template source input");
            AcDbObjectId templateId; check(reference.getAcDbObjectId(templateId, false, AcDbHandle(L"199F")), "find native axis template");
            {
                WorkingDatabaseScope referenceWorking(reference); templateBefore = scanEntity(templateId);
                auto axis = open<AcDbEntity>(templateId);
                if (_wcsicmp(axis->isA()->name(), L"TDbAxisLabelSet")) throw std::runtime_error("axis template has unexpected native type");
            }
            AcDbObjectIdArray one; one.append(templateId); AcDbIdMapping mapping; check(mapping.setDestDb(target.get()), "set axis template clone destination");
            const auto cloned = reference.wblockCloneObjects(one, modelId(*target), mapping, AcDb::kDrcMangleName, false); templateStatus = std::to_string(cloned); check(cloned, "clone single native axis collection");
            resources = symbolResourceMapping(mapping); addedObjectCount = modelCount(*target) - sourceCount;
            if (addedObjectCount != 1) throw std::runtime_error("axis template added extra model entities; none removed");
            AcDbIdPair pair; pair.setKey(templateId);
            if (!mapping.compute(pair) || !pair.isCloned() || pair.value().isNull()) throw std::runtime_error("native axis template mapping missing");
            const auto axisId = pair.value(); targetHandle = handleOf(axisId); before = scanEntity(axisId);
            AcGePoint3dArray stretch;
            { auto axis = open<AcDbEntity>(axisId); check(axis->getStretchPoints(stretch), "find template outer-axis coordinates"); }
            if (stretch.isEmpty()) throw std::runtime_error("native axis template exposes no stretch points");
            double leftX = std::numeric_limits<double>::infinity();
            for (int i = 0; i < stretch.length(); ++i) leftX = std::min(leftX, stretch[i].x);
            translation = AcGeVector3d(wallControls[0].axisX - leftX, 0, 0);
            {
                TransactionScope transaction(*target); AcDbObject* object{}; check(transaction.get()->getObject(object, axisId, AcDb::kForWrite), "open axis collection for translation");
                auto* axis = AcDbEntity::cast(object); if (!axis) throw std::runtime_error("axis collection is not an entity");
                const auto moved = axis->transformBy(AcGeMatrix3d::translation(translation)); transformStatus = std::to_string(moved);
                const auto finished = transaction.finish(moved == Acad::eOk); transformTransactionStatus = std::to_string(finished); check(finished, "finish native axis translation"); check(moved, "translate native axis collection");
            }
            transformed = scanEntity(axisId);
            if (adjust) steps = "[" + moveRightAxisStretchGroup(*target, axisId, wallControls[1].axisX) + "]";
            after = scanEntity(axisId); requireNewFile(output);
            const auto stored = target->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save scheme axis trial");
        }
        target.reset(); AcDbDatabase reopened(false, true);
        const auto loadedSaved = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loadedSaved); check(loadedSaved, "reopen scheme axis trial");
        const auto closedSaved = reopened.closeInput(true); closeInputStatus = std::to_string(closedSaved); check(closedSaved, "close reopened scheme axis input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray originalCopies;
            for (const auto& item : sourceMap) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(item.targetHandle.c_str())), "find original scheme entity in axis output"); originalCopies.append(id); }
            savedOriginalGeometry = scanEntityArray(originalCopies);
            AcDbObjectId axis; check(reopened.getAcDbObjectId(axis, false, AcDbHandle(targetHandle.c_str())), "find persisted native axis collection"); saved = scanEntity(axis);
        }
    } catch (const std::exception& failure) { const std::string text(failure.what()); error = quote(std::wstring(text.begin(), text.end())); }
    catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream out;
    out << "{\"id\":" << quote(name) << ",\"copyMethod\":\"readDwgFile separate working database\",\"loadCopyStatus\":" << loadCopyStatus
        << ",\"copyInputCloseStatus\":" << copyCloseStatus << ",\"sourceCount\":" << sourceCount << ",\"mappedCount\":" << mappedCount
        << ",\"sourceMap\":" << schemeMapJson(sourceMap) << ",\"sourceGeometry\":" << sourceGeometry << ",\"savedOriginalGeometry\":" << savedOriginalGeometry
        << ",\"allowedSourceHandles\":[],\"templateHandle\":\"199F\",\"templateCloneStatus\":" << templateStatus << ",\"addedObjectCount\":" << addedObjectCount
        << ",\"targetHandle\":" << quote(targetHandle) << ",\"resourceMapping\":" << resources << ",\"templateBefore\":" << templateBefore
        << ",\"before\":" << before << ",\"translation\":[" << number(translation.x) << ',' << number(translation.y) << ',' << number(translation.z)
        << "],\"transformStatus\":" << transformStatus << ",\"transformTransactionStatus\":" << transformTransactionStatus
        << ",\"transformed\":" << transformed << ",\"after\":" << after << ",\"saved\":" << saved << ",\"steps\":" << steps
        << ",\"wallControls\":" << axisWallControlsJson(wallControls) << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus
        << ",\"closeInputStatus\":" << closeInputStatus << ",\"reopenedModelCount\":" << reopenedCount << ",\"outputDWG\":" << quote(output)
        << ",\"error\":" << error << ",\"scope\":\"axis template transfer and two outer wall controls only; intermediate axes, numbering, vertical extents and associations are not designed\"}";
    return out.str();
}
void nativeSchemeAxisVerification() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\scheme-axis.json"); requireNewFile(report);
        const auto transferred = schemeAxisCase(p, false); const auto adjusted = schemeAxisCase(p, true);
        writeNewReport(report, "{\"contract\":\"stage1.scheme-axis.v1\",\"api\":\"ObjectARX native; no COM\",\"schemeInputPath\":"
            + quote(p.scheme) + ",\"cases\":[" + transferred + "," + adjusted + "]}");
        acutPrintf(L"\nSTAGE1_SCHEME_AXIS_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& error) { acutPrintf(L"\nSTAGE1_SCHEME_AXIS_FAILED: %hs\n", error.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_SCHEME_AXIS_FAILED: unknown C++ exception\n"); }
}
class NativeSelectionSet {
public:
    NativeSelectionSet() : createStatus_(acedSSAdd(nullptr, nullptr, selection_)), active_(createStatus_ == RTNORM) {}
    ~NativeSelectionSet() { if (active_) acedSSFree(selection_); }
    int createStatus() const { return createStatus_; }
    int add(AcDbObjectId id) {
        ads_name entityName{}; check(acdbGetAdsName(entityName, id), "get wall command entity name");
        return acedSSAdd(entityName, selection_, selection_);
    }
    ads_name& get() { return selection_; }
    int release() {
        if (!active_) return RTERROR;
        const auto status = acedSSFree(selection_);
        if (status == RTNORM) active_ = false;
        return status;
    }
    NativeSelectionSet(const NativeSelectionSet&) = delete;
    NativeSelectionSet& operator=(const NativeSelectionSet&) = delete;
private:
    ads_name selection_{};
    int createStatus_{};
    bool active_{};
};
std::string wallAxisCase(const Paths& p) {
    const auto control = fullPath(p.output + L"\\control.dwg");
    const auto output = fullPath(p.output + L"\\wall-axis-generated.dwg");
    std::string commandStatus = "null", saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string sourceGeometry = "[]", afterOriginal = "[]", savedOriginal = "[]", added = "[]", savedAdded = "[]", controls = "[]";
    std::string selectionCreateStatus = "null", selectionFreeStatus = "null", selectionAddStatuses = "[]";
    std::vector<SchemeMapping> sourceMap;
    int initialCount = 0, sourceCount = 0, addedCount = 0, reopenedCount = 0;
    try {
        if (!below(control, p.output) || !below(output, p.output)) throw std::runtime_error("wall-axis paths outside isolated run");
        requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("wall-axis probe requires document command context");
        auto* document = acDocManager->curDocument();
        if (!document || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("current drawing is not this run's control.dwg");
        auto* database = document->database();
        if (!database || acdbHostApplicationServices()->workingDatabase() != database)
            throw std::runtime_error("current document and native working database differ");
        const ACHAR* databaseFilename{}; check(database->getFilename(databaseFilename), "get current wall-axis database filename");
        if (!databaseFilename || _wcsicmp(fullPath(databaseFilename).c_str(), control.c_str()))
            throw std::runtime_error("current native database is not this run's control.dwg");
        if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
            throw std::runtime_error("wall-axis command requires no pre-existing database transaction");
        const auto originalIds = modelIds(*database); initialCount = originalIds.length(); sourceCount = initialCount;
        if (initialCount != 213) throw std::runtime_error("wall-axis control must contain the complete 213-object scheme");
        sourceGeometry = scanEntityArray(originalIds);
        for (int i = 0; i < originalIds.length(); ++i) {
            auto entity = open<AcDbEntity>(originalIds[i]); const auto handle = handleOf(originalIds[i]);
            sourceMap.push_back(SchemeMapping{handle, handle, entity->isA()->name()});
        }
        AcDbObjectIdArray selectedWalls; std::ostringstream wallControlArray; wallControlArray << '[';
        for (const auto* handle : {L"CDB4", L"CDB0", L"CD83"}) {
            AcDbObjectId id; check(database->getAcDbObjectId(id, false, AcDbHandle(handle)), "find native wall command fixture");
            auto entity = open<AcDbEntity>(id); const auto* curve = AcDbCurve::cast(entity.get());
            if (!curve || _wcsicmp(entity->isA()->name(), L"TDbWall")) throw std::runtime_error("wall-axis selected fixture is not a native wall curve");
            AcGePoint3d start, end; check(curve->getStartPoint(start), "wall-axis control start"); check(curve->getEndPoint(end), "wall-axis control end");
            if (selectedWalls.length()) wallControlArray << ',';
            wallControlArray << "{\"sourceHandle\":" << quote(handle) << ",\"type\":" << quote(entity->isA()->name())
                << ",\"start\":" << point(start) << ",\"end\":" << point(end) << ",\"lineControlLength\":" << number(start.distanceTo(end)) << '}';
            selectedWalls.append(id);
        }
        wallControlArray << ']'; controls = wallControlArray.str();
        // Every entity opened above has closed before entering Tianzheng's document command.
        {
            NativeSelectionSet selection; selectionCreateStatus = std::to_string(selection.createStatus());
            if (selection.createStatus() != RTNORM) throw std::runtime_error("cannot create native wall selection set");
            std::ostringstream addStatuses; addStatuses << '[';
            for (int i = 0; i < selectedWalls.length(); ++i) {
                const auto status = selection.add(selectedWalls[i]);
                if (i) addStatuses << ','; addStatuses << status;
                if (status != RTNORM) throw std::runtime_error("cannot add wall to native command selection");
            }
            addStatuses << ']'; selectionAddStatuses = addStatuses.str();
            const auto status = acedCommandS(RTSTR, L"TWall2Axis", RTPICKS, selection.get(), RTSTR, L"", RTNONE);
            commandStatus = std::to_string(status); selectionFreeStatus = std::to_string(selection.release());
            if (status != RTNORM) error = quote(L"TWall2Axis did not return RTNORM; actual geometry retained for inspection");
        }
        if (acDocManager->curDocument() != document || document->database() != database
            || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("wall command changed the active document context; output not saved");
        afterOriginal = scanEntityArray(originalIds);
        const auto afterIds = modelIds(*database); AcDbObjectIdArray addedIds;
        for (int i = 0; i < afterIds.length(); ++i) {
            bool wasOriginal = false;
            for (int j = 0; j < originalIds.length(); ++j) if (afterIds[i] == originalIds[j]) { wasOriginal = true; break; }
            if (!wasOriginal) addedIds.append(afterIds[i]);
        }
        addedCount = addedIds.length(); added = scanEntityArray(addedIds);
        std::vector<std::wstring> addedHandles;
        for (int i = 0; i < addedIds.length(); ++i) addedHandles.push_back(handleOf(addedIds[i]));
        requireNewFile(output);
        const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save documented wall-axis result");
        AcDbDatabase reopened(false, true);
        const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loaded); check(loaded, "reopen documented wall-axis result");
        const auto closed = reopened.closeInput(true); closeInputStatus = std::to_string(closed); check(closed, "close reopened wall-axis input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray persistedOriginals, persistedAdded;
            for (const auto& mapping : sourceMap) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(mapping.targetHandle.c_str())), "find original in reopened wall-axis result"); persistedOriginals.append(id);
            }
            for (const auto& handle : addedHandles) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find generated axis in reopened result"); persistedAdded.append(id);
            }
            savedOriginal = scanEntityArray(persistedOriginals); savedAdded = scanEntityArray(persistedAdded);
        }
    } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
    catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream out;
    out << "{\"contract\":\"stage1.wall-axis-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandS\",\"commandName\":\"TWall2Axis\""
        << ",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output) << ",\"commandStatus\":" << commandStatus
        << ",\"initialModelCount\":" << initialCount << ",\"sourceCount\":" << sourceCount << ",\"sourceMap\":" << schemeMapJson(sourceMap)
        << ",\"sourceGeometry\":" << sourceGeometry << ",\"afterOriginalGeometry\":" << afterOriginal << ",\"savedOriginalGeometry\":" << savedOriginal
        << ",\"wallControls\":" << controls << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded << ",\"addedCount\":" << addedCount
        << ",\"reopenedModelCount\":" << reopenedCount << ",\"selectionCreateStatus\":" << selectionCreateStatus << ",\"selectionAddStatuses\":" << selectionAddStatuses
        << ",\"selectionFreeStatus\":" << selectionFreeStatus << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus
        << ",\"closeInputStatus\":" << closeInputStatus << ",\"error\":" << error
        << ",\"scope\":\"documented wall-baseline axis creation on three fixed scheme walls only; full axis design, labels and host associations are not established\"}";
    return out.str();
}
void nativeWallAxisVerification() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\wall-axis-native.json"); requireNewFile(report);
        writeNewReport(report, wallAxisCase(p)); acutPrintf(L"\nSTAGE1_WALL_AXIS_NATIVE_COMPLETED; ASSERTIONS_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_WALL_AXIS_NATIVE_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_WALL_AXIS_NATIVE_FAILED: unknown C++ exception\n"); }
}
struct ReleaseAxisDxfBuffer {
    void operator()(resbuf* buffer) const { if (buffer) acutRelRb(buffer); }
};
bool isPublicDxfStringGroup(int code) {
    return (code >= 0 && code <= 9) || code == 100 || code == 102 || code == 105
        || (code >= 300 && code <= 309) || (code >= 1000 && code <= 1003) || code == 1005;
}
std::string publicAxisDxf(AcDbObjectId id) {
    ads_name entityName{}; const auto nameStatus = acdbGetAdsName(entityName, id);
    std::unique_ptr<resbuf, ReleaseAxisDxfBuffer> chain(nameStatus == Acad::eOk ? acdbEntGet(entityName) : nullptr);
    std::ostringstream strings; strings << '['; bool comma = false, truncated = false;
    std::map<int, int> otherGroups; int count = 0;
    for (const auto* item = chain.get(); item; item = item->rbnext) {
        if (count >= 1000000) { truncated = true; break; }
        if (isPublicDxfStringGroup(item->restype)) {
            if (comma) strings << ','; comma = true;
            strings << "{\"index\":" << count << ",\"code\":" << item->restype << ",\"value\":"
                << (item->resval.rstring ? quote(item->resval.rstring) : "null") << '}';
        } else ++otherGroups[item->restype];
        ++count;
    }
    strings << ']'; std::ostringstream out;
    out << "{\"method\":\"acdbEntGet\",\"adsNameStatus\":" << nameStatus << ",\"available\":" << (chain ? "true" : "false")
        << ",\"groupCount\":" << count << ",\"stringGroups\":" << strings.str() << ",\"otherGroupCounts\":[";
    comma = false;
    for (const auto& item : otherGroups) {
        if (comma) out << ','; comma = true;
        out << "{\"code\":" << item.first << ",\"count\":" << item.second << '}';
    }
    out << "],\"truncated\":" << (truncated ? "true" : "false")
        << ",\"xdataRequested\":false,\"interpretation\":\"raw public string groups only; ordinary EntGet absence does not establish absence of XData\"}";
    return out.str();
}
std::string axisRenumberCase(const Paths& p) {
    const auto control = fullPath(p.output + L"\\control.dwg");
    const auto output = fullPath(p.output + L"\\axis-renumber-generated.dwg");
    std::string commandStatus = "null", zoomStatus = "null", saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string sourceGeometry = "[]", afterOriginal = "[]", savedOriginal = "[]", before = "null", after = "null", saved = "null";
    std::string dxfBefore = "null", dxfAfter = "null", dxfSaved = "null", gripQueryStatus = "null", stretchQueryStatus = "null", transformStatus = "null";
    std::string pickWcs = "null", pickUcs = "null"; int gripIndex = -1, initialCount = 0, sourceCount = 0, reopenedCount = 0;
    std::vector<SchemeMapping> sourceMap;
    try {
        if (!below(control, p.output) || !below(output, p.output)) throw std::runtime_error("axis-renumber paths outside isolated run");
        requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("axis renumber requires document command context");
        auto* document = acDocManager->curDocument();
        if (!document || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("axis-renumber current drawing is not this run's control.dwg");
        auto* database = document->database();
        if (!database || acdbHostApplicationServices()->workingDatabase() != database)
            throw std::runtime_error("axis-renumber document and native working database differ");
        const ACHAR* filename{}; check(database->getFilename(filename), "get axis-renumber database filename");
        if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("axis-renumber native database path guard failed");
        if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
            throw std::runtime_error("axis renumber requires no pre-existing transaction");
        const auto allIds = modelIds(*database); initialCount = allIds.length();
        if (initialCount != 214) throw std::runtime_error("axis-renumber fixture must contain 213 original objects and one axis collection");
        AcDbObjectId axisId; check(database->getAcDbObjectId(axisId, false, AcDbHandle(L"D0D3")), "find axis-renumber native collection");
        { auto axis = open<AcDbEntity>(axisId); if (_wcsicmp(axis->isA()->name(), L"TDbAxisLabelSet")) throw std::runtime_error("axis-renumber fixture has wrong native type"); }
        AcDbObjectIdArray originalIds;
        for (int i = 0; i < allIds.length(); ++i) {
            if (allIds[i] == axisId) continue;
            originalIds.append(allIds[i]); auto entity = open<AcDbEntity>(allIds[i]); const auto handle = handleOf(allIds[i]);
            sourceMap.push_back(SchemeMapping{handle, handle, entity->isA()->name()});
        }
        sourceCount = originalIds.length();
        if (sourceCount != 213) throw std::runtime_error("axis-renumber original scheme coverage is incomplete");
        sourceGeometry = scanEntityArray(originalIds); before = scanEntity(axisId); dxfBefore = publicAxisDxf(axisId);
        try {
            const auto zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_E", RTNONE); zoomStatus = std::to_string(zoomed);
            if (zoomed != RTNORM) throw std::runtime_error("axis-renumber zoom did not return RTNORM");
            AcGePoint3dArray grips, stretch; AcDbIntArray snaps, geometry;
            {
                auto axis = open<AcDbEntity>(axisId);
                const auto gripStatus = axis->getGripPoints(grips, snaps, geometry); gripQueryStatus = std::to_string(gripStatus); check(gripStatus, "query current axis-renumber grips");
                const auto stretchStatus = axis->getStretchPoints(stretch); stretchQueryStatus = std::to_string(stretchStatus); check(stretchStatus, "query current axis-renumber stretch points");
            }
            if (grips.isEmpty() || stretch.isEmpty()) throw std::runtime_error("axis-renumber fixture exposes no selection points");
            double leftX = std::numeric_limits<double>::infinity(), lowestY = std::numeric_limits<double>::infinity();
            for (int i = 0; i < stretch.length(); ++i) leftX = std::min(leftX, stretch[i].x);
            for (int i = 0; i < grips.length(); ++i) {
                if (std::abs(grips[i].x - leftX) > 1e-6) continue;
                bool shared = false;
                for (int j = 0; j < stretch.length(); ++j) {
                    if (grips[i].distanceTo(stretch[j]) <= 1e-6) { shared = true; break; }
                }
                if (shared && grips[i].y < lowestY) { lowestY = grips[i].y; gripIndex = i; }
            }
            if (gripIndex < 0) throw std::runtime_error("axis-renumber bottom-left circle-edge fixture could not be matched in current arrays");
            const auto selected = grips[gripIndex]; pickWcs = point(selected);
            ads_point world{selected.x, selected.y, selected.z}, user{};
            resbuf from{}, to{}; from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
            const auto converted = acedTrans(world, &from, &to, 0, user); transformStatus = std::to_string(converted);
            if (converted != RTNORM) throw std::runtime_error("axis-renumber WCS-to-UCS conversion failed");
            pickUcs = point(AcGePoint3d(user[0], user[1], user[2]));
            // No database object or transaction remains open while Tianzheng interprets this point selection.
            const auto commanded = acedCommandS(RTSTR, L"TReAxisNo", RT3DPOINT, user, RTSTR, L"21", RTSTR, L"", RTNONE);
            commandStatus = std::to_string(commanded);
            if (commanded != RTNORM) throw std::runtime_error("TReAxisNo did not return RTNORM; observed result retained");
        } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
        if (acDocManager->curDocument() != document || document->database() != database
            || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("axis renumber changed document context; output not saved");
        afterOriginal = scanEntityArray(originalIds); after = scanEntity(axisId); dxfAfter = publicAxisDxf(axisId);
        requireNewFile(output);
        const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save native axis-renumber trial");
        AcDbDatabase reopened(false, true);
        const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loaded); check(loaded, "reopen axis-renumber trial");
        const auto closed = reopened.closeInput(true); closeInputStatus = std::to_string(closed); check(closed, "close reopened axis-renumber input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray persistedOriginals;
            for (const auto& mapping : sourceMap) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(mapping.targetHandle.c_str())), "find original in reopened axis-renumber output"); persistedOriginals.append(id);
            }
            savedOriginal = scanEntityArray(persistedOriginals); AcDbObjectId persistedAxis;
            check(reopened.getAcDbObjectId(persistedAxis, false, AcDbHandle(L"D0D3")), "find persisted axis-renumber collection");
            saved = scanEntity(persistedAxis); dxfSaved = publicAxisDxf(persistedAxis);
        }
    } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
    catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream out;
    out << "{\"contract\":\"stage1.axis-renumber-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandS\",\"commandName\":\"TReAxisNo\""
        << ",\"requestedNumber\":\"21\",\"axisHandle\":\"D0D3\",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
        << ",\"initialModelCount\":" << initialCount << ",\"sourceCount\":" << sourceCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"sourceMap\":" << schemeMapJson(sourceMap) << ",\"sourceGeometry\":" << sourceGeometry << ",\"afterOriginalGeometry\":" << afterOriginal
        << ",\"savedOriginalGeometry\":" << savedOriginal << ",\"before\":" << before << ",\"after\":" << after << ",\"saved\":" << saved
        << ",\"dxfBefore\":" << dxfBefore << ",\"dxfAfter\":" << dxfAfter << ",\"dxfSaved\":" << dxfSaved
        << ",\"pick\":{\"gripQueryStatus\":" << gripQueryStatus << ",\"stretchQueryStatus\":" << stretchQueryStatus << ",\"gripIndex\":" << gripIndex
        << ",\"pointWcs\":" << pickWcs << ",\"transformStatus\":" << transformStatus << ",\"pointUcs\":" << pickUcs
        << ",\"basis\":\"fixed trial circle-edge candidate: lowest common grip/stretch point at minimum native stretch X\"}"
        << ",\"zoomStatus\":" << zoomStatus << ",\"commandStatus\":" << commandStatus << ",\"saveStatus\":" << saveStatus
        << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeInputStatus << ",\"error\":" << error
        << ",\"labelVerificationStatus\":\"pending explicit DXF semantics or rendered verification; neither RTNORM nor arbitrary string 21 establishes renumbering\"}";
    return out.str();
}
void nativeAxisRenumberVerification() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\axis-renumber-native.json"); requireNewFile(report);
        writeNewReport(report, axisRenumberCase(p)); acutPrintf(L"\nSTAGE1_AXIS_RENUMBER_NATIVE_COMPLETED; LABEL_VERIFICATION_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_AXIS_RENUMBER_NATIVE_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_AXIS_RENUMBER_NATIVE_FAILED: unknown C++ exception\n"); }
}
std::string dimensionPointsCase(const Paths& p) {
    const auto control = fullPath(p.output + L"\\control.dwg");
    const auto output = fullPath(p.output + L"\\dim-points-generated.dwg");
    std::string commandStatus = "null", zoomStatus = "null", saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string sourceGeometry = "[]", afterOriginal = "[]", savedOriginal = "[]", before = "null", after = "null", saved = "null";
    std::string dxfBefore = "null", dxfAfter = "null", dxfSaved = "null", wallControl = "null", openingControl = "null", expectedSegments = "[]";
    std::string pickWcs = "null", pickUcs = "null", pickTransformStatus = "null";
    AcGePoint3dArray measuredPoints, userMeasuredPoints; AcDbIntArray pointTransformStatuses;
    std::vector<SchemeMapping> sourceMap; int initialCount = 0, sourceCount = 0, reopenedCount = 0;
    try {
        if (!below(control, p.output) || !below(output, p.output)) throw std::runtime_error("dimension-point paths outside isolated run");
        requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("dimension points require document command context");
        auto* document = acDocManager->curDocument();
        if (!document || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("dimension-point current drawing is not this run's control.dwg");
        auto* database = document->database();
        if (!database || acdbHostApplicationServices()->workingDatabase() != database)
            throw std::runtime_error("dimension-point document and working database differ");
        const ACHAR* filename{}; check(database->getFilename(filename), "get dimension-point database filename");
        if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("dimension-point native database path guard failed");
        if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
            throw std::runtime_error("dimension-point command requires no pre-existing transaction");
        const auto allIds = modelIds(*database); initialCount = allIds.length();
        if (initialCount != 214) throw std::runtime_error("dimension-point fixture must contain 213 original objects and one dimension");
        AcDbObjectId dimensionId; check(database->getAcDbObjectId(dimensionId, false, AcDbHandle(L"D0D3")), "find native dimension-point fixture");
        {
            auto dimension = open<AcDbEntity>(dimensionId);
            if (_wcsicmp(dimension->isA()->name(), L"TDbDimension2")) throw std::runtime_error("dimension-point fixture has wrong native type");
        }
        AcDbObjectIdArray originalIds;
        for (int i = 0; i < allIds.length(); ++i) {
            if (allIds[i] == dimensionId) continue;
            originalIds.append(allIds[i]); auto entity = open<AcDbEntity>(allIds[i]); const auto handle = handleOf(allIds[i]);
            sourceMap.push_back(SchemeMapping{handle, handle, entity->isA()->name()});
        }
        sourceCount = originalIds.length();
        if (sourceCount != 213) throw std::runtime_error("dimension-point original scheme coverage is incomplete");
        sourceGeometry = scanEntityArray(originalIds); before = scanEntity(dimensionId); dxfBefore = publicAxisDxf(dimensionId);
        AcGePoint3d wallStart, wallEnd, wallLeft, wallRight;
        {
            AcDbObjectId wallId; check(database->getAcDbObjectId(wallId, false, AcDbHandle(L"CD83")), "find dimension-point wall control");
            auto wall = open<AcDbEntity>(wallId); const auto* curve = AcDbCurve::cast(wall.get());
            if (!curve || _wcsicmp(wall->isA()->name(), L"TDbWall")) throw std::runtime_error("dimension-point wall is not a native wall curve");
            check(curve->getStartPoint(wallStart), "dimension-point wall start"); check(curve->getEndPoint(wallEnd), "dimension-point wall end");
            double endParameter{}, length{}; check(curve->getEndParam(endParameter), "dimension-point wall end parameter");
            check(curve->getDistAtParam(endParameter, length), "dimension-point wall length");
            if (std::abs(wallStart.y - wallEnd.y) > 1e-6 || std::abs(wallStart.z - wallEnd.z) > 1e-6 || std::abs(wallStart.x - wallEnd.x) < 1e-6)
                throw std::runtime_error("dimension-point wall fixture is not horizontal in one drawing plane");
            wallLeft = wallStart.x < wallEnd.x ? wallStart : wallEnd; wallRight = wallStart.x < wallEnd.x ? wallEnd : wallStart;
            wallControl = "{\"sourceHandle\":\"CD83\",\"type\":\"TDbWall\",\"start\":" + point(wallStart) + ",\"end\":" + point(wallEnd)
                + ",\"length\":" + number(length) + ",\"sortedPoints\":[" + point(wallLeft) + "," + point(wallRight) + "]}";
        }
        {
            AcDbObjectId openingId; check(database->getAcDbObjectId(openingId, false, AcDbHandle(L"CD84")), "find dimension-point opening control");
            auto opening = open<AcDbEntity>(openingId);
            if (_wcsicmp(opening->isA()->name(), L"TDbOpening")) throw std::runtime_error("dimension-point opening fixture has wrong native type");
            AcGePoint3dArray grips; AcDbIntArray snaps, geometry;
            const auto query = opening->getGripPoints(grips, snaps, geometry); check(query, "query actual opening side grips");
            int left = -1, right = -1;
            for (int i = 0; i < grips.length(); ++i) {
                if (std::abs(grips[i].y - wallStart.y) > 1e-6 || grips[i].x < wallLeft.x - 1e-6 || grips[i].x > wallRight.x + 1e-6) continue;
                if (left < 0 || grips[i].x < grips[left].x) left = i;
                if (right < 0 || grips[i].x > grips[right].x) right = i;
            }
            if (left < 0 || right < 0 || left == right) throw std::runtime_error("two opening side grips on this wall baseline were not found");
            measuredPoints.append(AcGePoint3d(grips[left].x, grips[left].y, wallStart.z));
            measuredPoints.append(AcGePoint3d(grips[right].x, grips[right].y, wallStart.z));
            openingControl = "{\"sourceHandle\":\"CD84\",\"type\":\"TDbOpening\",\"gripQueryStatus\":" + std::to_string(query)
                + ",\"rawGripIndices\":[" + std::to_string(left) + "," + std::to_string(right) + "],\"rawPoints\":[" + point(grips[left]) + "," + point(grips[right])
                + "],\"measurementPoints\":" + points(measuredPoints) + ",\"width\":" + number(measuredPoints[0].distanceTo(measuredPoints[1]))
                + ",\"projection\":\"preserve native opening grip X/Y; project Z to the native horizontal wall drawing plane\"}";
            expectedSegments = "[" + number(wallLeft.distanceTo(measuredPoints[0])) + "," + number(measuredPoints[0].distanceTo(measuredPoints[1]))
                + "," + number(measuredPoints[1].distanceTo(wallRight)) + "]";
        }
        AcGePoint3d lineStart, lineEnd, linePick;
        {
            auto dimension = open<AcDbEntity>(dimensionId); const auto* curve = AcDbCurve::cast(dimension.get());
            if (!curve) throw std::runtime_error("dimension-point native dimension does not expose a curve");
            check(curve->getStartPoint(lineStart), "dimension-point line start"); check(curve->getEndPoint(lineEnd), "dimension-point line end");
            linePick = AcGePoint3d((lineStart.x + lineEnd.x) / 2.0, (lineStart.y + lineEnd.y) / 2.0, (lineStart.z + lineEnd.z) / 2.0);
            pickWcs = point(linePick);
        }
        try {
            const auto zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_E", RTNONE); zoomStatus = std::to_string(zoomed);
            if (zoomed != RTNORM) throw std::runtime_error("dimension-point zoom did not return RTNORM");
            AcGePoint3dArray inputWorld, inputUser; inputWorld.append(linePick); inputWorld.append(measuredPoints[0]); inputWorld.append(measuredPoints[1]);
            for (int i = 0; i < inputWorld.length(); ++i) {
                const auto& worldPoint = inputWorld[i]; ads_point world{worldPoint.x, worldPoint.y, worldPoint.z}, user{};
                resbuf from{}, to{}; from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
                const auto converted = acedTrans(world, &from, &to, 0, user);
                if (i == 0) pickTransformStatus = std::to_string(converted); else pointTransformStatuses.append(converted);
                if (converted != RTNORM) throw std::runtime_error("dimension-point WCS-to-UCS conversion failed");
                inputUser.append(AcGePoint3d(user[0], user[1], user[2]));
            }
            pickUcs = point(inputUser[0]); userMeasuredPoints.append(inputUser[1]); userMeasuredPoints.append(inputUser[2]);
            ads_point pick{inputUser[0].x, inputUser[0].y, inputUser[0].z};
            ads_point first{inputUser[1].x, inputUser[1].y, inputUser[1].z}, second{inputUser[2].x, inputUser[2].y, inputUser[2].z};
            // The full documented point sequence executes without an open object or transaction.
            const auto commanded = acedCommandS(RTSTR, L"TBreakDim", RT3DPOINT, pick, RT3DPOINT, first, RT3DPOINT, second, RTSTR, L"", RTNONE);
            commandStatus = std::to_string(commanded);
            if (commanded != RTNORM) throw std::runtime_error("TBreakDim did not return RTNORM; observed result retained");
        } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
        if (acDocManager->curDocument() != document || document->database() != database
            || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("dimension-point command changed document context; output not saved");
        afterOriginal = scanEntityArray(originalIds); after = scanEntity(dimensionId); dxfAfter = publicAxisDxf(dimensionId);
        requireNewFile(output);
        const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save native dimension-point trial");
        AcDbDatabase reopened(false, true);
        const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loaded); check(loaded, "reopen dimension-point trial");
        const auto closed = reopened.closeInput(true); closeInputStatus = std::to_string(closed); check(closed, "close reopened dimension-point input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray persistedOriginals;
            for (const auto& mapping : sourceMap) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(mapping.targetHandle.c_str())), "find original in reopened dimension-point output"); persistedOriginals.append(id);
            }
            savedOriginal = scanEntityArray(persistedOriginals); AcDbObjectId persistedDimension;
            check(reopened.getAcDbObjectId(persistedDimension, false, AcDbHandle(L"D0D3")), "find persisted dimension-point object");
            saved = scanEntity(persistedDimension); dxfSaved = publicAxisDxf(persistedDimension);
        }
    } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
    catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream out;
    out << "{\"contract\":\"stage1.dim-points-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandS\",\"commandName\":\"TBreakDim\""
        << ",\"dimensionHandle\":\"D0D3\",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
        << ",\"initialModelCount\":" << initialCount << ",\"sourceCount\":" << sourceCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"sourceMap\":" << schemeMapJson(sourceMap) << ",\"sourceGeometry\":" << sourceGeometry << ",\"afterOriginalGeometry\":" << afterOriginal
        << ",\"savedOriginalGeometry\":" << savedOriginal << ",\"before\":" << before << ",\"after\":" << after << ",\"saved\":" << saved
        << ",\"dxfBefore\":" << dxfBefore << ",\"dxfAfter\":" << dxfAfter << ",\"dxfSaved\":" << dxfSaved
        << ",\"wallControl\":" << wallControl << ",\"openingControl\":" << openingControl << ",\"expectedSegments\":" << expectedSegments
        << ",\"input\":{\"pickWcs\":" << pickWcs << ",\"pickUcs\":" << pickUcs << ",\"pickTransformStatus\":" << pickTransformStatus
        << ",\"pointsWcs\":" << points(measuredPoints) << ",\"pointsUcs\":" << points(userMeasuredPoints) << ",\"pointTransformStatuses\":" << integers(pointTransformStatuses) << '}'
        << ",\"zoomStatus\":" << zoomStatus << ",\"commandStatus\":" << commandStatus << ",\"saveStatus\":" << saveStatus
        << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeInputStatus << ",\"error\":" << error
        << ",\"labelVerificationStatus\":\"pending rendered dimension labels; expectedSegments and RTNORM are not output-label verification\"}";
    return out.str();
}
void nativeDimensionPointsVerification() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\dim-points-native.json"); requireNewFile(report);
        writeNewReport(report, dimensionPointsCase(p)); acutPrintf(L"\nSTAGE1_DIM_POINTS_NATIVE_COMPLETED; LABEL_VERIFICATION_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_DIM_POINTS_NATIVE_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_DIM_POINTS_NATIVE_FAILED: unknown C++ exception\n"); }
}
AcDbObjectIdArray nativeEntitiesOfType(const AcDbObjectIdArray& ids, const wchar_t* type) {
    AcDbObjectIdArray result;
    for (int i = 0; i < ids.length(); ++i) {
        auto entity = open<AcDbEntity>(ids[i]); if (!_wcsicmp(entity->isA()->name(), type)) result.append(ids[i]);
    }
    return result;
}
struct AxisNativeTraceData {
    std::vector<std::string> commands, inputs;
    unsigned long long sequence{};
    bool traceDropped{}, editorAvailable{}, managerAvailable{}, editorAdded{}, editorRemoved{}, inputAddAttempted{}, inputRemoveAttempted{};
    bool inputAddReturned{}, inputRemoveReturned{};
    int inputAddStatus{}, inputRemoveStatus{};
    bool retained{}, moduleLockAttempted{}, moduleLockSucceeded{};
};
std::string axisTraceArray(const std::vector<std::string>& records) {
    std::ostringstream out; out << '[';
    for (std::size_t i = 0; i < records.size(); ++i) { if (i) out << ','; out << records[i]; }
    out << ']'; return out.str();
}
std::string axisCommandRegistry(bool* gridsCallable = nullptr) {
    if (gridsCallable) *gridsCallable = false;
    std::ostringstream out; out << '['; bool comma = false;
    for (const auto* query : {L"TMultAxisDim", L"GRIDS"}) {
        if (comma) out << ','; comma = true;
        auto* stack = acedRegCmds; auto* command = stack ? stack->lookupGlobalCmd(query) : nullptr; const bool globalLookup = command != nullptr;
        if (!command && stack) command = stack->lookupLocalCmd(query);
        out << "{\"query\":" << quote(query) << ",\"found\":" << (command ? "true" : "false");
        if (!command) { out << '}'; continue; }
        std::wstring group; std::unique_ptr<AcEdCommandIterator> iterator(stack->iterator());
        if (iterator) for (; !iterator->done(); iterator->next()) {
            if (iterator->command() == command) { if (iterator->commandGroup()) group = iterator->commandGroup(); break; }
        }
        const auto function = command->functionAddr(); bool moduleResolved = false; std::wstring modulePath;
        std::string moduleError = "null";
        if (function) {
            HMODULE module{};
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(function), &module)) {
                std::vector<wchar_t> buffer(32768); const auto length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length && length < buffer.size()) { modulePath.assign(buffer.data(), length); moduleResolved = true; }
                else moduleError = std::to_string(length ? ERROR_INSUFFICIENT_BUFFER : GetLastError());
            } else moduleError = std::to_string(GetLastError());
        }
        out << ",\"lookup\":" << quote(globalLookup ? L"global" : L"local") << ",\"globalName\":" << quote(command->globalName() ? command->globalName() : L"")
            << ",\"localName\":" << quote(command->localName() ? command->localName() : L"") << ",\"groupName\":" << (group.empty() ? "null" : quote(group))
            << ",\"flags\":" << command->commandFlags() << ",\"functionCode\":" << command->functionCode() << ",\"functionAddressAvailable\":" << (function ? "true" : "false")
            << ",\"moduleResolved\":" << (moduleResolved ? "true" : "false") << ",\"modulePath\":" << (moduleResolved ? quote(modulePath) : "null")
            << ",\"moduleError\":" << moduleError << ",\"functionInvoked\":false}";
        if (gridsCallable && !_wcsicmp(query, L"GRIDS")) *gridsCallable = moduleResolved && !group.empty();
    }
    out << ']'; return out.str();
}
class ScopedAxisNativeTrace final : public AcEditorReactor, public AcEdInputContextReactor {
public:
    ScopedAxisNativeTrace(AcApDocument* document, std::shared_ptr<AxisNativeTraceData> data) noexcept
        : dataOwner_(std::move(data)), data_(*dataOwner_) {
        try {
            editor_ = acedEditor; data_.editorAvailable = editor_ != nullptr;
            if (editor_) { editorAttached_ = true; editor_->addReactor(this); data_.editorAdded = true; }
            manager_ = document ? document->inputPointManager() : nullptr; data_.managerAvailable = manager_ != nullptr;
            if (manager_) {
                // Conservatively require removal even if an exceptional add partially registers.
                data_.inputAddAttempted = true; inputAttached_ = true;
                data_.inputAddStatus = manager_->addInputContextReactor(this);
                data_.inputAddReturned = true;
            }
        } catch (...) { data_.traceDropped = true; detach(); }
    }
    ~ScopedAxisNativeTrace() override { detach(); }
    ScopedAxisNativeTrace(const ScopedAxisNativeTrace&) = delete;
    ScopedAxisNativeTrace& operator=(const ScopedAxisNativeTrace&) = delete;
    bool detach() noexcept {
        if (inputAttached_) {
            try {
                data_.inputRemoveAttempted = true; data_.inputRemoveReturned = false;
                data_.inputRemoveStatus = manager_->removeInputContextReactor(this); data_.inputRemoveReturned = true;
                if (data_.inputRemoveStatus == Acad::eOk || data_.inputRemoveStatus == Acad::eInvalidKey) inputAttached_ = false;
                else data_.traceDropped = true;
            } catch (...) { data_.traceDropped = true; }
        }
        if (editorAttached_) {
            try { editor_->removeReactor(this); editorAttached_ = false; data_.editorRemoved = true; }
            catch (...) { data_.traceDropped = true; }
        }
        return !inputAttached_ && !editorAttached_;
    }
    void markRetained() noexcept {
        data_.retained = true;
        try {
            data_.moduleLockAttempted = nativeApplicationId != nullptr;
            if (nativeApplicationId) data_.moduleLockSucceeded = acrxDynamicLinker->lockApplication(nativeApplicationId);
        } catch (...) { data_.traceDropped = true; }
    }
    ScopedAxisNativeTrace* retainedNext{};
    void commandWillStart(const ACHAR* command) override { commandEvent(L"commandWillStart", command); }
    void commandEnded(const ACHAR* command) override { commandEvent(L"commandEnded", command); }
    void commandCancelled(const ACHAR* command) override { commandEvent(L"commandCancelled", command); }
    void commandFailed(const ACHAR* command) override { commandEvent(L"commandFailed", command); }
    void unknownCommand(const ACHAR* command, AcDbVoidPtrArray*) override { commandEvent(L"unknownCommand", command); }
    void beginEntsel(const ACHAR* prompt, int flags, const ACHAR* keywords) override {
        inputEvent(L"beginEntsel", [&] { return promptFields(prompt, flags, keywords); });
    }
    void endEntsel(Acad::PromptStatus status, AcDbObjectId& id, AcGePoint3d& pick, const ACHAR* keyword) override {
        inputEvent(L"endEntsel", [&] { return selectedFields(status, id, pick, keyword); });
    }
    void beginNentsel(const ACHAR* prompt, Adesk::Boolean pickFlag, int flags, const ACHAR* keywords) override {
        inputEvent(L"beginNentsel", [&] { return promptFields(prompt, flags, keywords) + ",\"pickFlag\":" + (pickFlag ? "true" : "false"); });
    }
    void endNentsel(Acad::PromptStatus status, AcDbObjectId id, const AcGePoint3d& pick,
        const AcGeMatrix3d&, const resbuf*, const ACHAR* keyword) override {
        inputEvent(L"endNentsel", [&] { return selectedFields(status, id, pick, keyword); });
    }
    void beginSSGet(const ACHAR* prompt, int flags, const ACHAR* keywords, const ACHAR* controls,
        const AcArray<AcGePoint3d>& inputPoints, const resbuf* mask) override {
        inputEvent(L"beginSSGet", [&] {
            std::ostringstream out; out << promptFields(prompt, flags, keywords) << ",\"controls\":" << nullable(controls)
                << ",\"entityMaskPresent\":" << (mask ? "true" : "false") << ",\"points\":[";
            for (int i = 0; i < inputPoints.length(); ++i) { if (i) out << ','; out << point(inputPoints[i]); }
            out << ']'; return out.str();
        });
    }
    void endSSGet(Acad::PromptStatus status, const AcArray<AcDbObjectId>& ids) override {
        inputEvent(L"endSSGet", [&] {
            std::ostringstream out; out << ",\"returnStatus\":" << static_cast<int>(status) << ",\"selectedHandles\":[";
            for (int i = 0; i < ids.length(); ++i) { if (i) out << ','; out << (ids[i].isNull() ? "null" : quote(handleOf(ids[i]))); }
            out << ']'; return out.str();
        });
    }
    void beginGetPoint(const AcGePoint3d* input, const ACHAR* prompt, int flags, const ACHAR* keywords) override {
        inputEvent(L"beginGetPoint", [&] { return promptFields(prompt, flags, keywords) + ",\"pointIn\":" + (input ? point(*input) : "null"); });
    }
    void endGetPoint(Acad::PromptStatus status, const AcGePoint3d& output, const ACHAR*& keyword) override {
        inputEvent(L"endGetPoint", [&] { return ",\"returnStatus\":" + std::to_string(static_cast<int>(status))
            + ",\"pointOut\":" + (status == Acad::eNormal ? point(output) : "null") + ",\"keyword\":" + nullableKeyword(status, keyword); });
    }
    void beginGetString(const ACHAR* prompt, int flags) override {
        inputEvent(L"beginGetString", [&] { return promptFields(prompt, flags, nullptr); });
    }
    void endGetString(Acad::PromptStatus status, ACHAR* output) override {
        inputEvent(L"endGetString", [&] { return ",\"returnStatus\":" + std::to_string(static_cast<int>(status))
            + ",\"value\":" + (status == Acad::eNormal ? nullable(output) : "null"); });
    }
    void beginGetKeyword(const ACHAR* prompt, int flags, const ACHAR* keywords) override {
        inputEvent(L"beginGetKeyword", [&] { return promptFields(prompt, flags, keywords); });
    }
    void endGetKeyword(Acad::PromptStatus status, const ACHAR*& keyword) override {
        inputEvent(L"endGetKeyword", [&] { return ",\"returnStatus\":" + std::to_string(static_cast<int>(status))
            + ",\"keyword\":" + nullableKeyword(status, keyword); });
    }
private:
    static std::string nullable(const ACHAR* value) { return value ? quote(value) : "null"; }
    static std::string nullableKeyword(Acad::PromptStatus status, const ACHAR* const& value) {
        return status == Acad::eKeyword ? nullable(value) : "null";
    }
    static std::string promptFields(const ACHAR* prompt, int flags, const ACHAR* keywords) {
        return ",\"prompt\":" + nullable(prompt) + ",\"initGetFlags\":" + std::to_string(flags) + ",\"keywords\":" + nullable(keywords);
    }
    static std::string selectedFields(Acad::PromptStatus status, AcDbObjectId id, const AcGePoint3d& pick, const ACHAR* keyword) {
        return ",\"returnStatus\":" + std::to_string(static_cast<int>(status)) + ",\"selectedHandle\":"
            + (status == Acad::eNormal && !id.isNull() ? quote(handleOf(id)) : "null")
            + ",\"pickPoint\":" + (status == Acad::eNormal ? point(pick) : "null") + ",\"keyword\":" + nullableKeyword(status, keyword);
    }
    template<class Details> void record(std::vector<std::string>& records, const wchar_t* event, Details details) noexcept {
        try {
            if (data_.commands.size() + data_.inputs.size() >= 10000) { data_.traceDropped = true; return; }
            records.push_back("{\"sequence\":" + std::to_string(++data_.sequence) + ",\"event\":" + quote(event) + details() + '}');
        } catch (...) { data_.traceDropped = true; }
    }
    template<class Details> void inputEvent(const wchar_t* event, Details details) noexcept { record(data_.inputs, event, details); }
    void commandEvent(const wchar_t* event, const ACHAR* command) noexcept {
        record(data_.commands, event, [&] {
            auto* manager = acDocManager; auto* current = manager ? manager->curDocument() : nullptr; auto* active = manager ? manager->mdiActiveDocument() : nullptr;
            return ",\"command\":" + nullable(command) + ",\"currentDWG\":" + (current && current->fileName() ? quote(fullPath(current->fileName())) : "null")
                + ",\"mdiActiveDWG\":" + (active && active->fileName() ? quote(fullPath(active->fileName())) : "null")
                + ",\"isApplicationContext\":" + (manager ? (manager->isApplicationContext() ? "true" : "false") : "null");
        });
    }
    std::shared_ptr<AxisNativeTraceData> dataOwner_;
    AxisNativeTraceData& data_;
    AcEditor* editor_{};
    AcEdInputPointManager* manager_{};
    bool editorAttached_{}, inputAttached_{};
};
// A failed removal must retain both the registered callback and its data.
// This list deliberately owns through the process lifetime if native removal fails;
// application locking plus the unload-entry guard prevent unloading its code.
ScopedAxisNativeTrace* retainedAxisNativeTraces{};
class AxisNativeTraceOwner {
public:
    AxisNativeTraceOwner(AcApDocument* document, std::shared_ptr<AxisNativeTraceData> data)
        : recorder_(std::make_unique<ScopedAxisNativeTrace>(document, std::move(data))) {}
    ~AxisNativeTraceOwner() noexcept {
        if (recorder_ && !recorder_->detach()) {
            auto* retained = recorder_.release(); retained->retainedNext = retainedAxisNativeTraces;
            retainedAxisNativeTraces = retained; retained->markRetained();
        }
    }
    AxisNativeTraceOwner(const AxisNativeTraceOwner&) = delete;
    AxisNativeTraceOwner& operator=(const AxisNativeTraceOwner&) = delete;
private:
    std::unique_ptr<ScopedAxisNativeTrace> recorder_;
};
std::string axisLabelsCase(const Paths& p, int axisCount) {
    const auto control = fullPath(p.output + L"\\control.dwg");
    const auto output = fullPath(p.output + L"\\axis-labels-" + std::to_wstring(axisCount) + L"-generated.dwg");
    std::string commandStatus = "null", zoomStatus = "null", saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", error = "null";
    std::string sourceGeometry = "[]", afterOriginal = "[]", savedOriginal = "[]", inputAxes = "[]", added = "[]", savedAdded = "[]";
    std::string labels = "[]", savedLabels = "[]", dimensions = "[]", savedDimensions = "[]", extentControl = "null", layerControl = "null";
    std::vector<SchemeMapping> sourceMap; std::vector<AxisWallControl> wallControls;
    AcGePoint3dArray picksWorld, picksUser; AcDbIntArray transformStatuses, appendStatuses;
    std::wstring firstAxisHandle, lastAxisHandle;
    auto traceData = std::make_shared<AxisNativeTraceData>(); AxisNativeTraceData& trace = *traceData;
    int initialCount = 0, sourceCount = 0, addedCount = 0, reopenedCount = 0;
    try {
        if (axisCount != 2 && axisCount != 3) throw std::runtime_error("axis-label fixture supports exactly two or three axes");
        if (!below(control, p.output) || !below(output, p.output)) throw std::runtime_error("axis-label paths outside isolated run");
        requireNewFile(output);
        if (!acDocManager || acDocManager->isApplicationContext()) throw std::runtime_error("axis labels require document command context");
        auto* document = acDocManager->curDocument();
        if (!document || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("axis-label current drawing is not this run's control.dwg");
        auto* database = document->database();
        if (!database || acdbHostApplicationServices()->workingDatabase() != database)
            throw std::runtime_error("axis-label document and native working database differ");
        const ACHAR* filename{}; check(database->getFilename(filename), "get axis-label database filename");
        if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("axis-label native database path guard failed");
        if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
            throw std::runtime_error("axis-label command requires no pre-existing transaction");
        const auto originalIds = modelIds(*database); initialCount = originalIds.length(); sourceCount = initialCount;
        if (initialCount != 213) throw std::runtime_error("axis-label fixture must begin with the complete 213-object scheme");
        sourceGeometry = scanEntityArray(originalIds);
        double minY = std::numeric_limits<double>::infinity(), maxY = -std::numeric_limits<double>::infinity(); int wallCount = 0;
        for (int i = 0; i < originalIds.length(); ++i) {
            auto entity = open<AcDbEntity>(originalIds[i]); const auto handle = handleOf(originalIds[i]);
            sourceMap.push_back(SchemeMapping{handle, handle, entity->isA()->name()});
            AcString layer; check(entity->layer(layer), "get original axis-label layer");
            if (!_wcsicmp(layer.kACharPtr(), L"DOTE")) throw std::runtime_error("axis-label fixture unexpectedly contains original DOTE entities");
            if (_wcsicmp(entity->isA()->name(), L"TDbWall")) continue;
            const auto* curve = AcDbCurve::cast(entity.get()); if (!curve) throw std::runtime_error("source wall does not expose a native curve");
            AcGePoint3d start, end; double endParameter{}, length{};
            check(curve->getStartPoint(start), "axis-label wall range start"); check(curve->getEndPoint(end), "axis-label wall range end");
            check(curve->getEndParam(endParameter), "axis-label wall range parameter"); check(curve->getDistAtParam(endParameter, length), "axis-label wall range length");
            if (std::abs(start.z) > 1e-6 || std::abs(end.z) > 1e-6 || std::abs(length - start.distanceTo(end)) > 1e-6
                || (std::abs(start.x - end.x) > 1e-6 && std::abs(start.y - end.y) > 1e-6))
                throw std::runtime_error("axis-label range fixture requires straight planar orthogonal native wall curves");
            minY = std::min(minY, std::min(start.y, end.y)); maxY = std::max(maxY, std::max(start.y, end.y)); ++wallCount;
        }
        if (!wallCount || !std::isfinite(minY) || !std::isfinite(maxY) || maxY - minY <= 1000.0)
            throw std::runtime_error("axis-label source wall range is insufficient for fixed point-selection fixture");
        extentControl = "{\"wallCount\":" + std::to_string(wallCount) + ",\"minY\":" + number(minY) + ",\"maxY\":" + number(maxY)
            + ",\"basis\":\"native endpoints of every original straight planar orthogonal TDbWall curve; no added axis length\"}";
        wallControls.push_back(readAxisWallControl(*database, L"CDB4")); wallControls.push_back(readAxisWallControl(*database, L"CDB0"));
        if (axisCount == 3) wallControls.push_back(readAxisWallControl(*database, L"CDA3"));
        std::sort(wallControls.begin(), wallControls.end(), [](const AxisWallControl& a, const AxisWallControl& b) { return a.axisX < b.axisX; });
        AcDbObjectId layerId;
        {
            AcDbLayerTable* raw{}; check(database->getLayerTable(raw, AcDb::kForRead), "get existing axis layer table"); Opened<AcDbLayerTable> table(raw);
            check(table->getAt(L"DOTE", layerId), "find documented existing DOTE axis layer"); auto layer = open<AcDbLayerTableRecord>(layerId);
            layerControl = "{\"name\":\"DOTE\",\"handle\":" + quote(handleOf(layerId)) + ",\"isOff\":" + (layer->isOff() ? "true" : "false")
                + ",\"isFrozen\":" + (layer->isFrozen() ? "true" : "false") + ",\"isLocked\":" + (layer->isLocked() ? "true" : "false")
                + ",\"isPlottable\":" + (layer->isPlottable() ? "true" : "false") + '}';
            if (layer->isOff() || layer->isFrozen() || layer->isLocked()) throw std::runtime_error("existing DOTE layer is unavailable; probe does not change layer state");
        }
        AcDbObjectIdArray inputIds;
        try {
            for (const auto& wall : wallControls) {
                auto model = open<AcDbBlockTableRecord>(modelId(*database), AcDb::kForWrite);
                auto line = std::make_unique<AcDbLine>(AcGePoint3d(wall.axisX, minY, 0), AcGePoint3d(wall.axisX, maxY, 0));
                line->setDatabaseDefaults(database); check(line->setLayer(layerId), "set new public LINE axis layer"); AcDbObjectId id;
                const auto appended = model->appendAcDbEntity(id, line.get()); appendStatuses.append(appended); check(appended, "append new public LINE axis");
                auto* resident = line.release(); check(resident->close(), "close new native LINE axis"); inputIds.append(id);
                model.reset(); inputAxes = scanEntityArray(inputIds);
            }
            firstAxisHandle = handleOf(inputIds[0]); lastAxisHandle = handleOf(inputIds[inputIds.length() - 1]);
            for (int index : {0, inputIds.length() - 1}) {
                auto line = open<AcDbLine>(inputIds[index]); AcGePoint3d start, end;
                check(line->getStartPoint(start), "read real axis pick start"); check(line->getEndPoint(end), "read real axis pick end");
                picksWorld.append(AcGePoint3d(start.x, std::max(start.y, end.y) - 500.0, start.z));
            }
            const auto zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_E", RTNONE); zoomStatus = std::to_string(zoomed);
            if (zoomed != RTNORM) throw std::runtime_error("axis-label zoom did not return RTNORM");
            for (int i = 0; i < picksWorld.length(); ++i) {
                const auto& worldPoint = picksWorld[i]; ads_point world{worldPoint.x, worldPoint.y, worldPoint.z}, user{};
                resbuf from{}, to{}; from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
                const auto converted = acedTrans(world, &from, &to, 0, user); transformStatuses.append(converted);
                if (converted != RTNORM) throw std::runtime_error("axis-label WCS-to-UCS point conversion failed");
                picksUser.append(AcGePoint3d(user[0], user[1], user[2]));
            }
            ads_point first{picksUser[0].x, picksUser[0].y, picksUser[0].z}, last{picksUser[1].x, picksUser[1].y, picksUser[1].z};
            // Current modeless-dialog settings are left untouched. Supply one complete documented selection cycle.
            int commanded = RTERROR;
            {
                AxisNativeTraceOwner traceScope(document, traceData);
                commanded = acedCommandS(RTSTR, L"TMultAxisDim", RT3DPOINT, first, RT3DPOINT, last, RTSTR, L"", RTSTR, L"", RTNONE);
            }
            commandStatus = std::to_string(commanded);
            if (commanded != RTNORM) throw std::runtime_error("TMultAxisDim did not return RTNORM; observed objects retained");
        } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
        if (acDocManager->curDocument() != document || document->database() != database
            || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str()))
            throw std::runtime_error("axis-label command changed document context; output not saved");
        afterOriginal = scanEntityArray(originalIds); const auto afterIds = modelIds(*database); AcDbObjectIdArray addedIds;
        for (int i = 0; i < afterIds.length(); ++i) {
            bool wasOriginal = false;
            for (int j = 0; j < originalIds.length(); ++j) if (afterIds[i] == originalIds[j]) { wasOriginal = true; break; }
            if (!wasOriginal) addedIds.append(afterIds[i]);
        }
        addedCount = addedIds.length(); added = scanEntityArray(addedIds);
        labels = scanEntityArray(nativeEntitiesOfType(addedIds, L"TDbAxisLabelSet")); dimensions = scanEntityArray(nativeEntitiesOfType(addedIds, L"TDbDimension2"));
        std::vector<std::wstring> addedHandles;
        for (int i = 0; i < addedIds.length(); ++i) addedHandles.push_back(handleOf(addedIds[i]));
        requireNewFile(output);
        const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save native new-axis-label trial");
        AcDbDatabase reopened(false, true);
        const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loaded); check(loaded, "reopen native new-axis-label trial");
        const auto closed = reopened.closeInput(true); closeInputStatus = std::to_string(closed); check(closed, "close reopened new-axis-label input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray persistedOriginals, persistedAdded;
            for (const auto& mapping : sourceMap) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(mapping.targetHandle.c_str())), "find original in reopened axis-label output"); persistedOriginals.append(id);
            }
            for (const auto& handle : addedHandles) {
                AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(handle.c_str())), "find actual added axis-label object in saved output"); persistedAdded.append(id);
            }
            savedOriginal = scanEntityArray(persistedOriginals); savedAdded = scanEntityArray(persistedAdded);
            savedLabels = scanEntityArray(nativeEntitiesOfType(persistedAdded, L"TDbAxisLabelSet")); savedDimensions = scanEntityArray(nativeEntitiesOfType(persistedAdded, L"TDbDimension2"));
        }
    } catch (const std::exception& failure) { const std::string message(failure.what()); error = quote(std::wstring(message.begin(), message.end())); }
    catch (...) { error = quote(L"unknown C++ exception"); }
    std::ostringstream out;
    out << "{\"contract\":\"stage1.axis-labels-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandS\",\"commandName\":\"TMultAxisDim\""
        << ",\"axisCount\":" << axisCount << ",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
        << ",\"initialModelCount\":" << initialCount << ",\"sourceCount\":" << sourceCount << ",\"reopenedModelCount\":" << reopenedCount
        << ",\"sourceMap\":" << schemeMapJson(sourceMap) << ",\"sourceGeometry\":" << sourceGeometry << ",\"afterOriginalGeometry\":" << afterOriginal
        << ",\"savedOriginalGeometry\":" << savedOriginal << ",\"wallControls\":" << axisWallControlsJson(wallControls) << ",\"axisExtentControl\":" << extentControl
        << ",\"axisLayer\":" << layerControl << ",\"inputAxes\":" << inputAxes << ",\"axisAppendStatuses\":" << integers(appendStatuses)
        << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded << ",\"addedCount\":" << addedCount
        << ",\"generatedLabelSets\":" << labels << ",\"savedGeneratedLabelSets\":" << savedLabels
        << ",\"generatedDimensions\":" << dimensions << ",\"savedGeneratedDimensions\":" << savedDimensions
        << ",\"input\":{\"firstAxisHandle\":" << quote(firstAxisHandle) << ",\"lastAxisHandle\":" << quote(lastAxisHandle)
        << ",\"picksWcs\":" << points(picksWorld) << ",\"picksUcs\":" << points(picksUser) << ",\"transformStatuses\":" << integers(transformStatuses) << ",\"pickInset\":500}"
        << ",\"commandTrace\":" << axisTraceArray(trace.commands) << ",\"inputTrace\":" << axisTraceArray(trace.inputs)
        << ",\"traceDropped\":" << (trace.traceDropped ? "true" : "false")
        << ",\"traceRetained\":" << (trace.retained ? "true" : "false") << ",\"traceModuleLockAttempted\":" << (trace.moduleLockAttempted ? "true" : "false")
        << ",\"traceModuleLockSucceeded\":" << (trace.moduleLockSucceeded ? "true" : "false")
        << ",\"inputReactorAddStatus\":" << (trace.inputAddReturned ? std::to_string(trace.inputAddStatus) : "null")
        << ",\"inputReactorRemoveStatus\":" << (trace.inputRemoveReturned ? std::to_string(trace.inputRemoveStatus) : "null")
        << ",\"reactorRegistration\":{\"editorAvailable\":" << (trace.editorAvailable ? "true" : "false")
        << ",\"inputPointManagerAvailable\":" << (trace.managerAvailable ? "true" : "false")
        << ",\"editorAddReturned\":" << (trace.editorAdded ? "true" : "false") << ",\"editorRemoveReturned\":" << (trace.editorRemoved ? "true" : "false")
        << ",\"editorStatusType\":\"void\",\"inputAddAttempted\":" << (trace.inputAddAttempted ? "true" : "false")
        << ",\"inputRemoveAttempted\":" << (trace.inputRemoveAttempted ? "true" : "false") << '}'
        << ",\"zoomStatus\":" << zoomStatus << ",\"commandStatus\":" << commandStatus << ",\"saveStatus\":" << saveStatus
        << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeInputStatus << ",\"error\":" << error
        << ",\"configuration\":\"current Tianzheng modeless-dialog settings; not read or changed by probe\",\"scope\":\"two or three fixed native LINE axes; actual label count, final numbering, dimensions and style require result verification\"}";
    return out.str();
}
void nativeAxisLabelsVerification(int axisCount) {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\axis-labels-native.json"); requireNewFile(report);
        writeNewReport(report, axisLabelsCase(p, axisCount)); acutPrintf(L"\nSTAGE1_AXIS_LABELS_NATIVE_COMPLETED; RESULT_VERIFICATION_PENDING\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_AXIS_LABELS_NATIVE_FAILED: %hs\n", failure.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_AXIS_LABELS_NATIVE_FAILED: unknown C++ exception\n"); }
}
void nativeAxisLabelsTwo() { nativeAxisLabelsVerification(2); }
void nativeAxisLabelsThree() { nativeAxisLabelsVerification(3); }
struct AxisLabelsAsyncState {
    Paths paths;
    int axisCount{}, initialCount{}, addedCount{}, reopenedCount{}, queueStatus{};
    bool queueReturned{}, queueAttempted{}, callbackInvoked{}, callbackCancelled{}, asynchronousCompleted{}, moduleLocked{};
    AcApDocument* document{};
    AcDbDatabase* database{};
    std::wstring control, output, report, done, firstHandle, lastHandle, commandName;
    AcDbObjectIdArray originalIds, inputIds;
    std::vector<SchemeMapping> sourceMap;
    std::vector<AxisWallControl> wallControls;
    AcGePoint3dArray picksWorld, picksUser;
    AcDbIntArray appendStatuses, transformStatuses;
    std::shared_ptr<AxisNativeTraceData> trace = std::make_shared<AxisNativeTraceData>();
    std::unique_ptr<AxisNativeTraceOwner> traceOwner;
    std::string error = "null", zoomStatus = "null", saveStatus = "null", reopenStatus = "null", closeStatus = "null";
    std::string sourceGeometry = "[]", afterOriginal = "[]", savedOriginal = "[]", inputAxes = "[]", added = "[]", savedAdded = "[]";
    std::string labels = "[]", savedLabels = "[]", dimensions = "[]", savedDimensions = "[]", extentControl = "null", layerControl = "null";
    std::string beforeRegistry = "[]", afterRegistry = "[]";
    ads_point first{}, last{};
    AxisLabelsAsyncState(const Paths& p, int count, const wchar_t* command = L"TMultAxisDim") : paths(p), axisCount(count), control(fullPath(p.output + L"\\control.dwg")),
        output(fullPath(p.output + L"\\axis-labels-" + std::to_wstring(count) + L"-generated.dwg")),
        report(fullPath(p.output + L"\\axis-labels-native.json")), done(fullPath(p.output + L"\\axis-labels-native.done")), commandName(command) {}
    void guard() const {
        if (!acDocManager || acDocManager->isApplicationContext() || acDocManager->curDocument() != document
            || acDocManager->mdiActiveDocument() != document || !document || !database || document->database() != database
            || !document->fileName() || _wcsicmp(fullPath(document->fileName()).c_str(), control.c_str())
            || acdbHostApplicationServices()->workingDatabase() != database)
            throw std::runtime_error("asynchronous axis-label document context guard failed");
        const ACHAR* filename{}; check(database->getFilename(filename), "get asynchronous axis-label database filename");
        if (!filename || _wcsicmp(fullPath(filename).c_str(), control.c_str())) throw std::runtime_error("asynchronous axis-label native filename guard failed");
    }
    void prepare() {
        if ((axisCount != 2 && axisCount != 3) || !below(control, paths.output) || !below(output, paths.output))
            throw std::runtime_error("invalid asynchronous axis-label fixture or isolated output path");
        requireNewFile(output); requireNewFile(report); requireNewFile(done); requireNewFile(done + L".tmp");
        if (!acDocManager) throw std::runtime_error("document manager absent");
        document = acDocManager->curDocument(); database = document ? document->database() : nullptr; guard();
        if (!database->transactionManager() || database->transactionManager()->numActiveTransactions() != 0)
            throw std::runtime_error("asynchronous axis labels require no pre-existing transaction");
        originalIds = modelIds(*database); initialCount = originalIds.length();
        if (initialCount != 213) throw std::runtime_error("asynchronous axis-label fixture must begin with the full 213-object scheme");
        sourceGeometry = scanEntityArray(originalIds); double minY = std::numeric_limits<double>::infinity(), maxY = -minY; int wallCount = 0;
        bool gridsCallable = false; beforeRegistry = axisCommandRegistry(&gridsCallable);
        if (!_wcsicmp(commandName.c_str(), L"GRIDS") && !gridsCallable)
            throw std::runtime_error("GRIDS public command/group/module ownership was not established; command not queued");
        for (int i = 0; i < originalIds.length(); ++i) {
            auto entity = open<AcDbEntity>(originalIds[i]); const auto handle = handleOf(originalIds[i]); sourceMap.push_back(SchemeMapping{handle, handle, entity->isA()->name()});
            AcString layer; check(entity->layer(layer), "get asynchronous source layer");
            if (!_wcsicmp(layer.kACharPtr(), L"DOTE")) throw std::runtime_error("asynchronous source unexpectedly contains DOTE entities");
            if (_wcsicmp(entity->isA()->name(), L"TDbWall")) continue;
            const auto* curve = AcDbCurve::cast(entity.get()); if (!curve) throw std::runtime_error("asynchronous source wall lacks native curve");
            AcGePoint3d start, end; double parameter{}, length{};
            check(curve->getStartPoint(start), "async wall range start"); check(curve->getEndPoint(end), "async wall range end");
            check(curve->getEndParam(parameter), "async wall range parameter"); check(curve->getDistAtParam(parameter, length), "async wall range length");
            if (std::abs(start.z) > 1e-6 || std::abs(end.z) > 1e-6 || std::abs(length - start.distanceTo(end)) > 1e-6
                || (std::abs(start.x - end.x) > 1e-6 && std::abs(start.y - end.y) > 1e-6)) throw std::runtime_error("asynchronous range fixture requires straight planar orthogonal native walls");
            minY = std::min(minY, std::min(start.y, end.y)); maxY = std::max(maxY, std::max(start.y, end.y)); ++wallCount;
        }
        if (!wallCount || !std::isfinite(minY) || !std::isfinite(maxY) || maxY - minY <= 1000) throw std::runtime_error("asynchronous source wall range is insufficient");
        extentControl = "{\"wallCount\":" + std::to_string(wallCount) + ",\"minY\":" + number(minY) + ",\"maxY\":" + number(maxY)
            + ",\"basis\":\"native endpoints of every original straight planar orthogonal TDbWall curve; no added axis length\"}";
        wallControls.push_back(readAxisWallControl(*database, L"CDB4")); wallControls.push_back(readAxisWallControl(*database, L"CDB0"));
        if (axisCount == 3) wallControls.push_back(readAxisWallControl(*database, L"CDA3"));
        std::sort(wallControls.begin(), wallControls.end(), [](const AxisWallControl& a, const AxisWallControl& b) { return a.axisX < b.axisX; });
        AcDbObjectId layerId;
        {
            AcDbLayerTable* raw{}; check(database->getLayerTable(raw, AcDb::kForRead), "get asynchronous existing layer table"); Opened<AcDbLayerTable> table(raw);
            check(table->getAt(L"DOTE", layerId), "get asynchronous existing DOTE layer"); auto layer = open<AcDbLayerTableRecord>(layerId);
            layerControl = "{\"name\":\"DOTE\",\"handle\":" + quote(handleOf(layerId)) + ",\"isOff\":" + (layer->isOff() ? "true" : "false")
                + ",\"isFrozen\":" + (layer->isFrozen() ? "true" : "false") + ",\"isLocked\":" + (layer->isLocked() ? "true" : "false")
                + ",\"isPlottable\":" + (layer->isPlottable() ? "true" : "false") + '}';
            if (layer->isOff() || layer->isFrozen() || layer->isLocked()) throw std::runtime_error("asynchronous existing DOTE layer is unavailable");
        }
        for (const auto& wall : wallControls) {
            auto model = open<AcDbBlockTableRecord>(modelId(*database), AcDb::kForWrite);
            auto line = std::make_unique<AcDbLine>(AcGePoint3d(wall.axisX, minY, 0), AcGePoint3d(wall.axisX, maxY, 0));
            line->setDatabaseDefaults(database); check(line->setLayer(layerId), "set asynchronous public LINE layer"); AcDbObjectId id;
            const auto appended = model->appendAcDbEntity(id, line.get()); appendStatuses.append(appended); check(appended, "append asynchronous public LINE");
            auto* resident = line.release(); check(resident->close(), "close asynchronous public LINE"); inputIds.append(id); model.reset(); inputAxes = scanEntityArray(inputIds);
        }
        firstHandle = handleOf(inputIds[0]); lastHandle = handleOf(inputIds[inputIds.length() - 1]);
        for (int index : {0, inputIds.length() - 1}) {
            auto line = open<AcDbLine>(inputIds[index]); AcGePoint3d start, end; check(line->getStartPoint(start), "async actual pick start"); check(line->getEndPoint(end), "async actual pick end");
            picksWorld.append(AcGePoint3d(start.x, std::max(start.y, end.y) - 500.0, start.z));
        }
        const auto zoomed = acedCommandS(RTSTR, L"_.ZOOM", RTSTR, L"_E", RTNONE); zoomStatus = std::to_string(zoomed);
        if (zoomed != RTNORM) throw std::runtime_error("asynchronous axis-label zoom did not return RTNORM");
        for (int i = 0; i < picksWorld.length(); ++i) {
            const auto& p = picksWorld[i]; ads_point world{p.x, p.y, p.z}, user{}; resbuf from{}, to{};
            from.restype = RTSHORT; from.resval.rint = 0; to.restype = RTSHORT; to.resval.rint = 1;
            const auto converted = acedTrans(world, &from, &to, 0, user); transformStatuses.append(converted);
            if (converted != RTNORM) throw std::runtime_error("asynchronous WCS-to-UCS conversion failed"); picksUser.append(AcGePoint3d(user[0], user[1], user[2]));
        }
        for (int i = 0; i < 3; ++i) { first[i] = i == 0 ? picksUser[0].x : i == 1 ? picksUser[0].y : picksUser[0].z; last[i] = i == 0 ? picksUser[1].x : i == 1 ? picksUser[1].y : picksUser[1].z; }
        guard();
    }
    void collect() {
        guard(); afterOriginal = scanEntityArray(originalIds); const auto all = modelIds(*database); AcDbObjectIdArray addedIds;
        for (int i = 0; i < all.length(); ++i) {
            bool original = false; for (int j = 0; j < originalIds.length(); ++j) if (all[i] == originalIds[j]) { original = true; break; }
            if (!original) addedIds.append(all[i]);
        }
        addedCount = addedIds.length(); added = scanEntityArray(addedIds);
        labels = scanEntityArray(nativeEntitiesOfType(addedIds, L"TDbAxisLabelSet")); dimensions = scanEntityArray(nativeEntitiesOfType(addedIds, L"TDbDimension2"));
        std::vector<std::wstring> handles; for (int i = 0; i < addedIds.length(); ++i) handles.push_back(handleOf(addedIds[i])); requireNewFile(output);
        const auto stored = database->saveAs(output.c_str(), false, AcDb::kDHL_CURRENT); saveStatus = std::to_string(stored); check(stored, "save asynchronous axis-label result");
        AcDbDatabase reopened(false, true); const auto loaded = reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false); reopenStatus = std::to_string(loaded); check(loaded, "reopen asynchronous axis-label result");
        const auto closed = reopened.closeInput(true); closeStatus = std::to_string(closed); check(closed, "close asynchronous reopened input");
        {
            WorkingDatabaseScope working(reopened); reopenedCount = modelCount(reopened); AcDbObjectIdArray originals, additions;
            for (const auto& m : sourceMap) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(m.targetHandle.c_str())), "find asynchronous saved original"); originals.append(id); }
            for (const auto& h : handles) { AcDbObjectId id; check(reopened.getAcDbObjectId(id, false, AcDbHandle(h.c_str())), "find asynchronous saved addition"); additions.append(id); }
            savedOriginal = scanEntityArray(originals); savedAdded = scanEntityArray(additions);
            savedLabels = scanEntityArray(nativeEntitiesOfType(additions, L"TDbAxisLabelSet")); savedDimensions = scanEntityArray(nativeEntitiesOfType(additions, L"TDbDimension2"));
        }
    }
    std::string json() const {
        const auto& t = *trace; std::ostringstream out;
        out << "{\"contract\":\"stage1.axis-labels-native.v1\",\"api\":\"ObjectARX native; no COM\",\"commandApi\":\"acedCommandC\",\"commandName\":" << quote(commandName)
            << ",\"axisCount\":" << axisCount << ",\"controlDWG\":" << quote(control) << ",\"outputDWG\":" << quote(output)
            << ",\"initialModelCount\":" << initialCount << ",\"sourceCount\":" << originalIds.length() << ",\"reopenedModelCount\":" << reopenedCount
            << ",\"sourceMap\":" << schemeMapJson(sourceMap) << ",\"sourceGeometry\":" << sourceGeometry << ",\"afterOriginalGeometry\":" << afterOriginal << ",\"savedOriginalGeometry\":" << savedOriginal
            << ",\"wallControls\":" << axisWallControlsJson(wallControls) << ",\"axisExtentControl\":" << extentControl << ",\"axisLayer\":" << layerControl
            << ",\"inputAxes\":" << inputAxes << ",\"axisAppendStatuses\":" << integers(appendStatuses) << ",\"added\":" << added << ",\"savedAdded\":" << savedAdded << ",\"addedCount\":" << addedCount
            << ",\"generatedLabelSets\":" << labels << ",\"savedGeneratedLabelSets\":" << savedLabels << ",\"generatedDimensions\":" << dimensions << ",\"savedGeneratedDimensions\":" << savedDimensions
            << ",\"input\":{\"firstAxisHandle\":" << quote(firstHandle) << ",\"lastAxisHandle\":" << quote(lastHandle) << ",\"picksWcs\":" << points(picksWorld) << ",\"picksUcs\":" << points(picksUser)
            << ",\"transformStatuses\":" << integers(transformStatuses) << ",\"pickInset\":500},\"commandTrace\":" << axisTraceArray(t.commands) << ",\"inputTrace\":" << axisTraceArray(t.inputs)
            << ",\"traceDropped\":" << (t.traceDropped ? "true" : "false") << ",\"traceRetained\":" << (t.retained ? "true" : "false")
            << ",\"inputReactorAddStatus\":" << (t.inputAddReturned ? std::to_string(t.inputAddStatus) : "null") << ",\"inputReactorRemoveStatus\":" << (t.inputRemoveReturned ? std::to_string(t.inputRemoveStatus) : "null")
            << ",\"reactorRegistration\":{\"editorAvailable\":" << (t.editorAvailable ? "true" : "false") << ",\"inputPointManagerAvailable\":" << (t.managerAvailable ? "true" : "false")
            << ",\"editorAddReturned\":" << (t.editorAdded ? "true" : "false") << ",\"editorRemoveReturned\":" << (t.editorRemoved ? "true" : "false") << ",\"editorStatusType\":\"void\"}"
            << ",\"zoomStatus\":" << zoomStatus << ",\"commandStatus\":" << (queueReturned ? std::to_string(queueStatus) : "null") << ",\"queueStatus\":" << (queueReturned ? std::to_string(queueStatus) : "null")
            << ",\"callbackInvoked\":" << (callbackInvoked ? "true" : "false") << ",\"callbackCancelled\":" << (callbackCancelled ? "true" : "false") << ",\"asynchronousCompleted\":" << (asynchronousCompleted ? "true" : "false")
            << ",\"beforeRegistry\":" << beforeRegistry << ",\"afterRegistry\":" << afterRegistry
            << ",\"saveStatus\":" << saveStatus << ",\"reopenStatus\":" << reopenStatus << ",\"closeInputStatus\":" << closeStatus << ",\"error\":" << error
            << ",\"entrypoint\":\"registered modal ARX command; returns immediately after coroutine tokens are queued\",\"processor\":\"acedCommandC coroutine callback; commandStatus is queue return only\""
            << ",\"configuration\":\"current Tianzheng modeless-dialog settings; not read or changed by probe\"}"; return out.str();
    }
};
struct AxisLabelsAsyncCompletion { std::wstring run, done; bool wasLocked{}; };
AxisLabelsAsyncState* pendingAxisLabelsAsync{};
AxisLabelsAsyncCompletion* pendingAxisLabelsCompletion{};
void finishAxisLabelsAsync(AxisLabelsAsyncState* state) noexcept {
    try {
        state->afterRegistry = axisCommandRegistry();
        state->traceOwner.reset();
        if (state->trace->retained) { state->error = quote(L"asynchronous trace removal failed; state and module retained"); writeNewReport(state->report, state->json()); return; }
        writeNewReport(state->report, state->json());
        auto completion = std::make_unique<AxisLabelsAsyncCompletion>(); completion->run = state->paths.output; completion->done = state->done; completion->wasLocked = state->moduleLocked;
        pendingAxisLabelsCompletion = completion.release(); pendingAxisLabelsAsync = nullptr; delete state;
        if (pendingAxisLabelsCompletion->wasLocked && !acrxDynamicLinker->unlockApplication(nativeApplicationId)) {
            acutPrintf(L"\nSTAGE1_AXIS_LABELS_C_PENDING: native module unlock failed; completion marker withheld\n"); return;
        }
        try {
            const auto temporary = pendingAxisLabelsCompletion->done + L".tmp";
            writeNewReport(temporary, "{\"run\":" + quote(pendingAxisLabelsCompletion->run) + ",\"safeToUnload\":true}");
            if (!MoveFileExW(temporary.c_str(), pendingAxisLabelsCompletion->done.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("cannot publish asynchronous completion marker");
        } catch (...) {
            acrxDynamicLinker->lockApplication(nativeApplicationId);
            acutPrintf(L"\nSTAGE1_AXIS_LABELS_C_PENDING: marker write failed; completion guard retained and module relocked\n"); return;
        }
        delete pendingAxisLabelsCompletion; pendingAxisLabelsCompletion = nullptr;
    } catch (...) {
        if (nativeApplicationId) {
            try { const bool locked = acrxDynamicLinker->lockApplication(nativeApplicationId); if (pendingAxisLabelsAsync) pendingAxisLabelsAsync->moduleLocked = locked; }
            catch (...) { }
        }
        acutPrintf(L"\nSTAGE1_AXIS_LABELS_C_PENDING: finalization failed; pending state/module retained\n");
    }
}
int axisLabelsAsyncCallback(void* data) {
    auto* state = static_cast<AxisLabelsAsyncState*>(data);
    if (!state || state != pendingAxisLabelsAsync) return RTERROR;
    state->callbackInvoked = true;
    try {
        state->callbackCancelled = acedCmdCWasCancelled();
        if (state->callbackCancelled) state->error = quote(L"acedCommandC callback reports cancellation or error; actual geometry retained");
        state->collect();
    } catch (const std::exception& failure) { const std::string message(failure.what()); state->error = quote(std::wstring(message.begin(), message.end())); }
    catch (...) { state->error = quote(L"unknown asynchronous callback exception"); }
    state->asynchronousCompleted = true; finishAxisLabelsAsync(state); return RTNORM;
}
void nativeAxisLabelsCoroutine(int count, const wchar_t* commandName = L"TMultAxisDim") {
    if (pendingAxisLabelsAsync || pendingAxisLabelsCompletion || retainedAxisNativeTraces) { acutPrintf(L"\nSTAGE1_AXIS_LABELS_C_REJECTED: previous native operation is still pending\n"); return; }
    try {
        pendingAxisLabelsAsync = new AxisLabelsAsyncState(paths(), count, commandName); auto* state = pendingAxisLabelsAsync; state->prepare();
        state->moduleLocked = nativeApplicationId && acrxDynamicLinker->lockApplication(nativeApplicationId);
        if (!state->moduleLocked) throw std::runtime_error("cannot lock native module before queuing coroutine");
        state->traceOwner = std::make_unique<AxisNativeTraceOwner>(state->document, state->trace); state->queueAttempted = true;
        state->queueStatus = acedCommandC(axisLabelsAsyncCallback, state, RTSTR, state->commandName.c_str(), RT3DPOINT, state->first, RT3DPOINT, state->last, RTSTR, L"", RTSTR, L"", RTNONE);
        state->queueReturned = true;
        acedCallBackOnCancel();
        return;
    } catch (const std::exception& failure) {
        if (pendingAxisLabelsAsync) {
            const std::string message(failure.what()); pendingAxisLabelsAsync->error = quote(std::wstring(message.begin(), message.end()));
            if (!pendingAxisLabelsAsync->queueAttempted) finishAxisLabelsAsync(pendingAxisLabelsAsync);
        }
    } catch (...) { if (pendingAxisLabelsAsync && !pendingAxisLabelsAsync->queueAttempted) { pendingAxisLabelsAsync->error = quote(L"unknown asynchronous entry exception"); finishAxisLabelsAsync(pendingAxisLabelsAsync); } }
}
void nativeAxisLabelsTwoC() { nativeAxisLabelsCoroutine(2); }
void nativeAxisLabelsThreeC() { nativeAxisLabelsCoroutine(3); }
void nativeAxisLabelsTwoG() { nativeAxisLabelsCoroutine(2, L"GRIDS"); }
void nativeAxisLabelsThreeG() { nativeAxisLabelsCoroutine(3, L"GRIDS"); }
struct NativeControlOpenState {
    Paths paths;
    std::wstring control;
    bool locked{}, dispatchAttempted{}, callbackInvoked{}, applicationContextObserved{};
    std::string openStatus = "null", currentDWG = "null", mdiActiveDWG = "null", error = "null";
    explicit NativeControlOpenState(const Paths& p) : paths(p), control(fullPath(p.output + L"\\control.dwg")) {}
};
struct NativeControlOpenCompletion { std::wstring run, report, done; std::string json; bool wasLocked{}; };
NativeControlOpenState* pendingNativeControlOpen{};
NativeControlOpenCompletion* pendingNativeControlOpenCompletion{};
void finishNativeControlOpen(NativeControlOpenState* state) noexcept {
    try {
        auto completion = std::make_unique<NativeControlOpenCompletion>(); completion->run = state->paths.output;
        completion->report = fullPath(state->paths.output + L"\\native-open.json"); completion->done = fullPath(state->paths.output + L"\\native-open.done");
        completion->wasLocked = state->locked;
        completion->json = "{\"contract\":\"stage1.native-control-open.v1\",\"api\":\"ObjectARX native; no COM\",\"controlDWG\":" + quote(state->control)
            + ",\"openStatus\":" + state->openStatus + ",\"error\":" + state->error + ",\"applicationContextObserved\":" + (state->applicationContextObserved ? "true" : "false")
            + ",\"currentDWG\":" + state->currentDWG + ",\"mdiActiveDWG\":" + state->mdiActiveDWG + ",\"callbackInvoked\":" + (state->callbackInvoked ? "true" : "false")
            + ",\"openApi\":\"AcApDocManager::appContextOpenDocument\",\"scope\":\"open isolated run control only; no entity writes, saves or user-document closes\"}";
        pendingNativeControlOpenCompletion = completion.release(); pendingNativeControlOpen = nullptr; delete state;
        if (pendingNativeControlOpenCompletion->wasLocked && !acrxDynamicLinker->unlockApplication(nativeApplicationId)) return;
        try {
            writeNewReport(pendingNativeControlOpenCompletion->report, pendingNativeControlOpenCompletion->json);
            const auto temporary = pendingNativeControlOpenCompletion->done + L".tmp";
            writeNewReport(temporary, "{\"run\":" + quote(pendingNativeControlOpenCompletion->run) + ",\"safeToUnload\":true}");
            if (!MoveFileExW(temporary.c_str(), pendingNativeControlOpenCompletion->done.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("cannot publish native-open completion marker");
        } catch (...) { acrxDynamicLinker->lockApplication(nativeApplicationId); return; }
        delete pendingNativeControlOpenCompletion; pendingNativeControlOpenCompletion = nullptr;
    } catch (...) {
        try { if (nativeApplicationId) acrxDynamicLinker->lockApplication(nativeApplicationId); } catch (...) { }
        acutPrintf(L"\nSTAGE1_NATIVE_CONTROL_OPEN_PENDING: report/cleanup failed; pending guard retained\n");
    }
}
void nativeControlOpenApplicationCallback(void* data) {
    auto* state = static_cast<NativeControlOpenState*>(data); if (!state || state != pendingNativeControlOpen) return;
    state->callbackInvoked = true;
    try {
        state->applicationContextObserved = acDocManager && acDocManager->isApplicationContext();
        if (!state->applicationContextObserved) throw std::runtime_error("native control-open callback is not in application context");
        const auto opened = acDocManager->appContextOpenDocument(state->control.c_str()); state->openStatus = std::to_string(opened);
        auto* current = acDocManager->curDocument(); auto* active = acDocManager->mdiActiveDocument();
        state->currentDWG = current && current->fileName() ? quote(fullPath(current->fileName())) : "null";
        state->mdiActiveDWG = active && active->fileName() ? quote(fullPath(active->fileName())) : "null";
        if (opened != Acad::eOk) throw std::runtime_error("appContextOpenDocument returned " + std::to_string(opened));
        // curDocument may differ in application context; the modal business entry checks both.
        if (!active || !active->fileName() || _wcsicmp(fullPath(active->fileName()).c_str(), state->control.c_str()))
            throw std::runtime_error("native open returned but the MDI active document does not match isolated control");
    } catch (const std::exception& failure) { const std::string text(failure.what()); state->error = quote(std::wstring(text.begin(), text.end())); }
    catch (...) { state->error = quote(L"unknown native control-open callback exception"); }
    finishNativeControlOpen(state);
}
void nativeOpenProbeControl() {
    if (pendingNativeControlOpen || pendingNativeControlOpenCompletion || pendingAxisLabelsAsync || pendingAxisLabelsCompletion || retainedAxisNativeTraces) {
        acutPrintf(L"\nSTAGE1_NATIVE_CONTROL_OPEN_REJECTED: another native operation remains pending\n"); return;
    }
    try {
        const auto p = paths(); auto state = std::make_unique<NativeControlOpenState>(p);
        if (!below(state->control, p.output)) throw std::runtime_error("native open target is outside isolated run");
        const auto attributes = GetFileAttributesW(state->control.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("isolated control DWG is absent");
        requireNewFile(p.output + L"\\native-open.json"); requireNewFile(p.output + L"\\native-open.done"); requireNewFile(p.output + L"\\native-open.done.tmp");
        if (!acDocManager || !nativeApplicationId) throw std::runtime_error("native application/document manager absent");
        state->locked = acrxDynamicLinker->lockApplication(nativeApplicationId);
        if (!state->locked) throw std::runtime_error("cannot lock native opener before application-context callback");
        pendingNativeControlOpen = state.release();
        pendingNativeControlOpen->dispatchAttempted = true;
        acDocManager->executeInApplicationContext(nativeControlOpenApplicationCallback, pendingNativeControlOpen);
        return; // The callback may execute immediately; never dereference the state after this call.
    } catch (const std::exception& failure) {
        if (pendingNativeControlOpen) {
            const std::string text(failure.what()); pendingNativeControlOpen->error = quote(std::wstring(text.begin(), text.end()));
            if (!pendingNativeControlOpen->dispatchAttempted) finishNativeControlOpen(pendingNativeControlOpen);
        }
        else acutPrintf(L"\nSTAGE1_NATIVE_CONTROL_OPEN_FAILED: %hs\n", failure.what());
    } catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_CONTROL_OPEN_FAILED: unknown entry exception\n"); }
}
#include "NativeStyleAudit.inc"
} // namespace

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { moduleHandle = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    if (message == AcRx::kInitAppMsg) {
        nativeApplicationId = id;
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
        const auto schemeAxisStatus = schemeStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1SCHEMEAXIS", L"S1SCHEMEAXIS", ACRX_CMD_MODAL, nativeSchemeAxisVerification)
            : schemeStatus;
        const auto wallAxisStatus = schemeAxisStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1WALLAXISVERIFY", L"S1WALLAXISVERIFY", ACRX_CMD_MODAL, nativeWallAxisVerification)
            : schemeAxisStatus;
        const auto renumberStatus = wallAxisStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISRENUMBER", L"S1AXISRENUMBER", ACRX_CMD_MODAL, nativeAxisRenumberVerification)
            : wallAxisStatus;
        const auto styleStatus = renumberStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1STYLEAUDIT", L"S1STYLEAUDIT", ACRX_CMD_MODAL, nativeStyleAudit)
            : renumberStatus;
        const auto dimPointsStatus = styleStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1DIMPOINTS", L"S1DIMPOINTS", ACRX_CMD_MODAL, nativeDimensionPointsVerification)
            : styleStatus;
        const auto axisLabelsTwoStatus = dimPointsStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISLABELS2", L"S1AXISLABELS2", ACRX_CMD_MODAL, nativeAxisLabelsTwo)
            : dimPointsStatus;
        const auto axisLabelsThreeStatus = axisLabelsTwoStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISLABELS3", L"S1AXISLABELS3", ACRX_CMD_MODAL, nativeAxisLabelsThree)
            : axisLabelsTwoStatus;
        const auto axisLabelsTwoCStatus = axisLabelsThreeStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISLABELS2C", L"S1AXISLABELS2C", ACRX_CMD_MODAL, nativeAxisLabelsTwoC)
            : axisLabelsThreeStatus;
        const auto axisLabelsThreeCStatus = axisLabelsTwoCStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISLABELS3C", L"S1AXISLABELS3C", ACRX_CMD_MODAL, nativeAxisLabelsThreeC)
            : axisLabelsTwoCStatus;
        const auto axisLabelsTwoGStatus = axisLabelsThreeCStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISLABELS2G", L"S1AXISLABELS2G", ACRX_CMD_MODAL, nativeAxisLabelsTwoG)
            : axisLabelsThreeCStatus;
        const auto axisLabelsThreeGStatus = axisLabelsTwoGStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1AXISLABELS3G", L"S1AXISLABELS3G", ACRX_CMD_MODAL, nativeAxisLabelsThreeG)
            : axisLabelsTwoGStatus;
        const auto nativeOpenStatus = axisLabelsThreeGStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE_ACCESS", L"S1OPENPROBECONTROL", L"S1OPENPROBECONTROL", ACRX_CMD_SESSION, nativeOpenProbeControl)
            : axisLabelsThreeGStatus;
        if (scanStatus != Acad::eOk || exerciseStatus != Acad::eOk || axisStatus != Acad::eOk || schemeStatus != Acad::eOk || schemeAxisStatus != Acad::eOk || wallAxisStatus != Acad::eOk || renumberStatus != Acad::eOk || styleStatus != Acad::eOk || dimPointsStatus != Acad::eOk || axisLabelsTwoStatus != Acad::eOk || axisLabelsThreeStatus != Acad::eOk || axisLabelsTwoCStatus != Acad::eOk || axisLabelsThreeCStatus != Acad::eOk || axisLabelsTwoGStatus != Acad::eOk || axisLabelsThreeGStatus != Acad::eOk || nativeOpenStatus != Acad::eOk) {
            acedRegCmds->removeGroup(L"STAGE1_NATIVE_ACCESS");
            acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_REGISTRATION_FAILED: scan=%d exercise=%d axis=%d scheme=%d schemeAxis=%d wallAxis=%d renumber=%d styles=%d dimPoints=%d axisLabels2=%d axisLabels3=%d axisLabels2C=%d axisLabels3C=%d\n", scanStatus, exerciseStatus, axisStatus, schemeStatus, schemeAxisStatus, wallAxisStatus, renumberStatus, styleStatus, dimPointsStatus, axisLabelsTwoStatus, axisLabelsThreeStatus, axisLabelsTwoCStatus, axisLabelsThreeCStatus);
            return AcRx::kRetError;
        }
        acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_LOADED SDK=23.1 MSC=%d\n", _MSC_VER);
    } else if (message == AcRx::kUnloadAppMsg) {
        if (retainedAxisNativeTraces || pendingAxisLabelsAsync || pendingAxisLabelsCompletion || pendingNativeControlOpen || pendingNativeControlOpenCompletion) {
            acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_UNLOAD_REFUSED: a native asynchronous operation or trace cleanup remains pending; callback and data remain alive\n");
            return AcRx::kRetError;
        }
        acedRegCmds->removeGroup(L"STAGE1_NATIVE_ACCESS"); acutPrintf(L"\nSTAGE1_NATIVE_ACCESS_UNLOADED\n");
    }
    return AcRx::kRetOK;
}
