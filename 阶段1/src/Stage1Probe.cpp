#include <Windows.h>
#include <olectl.h>
#include <aced.h>
#include <acadi.h>
#include <axboiler.h>
#include <dbapserv.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbtrans.h>
#include <rxregsvc.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
HMODULE moduleHandle{};
struct ReleaseCom { template<class T> void operator()(T* p) const { if (p) p->Release(); } };
template<class T> using ComPtr = std::unique_ptr<T, ReleaseCom>;
struct Variant : VARIANT {
    Variant() { VariantInit(this); }
    explicit Variant(double value) : Variant() { vt = VT_R8; dblVal = value; }
    explicit Variant(const wchar_t* value) : Variant() { vt = VT_BSTR; bstrVal = SysAllocString(value); if (!bstrVal) throw std::bad_alloc(); }
    ~Variant() { VariantClear(this); }
    Variant(const Variant&) = delete;
    Variant& operator=(const Variant&) = delete;
};
std::wstring fullPath(const std::wstring& p) {
    std::vector<wchar_t> b(32768);
    const auto n = GetFullPathNameW(p.c_str(), static_cast<DWORD>(b.size()), b.data(), nullptr);
    if (!n || n >= b.size()) throw std::runtime_error("invalid path");
    return std::wstring(b.data(), n);
}
std::wstring parent(const std::wstring& p) { return p.substr(0, p.find_last_of(L"\\/")); }
std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &result[0], n, nullptr, nullptr);
    return result;
}
std::string quote(const std::wstring& s) {
    std::ostringstream out; out << '"';
    for (unsigned char c : utf8(s)) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else out << c;
    }
    out << '"'; return out.str();
}
std::string number(double d) {
    if (!std::isfinite(d)) return "null";
    std::ostringstream out; out << std::setprecision(17) << d; return out.str();
}
std::string point(const AcGePoint3d& p) { return "[" + number(p.x) + "," + number(p.y) + "," + number(p.z) + "]"; }
std::wstring handleOf(AcDbObjectId id) {
    const AcDbHandle h = id.handle(); wchar_t b[32]{}; h.getIntoAsciiBuffer(b); return b;
}
void check(Acad::ErrorStatus es, const char* operation) {
    if (es != Acad::eOk) throw std::runtime_error(std::string(operation) + ": " + std::to_string(es));
}
struct CloseDbObject { void operator()(AcDbObject* p) const { if (p) p->close(); } };
template<class T> using Opened = std::unique_ptr<T, CloseDbObject>;
template<class T> Opened<T> open(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
    T* p{}; check(acdbOpenObject(p, id, mode), "open object"); return Opened<T>(p);
}
AcDbObjectId modelId(AcDbDatabase& db) {
    AcDbBlockTable* bt{}; check(db.getBlockTable(bt, AcDb::kForRead), "block table");
    Opened<AcDbBlockTable> table(bt); AcDbObjectId id;
    check(bt->getAt(ACDB_MODEL_SPACE, id), "model id"); return id;
}
AcDbObjectIdArray modelIds(AcDbDatabase& db) {
    auto model = open<AcDbBlockTableRecord>(modelId(db));
    AcDbBlockTableRecordIterator* raw{}; check(model->newIterator(raw), "iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> it(raw); AcDbObjectIdArray ids;
    for (; !it->done(); it->step()) { AcDbObjectId id; check(it->getEntityId(id), "entity id"); ids.append(id); }
    return ids;
}
std::string variantJson(const VARIANT& v) {
    switch (v.vt) {
    case VT_BSTR: return quote(v.bstrVal ? v.bstrVal : L"");
    case VT_R8: return number(v.dblVal);
    case VT_R4: return number(v.fltVal);
    case VT_I2: return std::to_string(v.iVal);
    case VT_I4: return std::to_string(v.lVal);
    case VT_UI4: return std::to_string(v.ulVal);
    case VT_I8: return std::to_string(v.llVal);
    case VT_UI8: return std::to_string(v.ullVal);
    case VT_BOOL: return v.boolVal ? "true" : "false";
    default: return "null";
    }
}
ComPtr<IDispatch> dispatchOf(AcDbObjectId id) {
    ComPtr<IDispatch> app(acedGetIDispatch(true));
    IUnknown* raw{};
    if (FAILED(AcAxGetIUnknownOfObject(&raw, id, app.get()))) return {};
    ComPtr<IUnknown> unknown(raw); IDispatch* result{};
    if (FAILED(unknown->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&result)))) return {};
    return ComPtr<IDispatch>(result);
}
HRESULT getProperty(IDispatch* d, const wchar_t* name, Variant& value) {
    if (!d) return E_NOINTERFACE;
    LPOLESTR member = const_cast<LPOLESTR>(name); DISPID id{};
    auto hr = d->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_INVARIANT, &id);
    if (FAILED(hr)) return hr;
    DISPPARAMS args{}; EXCEPINFO ex{};
    hr = d->Invoke(id, IID_NULL, LOCALE_INVARIANT, DISPATCH_PROPERTYGET, &args, &value, &ex, nullptr);
    SysFreeString(ex.bstrSource); SysFreeString(ex.bstrDescription); SysFreeString(ex.bstrHelpFile);
    return hr;
}
HRESULT putProperty(IDispatch* d, const wchar_t* name, Variant& value) {
    if (!d) return E_NOINTERFACE;
    LPOLESTR member = const_cast<LPOLESTR>(name); DISPID id{};
    auto hr = d->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_INVARIANT, &id);
    if (FAILED(hr)) return hr;
    DISPID named = DISPID_PROPERTYPUT; DISPPARAMS args{&value, &named, 1, 1}; EXCEPINFO ex{};
    hr = d->Invoke(id, IID_NULL, LOCALE_INVARIANT, DISPATCH_PROPERTYPUT, &args, nullptr, &ex, nullptr);
    SysFreeString(ex.bstrSource); SysFreeString(ex.bstrDescription); SysFreeString(ex.bstrHelpFile);
    return hr;
}
bool isSample(const std::wstring& h) {
    for (const auto* s : {L"CD83",L"CD84",L"CD89",L"CDC0",L"19A0",L"199F",L"19F3",L"17DF"})
        if (!_wcsicmp(h.c_str(), s)) return true;
    return false;
}
std::string entityJson(AcDbObjectId id, bool properties) {
    auto e = open<AcDbEntity>(id); const auto h = handleOf(id);
    AcDbExtents bounds; const auto extentStatus = e->getGeomExtents(bounds);
    AcString layerName; check(e->layer(layerName), "layer name");
    std::ostringstream out;
    out << "{\"handle\":" << quote(h) << ",\"type\":" << quote(e->isA()->name())
        << ",\"layer\":" << quote(layerName.kACharPtr()) << ",\"proxy\":" << (e->isAProxy() ? "true" : "false")
        << ",\"extentStatus\":" << extentStatus << ",\"min\":" << (extentStatus == Acad::eOk ? point(bounds.minPoint()) : "null")
        << ",\"max\":" << (extentStatus == Acad::eOk ? point(bounds.maxPoint()) : "null")
        << ",\"colorIndex\":" << e->colorIndex() << ",\"reactorCount\":" << (e->reactors() ? e->reactors()->length() : 0);
    if (auto* block = AcDbBlockReference::cast(e.get())) {
        auto record = open<AcDbBlockTableRecord>(block->blockTableRecord()); AcString name;
        check(record->getName(name), "block name");
        out << ",\"blockName\":" << quote(name.kACharPtr()) << ",\"position\":" << point(block->position());
    }
    e.reset();
    out << ",\"properties\":{";
    bool comma = false;
    if (properties) {
        auto d = dispatchOf(id);
        for (const auto* name : {L"Width",L"Height",L"Text",L"TextString",L"NameText",L"Scale",L"TextStyle",L"DimStyle",L"Label",L"GetKind",L"WallWidth",L"Thickness"}) {
            Variant value; const auto hr = getProperty(d.get(), name, value);
            if (SUCCEEDED(hr) && value.vt != VT_EMPTY && value.vt != VT_NULL) {
                if (comma) out << ','; comma = true;
                out << quote(name) << ':' << variantJson(value);
            }
        }
    }
    out << "}}"; return out.str();
}
std::string databaseJson(AcDbDatabase& db, bool allProperties) {
    const auto ids = modelIds(db); int proxies = 0; std::ostringstream entities;
    for (int i = 0; i < ids.length(); ++i) {
        { auto e = open<AcDbEntity>(ids[i]); if (e->isAProxy()) ++proxies; }
        if (i) entities << ',';
        entities << entityJson(ids[i], allProperties || isSample(handleOf(ids[i])));
    }
    return "\"insunits\":" + std::to_string(db.insunits()) + ",\"measurement\":" + std::to_string(db.measurement())
        + ",\"modelCount\":" + std::to_string(ids.length()) + ",\"proxies\":" + std::to_string(proxies) + ",\"entities\":[" + entities.str() + "]";
}
struct Paths { std::wstring stageRoot, scheme, reference, output; };
bool below(const std::wstring& path, const std::wstring& directory) {
    const auto prefix = fullPath(directory) + L"\\";
    return path.size() > prefix.size() && !_wcsnicmp(path.c_str(), prefix.c_str(), prefix.size());
}
Paths paths() {
    wchar_t module[32768]{};
    if (!GetModuleFileNameW(moduleHandle, module, 32768)) throw std::runtime_error("module path unavailable");
    const auto bin = parent(module); Paths p; p.stageRoot = parent(bin);
    std::wstring ini(module);
    const auto extension = ini.find_last_of(L'.');
    ini = (extension == std::wstring::npos ? ini : ini.substr(0, extension)) + L".ini";
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) ini = bin + L"\\probe.ini";
    wchar_t configuredRoot[32768]{};
    GetPrivateProfileStringW(L"probe", L"root", L"", configuredRoot, 32768, ini.c_str());
    if (configuredRoot[0]) p.stageRoot = fullPath(configuredRoot);
    const auto read = [&](const wchar_t* key) { wchar_t b[32768]{}; GetPrivateProfileStringW(L"probe", key, L"", b, 32768, ini.c_str()); if (!b[0]) throw std::runtime_error("missing probe.ini key"); return fullPath(b); };
    p.scheme = read(L"scheme"); p.reference = read(L"reference"); p.output = read(L"output");
    const auto snapshot = parent(p.stageRoot) + L"\\阶段0\\输入快照";
    if (!below(p.scheme, snapshot) || !below(p.reference, snapshot) || !below(p.output, p.stageRoot + L"\\runs"))
        throw std::runtime_error("path outside isolated probe directories");
    if (GetFileAttributesW(p.output.c_str()) == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("output directory absent");
    return p;
}
void writeFile(const std::wstring& path, const std::string& content) {
    FILE* f{}; if (_wfopen_s(&f, path.c_str(), L"wb") || !f) throw std::runtime_error("cannot create report");
    const auto written = fwrite(content.data(), 1, content.size(), f); const auto closed = fclose(f);
    if (written != content.size() || closed) throw std::runtime_error("report write incomplete");
}
std::unique_ptr<AcDbDatabase> readDb(const std::wstring& p) {
    auto db = std::make_unique<AcDbDatabase>(false, true);
    check(db->readDwgFile(p.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read DWG");
    check(db->closeInput(true), "close DWG input"); return db;
}
class WorkingDatabaseScope {
public:
    explicit WorkingDatabaseScope(AcDbDatabase* db)
        : services_(acdbHostApplicationServices()), previous_(services_->workingDatabase()) {
        services_->setWorkingDatabase(db);
    }
    ~WorkingDatabaseScope() { services_->setWorkingDatabase(previous_); }
    WorkingDatabaseScope(const WorkingDatabaseScope&) = delete;
    WorkingDatabaseScope& operator=(const WorkingDatabaseScope&) = delete;
private:
    AcDbHostApplicationServices* services_;
    AcDbDatabase* previous_;
};
void nativeRead(const Paths& p) {
    auto a = readDb(p.scheme); auto b = readDb(p.reference);
    const auto entry = [](const wchar_t* source, const std::wstring& path, AcDbDatabase& db) {
        WorkingDatabaseScope working(&db);
        return "{\"source\":" + quote(source) + ",\"path\":" + quote(path) + ",\"readStatus\":0,\"closeInputStatus\":0," + databaseJson(db, false) + "}";
    };
    writeFile(p.output + L"\\native-read.json", "{\"contract\":\"stage1.native.v1\",\"sdkMajor\":23,\"sdkMinor\":1,\"mscVer\":" + std::to_string(_MSC_VER)
        + ",\"databases\":[" + entry(L"scheme", p.scheme, *a) + "," + entry(L"reference", p.reference, *b) + "]}");
}
class TransactionScope {
public:
    explicit TransactionScope(AcDbDatabase& db) : manager_(db.transactionManager()) {
        if (!manager_) throw std::runtime_error("no transaction manager");
        transaction_ = manager_->startTransaction();
        if (!transaction_) throw std::runtime_error("cannot start transaction");
    }
    ~TransactionScope() { if (active_) manager_->abortTransaction(); }
    AcTransaction* get() const { return transaction_; }
    Acad::ErrorStatus commit() {
        const auto status = manager_->endTransaction();
        if (status == Acad::eOk) active_ = false;
        return status;
    }
    Acad::ErrorStatus abort() {
        const auto status = manager_->abortTransaction();
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
std::wstring layerOf(AcDbObjectId id) {
    auto e = open<AcDbEntity>(id); AcString name;
    check(e->layer(name), "layer name"); return name.kACharPtr();
}
std::string publicStyles(AcDbObjectId id) {
    auto d = dispatchOf(id); bool comma = false; std::ostringstream out; out << '{';
    for (const auto* name : {L"TextStyle",L"DimStyle",L"FontStyle"}) {
        Variant value;
        if (SUCCEEDED(getProperty(d.get(), name, value)) && value.vt != VT_EMPTY && value.vt != VT_NULL) {
            if (comma) out << ','; comma = true;
            out << quote(name) << ':' << variantJson(value);
        }
    }
    out << '}'; return out.str();
}
bool cohortSample(const wchar_t* source, const std::wstring& h) {
    const auto scheme = !_wcsicmp(source, L"scheme");
    const std::vector<const wchar_t*> handles = scheme
        ? std::vector<const wchar_t*>{L"CD83",L"CD84",L"CD89",L"CDC0"}
        : std::vector<const wchar_t*>{L"19A0",L"199F",L"19F3",L"17DF"};
    return std::any_of(handles.begin(), handles.end(), [&](const wchar_t* item) { return !_wcsicmp(h.c_str(), item); });
}
struct MappedSample {
    std::wstring sourceHandle, targetHandle, type, sourceLayer, targetLayer;
    std::string sourceStyles, targetStyles;
    AcDbObjectId targetId;
};
struct CloneCohort {
    std::wstring source;
    int sourceCount{}, mappedCount{};
    Acad::ErrorStatus status{Acad::eNotApplicable};
    AcDbObjectIdArray targetIds;
    std::vector<MappedSample> samples;
};
CloneCohort cloneCohort(const wchar_t* name, AcDbDatabase& source, AcDbDatabase& target) {
    CloneCohort result; result.source = name;
    const auto ids = modelIds(source); result.sourceCount = ids.length();
    AcDbIdMapping mapping;
    result.status = mapping.setDestDb(&target);
    if (result.status != Acad::eOk) return result;
    result.status = source.wblockCloneObjects(ids, modelId(target), mapping, AcDb::kDrcMangleName, false);
    if (result.status != Acad::eOk) return result;
    for (int i = 0; i < ids.length(); ++i) {
        AcDbIdPair pair; pair.setKey(ids[i]);
        if (!mapping.compute(pair) || !pair.isCloned() || pair.value().isNull()) continue;
        ++result.mappedCount; result.targetIds.append(pair.value());
        const auto h = handleOf(ids[i]);
        if (!cohortSample(name, h)) continue;
        MappedSample sample; sample.sourceHandle = h; sample.targetId = pair.value();
        sample.targetHandle = handleOf(pair.value());
        { auto entity = open<AcDbEntity>(pair.value()); sample.type = entity->isA()->name(); }
        sample.sourceLayer = layerOf(ids[i]); sample.targetLayer = layerOf(pair.value());
        sample.sourceStyles = publicStyles(ids[i]); sample.targetStyles = publicStyles(pair.value());
        result.samples.push_back(std::move(sample));
    }
    return result;
}
std::string cloneJson(const CloneCohort& c) {
    std::ostringstream out;
    out << "{\"source\":" << quote(c.source) << ",\"status\":" << c.status
        << ",\"sourceCount\":" << c.sourceCount << ",\"mappedCount\":" << c.mappedCount
        << ",\"duplicateRecordCloning\":\"kDrcMangleName\",\"sampleMap\":[";
    for (std::size_t i = 0; i < c.samples.size(); ++i) {
        const auto& s = c.samples[i]; if (i) out << ',';
        out << "{\"sourceHandle\":" << quote(s.sourceHandle) << ",\"targetHandle\":" << quote(s.targetHandle)
            << ",\"type\":" << quote(s.type) << ",\"sourceLayer\":" << quote(s.sourceLayer)
            << ",\"targetLayer\":" << quote(s.targetLayer) << ",\"styles\":{\"source\":" << s.sourceStyles
            << ",\"target\":" << s.targetStyles << "}}";
    }
    out << "]}"; return out.str();
}
const MappedSample& mappedSample(const CloneCohort& c, const wchar_t* handle) {
    const auto it = std::find_if(c.samples.begin(), c.samples.end(), [&](const MappedSample& s) { return !_wcsicmp(s.sourceHandle.c_str(), handle); });
    if (it == c.samples.end()) throw std::runtime_error("sample clone mapping absent");
    return *it;
}
std::string translateCohort(AcDbDatabase& target, const AcDbObjectIdArray& ids) {
    TransactionScope transaction(target);
    const auto matrix = AcGeMatrix3d::translation(AcGeVector3d(100000.0, 0.0, 0.0));
    int attempted = 0, passed = 0; std::ostringstream statuses;
    for (int i = 0; i < ids.length(); ++i) {
        AcDbObject* object{}; auto status = transaction.get()->getObject(object, ids[i], AcDb::kForWrite);
        ++attempted;
        if (status == Acad::eOk) {
            auto* entity = AcDbEntity::cast(object);
            status = entity ? entity->transformBy(matrix) : Acad::eWrongObjectType;
        }
        if (status == Acad::eOk) ++passed;
        if (i) statuses << ',';
        statuses << "{\"handle\":" << quote(handleOf(ids[i])) << ",\"status\":" << status << '}';
    }
    const bool allCallsPassed = attempted == passed;
    const auto finalStatus = allCallsPassed ? transaction.commit() : transaction.abort();
    std::ostringstream out;
    out << "{\"attempted\":" << attempted << ",\"passed\":" << passed
        << ",\"offset\":[100000,0,0],\"applied\":" << ((allCallsPassed && finalStatus == Acad::eOk) ? "true" : "false")
        << ",\"transactionStatus\":" << finalStatus << ",\"statuses\":[" << statuses.str() << "]}";
    check(finalStatus, "finish transform transaction"); return out.str();
}
std::string rollbackObservation(AcDbDatabase& target, AcDbObjectId id) {
    Adesk::UInt16 before{};
    { auto entity = open<AcDbEntity>(id); before = entity->colorIndex(); }
    const Adesk::UInt16 trial = before == 1 ? 2 : 1;
    TransactionScope transaction(target); AcDbObject* object{};
    check(transaction.get()->getObject(object, id, AcDb::kForWrite), "rollback open");
    auto* entity = AcDbEntity::cast(object);
    if (!entity) throw std::runtime_error("rollback sample not entity");
    const auto putStatus = entity->setColorIndex(trial);
    const auto during = entity->colorIndex();
    const auto abortStatus = transaction.abort();
    check(abortStatus, "abort color trial");
    Adesk::UInt16 after{};
    { auto reopened = open<AcDbEntity>(id); after = reopened->colorIndex(); }
    return "{\"handle\":" + quote(handleOf(id)) + ",\"before\":" + std::to_string(before)
        + ",\"trial\":" + std::to_string(trial) + ",\"during\":" + std::to_string(during)
        + ",\"writeStatus\":" + std::to_string(putStatus) + ",\"abortStatus\":" + std::to_string(abortStatus)
        + ",\"after\":" + std::to_string(after) + "}";
}
std::string nativeRollbackControl() {
    AcDbDatabase control(true, true); control.disableUndoRecording(false);
    WorkingDatabaseScope working(&control);
    AcDbObjectId id;
    {
        auto model = open<AcDbBlockTableRecord>(modelId(control), AcDb::kForWrite);
        auto line = std::make_unique<AcDbLine>(AcGePoint3d(0, 0, 0), AcGePoint3d(100, 0, 0));
        line->setDatabaseDefaults(&control);
        check(model->appendAcDbEntity(id, line.get()), "append native rollback control");
        auto* resident = line.release(); check(resident->close(), "close native rollback control");
    }
    return rollbackObservation(control, id);
}
struct EditObservation {
    std::wstring sourceHandle, targetHandle, property;
    std::string expected, before{"null"}, immediate{"null"}, actual{"null"};
    HRESULT status{E_FAIL}, beforeStatus{E_FAIL}, immediateStatus{E_FAIL}, reopenGetStatus{E_FAIL};
};
EditObservation editProperty(const MappedSample& sample, const wchar_t* name, Variant& expected) {
    EditObservation result; result.sourceHandle = sample.sourceHandle; result.targetHandle = sample.targetHandle;
    result.property = name; result.expected = variantJson(expected);
    auto d = dispatchOf(sample.targetId);
    { Variant before; result.beforeStatus = getProperty(d.get(), name, before); if (SUCCEEDED(result.beforeStatus)) result.before = variantJson(before); }
    result.status = putProperty(d.get(), name, expected);
    { Variant actual; result.immediateStatus = getProperty(d.get(), name, actual); if (SUCCEEDED(result.immediateStatus)) result.immediate = variantJson(actual); }
    return result;
}
void readPersistedEdit(AcDbDatabase& db, EditObservation& edit) {
    AcDbObjectId id;
    const auto lookupStatus = db.getAcDbObjectId(id, false, AcDbHandle(edit.targetHandle.c_str()));
    if (lookupStatus != Acad::eOk) { edit.reopenGetStatus = E_FAIL; return; }
    auto d = dispatchOf(id); Variant actual;
    edit.reopenGetStatus = getProperty(d.get(), edit.property.c_str(), actual);
    if (SUCCEEDED(edit.reopenGetStatus)) edit.actual = variantJson(actual);
}
std::string editJson(const EditObservation& e) {
    std::ostringstream out;
    out << "{\"sourceHandle\":" << quote(e.sourceHandle) << ",\"targetHandle\":" << quote(e.targetHandle)
        << ",\"property\":" << quote(e.property) << ",\"expected\":" << e.expected
        << ",\"before\":" << e.before << ",\"beforeStatus\":" << static_cast<long>(e.beforeStatus)
        << ",\"status\":" << static_cast<long>(e.status) << ",\"immediate\":" << e.immediate
        << ",\"immediateStatus\":" << static_cast<long>(e.immediateStatus) << ",\"actual\":" << e.actual
        << ",\"reopenGetStatus\":" << static_cast<long>(e.reopenGetStatus) << '}';
    return out.str();
}
void requireNewFile(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) throw std::runtime_error("probe output already exists; use a new run directory");
    const auto error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw std::runtime_error("cannot establish new output path");
}
void nativeRoundtrip(const Paths& p) {
    const auto dwg = fullPath(p.output + L"\\clone-cohort.dwg");
    const auto report = fullPath(p.output + L"\\roundtrip.json");
    if (!below(dwg, p.output) || !below(report, p.output)) throw std::runtime_error("roundtrip output outside run");
    requireNewFile(dwg); requireNewFile(report);
    std::string cloneEntries = "[]", transform = "null", rollback = "null", reopenedJson = "null";
    std::string saveStatus = "null", reopenStatus = "null", closeInputStatus = "null", failure = "null";
    std::vector<EditObservation> edits;
    bool completed = false;
    try {
        auto scheme = readDb(p.scheme); auto reference = readDb(p.reference);
        auto target = std::make_unique<AcDbDatabase>(true, true);
        target->disableUndoRecording(false);
        CloneCohort schemeClone, referenceClone;
        {
            WorkingDatabaseScope working(target.get());
            schemeClone = cloneCohort(L"scheme", *scheme, *target);
            referenceClone = cloneCohort(L"reference", *reference, *target);
            cloneEntries = "[" + cloneJson(schemeClone) + "," + cloneJson(referenceClone) + "]";
            check(schemeClone.status, "scheme clone"); check(referenceClone.status, "reference clone");
            if (schemeClone.mappedCount != schemeClone.sourceCount || referenceClone.mappedCount != referenceClone.sourceCount)
                throw std::runtime_error("not all top-level entities were mapped");
            transform = translateCohort(*target, schemeClone.targetIds);
            const auto& window = mappedSample(schemeClone, L"CD84");
            const auto& text = mappedSample(schemeClone, L"CDC0");
            rollback = rollbackObservation(*target, window.targetId);
            rollback.pop_back(); rollback += ",\"nativeControl\":" + nativeRollbackControl() + "}";
            Variant width(2500.0); edits.push_back(editProperty(window, L"Width", width));
            Variant title(L"庭院 [S1]"); edits.push_back(editProperty(text, L"Text", title));
            requireNewFile(dwg);
            const auto status = target->saveAs(dwg.c_str(), false, AcDb::kDHL_CURRENT);
            saveStatus = std::to_string(status); check(status, "save clone cohort");
        }
        // Destroy the in-memory result before reading the saved file into a fresh database.
        target.reset();
        auto reopened = std::make_unique<AcDbDatabase>(false, true);
        const auto readStatus = reopened->readDwgFile(dwg.c_str(), AcDbDatabase::kForReadAndAllShare, false);
        reopenStatus = std::to_string(readStatus); check(readStatus, "reopen saved cohort");
        const auto closeStatus = reopened->closeInput(true);
        closeInputStatus = std::to_string(closeStatus); check(closeStatus, "close reopened cohort input");
        {
            WorkingDatabaseScope working(reopened.get());
            for (auto& edit : edits) readPersistedEdit(*reopened, edit);
            reopenedJson = "{" + databaseJson(*reopened, true) + "}";
        }
        completed = true;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        failure = quote(std::wstring(message.begin(), message.end()));
    } catch (...) { failure = quote(L"unknown C++ exception"); }
    std::ostringstream editArray;
    for (std::size_t i = 0; i < edits.size(); ++i) { if (i) editArray << ','; editArray << editJson(edits[i]); }
    writeFile(report, "{\"contract\":\"stage1.roundtrip.v1\",\"cloneEntries\":" + cloneEntries
        + ",\"transform\":" + transform + ",\"rollback\":" + rollback + ",\"propertyEdits\":[" + editArray.str()
        + "],\"saveStatus\":" + saveStatus + ",\"reopenStatus\":" + reopenStatus + ",\"closeInputStatus\":" + closeInputStatus
        + ",\"reopened\":" + reopenedJson + ",\"associationStatus\":\"not validated: private Tianzheng relationships not inspected\",\"styleDefinitionStatus\":\"public style names only; full definitions not validated\",\"completed\":"
        + (completed ? "true" : "false") + ",\"error\":" + failure + "}");
    if (!completed) throw std::runtime_error("roundtrip incomplete; inspect roundtrip.json");
}
void roundtripCommand() {
    try { nativeRoundtrip(paths()); acutPrintf(L"\nSTAGE1_ROUNDTRIP_COMPLETED; ASSERTIONS_PENDING\n"); }
    catch (const std::exception& ex) { acutPrintf(L"\nSTAGE1_ROUNDTRIP_FAILED: %hs\n", ex.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_ROUNDTRIP_FAILED: unknown C++ exception\n"); }
}
void readCommand() {
    try { nativeRead(paths()); acutPrintf(L"\nSTAGE1_READ_OK\n"); }
    catch (const std::exception& ex) { acutPrintf(L"\nSTAGE1_READ_FAILED: %hs\n", ex.what()); }
    catch (...) { acutPrintf(L"\nSTAGE1_READ_FAILED: unknown C++ exception\n"); }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { moduleHandle = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    if (message == AcRx::kInitAppMsg) {
        acrxDynamicLinker->unlockApplication(id); acrxDynamicLinker->registerAppMDIAware(id);
        const auto readStatus = acedRegCmds->addCommand(L"STAGE1_NATIVE", L"S1READ", L"S1READ", ACRX_CMD_MODAL, readCommand);
        const auto roundtripStatus = readStatus == Acad::eOk
            ? acedRegCmds->addCommand(L"STAGE1_NATIVE", L"S1ROUNDTRIP", L"S1ROUNDTRIP", ACRX_CMD_MODAL, roundtripCommand)
            : readStatus;
        if (readStatus != Acad::eOk || roundtripStatus != Acad::eOk) {
            acedRegCmds->removeGroup(L"STAGE1_NATIVE");
            acutPrintf(L"\nSTAGE1_PROBE_REGISTRATION_FAILED read=%d roundtrip=%d\n", readStatus, roundtripStatus);
            return AcRx::kRetError;
        }
        acutPrintf(L"\nSTAGE1_PROBE_LOADED msc=%d SDK=23.1\n", _MSC_VER);
    } else if (message == AcRx::kUnloadAppMsg) {
        acedRegCmds->removeGroup(L"STAGE1_NATIVE"); acutPrintf(L"\nSTAGE1_PROBE_UNLOADED\n");
    }
    return AcRx::kRetOK;
}
