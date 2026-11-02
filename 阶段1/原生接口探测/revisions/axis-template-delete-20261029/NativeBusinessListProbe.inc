// Include inside NativeAccessProbe.cpp's anonymous namespace after its helpers.
// Uses only the existing SDK headers/libs. AcDbEntity::list() is a public void
// observation; this probe does not claim that Tianzheng exposes business fields.

struct BusinessResultRelease { void operator()(resbuf* value) const noexcept { if (value) acutRelRb(value); } };
using BusinessResult = std::unique_ptr<resbuf, BusinessResultRelease>;
struct BusinessCall {
    bool returned{}; int value{};
    void set(int actual) { value = actual; returned = true; }
    std::string json() const { return returned ? std::to_string(value) : "null"; }
    bool normal() const { return returned && value == RTNORM; }
};
struct BusinessVariable {
    BusinessCall status;
    int type{}, integer{};
    bool textKnown{}, integerKnown{};
    std::wstring text;
    std::string json() const {
        return "{\"getStatus\":" + status.json() + ",\"restype\":" + (status.returned ? std::to_string(type) : "null")
            + ",\"value\":" + (textKnown ? quote(text) : integerKnown ? std::to_string(integer) : "null") + '}';
    }
};
BusinessVariable businessGetVariable(const wchar_t* name) {
    // A heap result buffer lets acutRelRb release both the buffer and any string
    // allocated by acedGetVar. A stack result buffer must not be released this way.
    BusinessResult result(acutNewRb(RTNONE)); if (!result) throw std::runtime_error("allocate native business-list result buffer");
    result->rbnext = nullptr; result->resval.rstring = nullptr;
    BusinessVariable value; value.status.set(acedGetVar(name, result.get())); value.type = result->restype;
    if (value.status.normal() && value.type == RTSTR && result->resval.rstring) { value.text = result->resval.rstring; value.textKnown = true; }
    if (value.status.normal() && value.type == RTSHORT) { value.integer = result->resval.rint; value.integerKnown = true; }
    return value;
}
int businessSetString(const wchar_t* name, const std::wstring& value) {
    BusinessResult argument(acutBuildList(RTSTR, value.c_str(), RTNONE)); if (!argument) throw std::runtime_error("allocate native business-list string argument");
    return acedSetVar(name, argument.get());
}
int businessSetInteger(const wchar_t* name, short value) {
    resbuf argument{}; argument.restype = RTSHORT; argument.resval.rint = value; return acedSetVar(name, &argument);
}
bool businessSamePath(const std::wstring& a, const std::wstring& b) {
    if (a.empty() || b.empty()) return false;
    auto aa = fullPath(a), bb = fullPath(b);
    while (aa.size() > 3 && (aa.back() == L'\\' || aa.back() == L'/')) aa.pop_back();
    while (bb.size() > 3 && (bb.back() == L'\\' || bb.back() == L'/')) bb.pop_back();
    return !_wcsicmp(aa.c_str(), bb.c_str());
}
std::string businessError(const std::exception& failure) { const std::string message(failure.what()); return quote(std::wstring(message.begin(), message.end())); }

