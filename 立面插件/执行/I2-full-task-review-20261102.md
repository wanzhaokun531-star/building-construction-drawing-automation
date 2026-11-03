# I2 完整任务独立审查（2026-11-02）

## Findings

### [P2] 将普通命令捕获的状态、只读监测和恢复结果纳入断言 — 立面插件/tests/native/unitconfig/UnitConfigDriver.cpp:503

`printedCommand` 已在第179—195行取得 `status`、`readOnly`、`restored`，但第447、500、503行三个调用点均只根据日志子串判定通过，`recordPrinted` 也仅记录日志路径和哈希。尤其最后的 INFO 在最后一次 `preserved()` 检查之后执行：即使命令改变模型或全局变量，使监测明确得到 `readOnly=false`，只要仍打印预期文字，`InfoDisclosesUnitOnlyScope` 和最终 case 仍可通过。应让这三个普通命令断言同时要求 SDK 成功、`readOnly` 和 `restored` 为真，或在公共 helper 中强制失败。这是新驱动的漏判，不是当前产品存在该写入行为的证据，也不否认原始51项确实记录通过。

精确调用位置：第447行 `BCE_ISSUES after CONFIG` → `ConfigInvalidatesOrdinaryIssuePages`；第500行 `BCE_ISSUES after switch` → `SwitchInvalidatesOrdinaryIssuePages`；第503行 `BCE_INFO` → `InfoDisclosesUnitOnlyScope`。三个返回值都未门禁 `status == RTNORM`、`readOnly == true`、`restored == true`；其中 `readOnly` 包含数据库写事件、根集合、全局、资源、NOD、当前文档/working database、捕获中日志状态比较，`restored` 另比较进入时全局。`Transcript::stop()` 自身已强制日志 mode/path 恢复，故不是说现有代码完全没有日志恢复门禁；缺少的是这三个调用者对完整结果的验收。

最小验证期望：离线针对实际修复后的判定逻辑保留预期输出子串，分别注入 SDK 非 RTNORM、`readOnly=false`、`restored=false`，三类每类都应使相关断言失败；全部状态正常且输出正确才通过，输出错误仍须失败。应覆盖三个调用点的接入，避免只测一个未被真实调用的替代 helper。目前只建议该离线覆盖及源码/候选冻结；平台未放行前不启动新 CAD，也不把旧51/51改标为新驱动已原生通过。平台放行后的实际覆核应确认普通命令结果已经作为断言条件记录。

未发现第二项达到可操作缺陷标准的新增问题。

## 规格与质量结论

**规格：产品实现静态符合本次受限单位入口要求；相关原生结果已核实。** 单位配置、历史拒绝、当前图保护、实际撤销和跨项目沿用均有代码与相应原始证据支持。普通 INFO/ISSUES 的专门无写监测尚未成为驱动通过条件，因此不能把这部分监测当成已强制验收。

**质量：需修正上述一项 P2 驱动漏判；产品实现未发现独立的可证明新增缺陷。** 当前51/51是实际执行结果，不能扩写成所有已计算质量条件都参与了断言。本结论仅覆盖 I2，不是整分支、完整交互配置或施工图生成验收。

## 审查范围与绑定

- 工程仍为 `D:/Codex/建筑施工图自动化`，未迁移空聊天目录。按本地 review-agent、autocad2020-objectarx 技能以及工程 AGENTS 的保护和归档记录执行；平面任务的命令例外未作为本任务授权。
- 依据 `I2-项目单位配置入口简报.md`、实施报告、full-review README、manifest，完整阅读1190行 `full-I2-from-I1.diff`：冻结 I1 772efe6e → I2 f1f997b8，包含全部新文件及两处允许集成。仅对输出截断处补读；另读取具体依赖契约，不做无关工程遍历。
- 独立哈希检查全部通过：10个变更文件的当前源/包快照共20项、两份 I1 基线、35项候选 sourceFiles、31项 inputOrigins、3份依赖 receipt、48项证据、13项包文件；正式/驱动二进制也匹配。依赖保持 N4 2d31d01f、G2 3d68c479、N6 84664abf。
- 产品 SHA256：`DCCF7AD7A4661B38192EE45C0FA5273D5981F1CA49C51755AE1C37573352EE95`；驱动：`C9FECA129697CC26E343852386122236B25E407EF542755941CFB65839E2EE91`。项目配置维持 AutoCAD2020/R23.1、x64、v141、C++17，产品和驱动分离。

## 产品契约核对

