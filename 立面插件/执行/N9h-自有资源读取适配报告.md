# N9h：自有资源读取适配报告

2026-11-03。基于已完整review PASS的N9g冻结v24，完成公开自有ID目录、真实AcDbObject只读缓存及named get/release适配。冻结v27已由root唯一controller完成一次fresh实测1ce10761及收尾：A/B各92项真实read-open pin均在model删除后成功close；generic fault1也实际覆盖cache就绪后的异常清理。B实际get=0/release=0，没有动态请求角色识别或真实返回分支覆盖；原main/A/B仍零绘制、无RGB，原main失败保持。所有诊断仍diagnosticOnly=true、qualification=false。

## 冻结交接

- 入口：既有 `tools/build_text_footprint_gs.ps1 -Label n9h-owned-resource-v27-frozen`；AutoCAD2020 SDK23.1、v141 14.16.27023、x64、C++17。exitCode=0，0编译错误/警告；唯一既有链接警告LNK4099（SDK缺rxapi.pdb）。
- 冻结根：`D:/Codex/建筑施工图自动化/立面插件/build/n9c-20261103-133951-997-n9h-owned-resource-v27-frozen-9c7b8692`。
- `receipt.json` SHA256：`2791F797A94FACE4774C95809633CC87797D99755BCF993CC7274EC9F2D4764E`。
- `out/NativeGsTextFootprint.arx` SHA256：`222B6D94D331B232599A730CAFB6765FBB45B60C43A27D2252E10E463FFB9D9F`。
- `src/NativeGsTextFootprint.cpp` SHA256：`4C502988A4B97E9E89DDD239274253236C69404AAC7A4853C10D4F8E75D6619B`。
- 相对v24小diff：`n9h-v24-owned-resource.diff`，211行，SHA256 `DAFA9F36350FF7F59DA72C6B73E474A2612D07A4C0D9505851BF4C7CF05409E5`。
- 新SDK依据：`n9h-sdk-public-contracts.json`，SHA256 `9E9A6D8176E3A19BDBB1DE407EB77D56B7C4D07A2BDC915394D12CDD98EE5076`；本次使用的本机2020 CHM原文保存在同根 `sdk-public-contracts/`，只摘录公开对象继承/open-close、表/字典ID与迭代接口。原model/callback合同复用v24证据。
- 自审回执：`n9h-self-review-freeze.json`，SHA256 `AF5F3D02930F4484601A121CF2BFF852EB839BBAA9628E9C3B21BD131412A251`。

编译的9项origin、10项sourceFiles、实时输入、冻结副本及ARX SHA逐项复核一致。NativeEvidence、TextOccupancy、工程、build脚本和原validator与v24保持。v25/v26均只编译，未prepare或运行；自审修正close成功后先落拥有状态再构造日志，并使身份未核实的open不产生可借drawable。root冻结前核对指出SDK未保证逐get可观察release；v27仅去除“未归还数直接变协议失败”的前提，改为observedReturnIncomplete事实，保留真实unknown-pointer/underflow失败。未启动/控制CAD、派代理/审查、提交/切支、安装、改共享配置、用户图纸或旧冻结制品。

## 实现事实与限制

两fresh诊断臂仍为A manager/value、B kernel/value，使用相同PrimitiveRoot图元、设备/view工厂、trait值、相机、更新及固定标定。两臂共同先完成marker和owned style全部显式DB写入，闭合style后严格核16字段/source，再建立公开目录及read-open缓存，最后创建model/view。该准备顺序和cache就绪状态也相对v24改变，任何结果变化不能只归因provider。原main、fault3/fault2保持原ID/manager时序；仅原after-model-association fault1改generic/directvalue加完整catalog/cache，注入字符串不变。旧manager fault1冻结结果只是复用，不称本轮重测。