class NativeBusinessLoggingScope {
public:
    NativeBusinessLoggingScope(const Paths& paths, AcApDocument* document, AcDbDatabase* database)
        : paths_(paths), document_(document), database_(database) {}
    NativeBusinessLoggingScope(const NativeBusinessLoggingScope&) = delete;
    NativeBusinessLoggingScope& operator=(const NativeBusinessLoggingScope&) = delete;
    ~NativeBusinessLoggingScope() noexcept { restore(); }
    void activate() {
        beforeMode_ = businessGetVariable(L"LOGFILEMODE"); beforePath_ = businessGetVariable(L"LOGFILEPATH"); beforeName_ = businessGetVariable(L"LOGFILENAME");
        if (!beforeMode_.integerKnown || beforeMode_.integer != 0)
            throw std::runtime_error("business-LIST refused: original LOGFILEMODE is not confirmed zero; existing user logging was not interrupted");
        if (!beforePath_.textKnown || beforePath_.text.empty() || !beforeName_.textKnown)
            throw std::runtime_error("business-LIST could not capture original logging path/name; no setting changed");
        pathSetAttempted_ = true; pathSetStatus_.set(businessSetString(L"LOGFILEPATH", fullPath(paths_.output) + L"\\"));
        if (!pathSetStatus_.normal()) throw std::runtime_error("business-LIST temporary LOGFILEPATH setter failed");
        pathReadback_ = businessGetVariable(L"LOGFILEPATH"); nameBeforeEnable_ = businessGetVariable(L"LOGFILENAME");
        preEnablePathSafe_ = pathReadback_.textKnown && businessSamePath(pathReadback_.text, paths_.output)
            && nameBeforeEnable_.textKnown && !nameBeforeEnable_.text.empty() && below(fullPath(nameBeforeEnable_.text), paths_.output)
            && businessSamePath(parent(fullPath(nameBeforeEnable_.text)), paths_.output);
        if (!preEnablePathSafe_) throw std::runtime_error("business-LIST refused: LOGFILENAME is not confirmed inside this run before enabling; logging was not enabled");
        logPath_ = fullPath(nameBeforeEnable_.text);
        const auto attributes = GetFileAttributesW(logPath_.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES) throw std::runtime_error("business-LIST refused: prechecked log target already exists");
        preEnableFileError_ = GetLastError(); fileCheckReturned_ = true;
        if (preEnableFileError_ != ERROR_FILE_NOT_FOUND) throw std::runtime_error("business-LIST could not prove the target log is a new file in the existing run directory");
        preEnableFileAbsent_ = true;
        enableAttempted_ = true; enableStatus_.set(businessSetInteger(L"LOGFILEMODE", 1));
        if (!enableStatus_.normal()) throw std::runtime_error("business-LIST log enabling failed");
        modeAfterEnable_ = businessGetVariable(L"LOGFILEMODE"); nameAfterEnable_ = businessGetVariable(L"LOGFILENAME");
        if (!modeAfterEnable_.integerKnown || modeAfterEnable_.integer != 1 || !nameAfterEnable_.textKnown
            || !businessSamePath(nameAfterEnable_.text, logPath_) || !below(fullPath(nameAfterEnable_.text), paths_.output))
            throw std::runtime_error("business-LIST enabled logging did not retain the prechecked isolated filename/mode; no entity was listed");
    }
    void restore() noexcept {
        if (restoreAttempted_ || (!pathSetAttempted_ && !enableAttempted_)) return;
        restoreAttempted_ = true;
        try {
            if (enableAttempted_) {
                // No database object is open here. LOGFILEOFF's documented effect
                // is to close the history file; never send it in another document.
                const bool owned = acDocManager && !acDocManager->isApplicationContext() && document_ && database_
                    && acDocManager->curDocument() == document_ && acDocManager->mdiActiveDocument() == document_
                    && document_->database() == database_ && acdbHostApplicationServices()->workingDatabase() == database_;
                if (owned) {
                    closeAttempted_ = true;
                    try { closeStatus_.set(acedCommandS(RTSTR, L"_.LOGFILEOFF", RTNONE)); }
                    catch (...) { restoreThrew_ = true; } // Still attempt original zero below.
                }
                modeAfterClose_ = businessGetVariable(L"LOGFILEMODE");
                if (!modeAfterClose_.integerKnown || modeAfterClose_.integer != 0) {
                    // Restore the original global zero even if command-context
                    // closing failed; do not redirect an active log to oldPath.
                    emergencyOffStatus_.set(businessSetInteger(L"LOGFILEMODE", 0)); modeAfterClose_ = businessGetVariable(L"LOGFILEMODE");
                }
            } else modeAfterClose_ = businessGetVariable(L"LOGFILEMODE");
            if (!modeAfterClose_.integerKnown || modeAfterClose_.integer != 0) return;
            pathRestoreStatus_.set(businessSetString(L"LOGFILEPATH", beforePath_.text)); pathRestored_ = businessGetVariable(L"LOGFILEPATH");
            modeRestoreStatus_.set(businessSetInteger(L"LOGFILEMODE", 0)); modeRestored_ = businessGetVariable(L"LOGFILEMODE");
            restored_ = pathRestoreStatus_.normal() && modeRestoreStatus_.normal() && pathRestored_.textKnown
                && !_wcsicmp(pathRestored_.text.c_str(), beforePath_.text.c_str()) && modeRestored_.integerKnown && modeRestored_.integer == 0;
        } catch (...) { restoreThrew_ = true; }
    }
    bool restored() const { return restored_; }
    bool closeSucceeded() const { return closeStatus_.normal(); }
    const std::wstring& logPath() const { return logPath_; }
    std::string json() const {
        std::ostringstream out;
        out << "{\"beforeMode\":" << beforeMode_.json() << ",\"beforePath\":" << beforePath_.json() << ",\"beforeName\":" << beforeName_.json()
            << ",\"pathSetAttempted\":" << (pathSetAttempted_ ? "true" : "false") << ",\"pathSetStatus\":" << pathSetStatus_.json()
            << ",\"pathReadback\":" << pathReadback_.json() << ",\"nameBeforeEnable\":" << nameBeforeEnable_.json()
            << ",\"preEnablePathSafe\":" << (preEnablePathSafe_ ? "true" : "false") << ",\"preEnableFileAbsent\":" << (preEnableFileAbsent_ ? "true" : "false")
            << ",\"preEnableFileCheckError\":" << (fileCheckReturned_ ? std::to_string(preEnableFileError_) : "null")
            << ",\"enableAttempted\":" << (enableAttempted_ ? "true" : "false") << ",\"enableStatus\":" << enableStatus_.json()
            << ",\"modeAfterEnable\":" << modeAfterEnable_.json() << ",\"nameAfterEnable\":" << nameAfterEnable_.json()
            << ",\"closeAttempted\":" << (closeAttempted_ ? "true" : "false") << ",\"closeCommandStatus\":" << closeStatus_.json()
            << ",\"emergencyOffStatus\":" << emergencyOffStatus_.json() << ",\"modeAfterClose\":" << modeAfterClose_.json()
            << ",\"restoreAttempted\":" << (restoreAttempted_ ? "true" : "false") << ",\"pathRestoreStatus\":" << pathRestoreStatus_.json() << ",\"pathRestored\":" << pathRestored_.json()
            << ",\"modeRestoreStatus\":" << modeRestoreStatus_.json() << ",\"modeRestored\":" << modeRestored_.json()
            << ",\"restored\":" << (restored_ ? "true" : "false") << ",\"restoreThrew\":" << (restoreThrew_ ? "true" : "false") << '}'; return out.str();
    }
private:
    Paths paths_; AcApDocument* document_{}; AcDbDatabase* database_{}; std::wstring logPath_;
    BusinessVariable beforeMode_, beforePath_, beforeName_, pathReadback_, nameBeforeEnable_, modeAfterEnable_, nameAfterEnable_, modeAfterClose_, pathRestored_, modeRestored_;
    BusinessCall pathSetStatus_, enableStatus_, closeStatus_, emergencyOffStatus_, pathRestoreStatus_, modeRestoreStatus_;
    bool pathSetAttempted_{}, preEnablePathSafe_{}, preEnableFileAbsent_{}, enableAttempted_{}, closeAttempted_{}, restoreAttempted_{}, restored_{}, restoreThrew_{};
    DWORD preEnableFileError_{}; bool fileCheckReturned_{};
};

