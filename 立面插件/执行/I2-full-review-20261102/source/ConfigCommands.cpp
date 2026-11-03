#include "ConfigCommands.h"
#include "UnitConfig.h"
#include <aced.h>
#include <acutads.h>
#include <adscodes.h>
namespace bce::commands {
void configureUnits() noexcept {
  try {
    acutPrintf(L"\nBCE_CONFIG当前仅配置本图单位；不配置零点、轴、材料或生成资格。");
    NativeSceneContext context;
    const auto identity = context.documentIdentity();
    const auto revision = context.revision();
    double scale{};
    const auto status = acedGetReal(L"\n1模型单位相当于多少毫米（取消不修改）: ", &scale);
    if (status != RTNORM) { acutPrintf(L"\n单位配置已取消。\n"); return; }
    if (!context.currentDatabase() || context.documentIdentity() != identity ||
        context.revision() != revision) {
      acutPrintf(L"\nSTALE_DOCUMENT：输入期间当前文档发生变化；未修改档案。\n");
      return;
    }
    const auto result = configureProjectUnits(context, scale);
    if (result.canCommit()) acutPrintf(L"\n本图单位已保存；可用一次U撤销。尚未获得生成资格。\n");
    else for (const auto& issue : result.issues) {
      const auto text = native_scene::utf16("\n" + issue.code + ": " + issue.message);
      acutPrintf(L"%ls", text.c_str());
    }
  } catch (...) { acutPrintf(L"\n单位配置失败，未获得任何生成资格。\n"); }
}
}
