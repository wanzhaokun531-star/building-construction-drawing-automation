#pragma once
#include "NativeEvidence.h"
#include <aced.h>
#include <acutmem.h>
#include <adscodes.h>
#include <algorithm>
#include <filesystem>
namespace bce::unitconfig_test {
class Transcript final {
public:
  Transcript(const Transcript &) = delete;
  Transcript &operator=(const Transcript &) = delete;
  static short mode() {
    resbuf r{};
    native::require(acedGetVar(L"LOGFILEMODE", &r) == RTNORM &&
                        r.restype == RTSHORT,
                    "read logging mode");
    return r.resval.rint;
  }
  static std::wstring pathVar(const wchar_t *name) {
    resbuf r{};
    native::require(acedGetVar(name, &r) == RTNORM && r.restype == RTSTR,
                    "read logging path");
    std::wstring result(r.resval.rstring);
    acutDelString(r.resval.rstring);
    return result;
  }
  static void setMode(short value) {
    resbuf r{};
    r.restype = RTSHORT;
    r.resval.rint = value;
    native::require(acedSetVar(L"LOGFILEMODE", &r) == RTNORM,
                    "set owned log mode");
  }
  static void setPath(const std::wstring &value) {
    resbuf r{};
    r.restype = RTSTR;
    r.resval.rstring = const_cast<wchar_t *>(value.c_str());
    native::require(acedSetVar(L"LOGFILEPATH", &r) == RTNORM,
                    "set owned log path");
  }
  static bool sameFolder(const std::wstring &a, const std::wstring &b) {
    auto x = std::filesystem::weakly_canonical(a).wstring(),
         y = std::filesystem::weakly_canonical(b).wstring();
    while (x.size() > 3 && (x.back() == L'\\' || x.back() == L'/'))
      x.pop_back();
    while (y.size() > 3 && (y.back() == L'\\' || y.back() == L'/'))
      y.pop_back();
    return CompareStringOrdinal(x.c_str(), -1, y.c_str(), -1, TRUE) ==
           CSTR_EQUAL;
  }
  explicit Transcript(const std::wstring &folder)
      : beforeMode_(mode()), beforePath_(pathVar(L"LOGFILEPATH")) {
    native::require(beforeMode_ == 0,
                    "do not replace an existing logging session");
    native::require(std::filesystem::create_directory(folder),
                    "transcript directory must be new");
    diagnosticFolder_ = folder;
    capture("constructor-before-change");
    changed_ = true;
    try {
      setPath(folder + L"\\");
      file_ = pathVar(L"LOGFILENAME");
      native::require(
          sameFolder(std::filesystem::path(file_).parent_path().wstring(),
                     folder) &&
              !std::filesystem::exists(file_),
          "transcript filename outside owned new folder");
      setMode(1);
      native::require(mode() == 1, "log did not enable");
    } catch (...) {
      restore();
      throw;
    }
  }
  ~Transcript() noexcept { restore(); }
  bool restore() noexcept {
    if (!changed_) return restored_;
    capture("restore-before");
    try {
      resbuf value{};value.restype=RTSHORT;value.resval.rint=0;
      const int modeSet=acedSetVar(L"LOGFILEMODE",&value);
      capture("restore-after-set-mode",modeSet);
      native::require(modeSet==RTNORM,"set owned log mode");
      if(mode()!=0){capture("restore-mode-not-zero");return false;}
      value.restype=RTSTR;value.resval.rstring=const_cast<wchar_t*>(beforePath_.c_str());
      const int pathSet=acedSetVar(L"LOGFILEPATH",&value);
      capture("restore-after-set-path",pathSet);
      native::require(pathSet==RTNORM,"set owned log path");
      const auto actualMode=mode();const auto actualPath=pathVar(L"LOGFILEPATH");
      canonicalActual_=std::filesystem::weakly_canonical(actualPath).wstring();
      canonicalExpected_=std::filesystem::weakly_canonical(beforePath_).wstring();
      const bool same= sameFolder(actualPath,beforePath_);
      restored_=actualMode==beforeMode_&&same;
      capture(same?"restore-folder-equal":"restore-folder-diff");
      if(restored_)changed_=false;
      return restored_;
    }catch(const std::exception& error){capture(std::string("restore-exception:")+error.what());return false;}
    catch(...){capture("restore-exception:unknown");return false;}
  }
  std::string stop() {
    native::require(restore(), "transcript globals restoration failed");
    native::File f(CreateFileW(file_.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    native::require(f.valid(), "transcript missing");
    LARGE_INTEGER length{};
    native::require(GetFileSizeEx(f.get(), &length) && length.QuadPart >= 0 &&
                        length.QuadPart <= 4 * 1024 * 1024,
                    "transcript size");
    std::string bytes(static_cast<std::size_t>(length.QuadPart), '\0');
    DWORD read{};
    native::require(ReadFile(f.get(), bytes.data(),
                             static_cast<DWORD>(bytes.size()), &read,
                             nullptr) &&
                        read == bytes.size(),
                    "transcript read");
    bytes.erase(std::remove(bytes.begin(), bytes.end(), '\0'), bytes.end());
    return bytes;
  }
  const std::wstring &file() const noexcept { return file_; }

private:
  void capture(const std::string& stage,int setterStatus=0) noexcept {
    try {
      if(diagnosticFolder_.empty())return;
      native::Fields fields{{"stage",native::quote(stage)},
        {"setterStatus",std::to_string(setterStatus)},
        {"expectedMode",std::to_string(beforeMode_)},
        {"expectedPath",native::quote(beforePath_)},
        {"canonicalActual",native::quote(canonicalActual_)},
        {"canonicalExpected",native::quote(canonicalExpected_)},
        {"changed",changed_?"true":"false"},{"restored",restored_?"true":"false"}};
      for(const auto* name:{L"LOGFILEMODE",L"LOGFILEPATH",L"LOGFILENAME",L"UNDOCTL",L"CMDACTIVE"}){
        resbuf v{};const int status=acedGetVar(name,&v);const auto key=native::utf8(name);
        fields.push_back({key+"Status",std::to_string(status)});
        fields.push_back({key+"Restype",std::to_string(v.restype)});
        if(status==RTNORM&&v.restype==RTSTR){fields.push_back({key,native::quote(std::wstring(v.resval.rstring))});acutDelString(v.resval.rstring);}
        else if(status==RTNORM&&v.restype==RTSHORT)fields.push_back({key,std::to_string(v.resval.rint)});
      }
      const auto file=diagnosticFolder_+L"/logger-state-"+std::to_wstring(++diagnosticSequence_)+L".json";
      auto output=native::create_new(file);native::write_flushed(output.get(),native::object(fields));
    }catch(...){}
  }
  std::wstring diagnosticFolder_,canonicalActual_,canonicalExpected_;
  unsigned diagnosticSequence_{};
  short beforeMode_{};
  std::wstring beforePath_, file_;
  bool changed_{}, restored_{};
};
} // namespace bce::unitconfig_test