目录只来自本Session新建的自有空DB：九类SymbolTable本体及records，NOD公开字典路径/值（AcDbDictionary::cast识别派生类后递归），以及已知自有marker/modelspace别名。真实公开ID的asOldId是key，不从callback raw构造ID、反推handle或打开未知/foreign对象。按unique ID去重后才open，字典另作visited，重复路径保留别名、不丢额外open引用，也不设数量或样本上限。枚举/名称/打开失败及不可用原因、公开class、ID、database和指针均保留；不称覆盖所有GS资源。

每个可借对象须成功kForRead，核同ownDb、真实objectId及其key一致、未擦除且isReadEnabled。AcDbObject公开继承AcGiDrawable，返回的是真实static_cast指针。provider显式取得的单个read-open引用与原marker/BTR所有者的引用分开：前者独立pin，后者原清理器不代关它。iterator按SDK由调用方delete。

get/release使用同一互斥保护，局部catch防异常越过ABI。get只精确查内存目录，记录raw/原bNeedsValidation/thread、匹配角色/公开class/真ID/返回指针及borrow计数；未知或不可安全借出仍unsupported/null，不调用AcDb API猜角色。release只匹配实际已借指针并递减borrow，不close/delete。未调用get仍是“未动态验证”；未调用release不计已测归还。public合同没有补成unknown/null安全保证，也未编造validation协议。

清理顺序为实际delete model返回 → 撤provider（互斥等待现有callback退出）→ 逐项检查provider独立read-open的真实close状态 → 释放其它GS资源/ownDb。pin跨越全部view/model使用；cookie只作opaque数值标签，不能证明context数据库绑定。成功close立即清拥有标志，故后续日志失败不造成重关；失败项保留对象引用、db及Session，cleanup/terminal/unload不能假报成功。借用计数与底层引用分别报告：model删后仍未观察归还只记borrowedAtModelDeletion/observedReturnIncomplete，不单凭此增加失败前提，也不会编造release或声称已归还；真实未知release或下溢仍使诊断保护失败。model已消失后才允许收底层pin，物理引用已关闭不等于借用归还已被观察。

新增 `owned-resource-catalog.json` 是model前ready目录；trace记录setup完成、pin、actual get/return/release、delete model、provider撤销、每项close与db释放。最终provider观察保留每项开闭状态/计数、错误及未归还借用；比较文件仍为 `model-factory-comparison.json`，合同变为 `bce.n9h-owned-resource.v1`，明确共同准备变化。fault1复用原控制输出记录cache acquired/closed、provider撤销和ambient事实，无新故障框架或矩阵。

自审直接比对确认calibration几何/颜色、PrimitiveRoot真实回调、16字段source及ID/value绑定、point overrule、ATIL/固定0.55像素标定和RGB门、normal诊断执行/结果体、原main core及idle/modal/unload体与v24一致。真实callback之后才快照，已写raw在标定失败时保留；原main failed不能被诊断改判，无文字、glyph、BUILD或完整采集资格。

## 唯一fresh实测

