# N9i 限定完整 Task Review（Spec + Quality）

日期：2026-11-03。审查方式：只读冻结实现、公开 SDK 合同和已有构建/唯一 fresh actual 原件。**Spec：通过；Quality：通过；No findings.** 此结论仅指 N9i 有限诊断任务符合约定，绝不表示离屏绘制、文字或生产资格通过。

## Findings

No findings. 未发现由本次限定变更引入、可以从代码或现有原件证明且应修正的缺陷。

## 范围与读取边界

- 根目录：`D:/Codex/建筑施工图自动化`。需求为 [N9i 简报](D:/Codex/建筑施工图自动化/立面插件/执行/N9i-离屏物理调色板对照简报.md:1)；交付事实为 [N9i 实现报告](D:/Codex/建筑施工图自动化/立面插件/执行/N9i-离屏物理调色板对照报告.md:1) 和 [证据索引](D:/Codex/建筑施工图自动化/立面插件/执行/N9i-离屏物理调色板对照证据索引.json:1)。已读适用根 `AGENTS.md`，以本任务明确范围及纯 SDK、输入/宿主保护限制为准。
- 基线为 `立面插件/build/n9c-20261103-133951-997-n9h-owned-resource-v27-frozen-9c7b8692`；候选为 `立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246`。下文“冻结源”均指候选 `src/NativeGsTextFootprint.cpp`；“实际 session”均指 `阶段1/runs/elevation-native-20261103-135723-9dedc08f`。
- 完整限定 diff 为 [n9i-v27-v28-small.diff](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/n9i-v27-v28-small.diff:1)，共 97 行。只请求完整读取一次；合批工具输出截断了前部，随后仅补读缺失前部，没有重读整个 diff 或审整个 dirty 树。
- 超出 diff 的定点读取均有具体核对目的：Session/cleanup/unload 用于确认新成员地址寿命及失败保活；execute/bindStyle/lifecycle/runGsCore 用于确认两臂工厂、原资格门和 fault 路由；本机 2020 `gs.h` 及 CHM 两个 setter 条目用于确认参数、void 语义和离屏用途；receipt/build.log、binding、比较/trace/catalog/closure 原件用于核实交付主张。没有扩大为原有驱动全面审查。
- 本次没有改源码、编译、运行或控制 CAD、重跑有效测试、改 Git、派子代理或改共享设置。唯一新写文件为本报告；不存在提交、切支、安装或 DWG 保存操作。

## Spec 结论：通过

