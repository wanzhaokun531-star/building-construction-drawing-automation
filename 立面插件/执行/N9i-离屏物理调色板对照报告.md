# N9i：离屏物理调色板对照报告

2026-11-03。实现、编译及作者自审已冻结；root独立controller已完成唯一fresh actual，原件核对事实见末节。作者未启动或控制 CAD，未派代理或审查、提交/切支、改共享设置、安装模块或操作用户图纸。唯一候选为下列 v28；早期 135445 构建只编译，未准备或运行，不用于实际诊断。

## 冻结候选

- 基线：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-133951-997-n9h-owned-resource-v27-frozen-9c7b8692/src`（已通过完整限定复核的 N9h/v27）。
- 候选：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/src`。
- 构建回执：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/receipt.json`，SHA256 `F9ED92FDC23E42BAFF42C83DF064E8314A25CE69D088533652681DA9357E84D4`。
- ARX：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/out/NativeGsTextFootprint.arx`，SHA256 `71EF519F266414AE49750074FEE704B3A2BE6F95F2CF20E09F5EAFF515B698AF`。
- driver SHA256：`D622C17A1FE210FCFAE394205D3BBB088BE643E83F5E95DAD838453B1AFE7FE2`。
- 自审回执：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/n9i-self-review-freeze.json`，SHA256 `DF8D1230C17803E7A59420CB89FC5307ADF16A2970952838C669861F2508CD34`。
- 小差异：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-135519-503-n9i-physical-palette-v28-frozen-b3b0a246/n9i-v27-v28-small.diff`，SHA256 `75EE888E59FD48090562EB28B2A554C92DD975D9D619023FD11184ACA4A5CE9E`。

沿用现有 `tools/build_text_footprint_gs.ps1 -Label n9i-physical-palette-v28-frozen`，v141 14.16.27023 / x64 / C++17：exitCode=0，0错误，1个既有 rxapi.lib 缺 rxapi.pdb 的 LNK4099 警告；nativeExecuted=false。回执9项 origin、10项 sourceFiles、实际live输入/冻结副本及ARX SHA全部逐项核同。构建输入相对v27只有 `NativeGsTextFootprint.cpp` 和必要README变化。NativeEvidence、工程、TextOccupancy核心/头文件、ElevationModel、原validator和build脚本保持；未修改host/watch/bridge或旧冻结物。

## 精确合同及唯一因素

已先读本项简报和N9d报告的physical/logical合同续篇，并核本机2020 `inc/gs.h:738–741`：两setter均接收 `const AcGsColor*` 和 `int`，返回 `void`。N9d所核同版CHM精确条目 `AcGsDevice__setPhysicalPalette@AcGsColor__@int.html`（SHA256 `926F9173A19D00A101B4D5B370E676AC6C0A3AB87769B4E39BF142A1A3F58FCF`）指出physical palette应配合离屏设备；logical条目明确logical与physical可能不同。这只是单因素诊断依据，未证明其缺省状态、必需性或零回调根因。未添加onDisplayChange、renderer setter、内部suppression或共享GS配置。

| 项目 | A | B |
| --- | --- | --- |
| 输出目录 | `nonpersistent-control` | `nonpersistent-physical-palette-control` |
| 模型/销毁 | manager.createAutoCADModel / manager.destroyAutoCADModel | 同A |
| 样式 | direct AcGiVisualStyle value；原16字段及getter观察 | 同A |
| 自有DB准备/目录/缓存 | 复用N9h完整写入/source校验后公开资源read-open cache | 同A |
| logical palette | 原256项control数组 | 同A |
| physical palette | 不请求/不调用 | logical之后、创建任何view之前额外调用一次；传给logical的同一数组地址 |

两臂保留相同非持久PrimitiveRoot、设备/view工厂、相机、1024×768尺寸、默认renderer、原device+view update、真实traits观察、条件ATIL、固定标定和保护。B不再沿用N9g/N9h的generic模型因素；模型工厂和配对字段全部如实标为manager。旧generic provider实现保留，原fault1仍generic/value+cache；fault3、fault2和main仍manager/ID，physical默认false，资格条件保持。

原函数栈上的palette共同移入稳定 `Session::controlPalette` 成员。仍用同一原初始化代码：256项先全部 `0,0,0,0`，仅第0项设 `255,255,255,0`；最后一字段为padding。它是probe控制palette，不是通用ACI。两臂的logical输入、B额外physical输入均使用本Session同一个array.data()/size()，输入从setter前持续到device实际删除后，Session本身在清理/拒绝卸载期间仍保活。共同生命周期延长已明确记录；不得把由此产生的相对旧版表现变化单归physical因素。没有改颜色、校准色、几何或0.55像素阈值。

新增 `ownedDevice.setLogicalPalette.before/after`、B的 `ownedDevice.setPhysicalPalette.before/after`，以及每臂palette观察：scope=this owned offscreen device、请求/调用开始/正常返回、count=256、稳定member/logical/physical输入地址、同源数值及引用策略。公开接口无physical readback，`physicalReturned`只说明void调用正常返回，`physicalReadbackAvailable=false`、`physicalAdoptionVerified=false`；不宣称采纳成功。

本项比较文件为 `physical-palette-comparison.json`，contract=`bce.n9i-physical-palette.v1`。两臂fresh串行各一次，A若保护失败仍显式跳B；每臂及未完成/跳过记录保留真实manager工厂、physical请求/返回和原因。保留原main失败条件，不因诊断有图元改判main或放行5×2文字矩阵。两臂一直diagnosticOnly=true / qualification=false，无glyph、BUILD、N9b或生产采集资格。

