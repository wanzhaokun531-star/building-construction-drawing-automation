# N9f：非空非持久根的 ID/value 样式传递对照

2026-11-03。当前状态：**v21 唯一 fresh ID/value 对照已实测并闭合；两臂均执行原非空更新，仍零真实绘制回调，原 nativeStatus=failed 保持。** 直接 value + 非空根的旧覆盖空白现已补齐，但该改动仍不足以解决此链。作者只在 controller 闭合后只读核对；未启动或控制 CAD、派子代理/审查、提交或切换分支。

## 实现范围

基线冻结 v19 driver SHA256 `A331F4B036B87A657D6780B5816419D9AA85E71F23F7BD43F7CAAEC9B579E253` 已有完整 review/Spec/Quality PASS；本次不重审或重跑该项。按 N9f 简报及 N9d 报告末尾续篇，只改指定 `NativeGsTextFootprint.cpp` 与 README 必要增量；公共 NativeEvidence、TextOccupancy、host/watch/bridge、生产源和共享配置不改。

保留主明确 zero-WorldDraw、无文字帧/回调、主清理与 ambient 保护全部满足后才启动诊断的原条件。原条件非持久 Session 为 A（`nonpersistent-control`，仅 owned ObjectId setter）；A 安全完整清理后才新建 B（`nonpersistent-value-control`，仅 direct AcGiVisualStyle value setter）。两个 Session 对象分别留在 diagnosticSessions 至卸载，记录独立输出身份、Session 地址和销毁前 DB/marker/modelspace/style/root/kernel/model/device/view 实际地址。资源数字地址复用不称共享实例。B 记录 setter 前后 ID getter，不要求返回旧 ID，也不推断 value setter 清 ID。

两臂复用同一 configureStyle、持久 style 的关闭/重开/copyTo 16 原值及 op 严格核对、PrimitiveRoot 的原五处非文字校准、原 model/device/view 工厂、默认 renderer、相机/1024×768、原 device.update 及非交互 view.update。唯一渲染配置变量为最终样式 setter overload。两个初始及每帧位置的样式观察均保留 getter 返回、16字段/op/equivalence/observationAvailable；这两臂的 requested-traits 差异不阻断非空更新。source/资源/身份仍严格核对，真实 callbackError、真实 GS 回调先决条件及原 ATIL/标定检查保持。主流程及三原异常控制样式资格门不变。

每臂各有 gs-trace.jsonl/result.json；主目录 style-transport-comparison.json 汇总实际两臂执行/错误、运输方式、样式观察、根通知/身份、回调、已写 RGB 路径/哈希、标定与保护。A 只是 no-WD 或写入 RGB 后标定失败且保护完整可继续 B；清理/ambient/身份/callback 或非空前资源失败则 B 跳过，保留实际错误及检查项。实际 RGB 成功落盘才公布路径，先保存后标定；不改颜色、margin 或阈值。极端 setup/cleanup/evidence 异常在比较 JSON 区分 attempted 与实际进入 execute，保留错误及未执行臂。

两臂始终 diagnosticOnly=true/qualification=false、无文字/point overrule/手工 worldDraw；真实 context/database 只作观察，不称 SideDB 成功。root 继续保留到 view/model 销毁之后，两臂均参加 terminal 资源/unload 保护。任何诊断表现不撤销原 main/native-case failed，不运行五文字×两分辨率主矩阵。

## 编译、自审和冻结

