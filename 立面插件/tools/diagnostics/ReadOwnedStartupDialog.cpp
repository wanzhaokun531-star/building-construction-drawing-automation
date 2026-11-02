// One-session, read-only Win32 diagnostics. No messages, input, UIA, hooks,
// desktop switching, process mutation, AutoCAD API, COM, or child processes.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr DWORD expected_pid{32276};
constexpr DWORD expected_thread{17568};
constexpr std::uint64_t expected_created{134380067080529029ULL};
constexpr wchar_t expected_image[]{L"D:\\CAD2020\\AutoCAD 2020\\acad.exe"};
constexpr wchar_t expected_desktop[]{L"CodexS1_63158db09c924710a6bbdda721992376"};
constexpr std::uintptr_t expected_dialog{10362410};
constexpr std::uintptr_t expected_owner{6561388};

struct ProcessHandle {
    HANDLE value{};
    ~ProcessHandle() { if (value) CloseHandle(value); }
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    explicit ProcessHandle(HANDLE handle) : value{handle} {}
};
struct DesktopHandle {
    HDESK value{};
    ~DesktopHandle() { if (value) CloseDesktop(value); }
    DesktopHandle(const DesktopHandle&) = delete;
    DesktopHandle& operator=(const DesktopHandle&) = delete;
    explicit DesktopHandle(HDESK handle) : value{handle} {}
};
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("utf8 conversion failed");
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}
std::string json(const std::wstring& value) {
    std::string result{"\""};
    constexpr char digits[]{"0123456789abcdef"};
    for (const unsigned char character : utf8(value)) {
        if (character == '\\' || character == '"') {
            result += '\\'; result += static_cast<char>(character);
        } else if (character < 32) {
            result += "\\u00";
            result += digits[character >> 4]; result += digits[character & 15];
        } else result += static_cast<char>(character);
    }
    return result + '"';
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::wstring object_name(HANDLE handle) {
    std::array<wchar_t, 512> text{};
    DWORD needed{};
    require(GetUserObjectInformationW(handle, UOI_NAME, text.data(),
        static_cast<DWORD>(text.size() * sizeof(wchar_t)), &needed) != FALSE,
        "cannot read desktop object name");
    return text.data();
}
std::wstring class_name(HWND window) {
    std::array<wchar_t, 512> text{};
    require(GetClassNameW(window, text.data(), static_cast<int>(text.size())) > 0,
        "cannot read window class");
    return text.data();
}
void require_process_identity(HANDLE process) {
    require(WaitForSingleObject(process, 0) == WAIT_TIMEOUT, "target not running");
    std::array<wchar_t, 32768> image{};
    DWORD size{static_cast<DWORD>(image.size())};
    require(QueryFullProcessImageNameW(process, 0, image.data(), &size) != FALSE,
        "cannot read process image");
    require(_wcsicmp(image.data(), expected_image) == 0, "process image mismatch");
    FILETIME created{}, exited{}, kernel{}, user{};
    require(GetProcessTimes(process, &created, &exited, &kernel, &user) != FALSE,
        "cannot read process time");
    const auto actual = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32)
        | created.dwLowDateTime;
    require(actual == expected_created, "process creation time mismatch");
}
struct Enumeration {
    std::vector<HWND> own_windows;
    bool initial_thread_found{};
};
BOOL CALLBACK desktop_callback(HWND window, LPARAM parameter) {
    auto& found = *reinterpret_cast<Enumeration*>(parameter);
    DWORD pid{};
    const DWORD thread = GetWindowThreadProcessId(window, &pid);
    if (pid == expected_pid) {
        found.own_windows.push_back(window);
        found.initial_thread_found |= thread == expected_thread;
    }
    return TRUE;
}
struct Children {
    std::vector<HWND> windows;
    bool mismatch{};
};
BOOL CALLBACK child_callback(HWND window, LPARAM parameter) {
    auto& found = *reinterpret_cast<Children*>(parameter);
    DWORD pid{};
    GetWindowThreadProcessId(window, &pid);
    if (pid != expected_pid) { found.mismatch = true; return FALSE; }
    found.windows.push_back(window);
    return TRUE;
}
std::string window_record(HWND window, HWND root) {
    DWORD pid{};
    const DWORD thread = GetWindowThreadProcessId(window, &pid);
    require(pid == expected_pid, "window PID mismatch before read");
    require(window == root || IsChild(root, window), "window ancestry mismatch");
    const auto name = class_name(window);
    std::array<wchar_t, 16384> text{};
    SetLastError(0);
    const int copied = GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    const DWORD text_error = GetLastError();
    RECT bounds{};
    require(GetWindowRect(window, &bounds) != FALSE, "cannot read window rect");
    DWORD pid_after{};
    require(GetWindowThreadProcessId(window, &pid_after) == thread &&
        pid_after == expected_pid, "window identity changed during read");
    return "{\"hwnd\":" + std::to_string(reinterpret_cast<std::uintptr_t>(window))
        + ",\"pid\":" + std::to_string(pid) + ",\"thread\":" + std::to_string(thread)
        + ",\"class\":" + json(name) + ",\"getWindowText\":" + json(text.data())
        + ",\"copiedChars\":" + std::to_string(copied)
        + ",\"lastError\":" + std::to_string(text_error)
        + ",\"controlId\":" + std::to_string(GetDlgCtrlID(window))
        + ",\"rect\":[" + std::to_string(bounds.left) + ',' + std::to_string(bounds.top)
        + ',' + std::to_string(bounds.right) + ',' + std::to_string(bounds.bottom) + "]}";
}
std::string inspect() {
    ProcessHandle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
        FALSE, expected_pid)};
    require(process.value != nullptr, "cannot open exact owned process read-only");
    require_process_identity(process.value);
    DesktopHandle desktop{OpenDesktopW(expected_desktop, 0, FALSE,
        DESKTOP_READOBJECTS | DESKTOP_ENUMERATE)};
    require(desktop.value != nullptr, "cannot open expected private desktop read-only");
    require(object_name(desktop.value) == expected_desktop, "desktop name mismatch");
    Enumeration found{};
    require(EnumDesktopWindows(desktop.value, desktop_callback,
        reinterpret_cast<LPARAM>(&found)) != FALSE, "desktop enumeration failed");
    require(found.initial_thread_found, "initial thread not on private desktop");
    const HWND dialog = reinterpret_cast<HWND>(expected_dialog);
    bool dialog_found{};
    for (const HWND window : found.own_windows) dialog_found |= window == dialog;
    require(dialog_found, "expected dialog absent from fresh private desktop enumeration");
    DWORD pid{};
    require(GetWindowThreadProcessId(dialog, &pid) == expected_thread && pid == expected_pid,
        "dialog PID/thread mismatch");
    require(class_name(dialog) == L"#32770", "dialog class mismatch");
    const HWND owner = GetWindow(dialog, GW_OWNER);
    require(reinterpret_cast<std::uintptr_t>(owner) == expected_owner, "dialog owner mismatch");
    GetWindowThreadProcessId(owner, &pid);
    require(pid == expected_pid, "dialog owner PID mismatch");
    const HDESK original = GetThreadDesktop(GetCurrentThreadId());
    require(original != nullptr, "cannot read diagnostic thread desktop");
    require(SetThreadDesktop(desktop.value) != FALSE, "cannot attach diagnostic-only thread");
    std::string records;
    try {
        require(object_name(GetThreadDesktop(GetCurrentThreadId())) == expected_desktop,
            "diagnostic thread desktop mismatch after attach");
        require_process_identity(process.value);
        Children children{};
        SetLastError(0);
        EnumChildWindows(dialog, child_callback, reinterpret_cast<LPARAM>(&children));
        require(!children.mismatch, "child PID mismatch; abort");
        records = window_record(dialog, dialog);
        for (const HWND child : children.windows) records += ',' + window_record(child, dialog);
        require_process_identity(process.value);
    } catch (...) {
        SetThreadDesktop(original);
        throw;
    }
    require(SetThreadDesktop(original) != FALSE, "cannot restore diagnostic-only thread");
    return "{\"contract\":\"owned-startup-dialog-readonly.v1\",\"status\":\"read\","
        "\"pid\":32276,\"creationFileTime\":\"134380067080529029\",\"desktop\":"
        + json(expected_desktop) + ",\"processIdentityBeforeAndAfter\":true,"
        "\"dialogFreshlyEnumerated\":true,\"explicitMessagesSent\":0,"
        "\"desktopSwitched\":false,\"windows\":[" + records + "]}";
}
} // namespace

int main() {
    std::string report;
    int result{};
    // No window/message queue/hook is ever created by this diagnostic thread.
    std::thread reader{[&] {
        try { report = inspect(); }
        catch (const std::exception& error) {
            result = 1;
            const std::string message{error.what()};
            report = "{\"status\":\"stopped\",\"reason\":"
                + json(std::wstring(message.begin(), message.end()))
                + ",\"lastError\":" + std::to_string(GetLastError()) + "}";
        }
    }};
    reader.join();
    std::cout << report << '\n';
    return result;
}
