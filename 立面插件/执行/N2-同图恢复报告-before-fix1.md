# N2 同图恢复报告

当前状态：NATIVE_14_OF_14_PASSED，待一次任务独立复核。最终候选 f1f588a8 已实际执行全部 14 项通过；结论仅覆盖下述普通 Line 路径。

## 最终原生结果与复核冻结

- 最终构建：`../build/n2-20261101-184109-321-explicit-target-linetype-checked-f1f588a8/receipt.json`。ARX SHA256：`2E94DFB2C111EBA50284E151C935094F9DA6ED414E9149D4D11E59005D3B0A3F`；精确冻结源码/依赖/build script 与收据逐项 SHA 核对通过，工作区本任务源码与最终冻结副本逐字节相同。
- 原生会话：`阶段1/runs/elevation-native-20261101-184130-fbbd5d3b/`，PID 21360 / nonce `fbbd5d3bbd3f4d9aa38990ed1d6303ea`；binding 为 `../evidence/native-case-binding-fbbd5d3b.json`。case/inplace-case.json 的 status=passed，errors=[]，14/14 断言通过，末检查点为 final-verification-complete。
- 真实 U：`realUndoExecuted=true`、`undoCommandsStarted=1`、`undoCommandsEnded=1`，已读取原生字段确认。基线 4 根/88 资源，成功提交 5 根/89 资源，U 后 4 根/88 资源。完整 baseline 与 undoAfter（包括 DBMOD、文件名、实体、资源、全局值）结构相等，不仅计数相等。
- 原句柄 2F3 的既有文字及 2F5 的“保留未保存编辑”文字在撤销后内容、样式和几何保持。成功前的后续未保存 S 端点编辑也纳入最终基线并保留。
- 异常/取消均确实经过新增图层、新 Line、目标移动三个阶段后进入同一恢复路径。实际恢复后 lookupStatus=0 但 erased=true：rollback 新层/Line 2F6/2F7，cancel 2F8/2F9，U 2FA/2FB。没有宣称已物理 purge；它们在数据库中是不可见擦除记录，公开根/资源恢复原样。
- 通过项包括：未保存夹具、当前场景读取、独立准备库、异常恢复、取消恢复、大小写同名 Shared 样式冲突、外层事务保持、真实几何修订过期、跨上下文过期、目标库 Continuous 依赖实际回读、成功范围、全局保持、一次 U、未命名文档及磁盘输入保持。会话/output DWG 清单始终只有两份只读输入，不生成 DWG。
- 收尾：native-result-closure.json 与 reader-retirement.json 已复核；forcedTermination=true、normalQuit=false、timedOut=false；自有宿主精确停 70，4 暂存退役、6 输入哈希保持，自有 SHOW/前台 0，监测错误为空。不是正常 QUIT。
- 单次任务复核材料：`N2-inplace-review.diff`（首个真实 reader/prepared RED 的相对路径差异到最终候选），`N2-同图恢复-review-manifest.json`（完整 11 个本任务文件、全部冻结编译依赖、二进制/收据/原生证据 SHA）。不依赖其他 agent 正在变更的 CoreTests/Recognition 注册。待 root 派一次复核；尚无 review No findings 结论。

## 最终能力边界

已实测的是当前自有未命名文档中的明确普通平面 Line 目标、新增单 Line/图层，以及既有普通 Text 和图层/文字样式/线型公开读回保护。未支持类型明确拒绝，不能外推到天正、Polyline/Hatch/Dimension 或完整立面生成。文档切换/销毁失效 reactor 已实现；本套实际过期断言覆盖几何改动及跨上下文，未进行真实文档关闭重建的专项测试。未实现正式 BCE_BUILD、完整配置字典或一键施工图。成功撤销边界依赖独立完成的原生 modal 调用命令；外层事务/已有撤销组/禁用撤销明确拒绝。

以下保留开发、RED 和根因调查过程，历史“待实测”均以本节最终原生结果为准。

## 2026-11-01 新图层定义差异调查（非 review 修复轮）

