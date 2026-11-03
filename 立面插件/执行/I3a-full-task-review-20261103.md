# I3a 完整限定 Task Review（Spec + Quality）

日期：2026-11-03。结论：**需修改，2 项 P2 finding；本次仅审查，未实施修复。** 原有最终 GREEN 证据有效，但不覆盖下述两个数值反例。

## 可行动 Findings

### [P2] 拒绝舍入后退化成直线的窗口 — src/ProjectFacts.cpp:137

`windowCorners()` 只检查相邻角是否完全相等，没有验证导出四角仍有二维面积。取 `origin=(1e16,0,0)`、`u=(0.6,0.8,0)`、`v=(-0.8,0.6,0)`、`width=height=1`：输入两基向量为正交单位向量，所有数值有限；按第 130–134 行实际 double 运算，四角依次为 `(1e16,0,0)`、`(1e16,0.8,0)`、`(1e16,1.4,0)`、`(1e16,0.6,0)`。四条相邻边都不为零，因此第 137–138 行通过，但四角全部共线，已不表示人工选定矩形。普通有效的其余确认字段下，`confirmProjectFacts()` 会保存此窗口，后续相同观察还可得到 `currentSubsetMatches`。这违反简报的实际矩形窗口与退化拒绝要求，也超过实现报告所称“退化窗口拒绝”的证据范围。应验证实际导出边的独立性和矩形一致性，对无法以当前精度表达的窗口返回 `FACT_WINDOW_UNSUPPORTED`；在既有窗口测试中补此反例即可。

定位：[ProjectFacts.cpp:137](D:/Codex/建筑施工图自动化/立面插件/src/ProjectFacts.cpp:137)。现有窗口测试从 [ProjectFactsTests.cpp:89](D:/Codex/建筑施工图自动化/立面插件/tests/ProjectFactsTests.cpp:89) 开始，仅覆盖正常旋转、基向量退化和稍后测试中的角点溢出，未覆盖这种部分坐标舍入后的零面积。

### [P2] 避免单位比例先下溢后成功返回错误标高 — src/ProjectFacts.cpp:257

`validUnits()` 接受任意有限正比例，但换算先算 `mmPerInputUnit / 1000.0`。取可接受的比例 `1e-321`、`datumMeters=0`、零点 `(0,0,0)`、高度方向 `(1,0,0)`、待量点 `(1e308,0,0)`，其余依赖保持有效：该除法先下溢为 `0`，于是函数成功返回标高 `0`，第 258 行的有限性检查也不会发现错误；按实际可表示的输入比例计算，结果约为 `9.98012604599318e-17` 米，仍在 double 可表示范围。核心应采用能处理比例与距离组合的缩放换算，或者明确拒绝不能可靠换算的输入，不能把下溢悄然当成零高度。应补一个非零结果仍可表示、而中间比例下溢的反例。

定位：[ProjectFacts.cpp:257](D:/Codex/建筑施工图自动化/立面插件/src/ProjectFacts.cpp:257)。现有标高测试从 [ProjectFactsTests.cpp:76](D:/Codex/建筑施工图自动化/立面插件/tests/ProjectFactsTests.cpp:76) 开始，覆盖英寸、平移、非 +Y、非零基准和异平面，未覆盖此处的下溢。

以上两项均来自新增代码，均可由公开接口的有效输入触发；没有把既有问题、风格偏好或尚未接入的 SDK 功能列为缺陷。本审查只以独立 double 标量运算核对反例计算结果，未编写或运行新增 C++ 测试，未运行现有测试二进制或构建。

## Spec Review

**结论：限定功能和范围基本符合；窗口数值边界尚未完全符合，见 Finding 1。**

逐项对照简报、完整小 diff 与当前新增原件，确认如下：