std::string businessGeometryJson(const AcDbObjectIdArray& ids) {
    std::ostringstream out; out << '[';
    for (int i = 0; i < ids.length(); ++i) {
        if (i) out << ',';
        auto entity = open<AcDbEntity>(ids[i]); AcString layer; check(entity->layer(layer), "business-LIST compact layer");
        AcDbExtents extent; const auto extentStatus = entity->getGeomExtents(extent);
        AcGePoint3dArray grip, stretch; AcDbIntArray snaps, geometry;
        const auto gripStatus = entity->getGripPoints(grip, snaps, geometry), stretchStatus = entity->getStretchPoints(stretch);
        out << "{\"handle\":" << quote(handleOf(ids[i])) << ",\"type\":" << quote(entity->isA()->name()) << ",\"layer\":" << quote(layer.kACharPtr())
            << ",\"proxy\":" << (entity->isAProxy() ? "true" : "false") << ",\"extentStatus\":" << extentStatus
            << ",\"min\":" << (extentStatus == Acad::eOk ? point(extent.minPoint()) : "null") << ",\"max\":" << (extentStatus == Acad::eOk ? point(extent.maxPoint()) : "null")
            << ",\"curve\":" << curveJson(*entity) << ",\"grip\":{\"status\":" << gripStatus << ",\"points\":" << points(grip)
            << ",\"osnapModes\":" << integers(snaps) << ",\"geomIds\":" << integers(geometry) << "},\"stretch\":{\"status\":" << stretchStatus << ",\"points\":" << points(stretch) << "}}";
    }
    out << ']'; return out.str();
}

