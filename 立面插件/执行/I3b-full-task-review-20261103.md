# I3b 限定完整任务审查（Spec + Quality）

审查日期：2026-11-03。结论：**Spec 通过（按 root 已裁定的实际验证边界）；Quality：No findings.** 本轮未发现需要作者修正的 P0/P1/P2/P3 问题。结论只适用于当前 I3b 有限 typed reader 及其独立验证入口，不授予 BUILD、绑定或生成资格。

## 范围与方法

先读 `I3b-当前定义点读取简报.md`、唯一实现报告 `I3b-当前定义点读取报告.md` 及适用的根 `AGENTS.md`；依据 review-agent、AutoCAD 2020 ObjectARX、C++ coding standards 审查。完整检查聚合包 `tests/native/project-facts/review-i3b-20261103/04-current-six-new-files-complete.diff`，范围是 `scope-stat.json` 所列六个新增文件、562 行；`current/` 为定位所用冻结副本。01/02/03 未重复作为完整代码再审。

本轮为只读代码/证据审查，仅新增本报告；未改实现、编译、启动或控制 CAD、复跑已有测试、改 Git/共享设置，未派子代理，未审其它 dirty 树或旧 I3a/GS/I2。

## Spec 审查

**No findings.** 当前实现符合本切片的输入、读取域和输出边界。

| 合同 | 当前代码与结论 |
|---|---|
| 明确 expected 与当前文档库 | `src/ProjectFactsReader.cpp:42` 同时核 current/active/working 库、context 及实际 provenance；`:145` 入口、`:196` 完成前、`:201` 异常出口都执行核对。非零 revision、document/database identity 必须与 expected 一致，不刷新旧选择。 |
| 当前内存、exact/direct | `src/ProjectFactsReader.cpp:91` 起验证已打开对象的数据库、objectId、owner，保存真实 handle/class；`:105` 起拒绝非 direct，`:121` 起以 exact runtime descriptor 分支读取 Line、AlignedDimension、Text。非 direct/非 exact 保持 monostate 和定位诊断。 |
| 真实有限 getter | `src/ProjectFactsReader.cpp:71` 读取原 textString、position/alignmentPoint、normal、公开布局标量和 hasFields；`:123`、`:126` 分别读 Line start/end 和 AlignedDimension xLine1/xLine2。field 仅保留存在标志并报 `FACT_SOURCE_FIELD`。可见性在 `:114` 限定为实体 visibility 与自有图层 on/unfrozen。未引入 measurement、extents、字体加载、field evaluation、REGEN、setWorkingDatabase 或产品磁盘读取。 |
| missing 与 unavailable | `src/ProjectFactsReader.cpp:60` 复用 EntityKey 的非零 uint64 十六进制合同并规范大小写/前导零；`:175` 使用 `getAcDbObjectId(..., false, ...)`，不造 raw ID/stub。只有 eUnknownHandle 或真实 erased/effectively-erased ID 证明 missing；其它解析/open/身份异常是 unavailable，`:197` 阻止返回完成批次。 |
| 输出能力边界 | `src/ProjectFactsReader.h:5`、`:12`、`:17` 明确本轮 `ProjectFactSourceRead`/请求合同。只含本批 provenance、model owner 与源 subset，不捏造 projectId/确认单位，不构造 ObservedScene/语义未齐的 Observation，不写 archive.bound、RuleInputs 或生成资格。completed 与 supported/matched/eligible 分离。 |

为回答具体依赖疑点，定点读了旧 `SceneReader.cpp:56` 至 `:93` 的 NativeSceneContext 生命周期/数据库与文档 reactor、`:95` 至 `:125` 的 UTF/handle 转换，以及 `ElevationModel.cpp:44` 至 `:75` 的 handleValue/EntityKey 相等合同。前者证实修改推进 revision、切到其它当前文档使 context 失效、销毁使当前库不可得；后者证实零值/非法字符/溢出被拒，任意前导零可接受。另读 `ProjectFacts.h:10` 至 `:50` 确认字段与默认 unsupported 状态，并定点核本机 2020 SDK `dbents.h:123`、`dbdim.h:537`、`dbmain.h:2027` 的公开 const getter 签名。这些是本次新适配器的依赖合同检查，不是重审旧能力。

## Quality 审查

**No findings.** 成功 kForRead open 在 `src/ProjectFactsReader.cpp:20` 的局部 RAII 中配对 close；无数据库管理对象 delete，无对象指针逸出。单源读取异常收集为 unavailable，整批关闭后再次核 expected/current，再决定是否返回值；不会把部分未知源伪装成完整匹配输入。

独立夹具 `tests/native/project-facts/ProjectFactsDriver.cpp:28` 只在测试内从冻结方案 side DB 克隆指定源到当前未保存图，保存 source→current 映射；`:40` 起创建未保存中文/几何及必要反例，均先于 NativeSceneContext/Monitor 读区间。AttributeDefinition 是合法 direct Text 派生拒绝正控制，不依靠非法 append 的失败充当业务测试。

