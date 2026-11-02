#include "NativeEvidence.h"
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cwctype>

namespace bce::native {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
File::~File() noexcept { if (valid()) CloseHandle(handle_); }
File::File(File&& other) noexcept : handle_(other.handle_) { other.handle_ = INVALID_HANDLE_VALUE; }
File& File::operator=(File&& other) noexcept {
    if (this != &other) { if (valid()) CloseHandle(handle_); handle_ = other.handle_; other.handle_ = INVALID_HANDLE_VALUE; }
    return *this;
}
std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    require(text.size() < static_cast<std::size_t>((std::numeric_limits<int>::max)()), "UTF16 input too long");
    const auto count = static_cast<int>(text.size());
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), count, nullptr, 0, nullptr, nullptr);
    require(size > 0, "invalid UTF16 text");
    std::string result(static_cast<std::size_t>(size), '\0');
    require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), count, &result[0], size, nullptr, nullptr) == size, "UTF8 conversion failed");
    return result;
}
std::string quote(const std::string& text) {
    std::ostringstream out; out << '"';
    for (const unsigned char value : text) {
        if (value == '"' || value == '\\') out << '\\' << value;
        else if (value < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(value) << std::dec;
        else out << value;
    }
    out << '"'; return out.str();
}
std::string quote(const std::wstring& text) { return quote(utf8(text)); }
std::string object(const Fields& fields) {
    std::string result = "{";
    for (const auto& field : fields) { if (result.size() > 1) result += ','; result += quote(field.first) + ':' + field.second; }
    return result + '}';
}
std::string array(const std::vector<std::string>& items) {
    std::string result = "[";
    for (const auto& item : items) { if (result.size() > 1) result += ','; result += item; }
    return result + ']';
}
std::string sha256(const std::wstring& path) {
    File file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    require(file.valid(), "SHA input open failed: " + utf8(path));
    HCRYPTPROV provider{};
    require(CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) != FALSE, "SHA provider unavailable");
    struct ProviderScope { HCRYPTPROV value; ~ProviderScope() noexcept { CryptReleaseContext(value, 0); } } providerScope{provider};
    HCRYPTHASH hash{};
    require(CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE, "SHA initialization failed");
    struct HashScope { HCRYPTHASH value; ~HashScope() noexcept { CryptDestroyHash(value); } } hashScope{hash};
    std::array<BYTE, 65536> bytes{};
    for (;;) {
        DWORD count{};
        require(ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) != FALSE, "SHA input read failed");
        if (count == 0) break;
        require(CryptHashData(hash, bytes.data(), count, 0) != FALSE, "SHA update failed");
    }
    std::array<BYTE, 32> digest{}; DWORD length = static_cast<DWORD>(digest.size());
    require(CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &length, 0) != FALSE && length == digest.size(), "SHA finalize failed");
    std::ostringstream result;
    for (const auto byte : digest) result << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return result.str();
}
namespace {
bool equal_path(const std::wstring& left, const std::wstring& right) { return _wcsicmp(left.c_str(), right.c_str()) == 0; }
std::wstring canonical(const std::wstring& input) {
    require(input.size() >= 3 && ((input[0] >= L'A' && input[0] <= L'Z') || (input[0] >= L'a' && input[0] <= L'z')) &&
        input[1] == L':' && (input[2] == L'\\' || input[2] == L'/'), "path must be local drive absolute");
    require(input.find(L':', 2) == std::wstring::npos && input.find_first_of(L"*?\"<>|") == std::wstring::npos, "UNC/ADS/wildcard path forbidden");
    std::wstring path = input;
    std::replace(path.begin(), path.end(), L'/', L'\\');
    std::size_t at = 3;
    while (at < path.size()) {
        const auto end = path.find(L'\\', at);
        const auto component = path.substr(at, end == std::wstring::npos ? path.size() - at : end - at);
        require(!component.empty() && component != L"." && component != L".." && component.back() != L'.' && component.back() != L' ', "ambiguous path component forbidden");
        if (end == std::wstring::npos) break;
        at = end + 1;
    }
    const auto size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    require(size > 0 && size < 32768, "path canonicalization failed");
    std::wstring result(size, L'\0');
    const auto written = GetFullPathNameW(path.c_str(), size, &result[0], nullptr);
    require(written > 0 && written < size, "path canonicalization failed"); result.resize(written);
    while (result.size() > 3 && result.back() == L'\\') result.pop_back();
    return result;
}
void validate_existing(const std::wstring& path, bool directory) {
    require(GetDriveTypeW(path.substr(0, 3).c_str()) == DRIVE_FIXED, "only local fixed drive paths are allowed");
    for (std::size_t end = 3;;) {
        const auto prefix = path.substr(0, end);
        const auto attributes = GetFileAttributesW(prefix.c_str());
        require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0,
            "path component missing or reparse point: " + utf8(prefix));
        if (end == path.size()) {
            require(((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == directory, "path has wrong file/directory kind");
            break;
        }
        const auto separator = path.find(L'\\', end + (end == 3 ? 0 : 1));
        end = separator == std::wstring::npos ? path.size() : separator;
    }
}
bool child_path(const std::wstring& parent, const std::wstring& child) {
    return child.size() > parent.size() && child[parent.size()] == L'\\' &&
        _wcsnicmp(parent.c_str(), child.c_str(), parent.size()) == 0;
}
std::wstring module_path(HMODULE module) {
    require(module != nullptr, "DllMain module identity unavailable");
    std::wstring path(32768, L'\0'); const auto size = GetModuleFileNameW(module, &path[0], static_cast<DWORD>(path.size()));
    require(size > 0 && size < path.size(), "module path unavailable/truncated"); path.resize(size); return canonical(path);
}
std::wstring desktop_name(HDESK desktop) {
    require(desktop != nullptr, "desktop handle unavailable"); DWORD bytes{};
    GetUserObjectInformationW(desktop, UOI_NAME, nullptr, 0, &bytes);
    require(bytes >= sizeof(wchar_t) && bytes < 65536, "desktop name size unavailable");
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    require(GetUserObjectInformationW(desktop, UOI_NAME, &value[0], bytes, &bytes) != FALSE, "desktop name unreadable");
    value.resize(wcslen(value.c_str())); return value;
}
std::wstring trim(const std::wstring& text) {
    const auto first = text.find_first_not_of(L" \t\r"); if (first == std::wstring::npos) return {};
    return text.substr(first, text.find_last_not_of(L" \t\r") - first + 1);
}
std::map<std::wstring, std::wstring> read_ini(const std::wstring& path) {
    File file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    require(file.valid(), "module INI not readable"); LARGE_INTEGER size{};
    require(GetFileSizeEx(file.get(), &size) && size.QuadPart >= 2 && size.QuadPart <= 131072 && size.QuadPart % 2 == 0, "INI must be bounded UTF16LE");
    std::vector<wchar_t> words(static_cast<std::size_t>(size.QuadPart / 2)); DWORD read{};
    require(ReadFile(file.get(), words.data(), static_cast<DWORD>(size.QuadPart), &read, nullptr) && read == size.QuadPart, "INI read failed");
    require(words.front() == 0xFEFF, "INI must begin UTF16LE BOM");
    std::wstring content(words.begin() + 1, words.end()); require(content.find(L'\0') == std::wstring::npos, "INI embedded NUL forbidden");
    std::wistringstream lines(content); std::wstring line; bool inCase = false; bool seenCase = false;
    std::map<std::wstring, std::wstring> values;
    while (std::getline(lines, line)) {
        line = trim(line); if (line.empty() || line.front() == L';' || line.front() == L'#') continue;
        if (line.front() == L'[') { require(line == L"[case]" && !seenCase, "INI must have one [case] section"); inCase = seenCase = true; continue; }
        require(inCase, "INI key outside [case]"); const auto separator = line.find(L'=');
        require(separator != std::wstring::npos, "INI missing key separator");
        const auto key = trim(line.substr(0, separator)); const auto value = trim(line.substr(separator + 1));
        require(!key.empty() && !value.empty() && values.emplace(key, value).second, "INI empty/duplicate key");
    }
    require(values.size() == 7, "INI must contain exactly seven case keys"); return values;
}
}
Config load_config(HMODULE module) {
    Config config;
    config.modulePath = module_path(module); validate_existing(config.modulePath, false);
    config.iniPath = config.modulePath.substr(0, config.modulePath.find_last_of(L'.')) + L".ini";
    validate_existing(config.iniPath, false); const auto values = read_ini(config.iniPath);
    const auto value = [&values](const wchar_t* key) { const auto found = values.find(key); require(found != values.end(), "INI required key missing"); return found->second; };
    config.nonce = value(L"nonce");
    require(config.nonce.size() >= 8 && config.nonce.size() <= 64 && std::all_of(config.nonce.begin(), config.nonce.end(),
        [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'); }), "nonce must be 8..64 hex characters");
    config.session = canonical(value(L"session")); config.output = canonical(value(L"output"));
    config.scheme = canonical(value(L"scheme")); config.reference = canonical(value(L"reference"));
    config.caseName = value(L"caseName"); config.expectedModuleSHA256 = utf8(value(L"expectedModuleSHA256"));
    std::transform(config.expectedModuleSHA256.begin(), config.expectedModuleSHA256.end(), config.expectedModuleSHA256.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    require(config.expectedModuleSHA256.size() == 64 && std::all_of(config.expectedModuleSHA256.begin(), config.expectedModuleSHA256.end(),
        [](unsigned char c) { return std::isxdigit(c) != 0; }), "expectedModuleSHA256 must be SHA256 hex");
    validate_existing(config.session, true); validate_existing(config.output, true);
    validate_existing(config.scheme, false); validate_existing(config.reference, false);
    require(child_path(config.session, config.output) && child_path(config.session, config.scheme) && child_path(config.session, config.reference),
        "output and source files must be descendants of this session");
    require(!equal_path(config.scheme, config.reference), "source drawings must be distinct files");
    require(!child_path(config.output, config.scheme) && !child_path(config.output, config.reference), "source files cannot be inside output");
    config.iniSHA256 = sha256(config.iniPath); config.moduleSHA256 = sha256(config.modulePath);
    return config;
}
void validate_identity(Config& config) {
    require(config.moduleSHA256 == config.expectedModuleSHA256, "loaded module SHA differs from expectedModuleSHA256");
    config.desktop = desktop_name(GetThreadDesktop(GetCurrentThreadId()));
    require(config.desktop == L"CodexS1_" + config.nonce, "thread desktop must match nonce");
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    require(input != nullptr, "input desktop cannot be read");
    struct DesktopScope { HDESK value; ~DesktopScope() noexcept { CloseDesktop(value); } } scope{input};
    require(!equal_path(config.desktop, desktop_name(input)), "worker must not run on input desktop");
    config.schemeSHA256 = sha256(config.scheme); config.referenceSHA256 = sha256(config.reference);
    require(config.schemeSHA256 == "595CE82FBD20C81E83A857AB569385BEFA98ADAF743300A42668E280561559C1", "scheme source SHA mismatch");
    require(config.referenceSHA256 == "109BDCFEA4C55219793F2B6F9D8C8B619344B51A4C73E9129378C38532B2AF4D", "reference source SHA mismatch");
    require(sha256(config.iniPath) == config.iniSHA256 && sha256(config.modulePath) == config.moduleSHA256, "INI or module changed during identity validation");
}
File create_new(const std::wstring& path) {
    File result(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
    require(result.valid(), "exclusive new output creation failed: " + utf8(path)); return result;
}
void write_flushed(HANDLE file, const std::string& bytes) {
    require(bytes.size() <= MAXDWORD, "output too large"); DWORD written{};
    require(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size(), "output write failed");
    require(FlushFileBuffers(file) != FALSE, "output flush failed");
}
void replace_owned_report(HANDLE file, const std::string& bytes) {
    LARGE_INTEGER beginning{};
    require(SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN) != FALSE, "report rewind failed");
    write_flushed(file, bytes); require(SetEndOfFile(file) != FALSE && FlushFileBuffers(file) != FALSE, "report truncate/flush failed");
}
Evidence::Evidence(const Config& config) : output_(config.output), report_(create_new(config.output + L"\\native-case.json")) {
    error("INCOMPLETE: command did not reach final evidence commit"); finish(config, "failed"); errors_.clear();
}
void Evidence::checkpoint(const std::string& operation) {
    ++checkpointCount_;
    const auto path = output_ + L"\\checkpoint-" + std::to_wstring(checkpointCount_) + L".json";
    auto file = create_new(path);
    write_flushed(file.get(), object({{"contract", quote(std::string("bce.native-checkpoint.v1"))},
        {"pid", std::to_string(GetCurrentProcessId())}, {"sequence", std::to_string(checkpointCount_)}, {"operation", quote(operation)}}));
}
void Evidence::step(const std::string& name, const std::string& status, const std::string& details) {
    steps_.push_back(object({{"name", quote(name)}, {"status", quote(status)}, {"details", quote(details)}}));
}
void Evidence::error(const std::string& message) { errors_.push_back(quote(message)); }
void Evidence::capability(const std::string& json) { capabilities_.push_back(json); }
void Evidence::finish(const Config& config, const std::string& status) {
    replace_owned_report(report_.get(), object({
        {"contract", quote(std::string("bce.native-case.v1"))}, {"caseName", quote(config.caseName)}, {"status", quote(status)},
        {"pid", std::to_string(GetCurrentProcessId())}, {"nonce", quote(config.nonce)}, {"session", quote(config.session)},
        {"desktop", quote(config.desktop)}, {"modulePath", quote(config.modulePath)}, {"moduleSHA256", quote(config.moduleSHA256)},
        {"expectedModuleSHA256", quote(config.expectedModuleSHA256)}, {"iniPath", quote(config.iniPath)}, {"iniSHA256", quote(config.iniSHA256)},
        {"scheme", object({{"path", quote(config.scheme)}, {"sha256", quote(config.schemeSHA256)}})},
        {"reference", object({{"path", quote(config.reference)}, {"sha256", quote(config.referenceSHA256)}})},
        {"steps", array(steps_)}, {"capabilities", array(capabilities_)}, {"errors", array(errors_)},
        {"checkpointCount", std::to_string(checkpointCount_)}, {"sourceDrawingWritten", "false"},
        {"productionGenerationImplemented", "false"}, {"businessSetterAttempted", "false"}, {"tMergeDimExecuted", "false"},
        {"limitations", array({quote(std::string("Public snapshots cannot establish private TCH field completeness")),
            quote(std::string("13 segment prototype absent; connection validation pending")),
            quote(std::string("No production resource collision or business setter support is claimed"))})}
    }));
}
} // namespace bce::native
