# N9e：原离屏链有限 GS 边界实测报告

2026-11-03。状态：N9e 有限边界观察已完成；N9c 原生缓冲采集仍未完成，真实 nativeStatus 保持 failed。唯一 controller 已完成一次 v19 fresh 实测并闭合证据。作者未启动或控制 CAD、派子代理/审查、提交或切换分支。

## 本次实现及结构差异

基线是冻结 v17 `build/n9c-20261103-121817-090-idle-command-capture-v17-frozen-167b5fda/src/NativeGsTextFootprint.cpp`，SHA256 `B21EC93D28AA2A0C943092FB08A2BDF40DD74B45328227FF5DE72298C1B2A022`；开工时 live 完全相同。仅 driver 与本任务 README 增量；另存本机 SDK 原文、差异和本报告，不改公共 NativeEvidence、host/watch/bridge、TextOccupancy 或生产源。

1. model/view 创建后读实际 graphicsKernel，保留双方地址和比较事实；真实不一致立即拒绝后续绘图。view 创建、device 绑定、初始根/样式绑定、每帧 readd 和原 device/view 更新前后保存 getModelList 全部实际指针、数量与预期 model 是否在场。原 getModel(root)/device kernel 证据保留；不预置集合为匹配。
2. 每个 Session 内嵌稳定地址 RootObserver，kernel.addModelReactor 在自己 model 的任何 view.add 前登记。只覆盖 rootAdded/rootErased，均原样返回 true；只记 model/root 与预期比较、次数，每 Session 最多 32 条明细，其余仅累加计数。IO 例外不穿宿主，以持久失败位及计数保留，原真实 callback/RGB/资格门不放宽。cleanup 仍先 eraseAll、清理原对象链，再配对 removeModelReactor，后 destroyAutoCADModel；观察器活到 Session 销毁，并纳入资源检查和 unload 拒绝。原 fault 1/3/2 路径同用该生命周期，不增故障矩阵。
3. main 与原条件 nonpersistent 各一次 getConfig/currentDisplayDriver/driverName/getVirtualDeviceName；三异常 Session 不读显示配置。getter 输出标明 singleton 显示配置，绝非离屏活动 driver 或加速启用状态。三 WCHAR 缓冲各零初始化 32768 字符（65536 字节），传入数值 32768，兼容头文件/文档长度单位歧义；记录分配、实参、NUL/位置与 raw。空、无 NUL 或可能截断保留未知；仅正常返回且路径与名称均有效时按公开输出合同解释硬件能力 bool，否则 bool 只是 raw storage。没有无效 bool 哨兵、第二次换初值查询、setter、dialog 或内部 API。

所有原 model/device/view 工厂、associate、根、样式、相机、renderer、更新调用及 v17 一次 idle→modal capture 调度保持。未补 addSceneGraphRoot、未改 generic model、未新增参数矩阵。固定原帧矩阵若意外达到真实 context/RGB，仍执行原全套检查。

## SDK 2020 原文与安全长度

依据 `C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit/inc/gs.h` 与 `docs/arxref.chm`。本次从原 CHM 以 7-Zip stdout 提取 11 个精确条目至 `tests/native/text-footprint-gs/sdk-public-contracts/boundary-observation-n9e/`，`source.json` 逐项哈希。CHM SHA256 `92BB53D177903BB780329F5EBD5A72374C434F6DF4AF25CEB5449C90BB928BA2`。getConfig 明确返回 singleton；currentDisplayDriver 长度按字节描述，bool 表示配置 driver 的硬件加速能力。model.graphicsKernel HTML 文案误写 AcGsDevice 的既有不一致保留，以 gs.h model 声明/注释核对。

## 编译、自审与冻结

