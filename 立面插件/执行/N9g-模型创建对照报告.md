# N9g：纯非持久根模型创建配置对照报告

2026-11-03。冻结 **v24** 已由root唯一controller完成一次真实运行 `216c3fca` 并收尾：A manager/value 零绘制；B generic model 创建成功，但named get真实收到一次本probe不支持的持久请求并返回空，随后仍零绘制。B属于 **persistent-adapter-unavailable**，不能作为普通zero-WD或完整model适配结论。原main/source case仍failed，所有诊断qualification=false，无RGB/glyph资格。未重审或重跑N9f。

## 冻结候选

- 编译入口：原 `tools/build_text_footprint_gs.ps1 -Label n9g-model-factory-v24-frozen`；2020 SDK / v141 14.16.27023 / x64 / C++17，exitCode=0。0编译错误/警告；唯一既有链接警告LNK4099，SDK缺rxapi.pdb。
- 冻结根：`D:\Codex\建筑施工图自动化\立面插件\build\n9c-20261103-131202-815-n9g-model-factory-v24-frozen-34b66b1a`。
- receipt：`receipt.json`，SHA256 `C24B85974744C985602F19C4BD97B77557D5C484A73FA1338D757182FC0A16CC`。
- ARX：`out/NativeGsTextFootprint.arx`，SHA256 `F84E1C52799F675DDD32F83C88E0B266E2C4C6839AEA02F6B5105E58D36C561D`。
- driver：`src/NativeGsTextFootprint.cpp`，SHA256 `03E1BAFBFC77F132252B3A15B21ECCF133D40CB6AE0A1C965DB563E2FC74C623`。
- 基线为N9f v21 `57985B34BE1A27EF5D4DB76836F1E0103FC72080B3416DA4E5E9BC32DCBF943F`；本项小diff：`n9g-v21-model-factory.diff`，210行，SHA256 `971114013B36FA41CAC7E935883256551D10D296736E5CD8C7746C19E1D07733`。
- 自审冻结记录：`n9g-self-review-freeze.json`；SDK原文/指纹：`n9g-sdk-public-contracts.json`，只新增本任务证据，未修改既有SDK记录。

## 实现与准确边界

仅改既有driver及README必要增量。原main与三异常控制仍走ID/manager路线；真实main zero-WD、无文字帧、资源完全释放及ambient保护通过后才创建两fresh串行无文字Session。A完整保护后才可B。

| 臂 | 共用样式 | 模型创建及销毁 | 证据目录 |
| --- | --- | --- | --- |
| A | N9f B direct AcGiVisualStyle value | manager.createAutoCADModel / destroyAutoCADModel | nonpersistent-control |
| B | 同A | kernel.createModel(kMain,opaqueCookie,diagnosticGetInterface,diagnosticReleaseInterface) / deleteModel | nonpersistent-kernel-model-control |

两臂同PrimitiveRoot、绘图几何、默认device/kernel/view工厂、renderer、configureStyle原值、1024×768相机、原device.update/非interactive view.update和ATIL/固定0.55像素标定。traits差异完整记录且不前拦此两诊断臂。各model创建后仅各读一次默认ID/数据库与value overload；value=false仍按16字段记录未知，不调用model style setter。不补显式addSceneGraphRoot。

B的opaqueCookie仅为自有db指针的数值标签，不宣称context.database绑定。具名非空回调以2020 gs.h:1041–1043的typedef完成编译类型检查；bNeedsValidation仅原名/原值，不推断open/lock协议。稳定provider先于createModel，包含deleteModel返回前的全部生命周期；provider及trace使用互斥保护。model删除后才撤provider，PrimitiveRoot/db继续活至此之后，pending/provider/model资源参与terminal与卸载拒绝。

实际get仅记录raw ID/flag/线程/次数，标persistent-adapter-unavailable并返回空；实际release仅记录指针/线程/次数，不close/delete未由provider打开的对象。没有AcDb持久适配、手动callback、假drawable/context、raw ID解码。公开合同不保证GS处理空返回，真实请求属于该dev probe不支持且可能宿主失败的分支；不会被标普通zero-WD或合格采集。未调用则unavailable=false，不能拿适配不可用解释零回调。callback日志或边界异常会记录失败；真实WD满足原门时允许保留实际RGB，但unsupported/保护失败仍判diagnostic-failed。宿主原生异常不伪造终态，交唯一controller保留原始证据并精确收尾。

`model-factory-comparison.json`保留factory/配对销毁器、cookie边界、provider是否实际安装/调用、默认及view样式、root通知、真实update前后记录、回调、RGB/映射、清理与保护。manager内部callbacks仍未知，不把未安装自定义provider称作manager未调用。所有诊断qualification=false，原main failure不撤销，不运行主5×2文字矩阵。

## 编译与自审验证

