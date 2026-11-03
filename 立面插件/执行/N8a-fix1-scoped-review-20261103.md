# N8a 原 P2 修复续审：round 1/5

**Spec：本轮修复符合要求。原唯一 P2：ADDRESSED。**

**Quality：Approved（限定原 finding 与 fix diff）。No findings.** 本轮修复内未发现新增可操作缺陷；不重开完整任务审查。

## 原 finding 的处置

| 原 finding | 结论 | 修复定位与依据 |
| --- | --- | --- |
| [P2] 拒绝辅助函数捕获自身 sentinel，源 SHA 反例未走正式校验 | **ADDRESSED** | `RejectionAcceptance.h:9–16` 仅在 try 内执行被测 operation；正常返回后的 `fault_was_not_rejected` 在 catch 外抛出。`DimensionAdaptGreen.inc:186–187` 共用此 helper；`:324` 将旧全零 SHA 比较替换为正式控制。`RejectionFocus.inc:3–24` 先调用原 `validate_identity` 验证健康配置，再仅改变坏配置的 reference 路径，实际调用同一正式入口；`:17` 必须收到 `reference source SHA mismatch` 才接受。 |

`RejectionAcceptanceTests.cpp:8–16` 分别验证正常返回且 name==expected 必须失败、真正预期异常保留原消息、错误阶段异常必须失败；`RejectionFocus.inc:29–36` 在真实宿主重复同一 helper 控制。原来测试即使失效也自判成功的路径已经切断。

针对“坏源是否仍由另写比较式自行拒绝”的具体风险，检查了包内未变 `NativeEvidence.cpp:179–190`：正式函数确实计算传入 reference 文件的 SHA 并检查冻结值。新控制调用的就是该函数；其异常不是新 wrapper 自造的拒绝消息。`RejectionFocus.inc:6–15` 只读打开原参考，独占新建 output 字节副本并追加明确故障标记，关闭文件后计算实际坏 SHA；原输入没有被改写，也没有通过 CAD 打开或保存坏夹具。

## 现有证据核查

复核入口为 `review-fix-round1-07d5a532-20261103/review-index.json` 和 `fix-round1-complete.diff`。完整 diff 中 Driver、Green、build 脚本的实际改动及三个新增文件全文均已读；未采用旧的不完整 scoped.diff。另读原 finding、fix 报告、主报告最新修正声明和 final manifest 的绑定。本次复用此前已读技能与项目约束。

- 离线 RED 日志确实在编译和原拓扑测试之后报 `helper accepted normal return when name equals expected`，receipt exitCode=1；不是编译失败替代 RED。GREEN 日志有拓扑与拒绝测试各自的 PASSED，receipt exitCode=0。SDK rxapi.pdb 的既有 LNK4099 仍披露，未新增为本轮缺陷。
- `428e9c88` 的 `native-case.json` 为 closed partial、errors=[]、4 passed，caseName 为 `DimensionAdaptSourceBindingFocused`。实际 module SHA 为 `BC0EEB29B77CE4C16F950BFE1CD0B8709EFC572760D38CDD20D3787EBD730885`，与 `07d5a532` receipt/final manifest 一致。
- `formal-source-sha-rejection.json` 记录健康身份通过；独立坏文件 SHA 为 `0ABFD1534F7C9CD83C065E3593CE0A106CA828984113E2273EE6568FF820195F`，正式阶段诊断为 `reference source SHA mismatch`。这补上了原正式源绑定拒绝覆盖缺口。
- `rejection-focused.json` 记录三种 helper 控制成立、current/active/working 保持、globalsBefore==globalsAfter、原源 SHA 保持。其 normal-return diagnostic 正是 `fault_was_not_rejected:source_sha_mismatch`，现在该诊断被作为失败控制核验，未当成成功拒绝。
- 本 reviewer 对 complete diff、六个修复文件冻结输入和三份主要 focused JSON 共 10 个选定 SHA 绑定进行了只读核对，全部匹配。closure/retirement 记录与 root 既有核查一致：受控退出码 70、normalQuit=false、四暂存退役、六源 SHA 保持；没有把它称为正常 QUIT。

## 修复范围、资格与剩余边界

`DimensionAdaptDriver.cpp:217–218` 只新增 focused case 分派；`build_dimension_adapt.ps1:1,8,11,25` 只增加 case 参数、冻结新输入及离线拒绝测试执行。Green diff 限定 helper、正式控制声明和调用替换，未修改几何、resource binding 或 roundtrip 实现。新增三个文件直接服务于原 P2；修复范围内未发现新破坏。

作者最新报告已撤回旧“15 个有效反例”主张，明确区分 14 项原有有效证据与新的正式源 SHA 控制，保留旧缺陷/失败/raw/manifest。四个 mm1:100 场景及 3153 声明字段仍绑定 `1880dc7f/03f2f503`，没有迁移成 `07d5a532` 新跑矩阵。本轮只批准该 P2 修复及其 focused 证据，原 full-task 审查的其余结论继续适用。

**范围外观察：没有新增可操作发现，也未扩查未改的几何、resource、roundtrip 或整个插件。** glyph/upgrade 均 false；本结果不授完整生产、当前图提交或其它未证明维度的资格。

没有运行已有测试、编译、CAD 或 Git，没有派代理，没有改源码、旧报告、归档或共享环境；唯一写入为本续审报告。原唯一 P2 可关闭，无本轮剩余 NOT ADDRESSED 项。
