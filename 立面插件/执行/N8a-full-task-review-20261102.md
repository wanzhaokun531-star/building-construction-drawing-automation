# N8a 首次独立完整任务审查

**Spec compliance：❌ 有一项 P2 反例覆盖缺陷；最小 mm1:100 适配目标及本次允许的有界失败策略成立。**

**Code quality：Needs fixes。** 发现范围是拒绝测试的判定与源 SHA 覆盖，不否定最终四个 mm1:100 场景的原生适配、保存和重开事实。

## 已做好的部分

- `DimensionAdaptDriver.cpp:68` 的 explode 接管限定公开 cast、三空和 exact 类型，保留 isPersistent 原值；`DimensionAdaptDriver.cpp:115` 核真实非 proxy、直接模型 TDbDimension2，并读取实际分段、夹点、measurement、format 和 override。符合本次明确的生命周期裁定，没有以普通尺寸或 Text 替换整体对象。
- `DimensionAdaptGreen.inc:129` 按实际拓扑重取并匹配夹点，移动次序来自 `DimensionTopology.h:42`，每次动作后核测点；基线另匹配、另移动。`DimensionTopologyTests.cpp:10` 起覆盖乱序/反向分段、实际夹点顺序及重复、分叉、倒序、非有限值等边界。
- `DimensionAdaptGreen.inc:225` 区分真实 SDK pair 与公开 explode 派生 binding，检查 exact 类、数据库方向、表 owner/getAt、完整 key 的单层 mangle、声明定义、递归 TextStyle 和双向一致性；有实际标量修改、同定义异 key、错向和逆向 registry 控制。
- `DimensionAdaptGreen.inc:252` 每场景自有 scratch/candidate；保存仅发生在各项接受检查之后。`DimensionAdaptGreen.inc:172` 比较同一声明字段集合；`DimensionAdaptGreen.inc:288` 的 finally 审计独立采集后置错误和保护结果，未把缺失 after 或 scratch 懒写解释为受保护源零写。

## 可操作发现

### [P2] 让源 SHA 反例真正验证正式校验，并使未抛异常必然失败 — `tests/native/dimension-adapt/DimensionAdaptGreen.inc:186–187`（调用：`:323`）

`expect_reject` 把 `operation()` 正常返回后自行抛出的 `fault_was_not_rejected:` + name 放在同一个 catch 中，再以 `message.find(expected)` 判断成功。源 SHA 这项的 name 和 expected 都是 `source_sha_mismatch`，所以操作即使没有拒绝，辅助函数也会捕获自己的 `fault_was_not_rejected:source_sha_mismatch`，返回 `rejected=true`。此外，该项仅另写 `require(sha256(config.reference)==全零SHA)`，没有经过正式入口 `validate_identity` 的源绑定检查；正式检查若被移除，这个反例仍会通过。它不能作为简报要求的“源 SHA 不匹配到达相应阶段并被拒绝”的有效覆盖，也使报告和 manifest 的“15 个有效反例”主张过强。

建议只捕获被测操作抛出的错误，将“未拒绝”失败置于 catch 之外；源 SHA 控制应以错误源输入调用同一正式身份检查，或调用正式入口实际共用的源绑定校验。补一个 operation 正常返回且 name==expected 的小控制，确保辅助函数自身不能通过。无需为此重跑整个矩阵。

核查依据：最终 `case/dimension-adapt.json` 的该项 diagnostic 确为 `source_sha_mismatch`，说明本次独立零 SHA 比较确实抛错，未实际触发 sentinel；这不验证 sentinel 分支，也不验证正式入口拒绝能力。针对“是否另有真实校验已覆盖”这一命名风险，仅检查了未变依赖 `tests/native/NativeEvidence.cpp:179–190`：正式源 SHA 检查位于 `validate_identity`，本反例未调用它。其余反例的 name/expected 以及四项 binder 控制未发现同一 sentinel 误命中。优先级为 P2：这是必做拒绝覆盖和证据可信度缺陷，未发现本次源保护失效或生产写入路径。

**P0/P1：无。其它满足 actionable 标准的新增缺陷：无。**

## 规格与最终证据核查

审查依据为包内 `review-context.md`、原 N8a 简报、作者完整实施报告和 `full-task.diff`；base 为 `cb0b5837e4c0941f71c443948b8d425280d5aa75`，无 task commits。六个新增文件全文均纳入，按分段输出完成同一 diff 的阅读；没有另读 changed source 或运行 Git。项目适用约束及 review-agent、ObjectARX 2020、C++ 和 SDD task-reviewer 方法已应用。