冻结输入与编译副本/receipt逐项SHA一致；NativeEvidence、TextOccupancy、工程、build脚本、原validator相对v21指纹不变。直接比对确认calibration几何/颜色、PrimitiveRoot真实回调、原configureStyle、ID/value绑定、ATIL/固定标定、三异常控制、idle/modal调度段一致。2020 CHM createModel/deleteModel及model两visualStyle getter原文已读取，未见internal-only/Future Use限制；未引入其它路线。

作者未启动/控制CAD、派代理或审查、提交/切支、安装、修改共用配置/平台DLL/用户DWG或旧冻结制品。v22/v23仅为未运行的自审中间编译：v23纠正无model撤provider措辞及保护失败状态；v24将实际update记录纳入比较JSON。随后root只对冻结v24执行下述一次实测。作者本次只补报告及最小证据索引，未另试、编译或派审；一次N9g review由root统一安排。


## 唯一实测216c3fca

运行目录：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-131409-216c3fca`；PID54164，nonce `216c3fca96874bbeb653107f7363f3f9`。native module SHA与冻结v24完全一致。原三异常控制均通过；main原失败 `GS produced no real owned WorldDraw callback` 保持，native-case第二错误 `nonpersistent diagnostic protection or evidence failed` 同样保留，未改判source case。

| 实际观察 | A：manager/value | B：kernel/value |
| --- | --- | --- |
| model默认ID/value getter | 0 / false，值未知 | 0 / false，值未知 |
| direct value getter | 初始/重加后均true，四项traits差异原样保留 | 同A |
| 根及模型观察 | kernel、root/view/model身份一致，根added/erased各2 | 同A |
| 实际更新 | device.update及view.update前后均在记录中 | 同A |
| attributes/WorldDraw/viewportDraw | 0 / 0 / 0 | 0 / 0 / 0 |
| 实际get/release | 自定义provider未安装，manager内部未知 | 1 / 0，adapter-unavailable |
| 状态/保护 | diagnostic-failed；nextArmAllowed=true | diagnostic-failed；nextArmAllowed=false |
| RGB/映射 | 无 / false | 无 / false |
| cleanup/ambient | 完全清理，DBMOD1→1，零写事件 | 同A |

两臂实际traits差异均为edgeStyles、edgeModifiers、viewportTransparency、lightingEnabled；未前拦无文字更新。更新后view/device仍invalid。比较 `protectionVerified=false` 来源包含B真实unsupported状态，不能等同ambient写入或资源未清理。

B trace第7行证实kernel.createModel返回非空model；第27行named get实调：`objectIdRaw=3014301743760`、`bNeedsValidation=false`、`threadId=20880`、`call=1`，返回null并标persistent-adapter-unavailable。release=0，boundaryFailure=false、observationFailed=false、logFailures=0、orphanCalls=0。真实请求发生在device.addView之后、后来owned样式建立记录之前；无更细调用栈证据，不推定具体触发API。

该raw ID不等于本臂opaque dbCookie `3051711649328`、marker old ID `3014301739008`、modelspace old ID `3014301744896`，也不同于后来创建的style old ID `3014301739024`。这些只是数值身份比较，未解码或打开该ID，资源类型与为何请求仍未知；不猜它是样式、图层或其他对象。此实调证明该最小provider并未覆盖实际请求，不能声称generic纯指针路径已成功受测、manager有缺陷或glyph/model适配可用。公开合同仍无null返回安全保证，本次宿主未在该调用中崩溃也不能补成该保证。

B第79–81行依次为kernel.deleteModel.before/after及modelProvider.withdrawn，最后第92行resourcesReleased=true；providerStillRegistered=false，PrimitiveRoot/db释放发生在model之后。所有臂资源清理和ambient保护保持。原main终态仍failed/qualification=false，frames空、glyphComplete=false、capabilityUpgradeAllowed=false。

宿主按controller明确请求收尾：worker exit70，explicitStopRequested=true、forcedTermination=true、normalQuit=false，不能称正常QUIT。4个精确暂存文件已退役，6项源/快照/工作副本核对保持；自有SHOW/前台事件均0，relatedCadEventsNeedReview=false。完整结果见最小索引 `执行/N9g-模型创建对照证据索引.json`，原始证据不改写。

| 关键原始证据（相对该session） | SHA256 |
| --- | --- |
| case/model-factory-comparison.json | 85AAA61527E354FFCB9B757C60ACF22EE79C30D264BC0058626B5A355EFC1986 |
| case/nonpersistent-kernel-model-control/gs-trace.jsonl | 72C72AEF9241936FF9A4312465403F1A19FFC6C681EEF8F018F1E3F72CB0B687 |
| native-result-closure.json | 37C2B1D1534D3AC4B114BF28358970A9752F956337351970EA64282453702423 |