## 自审和实际证据边界

对v27冻结源码逐段核同：原几何/颜色、cleanup、catalog/provider、style/真实callback/render、三个fault controls、command state/idle/modal dispatch及卸载尾部完全相同。小diff只加入palette稳定成员及观察、额外physical调用条件、将本轮B改manager及比较语义；未改SDKgetter或真实traits差异的处理门。所有真实callback、context/身份、资源/目录、buffer/映射检查保持；已获得raw时仍先保留原RGB再做既有标定。标定颜色不符仍失败，不修改颜色补资格。

作者构建阶段仅编译和自审；下节actual来自root的独立controller。作者本次补证只读原件，未改冻结源码、编译、启动或控制CAD、派审。无原始buffer时不补图，不把编译或void正常返回写成离屏采用/绘制成功。


## 唯一fresh actual与收尾

会话：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-135723-9dedc08f`，PID47348，nonce=`9dedc08f6bdb473a98ca77f892747e64`。binding为 `立面插件/evidence/native-case-binding-9dedc08f.json`；实际module SHA与v28冻结ARX一致。dispatch记录真实captureCalls=1、captureFinished=true、gsCoreCompleted=true；重复entryCalls=2没有形成第二次capture。

主链仍为 `GS produced no real owned WorldDraw callback`，native-case/gs-result均failed，frames空，qualification=false。两臂均真实manager.createAutoCADModel / manager.destroyAutoCADModel、direct-value，依次执行且各有独立Session；比较protectionVerified=true仅表示清理/ambient/身份保护满足，不能替代绘制资格。

| 实际项目 | A logical-only | B logical + physical |
| --- | --- | --- |
| logical正常返回 | true | true |
| physical请求/开始/正常返回 | false / false / false | true / true / true |
| member/logical输入地址 | 2649101122410 | 2649101127306 |
| physical输入地址/count | null / 256 | 2649101127306 / 256 |
| attributes / WorldDraw / viewportDraw | 0 / 0 / 0 | 同A |
| 实际context / RGB / 标定 | 无 / 无 / false | 同A |
| 根通知added / erased | 2 / 2，身份核对通过 | 同A |
| 自有catalog unique / pin取得 / 关闭 | 92 / 92 / 92 | 同A |
| cleanup / ambient | true / DBMOD1→1、零写事件 | 同A |

B原始trace一基第114–115行是logical before/after，第116–117行是physical before/after，第118行才是kernel.createView.before。第117行实记logicalInput=physicalInput=sessionMemberInput=2649101127306、count=256、logicalReturned/physicalReturned=true；A第114–115行只有logical，第116行直接createView.before。数据值策略及共同延长的Session成员生命周期与冻结实现一致。physical仍无readback，physicalAdoptionVerified=false，不能把调用返回写成采用成功，亦不能据两臂零回调宣称physical永远无关或所有公开离屏路径不可行。本轮额外physical配置未使此链获得真实绘制；零回调原因仍未知，不再用旧样式/时序组合重试。

A/B的ready catalog快照及trace第99行均为readyForModel=true、enumerationComplete=true，每项真实openStatus=0、身份可读验证及ownReadOpenHeld=true；最终92项closeStatus=0、pinsClosed=true、closeErrors=0。A第173行manager模型销毁后、第174行才开始close，175–266行逐项关闭、267行汇总、275行ownDb销毁、278行resourcesReleased=true；B对应175、176、177–268、269、277、280行。最终catalog.readyForModel=false是清理后的状态，不能误写为model前未准备。

三个faultPoint按1/3/2全部真实达到、passed=true、cleanupComplete=true，均ambient DBMOD1→1且零写。fault1仍为generic/value/cache，92项pin逐项closeStatus=0，model删除后撤provider再close，trace第125/126/127/220/226/229行分别为deleteModel.after、撤provider、close开始、汇总、ownDb销毁、完全释放。其get/release仍0，只实证cache生命周期清理；A/B没有安装自定义provider，manager内部回调未知，不能将两臂0计数记成provider返回/归还分支已测。

本次没有任何.rgb文件、真实context或glyph结果；0WD门保留并未进入5×2文字矩阵。两臂一直diagnosticOnly=true / qualification=false，原main失败未撤销，没有N9b、BUILD、glyph或生产采集升级资格。

真实收尾为workerExited=true、workerExitCode=70、explicitStopRequested=true，同时forcedTermination=false、normalQuit=false、nativeQuitConfirmed=false、timedOut=false；如实保留这些字段，不把70自行翻译为强杀或正常QUIT。4个nonce绑定暂存已精确归档退役，原件及快照/工作副本共6项哈希重新逐项核同；own SHOW/foreground/sample均0、inputDesktopReadErrors=0、monitorError=null、relatedCadEventsNeedReview=false。

最小索引：`执行/N9i-离屏物理调色板对照证据索引.json`，包含原件SHA与上述一基trace定位。完整限定review的小diff仍为冻结目录 `n9i-v27-v28-small.diff`，没有重编译或修改冻结源码。

| 关键原件（相对session） | SHA256 |
| --- | --- |
| case/physical-palette-comparison.json | 58952E13A957ECBEFC847F8ECA74E4C40EBB1FF29854DB468852C9F95D319EB8 |
| case/nonpersistent-physical-palette-control/gs-trace.jsonl | 98CB1B98A3726C35328671640771A766131820BB27E897E116F1384FBAD3FC95 |
| native-result-closure.json | 06B3825A4922D7DC57D7308670AB9F120944739130182FAD6541C821396BDAB6 |