| 要求 | 独立代码核对 |
| --- | --- |
| Typed 定义值与明确 getter | `FactLine` 保存两端、normal/thickness；`FactAlignedDimension` 保存两 xLine 点与 normal；`FactText` 保存简报列出的原串、点、normal、排版数值、对齐、mirror、thickness/hasFields。`selectedPoint()` 第 113–121 行只接受四种显式 selector 与对应 variant；exact 类由 `sourceProblem()` 另外严格匹配。没有 canonicalGeometry 替代值。 |
| 项目、单位及出处 | `persistentProjectId`、单位比例和确认来源、datum 确认来源、选窗确认来源均保存；`sameRead()` 第 12–15 行要求非空文档/数据库身份与非零 revision，确认和当前观察都使用该检查。当前项目不同或缺失为 orphan；单位比例/来源变化为 stale。 |
| 历史与重开 | `validHistory()` 从确认值重建并校验派生值，但不把历史身份当当前证据；没有当前观察或 `readCompleted=false` 为 stale。当前 source 与当前批次一致即可，不要求与历史 runtime 身份/revision 相等，因此真正重读的同句柄依赖可在新身份下匹配。 |
| 来源支持域及定位 | 仅模型空间 directRoot、有效可见、合法句柄及受支持 exact 类；未知/派生类、嵌套、field、隐藏、异平面和无效 typed 值拒绝。`IssueSource` 带角色和相关根句柄，当前源问题使用当前读取身份。缺失、owner、定义值、单位、混批、重复根各有独立诊断。 |
| 多 Text 楼层说明 | `vector<FactTextDependency>` 保存每一项 role/source；遍历全部项比较，测试独立修改 A3 的楼层数值，未固定实际项目的数量或句柄。 |
| 高度与二维窗口 | 高度方向由确认输入归一化，支持非 +Y 和非零实际基准；窗口保持 origin/U/V/实际尺寸并导出旋转四角，没有 WCS min/max 替代。正常路径正确，但 Finding 1 和 Finding 2 阻止“所有受接受数值边界均正确”的结论。 |
| 仅有限失效核对 | Line/Dimension/Text 所列必要定义字段逐项比较；尺寸 measurement、显示数字和 DimStyle 不进入 datum subset。无关建筑定义变化不参加有限匹配。全部当前记录仍须满足同批身份和合法句柄要求，这与新观察自身必须一致的简报相符。 |
| 不扩大生成资格 | 新核心只包含 `ProjectFacts.h`/`ElevationModel.h` 的值依赖；没有 archive.bound、CommandQualification、RuleInputs、生成授权或输出归属写入。`currentSubsetMatches` 只是有限依赖匹配。测试中的现有 `planGenerationCommand` 调用位于测试文件，未成为新核心耦合。 |
| 实现范围 | 六文件中三个新增核心/测试文件，三个既有文件只增加测试注册、CoreTests 编译项和冻结输入项；未增加 SDK reader、codec、NOD、普通命令、完整 ObservedScene builder、生产 native BUILD 或额外故障框架。 |

首域拒绝非零 Line/Text thickness、使用 XY 平面及 1e-9 的约定已在实现报告中明示；这里未把将来的三维、SDK 读取或交互确认能力当作本切片必须交付项。测试中的样例数字和句柄只用于夹具，核心没有引用参考项目值或固定源数量。

## Quality Review

**结论：需修改，2 项 P2；未发现另外可行动的实现缺陷。**

核心为纯 C++ 值操作，variant 的取值路径有类型判定，未知 selector 不会默取点；句柄比较复用现有 `EntityKey`，其底层解析拒绝零、前缀、空白、符号和 uint64 溢出，大小写及前导零等价。历史出处与当前身份校验分离，字段比较和诊断分支可追踪；没有资源所有权、外部状态写入或 SDK 生命周期风险。窗口先验证有限正尺寸、单位基、正交性并检查角点溢出；高度方向先缩放再归一化，避免直接求巨大向量长度溢出。现有防护不能覆盖 Findings 所示的舍入退化和中间下溢，因此目前不宜直接接入后续窗口消费或实际标高流程。