- 复用 `tools/build_text_footprint_gs.ps1 -Label n9e-gs-boundary-observation-v19-frozen`，v141 14.16.27023 / C++17 / x64 / SDK 23.1 / /W4 /WX。
- 最终 receipt：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8\receipt.json`，exitCode=0；0 编译警告/错误，SDK rxapi.pdb 缺失的 LNK4099 为唯一链接警告。
- ARX SHA256 `24884E607FA9B727DE2FB72F8AE6CA1A95354E64469EB30F5BC305422664EE1D`；driver SHA256 `A331F4B036B87A657D6780B5816419D9AA85E71F23F7BD43F7CAAEC9B579E253`。
- 小 diff：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8\n9e-v17-boundary.diff`（157 行统一差异）；自审/保护指纹：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8\n9e-self-review-freeze.json`。已逐项核对所有构建输入 live=frozen，除 driver/README 外与 v17 冻结一致。
- 首次构建 `n9c-20261103-123834-338-n9e-gs-boundary-observation-v18-c21d1d24` 因 lambda length 捕获缺失失败；修正捕获后最终构建通过。未运行该失败二进制。
- 不为只读 getter 增镜像单元测试；真实根通知、三原异常控制、清理与源保护已由 controller 在同一路实测，见下文。未重跑旧 N9a/N9b/N8a。

v18 冻结未派 controller、未实际原生运行。冻结前自审校正唯一一处：getConfig 返回空时记录 `displayConfig.unavailable / unknown-config-unavailable` 后返回原链，不把辅助配置可得性增加为 GS 前提。v19 重新实际编译 exit 0，差异另存 `n9e-v18-config-null-correction.diff`；实际 model/view 跨 kernel 拒绝保持。

## 唯一 fresh 原生事实

本次 session：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-124216-72c8e7b0`，nonce `72c8e7b05bf942608432b838d1a918cb`，PID 22936，实际模块为上列冻结 v19 ARX。作者仅在 controller 闭合后只读核对，不操作宿主。最小证据定位及逐文件 SHA 见同目录报告配套 `N9e-实际证据索引.json`，不复制或改写原始 native 结果。

`case/dispatch-trace.jsonl:4,7,10` 实测 idle-ready 的 CMDACTIVE=0、CMDNAMES 为空、quiescent=true、inputPending=0；唯一 modal capture 为 CMDACTIVE=1、无 SCRIPT 位、inputPending=0、applicationContext=false。实际两次 entry 中第二次为 exact-noop，captureCalls=1，hook 已移除，capture-finished。不是启动命令中的伪采集。

| 边界 | main BTR | 条件 nonpersistent PrimitiveRoot |
| --- | --- | --- |
| model/view 实际 kernel | 均与 acquired kernel 匹配 | 均匹配 |
| 创建/接入 device 后模型列表 | 0，containsExpected=false | 0，containsExpected=false |
| 根/样式绑定后及 readd、原两种更新前后 | 1，唯一预期 model，所有 containsExpected=true | 同左 |
| 根通知 | added=2、erased=2，四次 model/root 均匹配，均返回 true | 同左 |
| observer 明细/记录失败 | 4/32；logFailures=0，observationFailed=false | 同左 |
| 更新后的图形回调 | WorldDraw、isApplicable、ownedIsApplicable、setAttributes、viewportDraw 五个计数均 0 | 五个计数均 0 |
| 更新后有效性及产物 | viewValid=false、deviceValid=false；无真实 context、RGB 或文字帧 | 同左；无 RGB，mappingVerified=false |

定位：main trace 的 kernel 在 5/16 行、列表在 17/20/30/34/43/45/48/50 行，通知在 23/32/33/55 行，更新计数在 46/51 行，观察器移除汇总在 76 行，resourcesReleased 在 87 行。nonpersistent trace 对应 kernel 5/16、列表 17/20/30/35/44/46/49/51、通知 23/33/34/56、更新 47/52、移除汇总 73、resourcesReleased 86。两 Session 顺序释放/重建后有数字地址复用，不能据此称共享同一个 model/view 实例。