std::string businessListSample(AcDbDatabase& database, const wchar_t* handle, const wchar_t* expectedType, bool& succeeded) {
    std::string lookupStatus{"null"}, openStatus{"null"}, type{"null"}, beginMarker{"null"}, endMarker{"null"}, error{"null"};
    std::string beginPrint{"null"}, endPrint{"null"}; bool invoked{}, returned{}; succeeded = false;
    try {
        AcDbObjectId id; const auto lookup = database.getAcDbObjectId(id, false, AcDbHandle(handle)); lookupStatus = std::to_string(lookup); check(lookup, "find business-LIST sample");
        AcDbEntity* raw{}; const auto opened = acdbOpenObject(raw, id, AcDb::kForRead); openStatus = std::to_string(opened); Opened<AcDbEntity> entity(raw); check(opened, "open business-LIST sample read-only");
        if (!entity) throw std::runtime_error("business-LIST read-open returned no entity");
        const auto actualType = std::wstring(entity->isA()->name()); type = quote(actualType);
        if (_wcsicmp(actualType.c_str(), expectedType)) throw std::runtime_error("business-LIST native sample type differs from the fixed observation fixture");
        const auto begin = L"[S1BUSINESSLIST_BEGIN handle=" + std::wstring(handle) + L" type=" + actualType + L"]";
        const auto end = L"[S1BUSINESSLIST_END handle=" + std::wstring(handle) + L" type=" + actualType + L"]";
        beginMarker = quote(begin); endMarker = quote(end); beginPrint = std::to_string(acutPrintf(L"\n%ls\n", begin.c_str()));
        invoked = true; entity->list(); returned = true; // list() has no Acad status.
        endPrint = std::to_string(acutPrintf(L"\n%ls\n", end.c_str())); succeeded = true;
    } catch (const std::exception& failure) { error = businessError(failure); }
      catch (...) { error = quote(L"unknown native business-LIST sample exception"); }
    std::ostringstream out;
    out << "{\"handle\":" << quote(handle) << ",\"expectedType\":" << quote(expectedType) << ",\"type\":" << type
        << ",\"lookupStatus\":" << lookupStatus << ",\"openStatus\":" << openStatus << ",\"listInvoked\":" << (invoked ? "true" : "false")
        << ",\"listReturned\":" << (returned ? "true" : "false") << ",\"beginMarker\":" << beginMarker << ",\"endMarker\":" << endMarker
        << ",\"beginPrintResult\":" << beginPrint << ",\"endPrintResult\":" << endPrint << ",\"businessReadStatus\":\"observation only\",\"error\":" << error << '}'; return out.str();
}

struct BusinessFileObservation { std::string json; bool read{}; };
struct BusinessFileRelease { void operator()(void* handle) const noexcept { if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); } };
BusinessFileObservation businessLogFile(const std::wstring& path) {
    bool observed{}, read{}; std::string status{"null"}, bytes{"null"}, bom{"unknown"};
    if (!path.empty()) {
        const auto attributes = GetFileAttributesW(path.c_str()); observed = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
        if (observed) {
            const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) status = std::to_string(GetLastError());
            else {
                std::unique_ptr<void, BusinessFileRelease> owner(handle);
                LARGE_INTEGER size{}; unsigned char prefix[4]{}; DWORD count{};
                if (!GetFileSizeEx(handle, &size)) status = std::to_string(GetLastError());
                else {
                    bytes = std::to_string(size.QuadPart);
                    if (!ReadFile(handle, prefix, static_cast<DWORD>(sizeof(prefix)), &count, nullptr)) status = std::to_string(GetLastError());
                    else {
                        status = "0"; read = true; bom = "none";
                        if (count >= 3 && prefix[0] == 0xEF && prefix[1] == 0xBB && prefix[2] == 0xBF) bom = "UTF-8";
                        else if (count >= 2 && prefix[0] == 0xFF && prefix[1] == 0xFE) bom = "UTF-16LE";
                        else if (count >= 2 && prefix[0] == 0xFE && prefix[1] == 0xFF) bom = "UTF-16BE";
                    }
                }
            }
        } else status = attributes == INVALID_FILE_ATTRIBUTES ? std::to_string(GetLastError()) : std::to_string(ERROR_DIRECTORY);
    }
    return {"{\"path\":" + (path.empty() ? std::string("null") : quote(path)) + ",\"observed\":" + (observed ? "true" : "false")
        + ",\"readApi\":\"Win32 CreateFileW/GetFileSizeEx/ReadFile prefix only; raw file preserved\",\"readStatus\":" + status
        + ",\"bytes\":" + bytes + ",\"bom\":" + quote(std::wstring(bom.begin(), bom.end())) + ",\"ansiCodePage\":" + std::to_string(GetACP()) + '}', read};
}

