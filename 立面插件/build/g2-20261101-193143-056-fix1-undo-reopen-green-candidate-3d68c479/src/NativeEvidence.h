#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <utility>
#include <vector>

namespace bce::native {
using Fields = std::vector<std::pair<std::string, std::string>>;
std::string utf8(const std::wstring& text);
std::string quote(const std::string& text);
std::string quote(const std::wstring& text);
std::string object(const Fields& fields);
std::string array(const std::vector<std::string>& items);
void require(bool condition, const std::string& message);
std::string sha256(const std::wstring& path);

class File final {
public:
    explicit File(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    ~File() noexcept;
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
    HANDLE get() const noexcept { return handle_; }
    bool valid() const noexcept { return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr; }
private:
    HANDLE handle_;
};

struct Config {
    std::wstring nonce, session, scheme, reference, output, caseName;
    std::wstring modulePath, iniPath, desktop;
    std::string expectedModuleSHA256, moduleSHA256, iniSHA256;
    std::string schemeSHA256, referenceSHA256;
};
Config load_config(HMODULE module);
void validate_identity(Config& config);
File create_new(const std::wstring& path);
void write_flushed(HANDLE file, const std::string& bytes);
void replace_owned_report(HANDLE file, const std::string& bytes);
class Evidence final {
public:
    explicit Evidence(const Config& config);
    void checkpoint(const std::string& operation);
    void step(const std::string& name, const std::string& status, const std::string& details);
    void error(const std::string& message);
    void capability(const std::string& json);
    void finish(const Config& config, const std::string& status);
private:
    std::wstring output_;
    File report_;
    unsigned checkpointCount_{0};
    std::vector<std::string> steps_, capabilities_, errors_;
};
} // namespace bce::native