- 有效 writer RED：session `阶段1/runs/elevation-native-20261101-182713-60cbfe7b/`，PID 1332。前三项前置均通过；恢复/取消/冲突/外层事务/过期/跨上下文/成功因实际 writer NOT_IMPLEMENTED 失败，U 明确未执行。root 已精确清理。
- GREEN 实跑：binding `../evidence/native-case-binding-1a83db29.json`，session `阶段1/runs/elevation-native-20261101-183203-1a83db29/`，PID 20432。读取和独立准备库通过；SharedStyleConflict、OuterTransaction、Stale、CrossContext 通过。三次写入均在 `created layer does not match prepared definition` 失败，observer stages=0。全局实际值相等但“成功写入不改全局”断言的成功前提不成立，不能计通过。未执行 U。root 已 forced 70 / 4 暂存退役 / 6 输入保持，无超时，不是正常 QUIT。
- systematic-debugging 阶段 1：已追踪 error → writer 新图层实际 resourceDefinition → 与 PreparedBatch 原生库的 definitionSignature 比较。原报告缺 expected/actual，不足以指出具体字段。三条路径同点失败，不能猜测是资源映射或直接放宽签名。
- 阶段 2 静态模式差异：两边均 new layer/setName/setColor/add；准备路径为 add→close→read，写入路径为 add→事务纳管→仍写打开时 read。数据库实例和生命周期阶段不同；这只是已识别差异，尚不是经实测确认的根因。
- 只读诊断候选：`../build/n2-20261101-183610-439-layer-definition-diagnostic-336c6d35/receipt.json`，exitCode=0。仅增强原失败消息，完整输出两侧 signature 及 name/handle/colorRaw/colorIndex/colorMethod/linetypeHandle/linetypeName/lineWeight/off/frozen/locked/plottable/transparencyAlpha/transparencyRaw/isNewObject/isWriteEnabled/isTransactionResident。原严格比较和回滚行为保留，没有 setter 修复。
- 阶段 3 实测证据：session `阶段1/runs/elevation-native-20261101-183729-f99d2a35/`、PID 31936。两侧逐字段唯一差异为 prepared 的 linetypeHandle=5D/name=Continuous，而 writer 当前新图层的 linetypeHandle/name 为空；颜色、线宽、开关/冻结/锁定、可打印、透明度值全部相等。准备图层已经 close/read，当前图层仍 write-enabled/transaction-resident。严格签名直接体现 `10:Continuous` 与 `0:` 的差别。root 已精确清理、无超时。
- 已据此确认根因：服务依赖新图层的 close-time 默认解析，但在事务纳管后关闭前即校核依赖，此时线型为空。没有把所有数据库差异归因资源映射，也没有放宽比较。
- 阶段 4 单一修正：从准备图层实际 getter 取得线型符号名，在目标数据库的 AcDbLinetypeTable 用 getAt 解析目标 ObjectId，再按 SDK2020 `dbsymtb.h` 的 `setLinetypeObjectId(AcDbObjectId)` 显式设置且检查返回状态。没有跨库复制 ID、没有关闭事务对象、没有删除签名字段。失败诊断完整保留。
- 已增加独立实际回读断言 `CommittedLayerUsesTargetDatabaseLinetype`：成功后新图层的线型 ID 必须等于目标数据库实际 Continuous 记录，并保持颜色及图层公开属性。随后 fbbd5d3b 会话已通过该项及异常/取消/成功+一次 U，完成本次单一根因修正的实际复验。
- 最新修正候选收据：`../build/n2-20261101-184109-321-explicit-target-linetype-checked-f1f588a8/receipt.json`，exitCode=0；早一份 184043 仅为编译过程，不作为本轮运行候选。

## 2026-11-01 当前候选与真实证据

- 第一轮原生 RED：`../evidence/inplace-red-05c0ab23.json`，PID 34888 / nonce 05c0ab23f9d14b86aa4d7c441cac54e5。4 根未保存夹具（含中文 Text）存在，DBMOD=1；reader/prepared 两个预期缺实现断言失败，stub 未改当前图，错误数组为空，0 新 DWG。root 已清理自有宿主。
- 第二轮：`../evidence/native-case-binding-e6aad0b9.json`，PID 32592 / nonce e6aad0b9c4ce48af80801af30d6eb3ff。PreparedBatch 独立库通过，但 reader 返回 `linetype text: 2`；writer 分项没有运行。此轮为实际读取故障，不计 writer 行为 RED。root 已精确停止/退役 4 暂存、6 输入保持；不是正常 QUIT。
- SDK 2020 `inc/acadstrc.h:26` 明确 2 是 `eNotApplicable`；SDK ARXDBG `samples/database/ARXDBG/Snoop/ArxDbgUiTdcDbObjectBase.cpp` 仅在 shapeStyle 非空时读取 textAt。现仅接受 **textAt=eNotApplicable、shapeStyle=null、shapeNumber=0、返回文字为空** 的普通 dash 无文字状态，并把实际状态写入资源签名；其他非 eOk 仍拒绝，不泛放宽 getter 错误。
- 后续 writer RED 冻结收据：`../build/n2-20261101-182113-881-writer-red-plain-dash-6406e8c7/receipt.json`，exitCode=0。真实 writer 仍返回 NOT_IMPLEMENTED，七项测试已写；必须先实际跑过读取前置并观察缺实现导致的 writer 断言失败。
- 后续 GREEN 冻结收据：`../build/n2-20261101-182403-323-writer-green-ownership-4f20e477/receipt.json`，exitCode=0。替代仅编译过的 182241 候选；修正 DB 入库后到事务纳管前不能经过收据字符串分配的所有权异常窗口，改为立即纳管后填收据。增加非 std observer 异常的同一路径回滚。
- 两份冻结源码、模块和日志均独立目录；本代理没有启动 CAD、操作进程或修改已加载候选。每份构建仍有 SDK rxapi.pdb 缺少调试信息的 LNK4099，不冒称零警告。

