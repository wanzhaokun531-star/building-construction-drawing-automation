No findings.

Spec verdict：PASS。Quality verdict：PASS。两项均仅适用于 N9f“fresh 非空非持久根 ID/value 样式传递对照”的限定任务；N9c 真实采集仍 failed，qualification=false。本结论不授予文字足迹、完整字形、N9b、SideDB、BUILD 或生产资格。

## 复核范围与绑定

2026-11-03。按 review-agent 与 AutoCAD2020 ObjectARX 技能实施一次只读、缺陷优先复核。先读完整任务简报、N9f 需求简报、实际报告、实际证据索引及 N9d 中对应样式传递续篇；不重审已经完成的 N9e/v19，也未把 N9d 后续通用 model 建议纳入本次实现要求。

仅比较：

- base：D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8/src。
- candidate：D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0/src。
- 实测：D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-125656-6a479330；nonce=6a479330de7b4ca59dc18baf26399c56，PID=12956。

下文源码行号均为 candidate 的 NativeGsTextFootprint.cpp；trace 行号均为上述实测目录。完整148行冻结 diff 已全部审阅，并以两个冻结文件直接重新生成内存 diff，逐行相同。除 driver/README 外，冻结 src 的其余7个构建输入及冻结 build 工具均与 v19 逐字节相同。receipt 所列10个输入的当前 SHA 均匹配，冻结 ARX 实际 SHA 匹配。

关键指纹：

| 对象 | SHA256 |
| --- | --- |
| v21 receipt | 2DE9716ACDCB39682D626EF32115C1EA9D273B797107BD79E0DF522A03F2184F |
| v21 driver | 57985B34BE1A27EF5D4DB76836F1E0103FC72080B3416DA4E5E9BC32DCBF943F |
| v21 ARX | DBECB9814CDAECCEE8B89D472F6F037C65D4165925BFAEFE8094CEC52EB6FF5D |
| 148行 diff | F7667C23EEB25A0BE5D1A5BAF86928C26B8A4E98AEC48BDB51CAACBAF250FC92 |
| comparison JSON | AC8D996F5CBFB919A386A695096C36EC1120C370D7CA4493FEA393558A31B642 |
| actual closure | 95FFFBE4193FCB7B0BE18DB712FE1F10DD3F8ADE3909F35ED0B39865B5353000 |

实际证据索引列出的18个文件全部重新计算 SHA 并匹配。binding 的 candidateSHA256/buildReceiptSHA256 与上述绑定一致；没有把通用 reader 的 binarySHA256 误当候选 ARX。v20 未作为原生运行候选。

## Spec 核对

| 要求 | 实现及实际证据 | 判定 |
| --- | --- | --- |
| fresh 同构两臂、唯一 setter 变量 | :444–447 每臂创建并保留独立 Session；:360–408 共用 DB/marker/modelspace/style 源、manager model、kernel offscreen device/view、默认 renderer、相机和1024×768；:251–263 只为受限 B 分派 direct value overload。实际 A Session=1730325803824，B=1767516344304；DB/root 亦不同，独立创建及销毁。地址复用不作为实例共享证据。 | PASS |
| A 仅 ID、B 仅 value，真实读回保留 | A trace :28,40 为本库源 ID 匹配，:29,41 value getter=false；B :27–29,38–40 为 prior/actual ID=0、actualDatabase=0、ownedIdSetterUsed=false。逐 trace 计数：A 有2次 ID setter/0次 value；B 有0次 ID/2次 value。两臂初始及每帧均保留16项原值/op/等价性和 observationAvailable。 | PASS |
| traits 差异不前拦本诊断，源核验保持 | :205–217 保留 getter 不可观测的 null 语义；:243–263 保持持久源严格核验；:342,407 仅在本无资格诊断中不要求 requested-traits 相等。B :41 activeContractSatisfied=false 后仍到 :44–51 原非空两次更新；并非 metadata 提前退出。 | PASS |
| A 清理及保护通过后 B 执行 | :451–466 清理完成、资源释放、ambient/观察/身份及 callback 检查共同决定 continuation。实际 A cleanup.return :87 resourcesReleased=true，A result nextArmAllowed=true；串行返回后才创建 B，主 trace :88,89 分别记两臂完成。 | PASS |
| 原 main failure 与原控制门不撤销 | 主启动条件 :477 保持明确 zero-WD、零 callback、frames 空、清理及 ambient 完整；:478–487 仍要求原 failure 为空才可能合格。主 gs-result/native-case 实际 failed，原错误保持，frames=[]、capabilities=[]。三异常控制实际命中原故障点且 passed，fault-2 原样式门满足后才命中。 | PASS |
| 独立结果、RGB/映射事实及范围 | :455–466 生成每臂 trace/result 和 comparison；两臂 result 与 comparison arms 逐结构相同，styleObservations 与各自 trace 完全相同。diagnosticOnly=true、qualification=false、textRequested=false、sideDbSuccessClaimed=false；实际零回调、无 snapshot、rawRGBPath=null、mappingVerified=false。 | PASS |