测试复用 `TestSupport.h`、原 CoreTests 工程和既有框架，新增 8 项而不是另建大矩阵。四 getter、错 selector、重开、非零 revision、依赖变化和定位等行为均有针对性断言；85/85 仅代表现有用例通过，不是全部有限数值输入正确的证明。有限匹配后对一个原本不具备完整证据的 `planGenerationCommand` 仍拒绝的用例，加上核心本身没有资格状态/调用，可支持本切片不授资格；它不构成真实 BUILD 集成验收。

## 证据核对

审查目标仅为 `I3a-evidence-20261103-140525/I3a-small.diff` 所列六文件。先读简报与唯一实现报告，diff 读取一次；首轮工具返回截断，故只补读遗漏的新增 `ProjectFacts.h/.cpp` 和测试前 95 行，并为句柄规范定点读 `ElevationModel` 的相关定义/运算符。未重新审查整个 dirty 树。

- 独立核得小 diff SHA256 为 `00CC7A0B24F6118FD75DB943D60A5FC4977CA8F6F0141E9F2EF5C99B226B0196`；`handoff.json` 的六文件范围与 diff 一致。
- 独立确认 `freeze-manifest.json` 有 207 项；本次对六个审查文件的当前原件与 `final-green/input` 分别取 SHA256，六项全部相同。协调者已完成 207 个 manifest 项及 31 个最终 live 输入的全量比对、均无差异；此全量结果明确作为协调者提供的前置证据，本审查未重复全量扫描。
- 独立读五轮原始 receipt、build.log 尾段和 tests-all.log 的结果；最终 `tests-unknown.log` 为 `unknown case: UnknownCase`。没有重跑构建、自检、未知 case 或 CAD。
- 最终原始 build-command 指定 MSVC `14.16.27023`；原始 build.log 可见 `/W4`、`/WX`、`/MT`、`std:c++17`，且五轮各为 0 警告、0 错误。最终 receipt 退出码为 build/test/unknown=`0/0/2`，总退出码 0，记录的实际二进制 SHA256 为 `9DAAD97F0D305BD11A48399E77368EBAAD8FE49A34EF1FC966154318D2203BCA`。

| 原始证据轮次 | build/test/unknown | 测试原始结果 | 审查判断 |
| --- | --- | --- | --- |
| red-initial | 0/1/2 | Executed 85, failed 8 | 新增 8 项均在“valid actual facts must be confirmable”业务断言失败；原 77 项通过。冻结实现为三个可链接 fail-closed 接口，非缺符号/编译失败。 |
| red-multitext | 0/1/2 | Executed 85, failed 8 | 仍为确认事实的业务断言失败，不能单独冒称已孤立多 Text 失败原因。 |
| green | 0/0/2 | Executed 85, failed 0 | 当轮既有全套通过。 |
| red-revision0 | 0/1/2 | Executed 85, failed 2 | 两条明确断言分别为整批 revision0 不应当前有效、revision0 不应建立确认出处；其余 83 项通过。 |
| final-green | 0/0/2 | Executed 85, failed 0 | revision0 收紧后的既有全套通过，原始证据有效。 |

证据根：[I3a-evidence-20261103-140525](D:/Codex/建筑施工图自动化/立面插件/执行/I3a-evidence-20261103-140525)。本审查结论来自 diff、当前原件与上述原始日志的直接核对，不将作者自审当作独立审查结论。

## 实际限制与状态保护

本次唯一写入为本报告；未改源代码、测试、工程、证据、Git 或共享状态，未派代理。未调用 CAD、COM、CUA、天正业务命令、面板或私有字段写入，也未触碰用户原图或未保存编辑；未通过读取现有用户 CAD 会话来补造验证。不能据此声称用户原图字节已重新核哈希或真实 SDK/CAD 行为已验收。

SDK typed getter reader、真实批次/修订来源、交互取消/确认、NOD 持久化、读回/事务/Undo、普通配置命令、完整生成资格和 BUILD 集成仍属后续工作。有限匹配不能证明其它建筑内容、完整场景或生成资格未变化；解决两项 finding 后仍需针对修复做必要的业务验证，不能只引用本轮旧 GREEN 代替。
