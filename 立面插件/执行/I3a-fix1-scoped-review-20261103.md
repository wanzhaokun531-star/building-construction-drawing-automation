# I3a Fix Round 1 限定复审

日期：2026-11-03。Reviewer：原 I3a 完整任务审查者。

**总 verdict：PASS（限定复审）。先前两项 P2 均为 ADDRESSED；修复 diff 未发现新的可行动 finding。**

## 复审范围

仅复审 [原完整审查报告](D:/Codex/建筑施工图自动化/立面插件/执行/I3a-full-task-review-20261103.md) 的两项 P2，以及本轮两文件 diff 引入的新问题。读取 `I3a-fix-round1.diff` 一次，再定点核对当前 `ProjectFacts.cpp` 的窗口/换算函数和 `ProjectFactsTests.cpp` 相应测试段；未重审未动代码或整个 dirty 树。已读实现报告末尾 Fix round 1 记录，但结论以当前原件和原始证据为依据。

目标 diff：[I3a-fix-round1.diff](D:/Codex/建筑施工图自动化/立面插件/执行/I3a-fix-round1-20261103-141929/I3a-fix-round1.diff)，独立核得 SHA256 为 `A329375AAA137418CDB0663D496477362A82CFE216754A1761BB10F0BF5B59C3`。内容仅涉及 `src/ProjectFacts.cpp` 与 `tests/ProjectFactsTests.cpp`。

## 逐项复核

### Finding 1：舍入后共线或失真的窗口 — ADDRESSED

原问题：输入 basis 合法且四个相邻角均不相等，实际导出四角仍可能共线。

当前实现 [ProjectFacts.cpp:138](D:/Codex/建筑施工图自动化/立面插件/src/ProjectFacts.cpp:138) 从实际四角求四条边，分别除以确认的宽/高，再将两组对边逐分量与 U/V basis 比较；随后在第 149–151 行核实际邻边点积与二维行列式。原反例 `origin=(1e16,0,0)`、`.6/.8` 旋转 basis、宽高 1 导出边的 X 分量为 0，无法通过与原 basis 的一致性检查，返回窗口不支持。校验同时拒绝未完全共线但已失真的四角，没有只按绝对 origin 大小禁止窗口。

对应测试 [ProjectFactsTests.cpp:107](D:/Codex/建筑施工图自动化/立面插件/tests/ProjectFactsTests.cpp:107) 保留原共线反例，并增加同 origin 宽高 4 的失真反例、同 origin 轴向 2×2 可精确表示的正例。既有正常旋转四角断言仍保留。原始 RED 日志明确在共线断言失败；GREEN 中该完整 case 通过。

**判定：ADDRESSED。** 实际边的有限性和 basis 一致性也为后续点积/面积运算提供有界输入；未发现此次新增校验引入的可行动缺陷。

### Finding 2：单位比例先除以 1000 下溢 — ADDRESSED

原问题：比例 `1e-321` 先除以 1000 变成 0，即使投影距离 `1e308` 能令最终结果保持可表示，仍成功返回零标高。

当前实现 [ProjectFacts.cpp:275](D:/Codex/建筑施工图自动化/立面插件/src/ProjectFacts.cpp:275) 分别 `frexp` 分解投影距离和单位比例，组合有界 mantissa 后除以 1000，最后用 `scalbn` 施加联合指数。对非零有限输入，mantissa 组合不会先因原比例大小而变零；double 的分解指数相加也在 int 范围内。实现保留非有限结果拒绝，并对非零投影最终完全下溢为零明确返回不支持，未偷偷提高单位比例的最低阈值。

对应测试 [ProjectFactsTests.cpp:88](D:/Codex/建筑施工图自动化/立面插件/tests/ProjectFactsTests.cpp:88) 使用原比例、X 方向、零点 0、datum 0 与待量点 `1e308`，以相对误差断言约 `9.98012604599318e-17` 米，避免宽松绝对误差让错误零值通过。原始 RED 在该业务断言失败，GREEN 通过；原 case 的正常比例、平移、方向及实际 datum 断言仍保留。

**判定：ADDRESSED。** 对零投影，零 offset 仍可正常返回；负投影的符号由 `frexp`/`scalbn` 保留。未发现此次联合指数换算引入的可行动缺陷。

## 修复引入的新问题

