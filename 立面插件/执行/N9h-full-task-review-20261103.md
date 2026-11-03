No findings.

**Spec verdict：PASS（本次限定实现、既有实测与证据表述）。Quality verdict：PASS（未发现本增量引入的可行动缺陷）。** 此结论不表示实际资源请求已被满足：B 的 get/release 均为 0，真实 drawable 返回及归还分支没有动态覆盖。原 main、A、B 仍失败且无 WorldDraw/RGB，完整采集和生产资格均未取得。

2026-11-03。本次依照 review-agent、autocad2020-objectarx 技能及 N9h 完整任务复核简报执行一次完整限定复核；使用 verification-before-completion 方法核对已有证据与本次文件读取结果。仅新增本报告，未改源码、Git、旧证据或冻结制品，未派代理/二审、构建、启动 CAD 或重跑全套。已闭环 N9e/f/g 和未来 physical palette 调查不在本次范围。

复核对象为工程 `D:/Codex/建筑施工图自动化/立面插件` 下，v24 `build/n9c-20261103-131202-815-n9g-model-factory-v24-frozen-34b66b1a/src` 至 v27 `build/n9c-20261103-133951-997-n9h-owned-resource-v27-frozen-9c7b8692/src`。已完整读取 211 行 `n9h-v24-owned-resource.diff`、本项需求简报/实际报告/证据索引及 SDK 合同；对生命周期、样式写入和 terminal/unload 的具名问题补读冻结源码上下文，未扫描共享 dirty 树。以下源码行号均指 v27 冻结 `src/NativeGsTextFootprint.cpp`。

| 核查点 | 结论与定位 |
| --- | --- |
| 公开目录、去重与别名 | 178–221 行以自有 DB 的九类公开 SymbolTable ID/record ID、NOD 公开 entry ID 和已知 marker/modelspace 建目录。ID 的 asOldId 仅作精确 key；先按 key 去重再 open，重复角色保留 alias。字典派生类经公开 cast 识别，visited 防止循环，不附加样本数量上限。foreign/擦除/不可读项保留 unavailable 和错误，不从 callback raw 反构造 ID 或打开 foreign 对象。 |
| 一次 read-open 与驻库所有权 | 187–196 行在 open 成功后先记 pinOwned/对象及 acquired，再进行可能分配内存的观测；逐项核 ownDb、真实 objectId/key、未擦除和 isReadEnabled，只有通过者暴露真实 AcDbObject→AcGiDrawable 指针。marker/BTR 原所有者与 provider 独立 read-open 分开计责。iterator 用 unique_ptr 删除，驻库对象按 close 归还，未引入 delete 驻库对象。 |
| 准备顺序 | 348–351、489–490、496–501、523 行显示两诊断臂及替换 fault1 先完成 marker/style 显式写入及源样式 16 字段核对，再 pin，再建 model/view；避免先 pin 待写字典造成 eWasOpenForRead。原 main、fault3/fault2 保持原 manager/ID 时序。比较元数据明确 preparation order/cache 与 provider 同时变化，未声称 provider 单因素因果。 |
| callback、线程及日志异常 | 223–247 行 get 仅在 providerMutex 内查内存目录，精确命中已验证 pin 才返回真指针；记录 raw/bNeedsValidation/thread/角色/class/ID/指针，不推断 bool 协议。release 仅匹配实际借出指针并扣借用数，不 close/delete。局部 catch 阻止 C++ 异常越过 ABI，日志/边界异常及 unknown release/underflow 保持失败状态。catalog 准备先于注册，撤销 provider 在同一互斥下等待已进入 callback 退出，随后才收 pin。本轮没有 callback 实调，以上为静态结论。 |
| model→provider→cache→DB | 140–171 行先移除 view/root/关联等使用，再实际销毁 model，随后撤 provider，再逐项 close 独立 read-open，最后释放 device/kernel/DB。成功 close 后先清拥有状态再构造日志，后续日志异常不会重关成功项。失败项保留 pin/对象/DB/Session，直接返回 false。 |
| 失败保活、terminal/unload | 138、153–164、560–565、576–605、615–616、684 行把 catalogPinsClosed 纳入 resourcesReleased；正常诊断、故障控制及 terminal 的 cleanup 判断均包含缓存。异常终结不删除仍持有资源的 Session，unload 对仍活动 provider/未完成 cleanup 拒绝释放，未见失败后先毁 DB 或假报全部释放的新增路径。close 失败与日志失败未作动态注入，仅作静态确认。 |
| 借用与物理 pin 分离 | 155–156、223–247 行分别保留 outstanding/borrowedAtModelDeletion 与 read-open close 事实。模型删除后未观察到 release 只记录 observedReturnIncomplete，不伪造归还，也不新增“SDK 保证逐 get 可观察 release”的失败前提。真实 unknown-pointer/underflow 仍阻断诊断保护。 |

SDK 依据为冻结 `n9h-sdk-public-contracts.json` 及本机 2020 CHM/头文件公开合同：AcDbObject 公开继承 AcGiDrawable，驻库后删除责任属于 DB；调用方配对 close 自己成功取得的引用；表/字典公开 ID 与 iterator 负责目录枚举。未扩展为 unknown/null 安全保证、context 绑定保证或 bNeedsValidation 协议。