void nativeBusinessListProbe() {
    try {
        const auto p = paths(); const auto report = fullPath(p.output + L"\\business-list-native.json"), control = fullPath(p.output + L"\\control.dwg"); requireNewFile(report);
        std::string error{"null"}, before{"[]"}, after{"[]"}, samples, logging{"null"}, current{"null"}, mdi{"null"}, working{"null"};
        int initialCount = -1, finalCount = -1; AcApDocument* document{}; AcDbDatabase* database{}; AcDbObjectIdArray originalIds;
        std::unique_ptr<NativeBusinessLoggingScope> logger;
        const auto guard = [&] {
            if (!acDocManager || acDocManager->isApplicationContext() || !document || !database || acDocManager->curDocument() != document
                || acDocManager->mdiActiveDocument() != document || document->database() != database || acdbHostApplicationServices()->workingDatabase() != database
                || !document->fileName() || !businessSamePath(document->fileName(), control) || !below(control, p.output)) throw std::runtime_error("business-LIST requires owned current/MDI/working control document");
            const ACHAR* filename{}; check(database->getFilename(filename), "business-LIST native filename"); if (!filename || !businessSamePath(filename, control)) throw std::runtime_error("business-LIST native filename guard failed");
            if (database->transactionManager() && database->transactionManager()->numActiveTransactions() != 0) throw std::runtime_error("business-LIST requires no active transaction");
        };
        try {
            if (!acDocManager) throw std::runtime_error("business-LIST document manager unavailable"); document = acDocManager->curDocument(); database = document ? document->database() : nullptr; guard();
            current = quote(fullPath(document->fileName())); mdi = quote(fullPath(acDocManager->mdiActiveDocument()->fileName())); const ACHAR* filename{}; check(database->getFilename(filename), "business-LIST working filename"); working = filename ? quote(fullPath(filename)) : "null";
            originalIds = modelIds(*database); initialCount = originalIds.length(); if (initialCount != 213) throw std::runtime_error("business-LIST requires the full frozen scheme213 control fixture");
            before = businessGeometryJson(originalIds); logger = std::make_unique<NativeBusinessLoggingScope>(p, document, database); logger->activate();
            const wchar_t* handles[]{ L"CD83", L"CD84", L"CD89", L"CD94", L"CF5F", L"CE8F", L"CDC0", L"CFCE" };
            const wchar_t* types[]{ L"TDbWall", L"TDbOpening", L"TDbOpening", L"TDbGroupOpening", L"TDbRectCornerStair", L"TDbSymbElevation", L"TDbText", L"TDbDrawingName" };
            bool allReturned = true;
            for (int i = 0; i < 8; ++i) { guard(); bool returned{}; if (i) samples += ','; samples += businessListSample(*database, handles[i], types[i], returned); allReturned = allReturned && returned; guard(); }
            if (!allReturned) error = quote(L"one or more native LIST invocations did not return; inspect sample errors and raw marker pairs");
        } catch (const std::exception& failure) { error = businessError(failure); }
          catch (...) { error = quote(L"unknown native business-LIST observation exception"); }
        if (logger) {
            logger->restore(); logging = logger->json();
            if (!logger->restored() && error == "null") error = quote(L"business-LIST logging restoration was not verified");
            if (!logger->closeSucceeded() && error == "null") error = quote(L"business-LIST LOGFILEOFF did not return RTNORM; file closure not verified");
        }
        try { guard(); const auto ids = modelIds(*database); finalCount = ids.length(); after = businessGeometryJson(ids); }
        catch (const std::exception& failure) { if (error == "null") error = businessError(failure); }
        const auto file = businessLogFile(logger ? logger->logPath() : L""); if (!file.read && error == "null") error = quote(L"business-LIST raw log file could not be observed/read after closure");
        std::ostringstream out;
        out << "{\"contract\":\"stage1.business-list-native.v1\",\"api\":\"ObjectARX native; no COM\",\"businessReadStatus\":\"observation only\""
            << ",\"controlDWG\":" << quote(control) << ",\"currentDWG\":" << current << ",\"mdiActiveDWG\":" << mdi << ",\"workingDWG\":" << working
            << ",\"initialModelCount\":" << initialCount << ",\"finalModelCount\":" << finalCount << ",\"dwgSaved\":false,\"originalBefore\":" << before << ",\"originalAfter\":" << after
            << ",\"logging\":" << logging << ",\"samples\":[" << samples << "],\"logFile\":" << file.json
            << ",\"interpretation\":\"capture success does not establish Width/Height/Kind availability; analyze only each object's actual text between its markers\",\"error\":" << error << '}';
        writeNewReport(report, out.str()); acutPrintf(L"\nSTAGE1_NATIVE_BUSINESS_LIST_COMPLETED; OBSERVATION_ONLY\n");
    } catch (const std::exception& failure) { acutPrintf(L"\nSTAGE1_NATIVE_BUSINESS_LIST_FAILED: %hs\n", failure.what()); }
      catch (...) { acutPrintf(L"\nSTAGE1_NATIVE_BUSINESS_LIST_FAILED: unknown C++ exception\n"); }
}