| 核对项 | 结论与实际来源 |
| --- | --- |
| A/B 都是 manager + direct value | 冻结源 [574](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:574) 只设置 physical 因素，`kernelModelControl` 保持默认 false；[504](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:504) 走 `manager.createAutoCADModel`，152 配对 `destroyAutoCADModel`；365–375 保留真实 direct-value setter/getter 和源样式验证。实际 A/B trace 各第 102 行记录 manager 配对；A 第 128/139、B 第 130/141 行记录 direct value。比较 JSON 两臂字段相符，未残留 kernel 旧标签。 |
| 完全相同的原控制值、同一数组引用 | 冻结源 [128](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:128)、518–520：`std::array<AcGsColor,256>` 成员，逐项原 `0,0,0,0`，仅 0 项 `255,255,255,0`；两 setter 都传同一成员的 `data()/size()`，没有校准色、阈值或通用 ACI 表替换。A/B 在各自独立 Session 以同一初始化代码取得同值，不要求两臂地址相等。B trace 第 117 行实际 member/logical/physical 输入均为 `2649101127306`、count=256；A 第 115 行为 `2649101122410`。 |
| 地址活到 device 删除后 | Session 通过全局 `unique_ptr` 容器保活，vector 搬移不会搬动 pointee（冻结源 252–254）。cleanup 的 [168](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:168) 删除 device，期间未销毁/修改 palette；688 仅在各 Session cleanup 成功后 clear/reset。提前 cleanup 返回 false 或抛出时 Session 仍保留并拒绝成功卸载。实际 A/B device 删除后事件分别在 trace 271/273，cleanup.return 在 278/280。报告明确这是两臂共同寿命延长，未归因为 physical 的独立效果。 |
| 仅 B 多一次 physical，logical 后、任何 view 前 | 冻结源 [519](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:519) logical，520 唯一 physical 调用受 B bool 控制，521 才 createView。实际 A 第 114–115 行 logical 后第 116 行 createView；B 第 114–115 行 logical、第 116–117 行 physical、第 118 行 createView。对原始 trace 计数：A logical=1/physical=0，B logical=1/physical=1；main/fault3/fault2 无 physical，fault1 在关联后注入故障、未到 device。 |
| 保持原 main / fault / 准入 | 冻结源 [556](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:556) fault 顺序仍 1/3/2；fault1 generic/value/cache，fault3/2 与 main manager/ID，physical 默认 false。604–611 保留 main failure，只有原 zero-WD + 无 frames + 清理/ambient 安全后才诊断。A 不满足保护则 594 明示跳 B，异常记录在 595–597 标记 incomplete/skipped、真实请求/返回和 manager 配对。535 先保存原 RGB，再做原固定标定；539 后主 5×2 文字路径没有放宽。 |
| 回调、身份、资源、RGB/标定保护 | diff 未改相关核心；定点核对 365–375、390–420、475–543 和 580–582，direct-value 只在 diagnosticOnly/非持久诊断允许实际 traits 差异继续，main 原路径不放宽。真实 context/owned DB、根身份、catalog 与 cleanup 仍是保护条件；无真实回调时不取代绘制。mapping 仍固定 0.55，未改颜色或矩阵。两臂输出一直 diagnosticOnly=true、qualification=false；main failure 不会因 comparison protection=true 而清空。 |
| 本 task 输出语义 | 冻结源 [176](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src/NativeGsTextFootprint.cpp:176) 记录 scope/requested/started/returned/count/输入策略；586/597 的输出使用 `physical-palette-comparison.json`、`bce.n9i-physical-palette.v1`，不假造 readback/adoption。B 使用独立 `nonpersistent-physical-palette-control` 路径。before 事件记录调用前状态，started 在 setter 前置 true，after 仅在正常返回后写 returned=true，未把 void 返回解释为采纳。 |
| 变更范围/工具链 | 复核 receipt 的 10 项 sourceFiles、9 组 live origin/冻结 copy、二进制 SHA 全相符；v27/v28 receipt 输入按文件名对比仅 driver 与 README 改变。公共 NativeEvidence、工程、TextOccupancy、ElevationModel、validator 和 build 脚本 hash 保持。未见本限定 diff 引入 COM/CUA、TCH 业务命令/面板、私有字段、共享 renderer/config setter 或保存/安装路径。构建记录为 AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17。 |

公开合同已独立核对本机 `C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit/inc/gs.h:738–741`，两 setter 都是 `const AcGsColor*`、`int`、返回 void；从同机 `docs/arxref.chm` 通过 7-Zip stdout 在内存读取精确条目，未写提取文件。CHM SHA256 为 `92BB53D177903BB780329F5EBD5A72374C434F6DF4AF25CEB5449C90BB928BA2`；physical HTML SHA 为 `926F9173A19D00A101B4D5B370E676AC6C0A3AB87769B4E39BF142A1A3F58FCF`，logical HTML SHA 为 `EA52485DABD35374827CA019D7807C77D19F0745E74A23E35B47549C41CD3A87`，与 N9d 合同续篇一致。合同支持此次离屏对照，但未保证默认物理状态、必须调用或零回调根因；报告没有超出合同作结论。

## Quality 结论：通过

新增 palette 成员与会话所有权匹配，未引入栈地址悬垂；两臂共享执行代码，避免配色、模型工厂或样式同时变化。原 cleanup 顺序仍先 view/model、再 cache/provider 和 device/kernel/DB，失败时资源/Session 留存；新增 setter/日志异常沿既有执行 catch 和 cleanup/卸载保护处理。comparison 的完成、未完成及跳过分支保留 main failure、diagnosticOnly/qualification=false 与真实工厂，请求与正常返回区分明确。未发现本次引入的资源释放、异常路径或结果语义缺陷。

已复用 [候选 build.log](D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/build.log:26)：已有构建成功，0 错误、1 警告。**确有 vendor `rxapi.lib(libinit.obj)` 缺失 `rxapi.pdb` 的 LNK4099**；同一警告也在 v27 基线 build.log:26/37，作用是相应供应商对象缺少调试信息，并非本 task 新增编译/链接缺陷，不妨碍本次有限诊断证据核对。没有重编译，不能把此只读复核写成新的构建实测。

## 实际原件逐项核对