两路各一次 singleton 配置输出相同（各 trace 8—11 行）：currentDisplayDriver path=`D:\CAD2020\AutoCAD 2020\Drv\`，driver=`gdi15.hdi`；driverName=`GSHEIDI`；getVirtualDeviceName=`gdi15.hdi`。三个缓冲均分配 32768 WCHAR/65536 字节、传参 32768，有 NUL，位置分别 28/9/7，均非空且未达到保守截断边界；公开输出合同下 hardwareAccelerationCapability=false。**这些仅证明 singleton 当前显示驱动配置及其能力输出；不证明该 offscreen device 的活动 driver、不证明加速启用状态，不能作为零回调原因。**

`case/gs-result.json` 保持 status=failed、qualification=false，错误为 `GS produced no real owned WorldDraw callback`，frames=[]；条件诊断 result 保持 diagnostic-failed、qualification=false、rawRGBPath=null、sideDbSuccessClaimed=false。没有调用 RGB 快照的真实先决 callback，session 内不存在 .rgb 产物，不能由空输出推断已渲染纯白。

## 清理、保护和收尾

三项原 controls 均达到原注入点，故障消息精确匹配，cleanupComplete/ambientUnchanged/passed 全为 true。fault 1/3 的根通知各 0/0，fault 2 为 1/1；三者均配对 removeModelReactor、logFailures=0、observationFailed=false、resourcesReleased=true。main 与 nonpersistent 均在模型销毁前移除观察器，primitive 在 model 删除后销毁；清理完资源释放，原数据库关联恢复成功。

main/diagnostic 及三个 controls 的 current/active/working 指针保持，ambient DBMOD=1→1，openedForModify/modified/appended/erased/headerWrites 全 0。闭合记录 sourceSHAsUnchanged=true，退役记录保留源图、快照及本次 inspection copy 共六项 SHA；四个精确 nonce 暂存模块/INI 已归档退役。

宿主收尾为 **watch exit 70 强制终止**，normalQuit=false、nativeQuitConfirmed=false、forcedTermination=true，不能写为正常 QUIT。175 个监测样本内自有宿主用户桌面 SHOW/前台事件及前台样本均 0，supervisorErrors 为空。但 `relatedCadEventsNeedReview=true`：其它 CAD PID 47676 在 Default 桌面记录了一次 foreground（relatedCadShowEvents=0）。原事件及归因待核标记保留，本任务未操作该进程；不能把所有 CAD 活动写成全 0。详见 `isolation-final.json`、`reader-retirement.json` 和 `isolation-related-cad-events.jsonl`。

关键原始 SHA256：

- native-result-closure.json：`C9329C25B9908855212123B1623EDC974F0DCEE2C3B6C5DEA3FC23525C8E07FD`。
- main gs-trace.jsonl：`C291E3A4ABC8A5E2305A33C400ADB328F64B08975EB50EF4256AAB18899F3188`。
- nonpersistent gs-trace.jsonl：`F9EE1B25CBBDF5E27E18B7A9B69ACF1CD5117D94F9486746373C2E5A9EAC3976`。

## 可据结论与限制

N9e 已实证 model/view/kernel 身份一致、实际模型集合绑定存在及正确根 added/erased 通知，进一步消除了“未观测模型自身 kernel/集合/根通知”的空白；没有证据支持补显式 addSceneGraphRoot。新观察器没有触发或代替图形遍历。上述边界一致时，原 main 与绕过 AcDb 持久 root 的 nonpersistent 链仍零图形回调，原因继续未知，不能凭配置名或根通知再猜参数。

**只完成 N9e 观察切片，N9c 采集仍未完成。** 不授予 BUILD、glyph-complete、布局接入或完整采集资格；原 failed 保留。既有 v17 事实仅作基线，本节结果均来自本次唯一 v19 actual。没有重跑旧 N9a/N9b/N8a，也未改变原工厂、绘图、更新、调度或生产配置。