## 实现说明（原候选阶段记录）

- WriteContracts 为无 SDK 值契约。NativeSceneContext 用独立随机 token、文档/数据库 reactor 管理身份及修订；文档切换、销毁、实体/资源修改使旧快照失效。SceneSnapshot 不保留 SDK 指针。读取当前未保存文档的明确目标根，其余 Line/Text 保留；未知实体明确拒绝。只读/锁层/原生命令上下文另作门禁。
- PreparedBatch 拥有独立 AcDbDatabase，准备一根普通平面 Line 及指定图层；writer 验证此受限准备内容。没有实现天正/文字/复杂批次写入能力，不炸开或替换专业对象。
- beginWriteScope 重读身份和修订，通过 F1 planWriteScope 校验白名单与资源。现有外层事务明确拒绝；不结束调用者事务。UNDO 禁用或存在调用者撤销组时拒绝。
- 当前图事务纳管新图层、新 Line 和明确目标平移；同名同定义复用，同名不同定义报 RESOURCE_CONFLICT。入库前由本地独占所有者删除，入库后立即移交数据库/事务；不默认 delete DB 对象，不重复 close 事务对象。
- 异常和取消返回创建句柄并只 abort 本服务事务；独立测试重读实体/资源/文字/全局，并检查新增句柄 invalid/erased，不以调用过 abort 代替恢复验收。不 saveAs/close/reopen，不从磁盘重载，不用反向变换假装撤销。
- 命令序列为 RUN → SETUP → APPLY（故意失败）→ CANCEL → CHECKS（冲突、外层事务、过期及跨上下文）→ SUCCESS → 一次真实 U → VERIFY。成功写入依靠独立完成的 ACRX_CMD_MODAL 原生撤销记录；测试 AcEditorReactor 要求 U started/ended 都为 1 且失败为 0。无成功提交时不排队 U。
- 最终 JSON 包含 baseline/after/rollbackAfter/cancelAfter/undoAfter、实际对象与资源、创建句柄状态、全局值、DWG 文件清单、检查点和模块/INI/源 SHA。输出兼容 native-case.json，但完整成功只能在 VERIFY 后写入。
- 上述原候选待实测项已在最终 fbbd5d3b 的 14 项中执行；未覆盖的类型与生命周期专项保留在“最终能力边界”。

## RED 候选

- 冻结构建收据：`../build/n2-20261101-174748-141-red-stubs-d62dbf32/receipt.json`。
- 模块：同目录 `out/ElevationInPlaceCases.arx`；SDK 2020 / x64 / v141 14.16.27023 / C++17，编译 exitCode=0，0 错误；一条 SDK rxapi.pdb 缺少调试信息的 LNK4099 链接警告。
- 收据包含 binary、binarySHA256、sourceFiles、compiledSourcesDirectory、inputOrigins、exitCode、log。编译使用冻结 ASCII 文件名副本，不改变 N1 Driver/Probe、F1/R1 源或统一构建脚本。
- 测试入口独立组 BCE_INPLACE_CASES；BCE_TEST_RUN 完成七键 INI、SHA、nonce、隔离桌面、源和输出目录校核后，只允许初始未命名且模型空间为空的 Drawing1。
- 命令序列：BCE_TEST_RUN → BCE_IP_SETUP → BCE_IP_APPLY → BCE_IP_VERIFY。SETUP 作为独立完成命令建立两根同层 Line、一处既有 Text、一处“保留未保存编辑”Text 和 Shared 文字样式，不保存图纸。
- 独立审计读回 Line/Text 几何文字、句柄和样式，读取图层/文字样式/线型以及 INSUNITS、LTSCALE、CELTSCALE、DIMSTYLE、FRAME、WIPEOUTFRAME、IMAGEFRAME、XCLIPFRAME。未覆盖实体类型拒绝审计，不默认为零。
- RED 生产服务明确返回 UNSUPPORTED_NATIVE_WRITE/NOT_IMPLEMENTED；预期 ReadSceneIncludesUnsavedEdits 和 PreparedBatchOwnsIndependentDatabase 两条真实断言失败。七项恢复场景明确标为 not_run，不能把“未写入”冒称恢复通过。
- 完整结束报告为 output/inplace-case.json（bce.inplace-case.v1）；兼容现有运行器同步写 native-case.json。独占句柄初稿状态 failed，最后 VERIFY 才替换为完整结果。异步排队不是成功证据。

## 后续

root 已完成真实 writer RED、GREEN 故障定位、单一修正和最终原生通过。现仅等待一次任务独立复核；不再修改源码或冻结候选。

最终支持边界仅为实际通过的普通 Line 路径，未外推天正。实现子代理没有启动 CAD、操作进程或用户图，所有实际宿主由 root 串行管理；未修改平面插件或安装包。