| 要求 | 审查结果 |
| --- | --- |
| 普通 CONFIG 明确限单位，有限正数，取消无写 | 原生 `acedGetReal`；输入前后核对上下文身份/revision；服务明确 `isfinite && >0`。取消/错误不写，无默认比例或固定项目尺度。 |
| 初始/严格单位-only 档案 | 对照 StoredProject、ProjectConfig、RuleInputs、GenerationState 和 codec 字段，零点三字段、全部视图/规则集合及来源、confirmedEntities、generation 集合和两个身份字段均枚举排除。只允许完整可读、无诊断档案或唯一 PROJECT_NOT_FOUND 分支。 |
| 保留历史、foreign、损坏内容 | 不调用确认过滤来抹除既存事实，不清空重建，不自动重绑实体句柄；受限入口拒绝含几何和归属历史的档案。既有 G2 再核项目 ID/历史保护。 |
| 独立 ID 和来源 | 系统 CryptoAPI 熵产生 UUIDv4；项目和每次明确确认分别随机。更新保持 persistentProjectId，比例与来源同时记录。无固定句柄、路径或输入 SHA 门槛。 |
| 单位资格与几何资格分离 | 仅限定本次单位写入的 context/database identity、revision、archive.bound；不构造 ObservedScene/absent sentinel，不调用完整几何 bind。读取仍 unbound，报告 generationAuthorized 恒 false。 |
| ANALYZE 只读沿用 | 仅接受经 decoder 和 unit-only 校验的明确比例/来源；不传历史 ProjectConfig、plan、零点、轴、材料。真实支持的 INSUNITS 4/6 经既有 resolveUnits 诊断冲突，不修改系统值。 |
| 当前 DWG/未保存编辑/Undo | 产品不 save/close/reopen，写入全部通过现有 G2 自有事务；外事务和外 Undo 组拒绝。CONFIG 普通 modal；ANALYZE/INFO/ISSUES 为 NO_UNDO_MARKER。取消/异常同样先使旧报告和分页失效。 |
| 所有权与卸载 | 初始化先检查四命令冲突，失败清理本模块已注册项；卸载按组和 owned 标志移除。没有 COM、CUA、天正业务命令/私有字段访问或产品日志设置修改。 |

## 原生证据核验

原始成功报告：`阶段1/runs/elevation-native-20261102-192313-d01b1cfc/case/native-case.json`，SHA256 `E0922838AC5670AFA5EDCD9360C402FB1AE2FD322E5639B9A5E0E771843A23B9`。严格 JSON 可读，status=passed，51个 steps 全部 passed、errors=[]；与 actual-51-tests.json 的51条逐项完全相同，产品加载绑定匹配上述二进制。

12份原始 state 与包内摘录逐字段一致，包含以下关键事实：

- 首次 U 真实 started/ended 各1、failed0，档案由存在变 PROJECT_NOT_FOUND，NOD 回到配置前，742根/原741根及未保存中文、shape/globals/resources 保持。
- 更新前单位2；一次真实 U 恢复单位1、旧项目 ID、来源和精确原始档案字节。Esc 真取消计数1，取消前后 raw 相同。
- 普通输入1/2/1000、0/-1拒绝、空响应、外事务/Undo组拒绝；9种既有历史/foreign/损坏夹具拒绝且无写，历史不提供单位或生成授权。
- 第二自有项目独立 ID、单位1000，实际 INSUNITS4冲突诊断；切图/返回报告失效、返回首图单位1。configured/returned 原始分析均4立面、2剖面，来源明确且 generationAuthorized=false。
- 普通 ISSUES 两份原日志明确要求重新 ANALYZE，INFO 原日志明确“仅配置本图单位”。这些输出事实成立；它们的额外无写状态存在本报告 P2 所述漏判。
- wblock 独立副本在显式复制项目 Xrecord 前没有档案；测试驱动经公开 clone/ID mapping/命名准备后，保存前和重开后 raw 与当前图一致，仍 unbound。它不证明产品提供自动 WBLOCK 档案传播；产品没有保存/重开当前图。
- foreign CONFIG 加载冲突保留外来回调，后续重载和四命令 SDK 卸载通过。退役记录明确5暂存移除、6输入保持；审查时六项源/快照/测试输入实际 SHA 再核均匹配。自有 SHOW/前台0、监测错误空；宿主显式停止70，forcedTermination=true、normalQuit=false。

有效 RED 从归档原路径只读核实：330a08d0/fa7b68b5，9项中仅 OrdinaryConfigPersistsExplicitUnit 失败、errors=[]。没有把被归档路径当前缺失算作未执行。

## 保留边界

本轮4组/20份原始 logger-state 的 getter/status/restype 符合记录，实际 setter 返回5100，四次最终严格恢复 mode0 与进入时的路径，loggerFirstDifference=null。进入路径确为先前 f288 自有日志目录，不是更早默认路径；旧故障未复现、根因未知。产品不改 logger，不能把此项未知原因当成本轮51项无效，也不能宣称旧间歇故障已解决。新的失败终结/cleanup 分支只有代码审查，本轮成功运行没有再次故障注入验证。

本轮原生非法输入覆盖0/-1，非有限值通过实现显式守卫核对，未另称 NaN/Infinity 的 CAD 交互已测试。没有新编译、启动 CAD、重跑全套、清理、修改注册表/源档案/index/HEAD、提交或推送；唯一写入是本报告。