**已有实测的独立核对。** 唯一实际运行 `阶段1/runs/elevation-native-20261103-134059-1ce10761`，PID 49032、nonce `1ce10761b60546afbc3c80f07474cb0a`。native-case 实际 moduleSHA256 与冻结 v27 一致。三份 model 前 catalog 各为 92 个 unique ID、92 个 verified/usable/read-open 条目，enumerationComplete=true、errors 空；各 14 个条目保留多角色别名。这个有限目录数不证明覆盖所有 GS 资源。

| 路径 | 已观察生命周期（trace 一基行号） | 实际结果 |
| --- | --- | --- |
| A manager/value | 写完 6 → ready 99 → model 销毁 170–171 → close 开始 172、92 项 173–264、结束 265 → DB 272–273 → released 276 | acquired/closed=92/92，逐项 status=0，零缓存错误；无自定义 provider 实调。 |
| B kernel/value | 写完 6 → ready 99 → createModel 102–103 → deleteModel 172–173 → provider 撤销 174 → close 175、92 项 176–267、结束 268 → DB 275–276 → released 279 | acquired/closed=92/92，逐项 status=0；get=0、release=0、matchedGet/Release=0，dynamicResourceMatchObserved=false、callbackCorrectnessProven=false。 |
| generic/directvalue fault1 | 写完 5 → ready 98 → createModel 101–102 → deleteModel 124–125 → provider 撤销 126 → close 127、92 项 128–219、结束 220 → DB 225–226 → released 229 | 到达原 `injected-after-model-association`，passed/cacheLifecycleProtected=true，92/92；get/release=0。本轮实际覆盖缓存就绪后的异常清理。 |

fault3/fault2 仍沿 manager 路径，约定注入均到达、passed=true；本轮替换的 fault1 不冒称重跑旧 manager fault1。三条缓存路径均有真实 92 pin/92 close 记录，未用比较 JSON 的汇总数代替逐项 close 检查。A/B 最终 catalog.readyForModel=false 是清理后的状态，model 前的独立快照与 trace 均为 ready=true。

原 `gs-result.json`/`native-case.json` 均 failed，错误为 `GS produced no real owned WorldDraw callback`，主 frames 空；A/B 的 attributes/WorldDraw/viewportDraw 均 0、rawRGBPath=null、mapping=false。ambient 指针保持、DBMOD 1→1、所有写事件 0。comparison.protectionVerified=true 仅表示本次诊断保护满足。B 的 allBorrowsReturned/observedReturnIncomplete 等零借用结果不构成 release 行为受测证据。

v24 出现过的实际请求本轮未重现，共同准备时序和缓存状态也变了，不能声称旧请求角色已识别、实际 drawable 返回合同已验证、动态适配接通或零绘制根因已解决；同样不能仅因本轮未调用 provider 就判定这段适配代码错误。未知 get、真实匹配 get/return/release、unknown release/underflow、callback 并发/日志异常、close 失败的运行分支仍未覆盖。上述为明确的证据边界，未发现足以据此提出新增代码缺陷的具体路径，未扩大动态验证矩阵。

controller 收尾为 workerExitCode=70、forcedTermination=true、normalQuit=false；4 个精确暂存 removed=true、6 项源/快照/工作副本保持。owned SHOW/前台为 0、relatedCadEventsNeedReview=false。强停未算正常 QUIT，失败记录未改判。所有诊断保持 diagnosticOnly=true、qualification=false，无文字、glyph、BUILD、主 5×2 或完整生产采集资格。

**身份及复核验证。** 本次只读计算确认 receipt 的 10 项冻结 sourceFiles 哈希全部吻合；冻结 src 的 9 项相对 v24 只有 driver 和 README 改变，其余 7 项相同。完整 diff 已逐 hunk 读完。证据索引实际列出 13 项 artifact，逐项 SHA256 全部匹配；没有将简报中的概括数量当作索引事实。沿用既有构建 exitCode=0 的回执，不声称本次重新构建或运行 CAD。

| 文件 | 本次核得 SHA256 |
| --- | --- |
| v27 receipt.json | 2791F797A94FACE4774C95809633CC87797D99755BCF993CC7274EC9F2D4764E |
| v27 out/NativeGsTextFootprint.arx | 222B6D94D331B232599A730CAFB6765FBB45B60C43A27D2252E10E463FFB9D9F |
| n9h-v24-owned-resource.diff | DAFA9F36350FF7F59DA72C6B73E474A2612D07A4C0D9505851BF4C7CF05409E5 |
| N9h-自有资源读取适配报告.md | B3C3E48337F040A7B0C7D095A689629C7B60947C77CC51CFBEC6C7C768F1939A |
| N9h-自有资源读取适配证据索引.json | D6C2B7FEB8A2888768941195CBF991DB896ECA8600CD10922C13295C1E43F98E |

Spec PASS 仅覆盖本轮限定的公开目录/真实 read-open provider 实现、生命周期保护、约定 generic fault1 验证及诚实证据报告；动态识别并满足实际 GS 请求仍未证明。Quality PASS 表示这次完整 diff 和必要上下文检查未发现可行动新增缺陷，不能替代未执行分支的运行证明。
