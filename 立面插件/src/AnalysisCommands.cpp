#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "AnalysisCommands.h"
#include "ConfigCommands.h"
#include "UnitConfig.h"
#include "AnalysisCommandAbi.h"
#include "AnalysisReader.h"
#include "ProjectStore.h"
#include <Windows.h>
#include <acdocman.h>
#include <aced.h>
#include <acutads.h>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
namespace {
constexpr auto group = L"BCE_ANALYSIS_PRODUCT_8D5BB4A2";
bool analyzeOwned{}, infoOwned{}, issuesOwned{}, configOwned{};
DWORD ownerThread{};
std::unique_ptr<bce::NativeSceneContext> context;
std::string lastReport;
std::vector<bce::Issue> lastIssues;
std::size_t issueCursor{};
std::uint64_t sequence{}, lastRevision{};
AcApDocument *reportDocument{};
void invalidate() noexcept {
  lastReport.clear();
  lastIssues.clear();
  issueCursor = 0;
  lastRevision = 0;
  reportDocument = nullptr;
}
// NativeSceneContext invalidates its SDK identity on switch/destroy. Never
// delete another reactor during notification; release it at the next command or
// unload.
class Lifecycle final : public AcApDocManagerReactor {
public:
  void documentBecameCurrent(AcApDocument *doc) override {
    if (doc != reportDocument)
      invalidate();
  }
  void documentToBeDestroyed(AcApDocument *doc) override {
    if (doc == reportDocument)
      invalidate();
  }
};
std::unique_ptr<Lifecycle> lifecycle;
void say(const wchar_t *text) noexcept {
  try {
    acutPrintf(L"%ls", text);
  } catch (...) {
  }
}
std::string field(const std::string &value) {
  std::string result;
  for (const auto c : value) {
    if (c == '\\')
      result += "\\\\";
    else if (c == '\t')
      result += "\\t";
    else if (c == '\n')
      result += "\\n";
    else if (c == '\r')
      result += "\\r";
    else
      result += c;
  }
  return result;
}
void issueRow(std::ostream &out, const bce::Issue &i) {
  out << "issue\t" << field(i.code) << '\t' << field(i.source.viewIdentity)
      << '\t' << (i.source.entity ? field(i.source.entity->handle) : "") << '\t'
      << field(i.message) << '\n';
}
constexpr std::size_t issuePageSize = 12;
bool criticalInput(const bce::Issue &issue) {
  return issue.code == bce::diagnostic::unitRequired ||
         issue.code == bce::diagnostic::unitConflict ||
         issue.code == bce::diagnostic::planRequired ||
         issue.code.rfind("PROJECT_", 0) == 0;
}
void showIssue(const bce::Issue &issue, const std::string &prefix = {}) {
  std::string location;
  if (issue.source.entity)
    location += " [" + issue.source.entity->handle + "]";
  if (!issue.source.viewIdentity.empty())
    location += " [" + issue.source.viewIdentity + "]";
  const auto message = issue.code == "PROJECT_NOT_FOUND"
                           ? std::string(u8"当前图纸没有已保存的本项目配置。")
                           : issue.message;
  const auto text = bce::native_scene::utf16("\n" + prefix + issue.code +
                                             location + "：" + message);
  say(text.c_str());
}
void showIssues(const std::vector<bce::Issue> &issues) noexcept {
  try {
    std::set<std::string> emittedCriticalCodes;
    std::size_t shown{};
    // Critical input gaps cannot be hidden behind geometry candidates. Repeated
    // per-view copies remain available in the complete paged list.
    for (const auto &issue : issues)
      if (criticalInput(issue) &&
          emittedCriticalCodes.insert(issue.code).second) {
        showIssue(issue);
        ++shown;
      }
    for (const auto &issue : issues) {
      if (shown >= issuePageSize)
        break;
      if (criticalInput(issue))
        continue;
      showIssue(issue);
      ++shown;
    }
    if (!issues.empty())
      say(L"\n执行 BCE_ISSUES "
          L"查看全部定位问题：每次12项，再次执行继续下一页，Esc可暂停。");
  } catch (...) {
    say(L"\n部分诊断文字显示失败，分析资格仍未授予。");
  }
}
bool currentReport() noexcept {
  if (!context || !context->currentDatabase() || lastReport.empty() ||
      context->revision() != lastRevision ||
      acDocManager->curDocument() != reportDocument) {
    invalidate();
    return false;
  }
  return true;
}
void issuesCommand() noexcept {
  try {
    if (!currentReport()) {
      say(L"\n没有当前有效的问题列表；请先执行 BCE_ANALYZE。\n");
      return;
    }
    if (lastIssues.empty()) {
      say(L"\n本次分析没有记录问题。\n");
      return;
    }
    const auto end = (std::min)(lastIssues.size(), issueCursor + issuePageSize);
    while (issueCursor < end) {
      if (acedUsrBrk()) {
        say(L"\n问题查看已暂停，再次执行 BCE_ISSUES 继续。\n");
        return;
      }
      showIssue(lastIssues[issueCursor],
                "[" + std::to_string(issueCursor + 1) + "/" +
                    std::to_string(lastIssues.size()) + "] ");
      ++issueCursor;
    }
    if (issueCursor == lastIssues.size()) {
      issueCursor = 0;
      say(L"\n已显示全部问题；再次执行 BCE_ISSUES 从第一页开始。\n");
    } else
      say(L"\n再次执行 BCE_ISSUES 显示下一页。\n");
  } catch (...) {
    invalidate();
    say(L"\n问题显示失败，请重新执行 BCE_ANALYZE。\n");
  }
}
void analyze() noexcept {
  invalidate();
  try {
    if (sequence == (std::numeric_limits<std::uint64_t>::max)())
      throw std::runtime_error("analysis sequence exhausted");
    ++sequence;
    if (!context || !context->currentDatabase())
      context = std::make_unique<bce::NativeSceneContext>();
    if (acedUsrBrk()) {
      say(L"\n分析已取消，旧结果已清除。\n");
      return;
    }
    const auto identity = context->documentIdentity();
    const auto revision = context->revision();
    auto archive = bce::readProjectStore(*context->currentDatabase());
    bce::AnalysisReadOptions
        options; // No project-specific assumptions or qualification inputs.
    const bool unitOnly = archive.readable() && archive.issues.empty() &&
                          bce::isUnitOnlyProject(archive.value->project);
    std::string unitSource;
    if (unitOnly && archive.value->project.config.units.confirmedMmPerInputUnit) {
      options.confirmedMmPerInputUnit =
          archive.value->project.config.units.confirmedMmPerInputUnit;
      unitSource = archive.value->project.config.unitConfirmationSource;
    }
    // No historic ProjectConfig, plan, views, zero, axes or materials are passed.
    auto result = bce::readCurrentAnalysis(*context, options);
    if (acedUsrBrk()) {
      say(L"\n分析已取消，旧结果已清除。\n");
      return;
    }
    if (!result.value) {
      showIssues(result.issues);
      say(L"\n分析未完成，旧结果已清除。\n");
      return;
    }
    auto &analysis = *result.value;
    if (analysis.scene.documentIdentity != identity ||
        analysis.scene.revision != revision ||
        analysis.scene.databaseIdentity.empty())
      throw std::runtime_error("reader identity mismatch");
    auto issues = analysis.recognition.issues;
    for (const auto &view : analysis.recognition.views)
      issues.insert(issues.end(), view.unconfirmed.begin(),
                    view.unconfirmed.end());
    auto unitDeclaration = analysis.scene.units;
    if (options.confirmedMmPerInputUnit &&
        (unitDeclaration.insunits == 4 || unitDeclaration.insunits == 6))
      unitDeclaration.trustedInsunits = true;
    const auto units = bce::resolveUnits(unitDeclaration);
    issues.insert(issues.end(), units.issues.begin(), units.issues.end());
    if (archive.value && !unitOnly)
      issues.push_back({"PROJECT_HISTORY_UNBOUND",
                        u8"已读取历史项目档案；未与本次内存完整绑定，单位、视图"
                        u8"、轴及材料确认均未沿用。",
                        {identity, {}, {}},
                        true});
    else if (!archive.value)
      issues.insert(issues.end(), archive.issues.begin(), archive.issues.end());
    if (!context->currentDatabase() || context->revision() != revision ||
        context->documentIdentity() != identity)
      throw std::runtime_error("analysis context changed");
    std::size_t elevations{}, sections{};
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "contract\tbce.analysis-report.v1\nidentity\t" << identity
        << "\ndatabaseIdentity\t" << field(analysis.scene.databaseIdentity)
        << "\nrevision\t" << revision << "\nsequence\t" << sequence << "\n";
    if (options.confirmedMmPerInputUnit)
      out << "unitMmPerInputUnit\t"
          << std::setprecision(std::numeric_limits<double>::max_digits10)
          << *options.confirmedMmPerInputUnit << "\nunitSource\t"
          << field(unitSource) << "\nunitResolved\t"
          << (units.canCommit() ? "true" : "false") << "\n";
    for (const auto &view : analysis.recognition.views) {
      elevations += view.kind == bce::ViewKind::elevation;
      sections += view.kind == bce::ViewKind::section;
      out << "view\t" << field(view.identity) << '\t'
          << field(view.titleKey.handle) << '\t' << static_cast<int>(view.kind)
          << '\t' << view.members.size() << '\n';
      for (const auto &member : view.members)
        out << "member\t" << field(member.handle) << '\t'
            << field(view.titleKey.handle) << '\n';
    }
    out << "elevations\t" << elevations << "\nsections\t" << sections
        << "\nroots\t" << analysis.scene.primitives.size() << "\npreserved\t"
        << analysis.recognition.preserved.size()
        << "\ngenerationAuthorized\tfalse\n";
    for (const auto &primitive : analysis.scene.primitives)
      if (primitive.text)
        out << "text\t" << field(primitive.key.handle) << '\t'
            << field(*primitive.text) << '\n';
    for (const auto &i : issues)
      issueRow(out, i);
    lastReport = out.str();
    lastIssues = issues;
    lastRevision = revision;
    reportDocument = acDocManager->curDocument();
    const auto summary = bce::native_scene::utf16(
        u8"\n只读分析：" + std::to_string(analysis.recognition.views.size()) +
        u8"个视图（立面" + std::to_string(elevations) + u8"、剖面" +
        std::to_string(sections) + u8"），保留" +
        std::to_string(analysis.recognition.preserved.size()) +
        u8"个对象，问题" + std::to_string(issues.size()) +
        u8"项。未生成施工图。");
    say(summary.c_str());
    if (options.confirmedMmPerInputUnit) {
      std::ostringstream unitSummary;
      unitSummary.imbue(std::locale::classic());
      unitSummary << u8"\n本图明确单位：1模型单位="
                  << std::setprecision(std::numeric_limits<double>::max_digits10)
                  << *options.confirmedMmPerInputUnit << u8"毫米；来源：" << unitSource;
      say(bce::native_scene::utf16(unitSummary.str()).c_str());
    }
    showIssues(issues);
    say(L"\n");
  } catch (const std::exception &failure) {
    invalidate();
    say(L"\nBCE_ANALYZE分析失败，旧结果已清除。\n");
    try {
      const auto detail = bce::native_scene::utf16(failure.what());
      say(detail.c_str());
      say(L"\n");
    } catch (...) {
    }
  } catch (...) {
    invalidate();
    say(L"\nBCE_ANALYZE分析失败，旧结果已清除。\n");
  }
}
void configCommand() noexcept {
  invalidate();
  bce::commands::configureUnits();
}
void info() noexcept {
  say(L"\n立面插件开发候选：BCE_"
      L"ANALYZE只读分析当前内存图纸；BCE_"
      L"ISSUES分页查看本次全部定位问题；BCE_CONFIG仅配置本图单位；尚无施工图生成、零点、轴及材料交互配置功能。\n");
}
bool removeOwned(const wchar_t *name, bool &owned) {
  if (!owned)
    return true;
  const bool removed = acedRegCmds->removeCmd(group, name) == Acad::eOk;
  if (removed)
    owned = false;
  return removed;
}
} // namespace
namespace bce::commands {
bool shutdown() noexcept {
  try {
    invalidate();
    if (lifecycle) {
      acDocManager->removeReactor(lifecycle.get());
      lifecycle.reset();
    }
    context.reset();
    const bool configRemoved = removeOwned(L"BCE_CONFIG", configOwned);
    const bool issuesRemoved = removeOwned(L"BCE_ISSUES", issuesOwned);
    const bool infoRemoved = removeOwned(L"BCE_INFO", infoOwned);
    const bool analyzeRemoved = removeOwned(L"BCE_ANALYZE", analyzeOwned);
    return configRemoved && issuesRemoved && infoRemoved && analyzeRemoved;
  } catch (...) {
    return false;
  }
}
bool initialize() noexcept {
  try {
    ownerThread = GetCurrentThreadId();
    if (acedRegCmds->lookupGlobalCmd(L"BCE_ANALYZE") ||
        acedRegCmds->lookupLocalCmd(L"BCE_ANALYZE") ||
        acedRegCmds->lookupGlobalCmd(L"BCE_INFO") ||
        acedRegCmds->lookupLocalCmd(L"BCE_INFO") ||
        acedRegCmds->lookupGlobalCmd(L"BCE_ISSUES") ||
        acedRegCmds->lookupLocalCmd(L"BCE_ISSUES") ||
        acedRegCmds->lookupGlobalCmd(L"BCE_CONFIG") ||
        acedRegCmds->lookupLocalCmd(L"BCE_CONFIG"))
      return false;
    analyzeOwned =
        acedRegCmds->addCommand(group, L"BCE_ANALYZE", L"BCE_ANALYZE",
                                ACRX_CMD_MODAL, analyze) == Acad::eOk;
    if (!analyzeOwned)
      return false;
    infoOwned = acedRegCmds->addCommand(group, L"BCE_INFO", L"BCE_INFO",
                                        ACRX_CMD_MODAL, info) == Acad::eOk;
    if (!infoOwned) {
      shutdown();
      return false;
    }
    issuesOwned =
        acedRegCmds->addCommand(group, L"BCE_ISSUES", L"BCE_ISSUES",
                                ACRX_CMD_MODAL, issuesCommand) == Acad::eOk;
    if (!issuesOwned) {
      shutdown();
      return false;
    }
    configOwned = acedRegCmds->addCommand(group, L"BCE_CONFIG", L"BCE_CONFIG",
        ACRX_CMD_MODAL, configCommand) == Acad::eOk;
    if (!configOwned) { shutdown(); return false; }
    lifecycle = std::make_unique<Lifecycle>();
    acDocManager->addReactor(lifecycle.get());
    return true;
  } catch (...) {
    shutdown();
    return false;
  }
}
} // namespace bce::commands
extern "C" __declspec(dllexport) int __cdecl BceCopyLastAnalysisV1(
    BceAnalysisReportV1 *report, char *bytes, std::uint64_t capacity) noexcept {
  try {
    if (GetCurrentThreadId() != ownerThread)
      return 3;
    if (!report || report->size != sizeof(BceAnalysisReportV1) ||
        report->version != 1 || (!bytes && capacity))
      return 3;
    if (!currentReport())
      return 1;
    if (report->sequence && report->sequence != sequence)
      return 4;
    const auto identity = context->documentIdentity();
    if (identity.size() >= sizeof(report->documentIdentity))
      return 3;
    // Query (sequence=0) acquires a snapshot. A copy must echo its
    // identity/revision.
    if (bytes &&
        (report->sequence != sequence || report->revision != lastRevision ||
         std::memcmp(report->documentIdentity, identity.c_str(),
                     identity.size() + 1) != 0))
      return 4;
    BceAnalysisReportV1 next{};
    next.size = sizeof(next);
    next.version = 1;
    next.sequence = sequence;
    next.revision = lastRevision;
    next.requiredBytes = lastReport.size() + 1;
    std::memcpy(next.documentIdentity, identity.c_str(), identity.size() + 1);
    *report = next;
    if (!bytes || capacity < next.requiredBytes)
      return 2;
    std::memcpy(bytes, lastReport.c_str(),
                static_cast<std::size_t>(next.requiredBytes));
    return 0;
  } catch (...) {
    invalidate();
    return 1;
  }
}