**No findings.** 本次 diff 没有新增 SDK、外部状态、生成授权、存储、命令或依赖框架；修复只修改实际窗口数值可靠性与距离/单位换算，并在原有两个 case 内补断言。未以“现有测试通过”替代对两段修复代码的直接核对。

## 原始证据核对

证据根：[I3a-fix-round1-20261103-141929](D:/Codex/建筑施工图自动化/立面插件/执行/I3a-fix-round1-20261103-141929)。

- 独立读取 `handoff.json`、RED/GREEN receipt、两份 build.log 尾段和全部本轮 case 原始日志；只读查看 `run-focused.ps1` 与实际 `build-command.cmd`，未执行它们。
- 本次独立对两文件当前原件与 `green/input` 取 SHA256，均相等。当前 `ProjectFacts.cpp` 为 `D9209C4C8183F1462B0A6B2A9D4ED5B1B61B8D5B464BC890600B66123F39D3E4`，测试文件为 `177C0B3488BDE0FEF7C0F0C3885AB7C10368104F4AA7F988C264E27A505AA295`。
- 独立确认本轮 manifest 为 142 项。协调者已完成全部 142 项及 31 项 live 输入比对、均相同；这部分全量结果为协调者提供的前置证据，本次未重复全量扫描。
- 两份原始 build.log 均显示构建成功、0 警告、0 错误；原始命令指定 vcvars 14.16、MSBuild `/t:Rebuild`、`VCToolsVersion=14.16.27023`。两次构建退出 0，RED 总退出 1，GREEN 总退出 0。
- GREEN receipt 绑定的实际二进制 SHA256 为 `2F690A1A5EF37B90CE78410AA73B613A55DDD688F5FE5F40D9F634008B9B058B`。

实际作者执行入口为证据根 `run-focused.ps1 -Phase red` / `-Phase green`；包装内部构建为 `cmd.exe /d /c ..\..\build-command.cmd`，cwd 是各 phase 的 `input/tests`，逐项测试为 `out/ElevationCoreTests.exe --case <case>`。脚本和 receipt 的 case 列表一致，没有 `--all`、旧 77 项或未知 case 调用。

| case | RED 原始日志 / 退出码 | GREEN 原始日志 / 退出码 |
| --- | --- | --- |
| FactRotatedWindowRetainsFourCorners | 共线窗口业务断言 FAIL；Executed 1, failed 1 / 1 | PASS；Executed 1, failed 0 / 0 |
| FactElevationUsesConfirmedUnitsDirectionAndDatum | 中间下溢业务断言 FAIL；Executed 1, failed 1 / 1 | PASS；Executed 1, failed 0 / 0 |
| FactSelectorsReadActualDefinitionPoints | 未执行 | PASS；Executed 1, failed 0 / 0 |
| FactChangesLocateOnlyRequiredSubset | 未执行 | PASS；Executed 1, failed 0 / 0 |
| FactReopenRequiresFreshConsistentObservation | 未执行 | PASS；Executed 1, failed 0 / 0 |
| FactUnsupportedSourcesAndNonfiniteValuesAreRejected | 未执行 | PASS；Executed 1, failed 0 / 0 |
| FactObservationRejectsAmbiguityAndUnsupportedChanges | 未执行 | PASS；Executed 1, failed 0 / 0 |
| FactSubsetMatchNeverAuthorizesGeneration | 未执行 | PASS；Executed 1, failed 0 / 0 |

因此本轮是两条指定业务 RED → 两条 GREEN，外加另外 6 个有限事实 case 回归，共 **8/8 focused GREEN**。没有把构建失败冒充 RED，也没有把本轮称为 85/85 全套重跑。

## 范围外观察与限制（不阻断）

未新增范围外缺陷观察，也未重新评判未动代码。原完整审查及初版 85 项证据保留为当时记录；本报告关闭其中两项 P2，不覆盖或重写原报告。

SDK reader、真实读取出处、配置交互、持久化及完整生成/BUILD 接入仍未在本切片实现；此次限定 PASS 不扩大这些能力，也不把 `currentSubsetMatches` 解释为整体生成资格。这些既定范围限制不阻断本轮修复验收。

本次唯一写入为本报告。未派子代理、未改源码/测试/工程/证据、未运行构建或测试二进制、未操作 Git/共享状态/CAD/COM/CUA/天正命令或私有字段，未触碰用户原图或未保存编辑。不存在需要为本次复审补做的测试或原生调用。