`ProjectFactsDriver.cpp:68` 的单一 read 包装器在每批前后用 `ProjectFactsFixture.h:74` 的数据库 reactor 和 `:107` 的 Boundary 检查 current/active/working/context、revision、DBMOD、根 inventory、资源定义及 write-open/modify/append/erase/unappend/reappend/header 写事件。`:104`、`:105` 的主动 Line/Text 修改位于读区间外。源 getter 原值由 capability 输出，未以 all-pass 开关代替数据。

32 项断言逐组检查如下；循环展开后的项目数与既有实际报告一致：

| 断言组 | 数量 | 定位 |
|---|---:|---|
| 九批 ReadOnly | 9 | `ProjectFactsDriver.cpp:71` |
| 四幅真实 datum/title/两段楼层文字及 batch provenance/model owner | 5 | `ProjectFactsDriver.cpp:79` |
| 未保存中文布局、Line 定义点、Aligned 定义点不随显示覆盖改变 | 3 | `ProjectFactsDriver.cpp:98` |
| 当前未保存修改确实替代旧值 | 1 | `ProjectFactsDriver.cpp:108` |
| 零 revision、旧 revision、错 document identity、错 database identity | 4 | `ProjectFactsDriver.cpp:109` |
| entity hidden/layer off/layer frozen | 3 | `ProjectFactsDriver.cpp:117` |
| 非 direct 真 owner/class；BlockReference/AttributeDefinition/MText 真类拒绝 | 4 | `ProjectFactsDriver.cpp:119` |
| field 存在、两类 proven missing 定位、malformed unavailable 不冒充 missing | 3 | `ProjectFactsDriver.cpp:122` |
| 合计 | 32 | |

构建闭包检查了 `ElevationProjectFactCases.vcxproj:6`、`:10`、`:13` 与 `tools/build_project_facts_reader.ps1:11` 至 `:49`：SDK2020/R23.1、x64、v141 14.16.27023、C++17；工程的编译单元与冻结复制输入吻合，生成的 AuditedFactLocations.h、audit/TSV、脚本及构建命令都纳入 receipt。ProjectFacts.cpp 没有在本 native driver 中调用或链接需求；EntityKey 实现由已列入的 ElevationModel.cpp 提供。receipt 的 `nativeExecuted=false` 是构建回执边界，实际执行结论来自独立宿主原件。

复用 root 已完成的最终 22 项构建输入、20 项 live origin、compiled copy 与实际模块一致性终核，不重复哈希/编译。冻结 receipt SHA 为 `C0AF93E4CECEB83CE645A716DA38ED627211FD23F0F99DC15E1E24D0C4DA9211`，模块 SHA 为 `FC2D9320C3A7B741B3D25267A0D0A4E992E1158CCB5117AB0A7BB9CCA2D377E4`；构建已成功，保留既有 rxapi.pdb 缺失的单项 LNK4099 警告。

## 实际验证证据与限度

本审查复用 root 已直接读取原件的事实，并核其与 `evidence-index.json`、实现报告和冻结代码的对应关系；未重新运行任何测试。最终 `1b7be28c` 为同冻结候选 fresh 原生复验，32/32 passed、errors=[]、11 checkpoints，9 读取区间 ReadOnly passed、DBMOD 均 1→1。`ReadUnsavedEdits` revision=3 实际读到“未保存修改中文”和 Line start=(123,456,0)。全 case 总计 4 条事件来自 driver 主动 Line/Text openWrite+modified，不能写成整 case 零写。

历史四记录保持区分：`5e06e911` fixture append:21、无业务 RED；`07a2fef5` 才是有效业务 RED（9 pass/23 fail、errors=[]）；`c3c34ac9` 是 ready 前 exit1、无业务报告的启动中断；`1b7be28c` 是最终 GREEN。未用构建/基础设施失败替代业务 RED，也未把启动中断写成 getter 失败。

GREEN 收尾为 exit70、explicitStop=true、forced=true、normalQuit/nativeQuit=false；4 退役、6 保护检查、own0，自有 SHOW/foreground/readErrors=0。`relatedCadEventsNeedReview=true` 仍保留：20 条日志均为既有其它 PID35036 的 create2/destroy2/foreground4/hide6/show6，本任务未操作，不能改写为全0或归责 own。

**实际切图和读取中途切图没有执行。** 本轮仅确认静态入口/出口门及 context 失效合同；按 root 裁定留给后续普通命令实际验收，不声称动态切图已通过。非 Z 默认法向/其它排版组合、TCH 专有 getter、完整绘制可见性、保存重开和生成链也没有因此获得新的验证结论。源 typed subset 的成功读取不等于项目配置确认或可生成施工图。

所有原件定位以 `tests/native/project-facts/review-i3b-20261103/evidence-index.json` 的 35 项索引为准；本报告不修改唯一实现报告或原始证据。
