# N9e 完整任务复核（Spec + Quality，限定增量）

2026-11-03。Spec verdict：**PASS**。Quality verdict：**PASS**。结论只针对 N9e 有限边界观察切片，不代表 N9c 完整采集或产品准入。

**No findings.** 本次完整增量中未发现满足“新增、可证明、离散且应修复”条件的可行动缺陷。

## 范围与证据身份

依据 `执行/N9e-完整任务复核简报.md`、`执行/N9e-GS边界实测简报.md`、补齐实际段的 `执行/N9e-GS边界实测报告.md` 和 `执行/N9e-实际证据索引.json`，并参考 N9d 的精确 SDK 合同定位。已读适用 AGENTS.md，采用 review-agent、autocad2020-objectarx、cpp-coding-standards 技能。

- Base：`build/n9c-20261103-121817-090-idle-command-capture-v17-frozen-167b5fda/src/`。
- Candidate：`build/n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8/src/`。
- 完整阅读两者 `n9e-v17-boundary.diff` 的 157 行统一差异，包括 driver 和 README；未以共享 Git HEAD 冒充任务基线。
- 本次读回 receipt SHA256 为 `1921AE812258CE0B3291E820AE0CCE18B1B2A35A30962031D991A8CD23543861`；diff 为 `86E7160B814B8EB091D93F127D021FF3C77327DE3AEFE38273787DC47B3779C7`；冻结 driver 为 `A331F4B036B87A657D6780B5816419D9AA85E71F23F7BD43F7CAAEC9B579E253`，均匹配既有冻结记录。
- 实测唯一 session：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-124216-72c8e7b0`，PID 22936；native-case 与 closure 绑定模块 SHA256 `24884E607FA9B727DE2FB72F8AE6CA1A95354E64469EB30F5BC305422664EE1D`。
- 独立读回证据索引的全部 15 项原始文件 SHA256，全部一致；closure SHA256 为 `C9329C25B9908855212123B1623EDC974F0DCEE2C3B6C5DEA3FC23525C8E07FD`。

以下源码行号均以冻结 candidate 的 `NativeGsTextFootprint.cpp` 为准；实测行号以该 session 下文件为准。

## Spec 核对

| 要求 | 实现与实证 | 判断 |
| --- | --- | --- |
| 读取实际 model/view kernel，真实不兼容停止后续绘图 | 源码 365、378 行保留实际地址及比较后 require；main/diagnostic trace 第 5、16 行均实测相等，未预写结果 | 符合 |
| 实际模型列表，创建/绑定和既有更新前后有限观察 | 源码 155–159、326–333、378–388 行；两路创建及接入 device 后为 0，根/样式绑定后及更新前后为唯一预期 model、containsExpected=true | 符合 |
| 自有 model 在首次 view.add 前登记观察器，只观察根 added/erased，默认 true、最多 32 明细 | Session 内嵌观察器，源码 112、146–154、366 行；main/diagnostic 各 2 added、2 erased，四次 model/root 全匹配、returnValue=true、detailAttempts=4、logFailures=0 | 符合 |
| 回调 IO 例外不穿宿主，观察失败不变绘制成功 | record 为 noexcept，构造记录与写入均在局部 catch 范围；失败位/计数持久保留，execute/render/controls/主结果及 diagnostic 保护返回值检查失败位 | 符合 |
| cleanup 配对移除后销毁 model，异常及 unload 保活 | 源码 119、123–139、524 行；主、诊断及 fault 1/3/2 均真实 remove 后 destroy，resourcesReleased=true；提前返回/抛异常时 Session 不提前销毁，卸载通过同一 cleanup 成功后才 clear/reset | 符合 |
| main/conditional nonpersistent 各一次 singleton getter；安全长度、空/截断未知；不可得不增加绘图前提 | 源码 160–173、367 行；三异常控制跳过配置；null config 记录 unknown 后 return 至原链，v18→v19 校正差异与冻结代码一致 | 符合 |
| 保留原工厂、根、样式、相机、renderer、更新、调度与资格 | 完整小 diff 只增加观测及失败记录；未新增 root 登记、generic model、参数矩阵或生产链修改；真实一次 idle 后 modal capture，captureCalls=1 | 符合 |
| 报告区分配置、能力、原因和完整采集 | 作者实际段与原始 trace/result 一致；明确 singleton 配置不证明 offscreen 活动 driver、加速启用或零回调根因，未将 failed 改为采集通过 | 符合 |

## Quality 定点核查

观察器由堆上 Session 的成员持有；容器移动 unique_ptr 不移动 Session，登记地址稳定。回调只累计计数/写日志，不绘图、不改参数，两个覆盖函数都返回 SDK 原默认 true。达到 32 条后仍累计总数。清理从 view.eraseAll 开始，观察器保持到根移除、view 清理及关联恢复后，先 removeModelReactor 再 destroyAutoCADModel；nonpersistent primitive 又保持到 model 删除后。resourcesReleased 新增 attached 位，正常及三个原异常路径均实际配对移除。卸载入口复用 cleanup，在清理未成功或例外时拒绝卸载，不提前清空含观察器的 Session。

为核查有名的字符/字节歧义，定点阅读了本次提取的 SDK currentDisplayDriver、driverName、getConfig HTML 和本机 2020 `inc/gs.h`，没有依赖新版签名。currentDisplayDriver 文档确实以字节描述长度，bool 确为配置 driver 的硬件加速能力输出；getConfig 确为 singleton。三个零初始化 ACHAR 数组各有 32768 WCHAR/65536 字节，数值实参 32768 同时不超过两种容量。buffer/usable 在数组界内查找 NUL；无 NUL、空及达到最小解释容量边界时保留 unknown，不越界读字符串。能力只在 path/driver 非空、终止且未达到保守边界时按公开输出合同解释；没有把 false 初值直接当不可得时的已观测能力。

源码检查补读限于 diff 上下文未展开的 Session cleanup、原调用位置、runGsCore 和 unload 保护，以核对新成员是否可提前析构或漏入保护；未重新审查旧绘制主体。SDK 补读用于判定长度与默认返回值两个具体问题。没有运行额外 focused 测试，已有实测足以回答本次路径问题；没有重跑构建、CAD 或旧全套测试。

## 实际结果与解释核对

主 trace 5/16 行和诊断 trace 5/16 行确认 kernel 一致；模型列表分别在主 17/20/30/34/43/45/48/50 行、诊断 17/20/30/35/44/46/49/51 行，确为 0→1 的实际读回。根通知在主 23/32/33/55 行、诊断 23/33/34/56 行，观察器移除汇总为主 76、诊断 73 行。顺序销毁重建存在数字地址复用，不能推成同一个存活实例。

两路 trace 8–11 行保留 singleton 来源、原始路径 `D:\CAD2020\AutoCAD 2020\Drv\`、`gdi15.hdi`、`GSHEIDI`，以及 capability=false；缓冲 NUL 索引 28/9/7 与作者报告一致。没有证据将该能力输出解释为 offscreen 活动 driver 或绘制失败原因。

主 46/51 行和诊断 47/52 行仍为五项图形回调计数全 0、view/device valid=false。`case/gs-result.json` 原样为 status=failed、qualification=false、frames=[]，错误仍为 `GS produced no real owned WorldDraw callback`。诊断 result 为 diagnostic-failed、rawRGBPath=null、mappingVerified=false、sideDbSuccessClaimed=false。边界观察完成与采集失败同时成立，不能据此授予 BUILD、glyph、布局接入或完整采集资格。

`case/lifecycle-controls.json` 三原注入点准确到达，faultReached、cleanupComplete、ambientUnchanged、passed 均为 true；fault 1/3 根通知为 0/0，fault 2 为 1/1；三个 trace 的移除汇总及资源释放与报告定位一致。main、diagnostic 的 ambient 保护也保持，原结果没有因新观察而放宽。

closure/retirement 保留六项源/快照/inspection copy 指纹、四个精确 nonce 暂存退役。isolation-final 实际 workerExitCode=70、forcedTermination=true、nativeQuitConfirmed=false，closure/retirement 的 normalQuit=false；作者如实说明强停。175 样本内自有用户桌面 SHOW/前台均为 0、supervisorErrors=[]，同时另有 PID 47676 的一次其它 CAD foreground、relatedCadEventsNeedReview=true；原事件保留，没有冒称全局全零或归因为本任务操作。

## 总体判断与剩余限制

N9e 所需新增观察事实、实现范围、生命周期和报告解释均通过本次 Spec/Quality 复核，无需基于当前证据提出修复。已有编译 exit 0 与原生证据复用，不宣称本次重新编译或运行。供应商 PDB 的既有 LNK4099 不属于本增量源编译缺陷。

本次实际运行仍未到达真实 WorldDraw/context/RGB 或完整帧矩阵；零回调根因继续未知。IO 故障、空配置、长值/截断及跨 kernel 拒绝分支以代码/SDK 合同核查，当前实测没有触发，不扩大声明为已动态覆盖，也不据此新增本切片的故障矩阵或产品门槛。

本复核唯一写入为本报告；未修改源码、索引、冻结/旧证据、Git、生产安装或用户图，未派代理或第二审。