运行目录：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-134059-1ce10761`；PID49032，nonce `1ce10761b60546afbc3c80f07474cb0a`。native module SHA与冻结v27相同。原main仍为 `GS produced no real owned WorldDraw callback`，native-case/gs-result均failed，主frames空。比较保护protectionVerified=true只表示本次清理/ambient/身份保护满足，不是业务或provider返回合同已动态验证。

| 实际项目 | A：manager/value | B：kernel/value |
| --- | --- | --- |
| 自有目录 | 92 unique，enumerationComplete=true | 同A |
| 实际独立kForRead pin/关闭 | 92 / 92，逐项closeStatus=0 | 同A |
| cache错误/未关闭 | 0 / 0，pinsClosed=true | 同A |
| model前目录ready | true | true，createModel前已ready |
| provider安装及实调 | 未安装自定义provider，manager内部未知 | 已安装，get=0/release=0，actualInvocation=not-called |
| 匹配返回/匹配release | 0 / 0（未受测） | 0 / 0（未受测） |
| attributes/WorldDraw/viewportDraw | 0 / 0 / 0 | 同A |
| RGB/context/映射 | 无 / 无真实回调context / false | 同A |
| 根通知及身份 | added/erased各2，identityVerified=true | 同A |
| 状态/cleanup/ambient | diagnostic-failed / 完全清理 / DBMOD1→1、零写事件 | 同A |

三个目录ready快照（A、B、generic fault1）均逐项记录openStatus=0、ownReadOpenHeld=true、ownershipIdReadabilityVerified=true；各为92个真实公开ID，不将该数解释为全部GS资源覆盖。最终JSON的readyForModel=false是清理后撤下缓存的状态；model前ready=true来自独立快照及trace，不以最终false误称准备失败。

trace以下均按**一基行号**定位：A第99行ready，170–171行destroyAutoCADModel，172行开始close，173–264行92项close、265行汇总，272–273行db销毁，276行resourcesReleased=true。B第99行ready，102–103行kernel.createModel，172–173行deleteModel，174行provider撤销，175行close开始、176–267行92项close、268行汇总，275–276行db销毁，279行resourcesReleased=true。两臂完整setup写入完成→pin→model→delete model→close cache→db释放顺序有原始记录。

替换后的fault1在同预期 `injected-after-model-association` 确实注入、passed=true、cacheLifecycleProtected=true。其trace第98行ready，第101–102行createModel，第124–125行deleteModel，第126行provider撤销，第127行close开始、128–219行92项close、220行汇总，第225–226行db销毁，229行resourcesReleased=true。故本轮实际验证了generic模型/cache就绪后异常清理及所有独立引用配对关闭。fault1 get/release也均0，不将它计作借出/归还测试；旧manager fault1只复用冻结结果。fault3/fault2维持manager路径，两注入均实际passed，ambient保护保持。

本次B `matchedGetReturns=0`、`matchedReleaseReturns=0`、`dynamicResourceMatchObserved=false`、`callbackCorrectnessProven=false`、boundaryFailure/observationFailed/persistentAdapterUnavailable均false，providerLogFailures/orphanCalls均0。borrowedAtModelDeletion=0、observedReturnIncomplete=false只是本次没有任何借出；allBorrowsReturned=true同样不能证明release回调协议已受测。没有实际返回drawable的请求角色可报告，也没有验证unknown get、未知release/underflow或close失败的动态分支，不用静态目录角色替代真实请求角色。

**v24曾有的真实请求本轮未重现。** 本轮先完成DB写入再pin/cache ready的共同准备顺序也已改变，不能将请求消失归因于provider成功满足；没有actual get就不能称动态适配接通、原请求角色已识别、返回合同已验证或零绘制根因已解决。两臂仍无文字、RGB、glyph或生产采集资格，也不能据零回调宣布所有公开路线不可行。

controller收尾记录workerExitCode=70、explicitStopRequested=true、forcedTermination=true、normalQuit=false，是精确强停而非正常QUIT。4个精确暂存已退役，6项源/快照/工作副本保持；owned SHOW/前台均0，relatedCadEventsNeedReview=false。原始记录未改写。最小索引：`执行/N9h-自有资源读取适配证据索引.json`。

| 关键原始证据（相对session） | SHA256 |
| --- | --- |
| case/model-factory-comparison.json | D222E391D4665DA15A5E3B7833101487D9B3E3600BCEB7E120366E458DD2404E |
| case/lifecycle-controls.json | FAB2643C507561F8E3ACDB63C0CFA01BCA2FF40C572164749D880E7573D6FE1F |
| case/nonpersistent-kernel-model-control/gs-trace.jsonl | 8D81CA88D5C7694CBF9F2E6B7A31A34E468CA91793D8A1042DFB581EE398A4B3 |
| native-result-closure.json | 6F07F81592829AD0AA694882A0C4419587FA056CC99C539F32CAED91761D6FB5 |