最终 receipt：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0\receipt.json`，SHA256 `2DE9716ACDCB39682D626EF32115C1EA9D273B797107BD79E0DF522A03F2184F`。

- 实际执行现有 build_text_footprint_gs.ps1，label n9f-style-transport-v21-frozen，case NativeGsTextFootprint；v141 14.16.27023 / C++17 / x64 / SDK23.1 / /W4 /WX，exitCode=0。0 编译警告、0 错误；唯一链接警告为 SDK rxapi.pdb 缺失 LNK4099。
- ARX SHA256 `DBECB9814CDAECCEE8B89D472F6F037C65D4165925BFAEFE8094CEC52EB6FF5D`；driver SHA256 `57985B34BE1A27EF5D4DB76836F1E0103FC72080B3416DA4E5E9BC32DCBF943F`。
- v19→N9f 小 diff：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0\n9f-v19-style-transport.diff`，148 行；自审冻结指纹：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0\n9f-self-review-freeze.json`。
- 所有构建输入 live=frozen；除 driver/README 外各原构建输入及 build 工具逐字节同 v19。静态分段核对确认：PrimitiveRoot flags/真实绘制、configureStyle/持久源、ATIL/原标定、原三异常控制、原 cleanup、idle/modal调度和卸载代码完全相同。主代码仅通过限定 bool 分派 setter 及诊断绕开 traits 门，资格表达式不变。
- v20 已编译但未派 controller、未原生执行；自审在 v21 补齐诊断根身份异常非空前拒绝及证据异常路径 attempted/execute 区分。v20 不作为运行候选。不为低影响 metadata 新增镜像测试；必要运行验证为唯一真实 A/B 加原异常/保护覆盖，本次 controller 已完成，见下文。

## 唯一 fresh 实测

session：`D:\Codex\建筑施工图自动化\阶段1\runs\elevation-native-20261103-125656-6a479330`；nonce `6a479330de7b4ca59dc18baf26399c56`；PID `12956`；实际 ARX 与冻结 v21 SHA256 一致。证据定位及逐文件 SHA 见 `执行/N9f-实际证据索引.json`。本节只读核对原始文件，不修改运行证据。

`case/dispatch-trace.jsonl:4,7` 实际 idle-ready 为 CMDACTIVE=0、空 CMDNAMES、inputPending=0、quiescent=true；唯一 capture 为 CMDACTIVE=1、无 SCRIPT 位、inputPending=0、applicationContext=false。主 `gs-trace.jsonl:44–51` 实际原两次更新仍零回调，随后主清理资源释放（87行）通过，才开始 A。主 error 原样为 `GS produced no real owned WorldDraw callback`，frames=[]、qualification=false。

| 实际事实 | A owned ObjectId | B direct value |
| --- | --- | --- |
| executed / freshExclusiveSession / freshViewCreated | 均 true | 均 true，A 安全清理后执行 |
| Session 身份 | 1730325803824 / nonpersistent-control | 1767516344304 / nonpersistent-value-control |
| 初始及每帧 ID getter | 两次与自库源1730028087312一致，db正确 | setter前后均0，actualDatabase=0；两次 ownedIdSetterUsed=false |
| value getter / observationAvailable | 两次 false，actual值/op/equivalence均null | 两次 true，16字段/op全部保留，4项active差异 |
| requested-traits 门 | 两次 gateApplied=false | 两次 gateApplied=false；frame activeContractSatisfied=false后仍更新 |
| 原非空更新 | device.update和非交互view.update各一次 | 同左 |
| 五计数 / valid | WorldDraw/isApplicable/ownedIsApplicable/attributes/viewportDraw均0；view/device valid=false | 同左 |
| 根通知 | added2/erased2，身份不匹配false | 同左 |
| callbackError / 原RGB / 标定 | 空 / 无RGB / 未进入标定 | 同左 |
| cleanup/resourcesReleased/ambient | 均true，DBMOD 1→1，五类写通知均0 | 同左 |

A 的 ID/value 读回在 trace 28–29、40–41行，更新46–53行，cleanup.return 87行；B 的 setter前后/getter/16traits在27–29、38–40行，41行保留差异，44–51行证明随后真实进入两次原非空更新，85行资源完全释放。B没有`view.setOwnedStyleId.before/after`事件。每臂原持久style源重开/copyTo的16字段/op均严格一致，源身份通过；这不等于view采用的样式相同。

B 两个观察阶段的四项差异完全一致（其余12字段/op等价）：

| 字段 | requested值/op | actual值/op |
| --- | --- | --- |
| edgeStyles | 5 / 1 | 4 / 3 |
| edgeModifiers | 0 / 1 | 8 / 1 |
| viewportTransparency | false / 1 | true / 1 |
| lightingEnabled | false / 1 | true / 1 |

A和B持有不同Session对象、DB/primitive身份与独立创建/销毁证据。model/device/view数字地址在清理后被复用，kernel地址也相同；不据数字相同称共享同一实例。N9e原model/view/kernel、模型集合及root通知观察仍保留，未改工厂或参数。

`style-transport-comparison.json` protectionVerified=true、两臂均诊断失败但保护通过；SHA256 `AC8D996F5CBFB919A386A695096C36EC1120C370D7CA4493FEA393558A31B642`。原三异常控制全部真实命中并passed、完整清理及ambient保护通过。`gs-result.json`和`native-case.json`仍failed；无文字帧、真实context/RGB或glyph/N9b资格。

## 收尾与结论边界

closure SHA256 `95FFFBE4193FCB7B0BE18DB712FE1F10DD3F8ADE3909F35ED0B39865B5353000`。实际 workerExitCode=70、explicitStopRequested=true、forcedTermination=false、normalQuit=false、nativeQuitConfirmed=false；不能称正常QUIT或把本次forcedTermination套成旧值true。quiet-exit-error记录最后自有文档已修改，QUIT被保护拒绝。自有进程已退出、4个暂存文件精确退役、6项输入检查保持；ownedUserShows/ownedForeground均0、inputDesktopReadErrors=0、relatedCadEventsNeedReview=false。作者未另开宿主或重编。

本次证明直接value + 非空非持久根实际已执行且没有解决此原离屏链的零绘制问题；也证明本次不再被旧traits门提前截断。A的值型观察不可得，B有4项实际active差异，不能声称两臂已采用完全相同样式。根因仍未知，不扩成所有SideDB/GS的结论。原失败保留，无主文字矩阵、N9a/N9b/N8a重跑或能力升级。本报告与最小索引供本轮一次完整N9f review，未声称该review已经通过。