| 文件 | 核查范围与结论 |
| --- | --- |
| `DimensionAdaptDriver.cpp` | 公共读取、所有权、working DB 恢复、命令异常边界和 Evidence 接入；未发现其它新增缺陷。 |
| `DimensionAdaptGreen.inc` | 显式 fixture、七场景、独占克隆、动态适配、资源绑定、纸面接受、保存/重开、反例、保护审计及资格输出；发现上述 P2。 |
| `DimensionTopology.h` | 完整邻接路径、方向/有限值/唯一性、非穿越移动调度；未发现新增缺陷。 |
| `DimensionTopologyTests.cpp` | 真实纯规则输入及拒绝断言；未发现新增缺陷，未重跑。 |
| `ElevationDimensionAdaptCases.vcxproj` | x64/v141/C++17/SDK 2020、独立 ARX 和只读 NativeEvidence 依赖；符合范围。 |
| `tools/build_dimension_adapt.ps1` | 独立冻结目录、编译副本及 live 输入绑定、构建/纯测试 exit、receipt；未发现新增缺陷。 |

最终取证使用 manifest 指向的 `1880dc7f` receipt/build.log、`014cca66` 有效 RED、`03f2f503` 最终原生 JSON 及 closure，均只读。直接检查了场景/反例记录、两段 clone 基线和完整 SDK map、mm50 transform 后观测、源保护 after，以及最终三份收尾 JSON。411 个 path+SHA 绑定、388 JSON 严格解析和四个 DWG 的完整批量校核复用 controller 已完成的结果，不冒称本 reviewer 又做了一次。没有启动 CAD、编译、运行原生或纯测试，也没有改源码、Git、归档或共享环境；唯一写入是本报告。

- 有效 RED 的 setupReachedTargetAssertion=true，stationsMatch/baselineMatch=false；这是目标未适配的失败，未把早期 setup/编译失败当成 RED。
- 最终七场景是四个 `supported_public_observations`、三个本方法 `unsupported`。四个支持场景分别比较 460/359/864/1470 项，共 3153 声明字段无差异；七个 setup 均成功。原生 case 为 closed partial，123 passed + 5 unsupported、errors=[]；后两项 unsupported 是故障注入场景，不能称 128 全通过。
- mm50 原始 transform 之后，几何首段约 199.983196，measurement 约 399.966392，dimlfac 约 2，直接 formatMeasurement 得 `\\A1;800`。作者最新报告准确限定为本方法的 measurement/格式语义前提未成立，没有声称所有公开 API 不支持 1:50；m50 和非共线同样保留实际动作后的失败，未保存候选。该任务允许这些未知维度实际失败并拒绝，因此不以未成功为缺项。
- 最终保护 after 的 baselineComplete/postObservationComplete/currentActiveWorkingPreserved/allPreserved 均 true，errors 空，三库七类写事件均为零；scratch 写事件单列。旧 reference 真写失败仍保留，未迁为 passed。
- closure 明确为自有宿主退出码 70、四暂存退役、六源 SHA 保持、无仍运行自有 worker，normalQuit=false；作者未称正常 QUIT 已通过。
- 最终 build exitCode=0、日志包含 `N8A_TOPOLOGY_TESTS_PASSED`；LNK4099 是 SDK rxapi.pdb 缺失的已披露调试信息警告，没有把构建输出写成无警告。该既有 SDK 环境噪声不是本次新增源码缺陷。

其它明确风险定向检查：两段 root linetype 记录 source `15` / target `5C` 与实际 SDK map 的 `15→5C` 一致；root 主 pair、Layer 和其 Linetype 也在本次材料内核对。没有因此扩查完整生产资源图或整个插件。

## 资格与结论

作者 `DONE_WITH_CONCERNS` 对四类 mm1:100 公开观测成功和三个有界失败的界定基本准确；必须修正的是源 SHA 反例及其“15 个有效”覆盖主张。四份保存/重开成果仍可保留为该明确支持域的观察证据。

真实 TCH host 文字请求、完整 glyph、1:50/m 的语义闭环、非共线成功和任意类别/段数生产能力仍未证明，报告已有明确保留。`glyphComplete=false`、`capabilityUpgradeAllowed=false`、不升级 fieldSetter/glyphPreparation、不授当前图提交资格与任务范围一致；这些边界不构成本轮额外修复要求。修好上述 P2 前，本次 full-task gate 结论为 **Needs fixes**。
