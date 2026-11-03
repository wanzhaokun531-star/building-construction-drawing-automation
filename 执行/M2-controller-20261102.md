# M2 原生执行记录（2026-11-02）

已接管本轮唯一 CAD 启停权；现有 PID 43792、12608、40840 均非本任务，不触碰。

源工程：D:/Codex/建筑施工图自动化。归档：D:/Codex/项目清理归档/建筑施工图自动化_20261102-181100，仅复制明确缺失文件，不移动/删除归档，不覆盖不同内容。

候选 receipt SHA256：801F50114644FAC063832BD3201C9685B1280948D053202C3488FF2E69C1BB5C。ARX SHA256：DA2970C57935A03AA4439475418FFA7837673E455212F1108884B73E157E15B7。

最小恢复清单：M2-restore-manifest-20261102.json。全部原 receipt 指纹逐项相符。准备/监测/退役工具保持原内容；当前尚未启动 CAD。

## 本轮执行结果

- 新会话：`D:\Codex\建筑施工图自动化\阶段1\runs\elevation-native-20261102-184814-a1989210`。Binding：`D:\Codex\建筑施工图自动化\立面插件\evidence\native-case-binding-a1989210.json`。
- PID 26376，thread 42320，creationFileTime 134380901116476959，nonce `a1989210af3445b6a6d1d419c6401c4f`；恢复时CIM创建时间/命令行和receipt身份相同，未另启动宿主。
- 原生合法闭合报告：failed；steps 0，checkpoint 1；实际错误 `public Xrecord iterator:129`。`material-tiles-case.json`为0字节；没有整墙DWG，没有Plot。该错误未归因网络，也未解释129含义。
- 网络/工具中断后自有宿主额外存活约14分钟；恢复后先只读身份/ready/heartbeat/报告，随后立即按失败收尾。此延时是基础设施中断影响，不能称原定时序。
- H1 watcher完成精确退出和4文件暂存退役。workerExitCode=70，raw forcedTermination=true，explicitStopRequested=true，helperExit=0，normalQuit=false，retirement normalQuit=false。不称正常QUIT。
- 六输入SHA逐项保持；自有SHOW=0、前台=0、输入桌面读错=0、丢事件=0、monitorError=None。其他任务CAD 43792/12608/40840未触碰。
- 原生报告SHA：`CB962875AB59C82775E01E5307E8E96122C6AFF24127BF85E297750F1AC9430A`；详细结果/输入指纹及closure/retirement索引：`D:\Codex\建筑施工图自动化\执行\M2-controller-result-20261102.json`。
- 本轮不改源码/桥/登记/正式包，不提交推送；不复跑同候选，唯一CAD启动权返还root。

## 后续明确授权的串行验证

- N7 15018a85 有效归属 RED：binding `D:\Codex\建筑施工图自动化\立面插件\evidence\native-case-binding-5bad165f.json`；PID 41576；raw status failed，steps 78；退出码 70，精确退役 4 文件，六输入SHA保持，normalQuit=false。
- M2 71bf iterator 修正：binding `D:\Codex\建筑施工图自动化\立面插件\evidence\native-case-binding-0a2c340a.json`；PID 51840；raw status failed，steps 18；退出码 70，精确退役 4 文件，六输入SHA保持，normalQuit=false。
- N7 fd78 归属 GREEN：binding `D:\Codex\建筑施工图自动化\立面插件\evidence\native-case-binding-5b64a6f1.json`；PID 21372；raw status partial，steps 78；退出码 70，精确退役 4 文件，六输入SHA保持，normalQuit=false。
- I2 912f 日志恢复基础设施中断：binding `D:\Codex\建筑施工图自动化\立面插件\evidence\native-case-binding-f288f42c.json`；PID 45436；raw status failed，steps 0；退出码 70，精确退役 5 文件，六输入SHA保持，normalQuit=false。
- M2 035 克隆/回滚定位：binding `D:\Codex\建筑施工图自动化\立面插件\evidence\native-case-binding-29f8702c.json`；PID 41968；raw status failed，steps 19；退出码 70，精确退役 4 文件，六输入SHA保持，normalQuit=false。

N7 RED 78项74通过/4预期归属失败；GREEN 78/78通过但顶层partial、glyphComplete/upgrade均false。M2 71bf计数0/1/3通过，五整墙adopt254失败；M2 035五整墙primary RESOURCE_MAPPING_MISSING，254仅secondary，强制abort0但全资源census恢复false（119→128对象、遗留Hatch79等9对象），不得称回滚成功。I2 912f首次U已去档案，后日志全局恢复失败导致未final commit，停止后sentinel不能算51项完成；详各session只读摘要。全程未并发CAD，未改桥或源码、未提交推送；最终唯一启动权已返还root。

## I2 f1f997b8 严格日志诊断

- Binding `立面插件/evidence/native-case-binding-d01b1cfc.json`；nonce `d01b1cfcea564aa2b14c68e29584dbaa`；PID 52340/thread 28120/creationFileTime 134380922050022634。
- 本轮合法UTF8闭合 passed，51/51、errors空。首次U实际去档案，undo started/ended/failed=1/1/0；更新后U回旧unit1/bytes，Esc保持，显式测试副本重开SDKstatus0/unit1，回原项目unit1。上轮912f中断不改写为通过。
- 四组共20份logger-state严格恢复成功，无首次差异或exception；expected/actual mode0，canonical路径相等。初始路径实际为上一f288会话ordinary-unitconfig-output，本轮按实际初始值恢复。详情 `阶段1/runs/elevation-native-20261102-192313-d01b1cfc/unitconfig-logger-observation.json`。
- Report SHA E0922838AC5670AFA5EDCD9360C402FB1AE2FD322E5639B9A5E0E771843A23B9。5暂存退役，6输入SHA保持；workerExit70/rawForced=true/explicitStopRequested=true/helperExit0/normalQuit=false/retirementNormalQuit=false。SHOW/foreground/desktopErrors0、monitorError=null；无本任务活跃CAD，唯一启动权已返还root。

## M2 34c407a8 资源路径与mixed Undo对照

- binding `立面插件/evidence/native-case-binding-ab61d588.json`；PID40132/thread32380/creation134380926205770384；nonce ab61d58866264f7ca8ff7e3751a0fca1。19:30:20启动，19:31:18退出。
- 合法closed failed，20项14通过/6失败（Undo ON完整census恢复及五整墙Prepared），无DWG/Plot。缺映射精确为Hatch79→Layer7A→Material21(key Global)→Dictionary22：compute0/pairValue null，目标22可按owner-key解析但仍严格拒绝；254为cleanup secondary。
- OFF/ON均clone0/abort0但fullcensus false，119→128原119不变，遗留78Layer/79Hatch/7ALayer/7BDictionary/7CXrecord及7D..80四RegApp；事务1→0，不能称回滚恢复。ON退出恢复Undo原状态true，不等于资源恢复。
- 原始两份case/material-clone-mixed-undo-{off,on}.json保留；精简事实 `阶段1/runs/elevation-native-20261102-193016-ab61d588/material-mixed-undo-observation.json`。reportSHA B6CE9DAA7453F33E786C24BF011F817EB286989560F47F145086B434FEDA7FA1。
- watcher已请求S1QUIETEXIT后精确stop并4暂存退役/6SHA保持；workerExit70/rawForce=true/explicitStop=true/helper0/normalQuit=false。SHOW/前台/desktopErrors0、monitor=null。收到跨任务暂停启动协调后未新开宿主，未触其他CAD或共享DLL/profile；无自有活跃CAD，返权并保持暂停。
