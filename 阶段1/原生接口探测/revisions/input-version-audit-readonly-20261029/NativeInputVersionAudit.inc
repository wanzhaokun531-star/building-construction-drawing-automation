// Include after NativeTextBridgeProbe.inc and NativeStyleAudit.inc in the same
// anonymous namespace. No new SDK headers/libs, no editor commands or sysvars.
// Only frozen p.scheme and this run's observed-original.dwg are read. The
// original user's file path is deliberately neither accepted nor opened here.

std::string inputVersionAuditSource(const wchar_t* role, const std::wstring& path) {
    std::string readStatus{"null"}, closeStatus{"null"}, error{"null"}, entities{"[]"}; int count = -1;
    {
        AcDbDatabase source(false, true);
        const auto read = source.readDwgFile(path.c_str(), AcDbDatabase::kForReadAndAllShare, false); readStatus = std::to_string(read);
        try {
            check(read, "read input-version side database");
            const auto closed = source.closeInput(true); closeStatus = std::to_string(closed); check(closed, "close input-version input stream");
            WorkingDatabaseScope working(source); const auto ids = modelIds(source); count = ids.length();
            std::ostringstream values; values << '['; bool incomplete{};
            for (int i = 0; i < ids.length(); ++i) {
                if (i) values << ',';
                try { values << textBridgeState(ids[i], true); }
                catch (const std::exception& failure) {
                    incomplete = true;
                    values << "{\"geometry\":{\"handle\":" << quote(handleOf(ids[i])) << "},\"readError\":" << styleAuditError(failure) << '}';
                } catch (...) {
                    incomplete = true;
                    values << "{\"geometry\":{\"handle\":" << quote(handleOf(ids[i])) << "},\"readError\":\"unknown public state read exception\"}";
                }
            }
            values << ']'; entities = values.str();
            if (incomplete) error = quote(L"One or more public ModelSpace snapshots failed; all identities/placeholders were retained");
        } catch (const std::exception& failure) { error = styleAuditError(failure); }
          catch (...) { error = quote(L"unknown input-version side-database exception"); }
    } // Working database restored and entity database destroyed before resources.
    const auto resources = styleAuditSource(role, path); // Its own read-only side DB.
    std::ostringstream out;
    out << "{\"role\":" << quote(role) << ",\"path\":" << quote(path) << ",\"readOnly\":true,\"readStatus\":" << readStatus
        << ",\"closeInputStatus\":" << closeStatus << ",\"modelCount\":" << count << ",\"entities\":" << entities
        << ",\"resources\":" << resources << ",\"resourceReadMethod\":\"styleAuditSource independent read-only database\",\"error\":" << error << '}';
    return out.str();
}

void nativeInputVersionAudit() {
    try {
        const auto p = paths(); const auto observed = fullPath(p.output + L"\\observed-original.dwg"), report = fullPath(p.output + L"\\input-version-audit.json");
        if (!below(observed, p.output) || !below(report, p.output) || _wcsicmp(observed.c_str(), fullPath(p.scheme).c_str()) == 0)
            throw std::runtime_error("input-version audit requires distinct frozen and run-local observed copy paths");
        requireNewFile(report);
        auto* services = acdbHostApplicationServices(); if (!services) throw std::runtime_error("input-version host services unavailable");
        auto* prior = services->workingDatabase();
        const auto frozen = inputVersionAuditSource(L"frozen", p.scheme), current = inputVersionAuditSource(L"currentCopy", observed);
        const bool restored = services->workingDatabase() == prior;
        writeNewReport(report, "{\"contract\":\"stage1.input-version-audit.v1\",\"api\":\"ObjectARX native; no COM\",\"drawingWritten\":false,\"workingDatabaseRestored\":"
            + std::string(restored ? "true" : "false") + ",\"sources\":[" + frozen + ',' + current + "]"
            + ",\"scope\":\"ModelSpace public geometry/common/native text/public EntGet strings and four symbol tables/database settings only; no private semantic equivalence or change causation asserted\",\"error\":"
            + (restored ? std::string("null") : quote(L"working database was not restored")) + '}');
    } catch (...) {
        // An invalid destination is never used for fallback output. No command-
        // history/logging side effect is introduced; the controller sees no report.
    }
}