下列结论来自实际文件内容和现算 SHA，非仅接受作者 self-review：索引列出的 14 项 evidence 全部逐项匹配；binding、receipt、small diff、ARX、冻结输入/live origin 哈希也全部匹配。receipt SHA 为 `F9ED92FDC23E42BAFF42C83DF064E8314A25CE69D088533652681DA9357E84D4`，diff SHA 为 `75EE888E59FD48090562EB28B2A554C92DD975D9D619023FD11184ACA4A5CE9E`，ARX SHA 为 `71EF519F266414AE49750074FEE704B3A2BE6F95F2CF20E09F5EAFF515B698AF`。

| 原件及定位（相对实际 session） | 独立核对事实 |
| --- | --- |
| `case/native-case.json:1`、`case/gs-result.json:1`、`case/dispatch-state.json:1` | PID47348、nonce `9dedc08f6bdb473a98ca77f892747e64`、实际 module SHA 均与 binding/v28 相同；entryCalls=2、captureCalls=1、captureFinished/gsCoreCompleted=true，未形成第二次 capture。main failed，错误为无真实 owned WorldDraw；frames 为空、qualification=false、glyphComplete/capabilityUpgradeAllowed/layoutIntegrated=false。 |
| `case/physical-palette-comparison.json:1`，SHA `58952E13A957ECBEFC847F8ECA74E4C40EBB1FF29854DB468852C9F95D319EB8` | 两个 executed=true、fresh 独立 Session，manager/value、A/B 顺序及 palette 事实与上述源码/trace 一致。两臂 attributes/WD/viewportDraw 全为 0、rawRGBPath=null、mappingVerified=false，status=diagnostic-failed。protectionVerified=true 仅为保护与安全续行，不是绘制或资格通过；physicalAdoptionClaimed/Verified 和 readback 均 false。 |
| `case/nonpersistent-control/gs-trace.jsonl:99`、`case/nonpersistent-physical-palette-control/gs-trace.jsonl:99`；各 `owned-resource-catalog.json:1` | model 前 readyForModel=true、enumerationComplete=true，各 92 个真实资源；逐项 openStatus=0、身份可读验证=true、ownReadOpenHeld=true。最终比较中 readyForModel=false 是 cleanup 后值，不能倒置时序。 |
| A trace:173–178、175–266、267、271、275、278；B trace:175–180、177–268、269、273、277、280 | manager 模型销毁后才关闭 cache；分别 92 个 close.item 均 status=0，closeErrors=0；其后 device 删除、own DB 删除，最后 resourcesReleased=true。根 added/erased 都为 2/2，身份验证通过；ambient DBMOD 1→1、写事件均 0。 |
| `case/lifecycle-controls.json:1`；`case/lifecycle-1/gs-trace.jsonl:125`、126、127、128–219、220、226、229 | 1/3/2 三故障都真实达到、passed/cleanupComplete=true，DBMOD1→1、零写。fault1 为 generic/value/cache，模型删除后撤 provider，再关闭 92 pin，最后 own DB 释放；get/release 为 0，不能记为动态返回/归还分支已测。A/B 自定义 provider 未安装，manager 内部 callback 未观测。 |
| `native-result-closure.json`、`isolation-final.json`、`reader-retirement.json`（均按索引 SHA 核同） | workerExited=true、workerExitCode=70、explicitStopRequested=true；forcedTermination=false、normalQuit=false、nativeQuitConfirmed=false、timedOut=false。既不自行改称强杀，也不算正常 QUIT。4 项 nonce 暂存原路径均已不存在，归档副本逐项 SHA 相同；6 项源/快照/工作副本实际重新计算 SHA 全相同。自有 SHOW/foreground/sample=0，inputDesktopReadErrors=0、monitorError=null、relatedCadEventsNeedReview=false。 |
| session 只读查找 `*.rgb` | 无 RGB 原件；不需要也没有执行图像补造/替代绘制。 |

## 残余风险与验证限度

唯一 fresh actual 只证明本机此链在 B 额外 physical 正常返回后仍未得到真实绘制；无 context、RGB 或 glyph，因此根因仍未知，不能推出所有 physical 配置/公开离屏路径无效。无绘制时未实际覆盖颜色不符的标定失败分支；A 不安全时跳 B、setter/证据写入异常路径及 palette 延长后的失败保活由本次静态审查确认，没有新增故障注入或 CAD 重跑。现有原件已足够回答本 task，故未扩大实验。既有主链 failed/qualification=false 保持，N9b、BUILD、glyph 和生产采集资格均未升级。