实际样式不能声称完全相同。A 的 view value 观察不可得；B 初始及每帧两次观察均有以下4项 active 差异，其余12字段/op等价，持久源自身关闭/重开/copyTo的16字段/op全部严格等价：

| 字段 | requested 值/op | B actual 值/op |
| --- | --- | --- |
| edgeStyles | 5 / 1 | 4 / 3 |
| edgeModifiers | 0 / 1 | 8 / 1 |
| viewportTransparency | false / 1 | true / 1 |
| lightingEnabled | false / 1 | true / 1 |

本任务完成的是此前未执行的“直接 value + 非空根更新”组合。两臂都零真实绘制，说明这次 transport 替换没有解决该离屏链；没有证据确定根因，更不能推广为所有 GS 或所有 SideDB 行为。

## Quality 核对

完整 diff 之后，为回答具名疑点，只定点补读了 :122–140 清理、:176–182 真实 PrimitiveRoot 回调、:243–263 setter、:308–349 snapshot/标定及真实回调门、:360–408 同构资源建立、:429–487 异常控制/continuation/原资格，以及 :557 卸载保护；未再次扫描源码全树。内存定段比较还确认 cleanup、PrimitiveRoot 回调绘制、configureStyle/持久源、snapshot/原标定、三异常控制及 dispatch/unload 区段与 v19 完全相同。

- 真实回调门保持在 :347–349：先检查 callbackError、非交互 view.update、观察错误及 frameCallbacks>0，才调用 snapshot；没有手工 worldDraw 或新增 point overrule。实际 A :46–53、B :44–51 证实 device/view update 均返回，WorldDraw/isApplicable/ownedIsApplicable/attributes/viewportDraw 五计数全零，callbackError 为空、view/device valid=false。没有将 context/database 观察或 model/root 通知当作像素成功。
- 原 ATIL channel order、32bit、read context及固定0.55像素标定检查未变。:408 在 raw 保存成功后才公开 positiveRawPath，再标定；标定失败仍可保留已写原 RGB。实际两臂无回调，未进入此分支，也没有假造 RGB 或标定通过。
- RootObserver 的新增 mismatch 标记发生在日志明细上限判断前（:149），:344 在非空更新前检查该标记和 view/device/root/model 身份。实际两臂 added=2、erased=2，全部通知身份匹配，rootObservationFailed=false。
- 原清理中先 eraseAll/移除 view，再销毁 view/model，PrimitiveRoot 至 :134 才销毁；保留的 Session/root observer 稳定存活。:478 所有 diagnosticSessions 参与 terminal 资源检查；:557 保留所有 Session 的 cleanup/unload 拒绝保护。实际 A :87、B :85 resourcesReleased=true，ambient DBMOD 均1→1、五类写通知全0。
- :452 仅在清理、ambient、身份、观察、callback 均安全且正常完成/明确 zero-WD/已有原 RGB 后允许继续。未识别的非空前错误不满足这些条件，B 在 :463 记录具体 blockReason；setup/cleanup/evidence 抛出走 :464–465，区分 attempted 与 execute 已进入，缺臂明确记 incomplete-or-skipped，protection=false。此处保持原主失败且 terminal/unload 资源保护仍适用。
- 复用冻结 build log：exitCode=0，v141 14.16.27023 / C++17 / x64 / SDK23.1，/W4 /WX；无本源编译警告或错误。唯一 LNK4099 为既有 rxapi.lib 缺 SDK rxapi.pdb 的信息，日志末尾重复汇总不构成第二个新警告，不要求修改供应商文件。

本次动态覆盖为唯一 fresh 实测的两臂零回调正常清理路径及原三故障控制。A/B 的 callback 异常、身份异常、清理/证据异常导致跳过，以及有回调后的 raw/标定成功或失败路径，均仅完成静态核对，未声称有动态覆盖。不为这些未触发分支重开 CAD、构建或新增镜像 metadata 测试；这是本报告保留的测试边界，现有证据未形成可行动缺陷。

## 原生收尾事实与结论边界

closure/isolation-final/reader-retirement 及 quiet-exit-error 相互一致：workerExitCode=70、explicitStopRequested=true、forcedTermination=false、normalQuit=false、nativeQuitConfirmed=false。末自有文档已修改，QUIT 被保护拒绝；不能写成正常 QUIT，也不能套用旧模板的 forcedTermination=true。

自有 worker 已退出；4项 nonce 暂存文件已精确退役、6项输入检查保持；ownedUserShows=0、ownedForeground=0、inputDesktopReadErrors=0、relatedCadEventsNeedReview=false，userCadTouched=false。没有据这些保护记录推导业务采集成功。本复核未控制 PID12956、其他47676或任何 CAD 进程。

N9f 限定 Spec 与 Quality 均通过，未发现作者会需要修复的可证明新增缺陷。原 N9c failed 与原因未知继续保留。唯一写入为本报告；未改源码、冻结文件或实际证据，未派代理/第二审，未运行 CAD、重构建、全套测试或 git 共享 dirty 树扫描。
