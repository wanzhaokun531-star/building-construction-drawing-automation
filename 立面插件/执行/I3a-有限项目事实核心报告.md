# I3a 有限项目事实核心报告

日期：2026-11-03。状态：本简报指定的纯 C++ 值核心和有限失效比较已实现并完成自审；尚无 SDK reader、普通 CONFIG 交互、持久化或 BUILD 接入。完成本切片不表示用户现在可以运行实际零点配置命令，也不表示完整生成资格已完成。

工作根为 `D:/Codex/建筑施工图自动化`。唯一实施作者；未派子代理或审查，未操作 CAD、共享 profile、平台 DLL，未执行 Git 命令、提交或切支，未进入另一 driver 的源码。使用 cpp-coding-standards、cpp-testing、test-driven-development 和 verification-before-completion 技能，复用现有 TestSupport、CoreTests 工程和 build.ps1。

## 已实现接口及支持域

新增 `src/ProjectFacts.h/.cpp`，共 374 行。接口为：

- `confirmProjectFacts(ProjectFactConfirmation)`：校验人工已确认输入，生成 subsetVersion=1 的 `ProjectFacts`，仅从显式 selector 指定的 typed 定义点导出 datumPoint，并导出单位化高度方向与实际窗口四角。
- `checkProjectFacts(history, optional current)`：核对一次新的 typed 依赖观察，只返回 `FactSubsetCheck` 状态及带角色/根句柄/当前读取身份的 Issue。无当前观察时历史不能自证有效。
- `projectFactElevationMeters(facts, point)`：按显式方向投影，使用已确认 mmPerInputUnit 和实际 datumMeters 换算米值；不默认 +Y、mm1 或 +0，拒绝非有限、异平面及溢出结果。该函数只计算有限事实的实际标高数值，不构造 RuleInputs。

`FactSource` 保存实际 rootHandle、ownerHandle、exactClass、directRoot、effectiveVisible、文档/数据库/revision 出处和 typed variant；不把 canonicalGeometry 当作 typed 值。

| 类型 | subsetVersion=1 保存及比较的定义值 |
| --- | --- |
| exact AcDbLine | start、end、normal、thickness |
| exact AcDbAlignedDimension | xLine1Point、xLine2Point、normal；不要求 measurement、显示数字、DimStyle 或字形 |
| exact 无 field AcDbText | 原串、position、alignmentPoint、normal、rotation、height、widthFactor、oblique、水平/垂直对齐、两 mirror、thickness、hasFields |

标题为一个明确选择的 Text 依赖；楼层说明为 `vector<FactTextDependency>`，每项含显式 role 与 Text 来源，可以保留名称和值等多个独立文字，不固定句柄、数量或自动拼串。此数组只记录用户明确关联，不推断楼层语义。

确认输入保留 persistentProjectId、单位比例及确认来源、标题/楼层来源、四种点 selector、datumMeters 及确认来源、高度方向、人工选窗及其确认来源。窗口采用 origin、两正交单位 basis 与实际 width/height，导出按 origin/U/U+V/V 排列的四角；旋转 XY 窗口不转成 WCS min/max 包围框。

首域只接收同一 XY 平面的有限值：点的 z 一致性及 basis 正交/单位长度容差为 1e-9；normal 必须为非零 Z 方向；高度方向和窗口 basis 为非零 XY 向量；为避免把厚度实体当二维依赖，Line/Text 的非零 thickness 不在当前支持域。Text 的 height/widthFactor 必须为有限正数，对齐枚举须在公开模式范围。退化、溢出或因大坐标舍入完全塌缩的窗口拒绝。

未知/派生类、嵌套来源、field、不可见来源、无效句柄、与显式模型空间 owner 不符或不支持的平面返回定位诊断，不默认有效。不存在 SDK 调用，也不从拾取附近点、文字墨迹、历史参考数字推断零点。

## 核对行为及资格边界

| 情况 | 结果 |
| --- | --- |
| 项目 ID 缺失或不同 | orphan / FACT_PROJECT_ORPHAN |
| 没有当前观察、读取未完成 | stale / FACT_READ_REQUIRED |
| 有效新读取中的所需依赖全部相同 | currentSubsetMatches；只表示这组有限依赖匹配 |
| 单位比例或其确认来源改变 | stale / FACT_UNITS_CHANGED |
| 所选根未出现在当前读取 | stale / FACT_SOURCE_MISSING，定位原根 |
| owner 改变 | stale / FACT_OWNER_CHANGED，定位原根 |
| 所需定义值改变 | stale / FACT_DEFINITION_CHANGED，定位根和角色 |
| 隐藏、field、不支持 exact 类/typed 定义/平面 | unsupported / FACT_SOURCE_UNSUPPORTED，定位当前根 |
| 无效身份、revision 0、混批身份/revision、非法句柄 | unsupported / FACT_READ_INVALID；源级问题定位根 |
| 当前观察出现大小写/前导零等价的重复根 | unsupported / FACT_SOURCE_AMBIGUOUS |
| 历史 subset 版本、确认定义或派生值被破坏 | unsupported / FACT_HISTORY_INVALID |

句柄等价与合法性复用现有 EntityKey 运算符：非零 uint64 十六进制、大小写及前导零等价，前缀/空白/负号/溢出拒绝。没有另建句柄解析规则。

历史 runtime document/database/revision 只作出处，不与新批次强求相等；同项目同依赖在新身份/新有效 revision 重新读到时可匹配。新观察自身须 readCompleted，且其全部记录的身份/revision 与批次一致、revision 非零。这个布尔值是未来真实 reader 的明确断言，当前核心不假装自行读取 CAD。

核心仅依赖 ElevationModel 的值/诊断/句柄规范，不依赖 GenerationState、ProjectArchive、CommandQualification 或 RuleInputs。返回值没有 bound、generationAuthorized、outputs、generationId 或生成资格；没有注册、写图、完整场景 builder 或新生成框架。当前观察的其它建筑定义不参与有限匹配，因此有限匹配也不承诺其它建筑或完整视图未变。测试同时验证：有限匹配后，现有不具备完整输入/准备证据的 planGenerationCommand 仍拒绝生成；此测试不代替未来真实 BUILD 集成验证。

## 业务 RED → GREEN

新增 `tests/ProjectFactsTests.cpp` 212 行，在既有 CoreTests 注册 8 项，原有 77 项保持。没有新测试框架、SDK/native 测试工程或生产 native build。既有工程继续 v141 / C++17 /W4 /WX /MT；实际编译器为 MSVC 14.16.27023。

8 项围绕真实接口行为，覆盖：四种 typed getter 与错 selector；非 +Y、平移、英寸比例及非零基准；旋转窗口手算四角；点/标题/独立楼层值/定位/可见性/owner/单位变化；新身份重开、历史/缺失/混批/零版本；未知类、嵌套、field、非有限、异平面、非法/溢出句柄；重复根及损坏历史；有限匹配不授生成资格、无关建筑依赖不冒称完整视图相同。尺寸 subset 本身没有显示数字/样式字段，匹配不以它们的伪造 sentinel 为前置条件。

| 隔离构建 | 构建/测试/未知case退出码 | 观察结果 |
| --- | --- | --- |
| I3a-red | 0 / 1 / 2 | 可链接的 fail-closed 接口骨架；8 项业务确认断言失败，原 77 项通过；不是缺符号或编译失败 |
| I3a-red-multitext | 0 / 1 / 2 | 多 Text 需求进入测试后，8 项仍因有效事实尚不能确认而失败 |
| I3a-green | 0 / 0 / 2 | 首轮实现后 85/85；0 警告、0 错误 |
| I3a-red-revision0 | 0 / 1 / 2 | 自审补整批 revision0 与零版本确认的反例，2 项业务断言失败、其余 83 项通过 |
| I3a-final-green | 0 / 0 / 2 | 收紧同批有效身份为非零版本；85/85；0 警告、0 错误 |

最终命令：`./立面插件/tools/build.ps1 -Target CoreTests -Label I3a-final-green`，其正常自检已全跑 85 项并检查未知 case=2，未再重复全套测试。

自审检查了接口与简报边界、字段逐项比较、历史与当前身份的分离、多 Text 定位、窗口旋转数值、句柄规范复用、工程/冻结输入闭包及新增核心没有生成资格调用。自审对照现有 reader 约定发现 revision0 也必须拒绝，已通过上述针对性 RED→GREEN 修正。没有另派审查，未宣称 fresh 独立审查或 CAD 验收。

## 冻结、小 diff 和回执

证据根目录：

`D:/Codex/建筑施工图自动化/立面插件/执行/I3a-evidence-20261103-140525/`

- `baseline/` 与 `baseline-manifest.json`：任何修改前冻结的受影响既有 CoreTests.cpp、ElevationCoreTests.vcxproj、build.ps1。ProjectFacts.h/.cpp 与 ProjectFactsTests.cpp 为新增文件。
- `I3a-small.diff`：相对该基线的 6 文件小 diff；既有文件只增加测试注册、项目项及冻结输入项。SHA256 `00CC7A0B24F6118FD75DB943D60A5FC4977CA8F6F0141E9F2EF5C99B226B0196`。
- `red-initial/`、`red-multitext/`、`green/`、`red-revision0/`、`final-green/`：每次实际编译的完整 input、out 可执行文件/PDB、build-command.cmd、原始 build.log/tests-all.log/tests-unknown.log 及 build.ps1 原始 receipt.json。原始 receipt 中 buildRoot 保留实际运行路径，不改写历史。
- `final-green/receipt.json`：最终构建、全测及未知 case 的原始回执；对应实际 buildRoot 为 `D:/Codex/建筑施工图自动化/立面插件/build/f1-20261103-141138-274-I3a-final-green-18fa9522/`。
- `final-green/out/ElevationCoreTests.exe`：最终实际测试二进制，SHA256 `9DAAD97F0D305BD11A48399E77368EBAAD8FE49A34EF1FC966154318D2203BCA`。
- `freeze-manifest.json`：冻结文件字节数/SHA256；`handoff.json`：6 文件范围、各轮实际/冻结路径及退出码机器回执。所用本地 quoted include 闭包已检查无漏项，最终工作输入逐项与最终实际编译输入一致。

本项只改上述 6 个代码/测试/工程/输入列表文件和本报告/本项证据；未改共享业务源、AnalysisReader、I2、codec、NOD、普通命令、生产 ARX 项目、平台 DLL 或 Git 状态。

## 后续尚未接入

SDK typed getter reader、source-handle 映射、真实当前批次/修订采集，普通 CONFIG 选择/确认/取消交互，NOD codec/writer/读回/事务/Undo，分析中的持久历史加载与状态展示，以及完整 ObservedScene/资源/字形/RuleInputs/BUILD 的资格及原子提交，均未实现。这些是后续各自工程；本切片没有借有限 subset 匹配替代它们。

## Fix round 1：限定修复两项 P2

日期：2026-11-03。依据 `执行/I3a-full-task-review-20261103.md` 的两项 findings，唯一实施作者仅修改 `src/ProjectFacts.cpp` 与 `tests/ProjectFactsTests.cpp`。未重写以上初版记录，原审查报告、207 项冻结清单及原 85 项证据保持为当时证据。本节记录修复后的实际验证，尚待原 reviewer 作限定复核，不宣称已取得复核结论。

1. **舍入窗口退化/失真。** 原检查只排除相邻角相等，不能排除四角共线，也不能证明实际四角仍符合人工矩形。修复对四条实际导出边分别按确认宽/高缩放，核对其与原 U/V basis 的分量一致性（维持 1e-9 无量纲容差），再核对实际邻边正交及非零二维面积；不可靠的表示返回 `FACT_WINDOW_UNSUPPORTED`。在原 `FactRotatedWindowRetainsFourCorners` 中加入审查给出的 `(1e16,0,0)`、旋转 `.6/.8`、宽高 1 的共线反例；再覆盖宽高 4 时虽不共线但已失真的情形，以及同样大 origin 下可精确表示的轴向 2×2 窗口正例，避免按绝对坐标大小拒绝。
2. **单位比例中间下溢。** 换算改用 `frexp` 分离距离和单位的二进制指数，先组合有界 mantissa 并除以 1000，再用 `scalbn` 施加联合指数；不再提前把单位比例除成零。仍拒绝最终非有限或非零距离的换算结果完全下溢为零，未添加单位比例阈值。原 `FactElevationUsesConfirmedUnitsDirectionAndDatum` 加入比例 `1e-321`、方向 X、实际零点 0、datum 0、待量点 `(1e308,0,0)`，独立期望值约 `9.98012604599318e-17` 米，以相对误差校验非零正确结果。

先冻结两文件修前字节，再只加测试：v141 / C++17 /W4 /WX 编译 0 警告、0 错误，指定两项分别在上述“共线窗口不能接受”和“可表示标高不能因单位中间下溢变零”的业务断言失败；不是构建/缺符号失败。修复后同样编译条件下 0 警告、0 错误，两项通过；随后只执行当前另外 6 项有限事实用例，合计 **8/8 通过**。未运行旧 77 项、`--all` 或未知 case；没有改通用 build.ps1、项目注册或新建测试框架。

本轮直接复用初版冻结的 `build-command.cmd`（vcvars 14.16、MSBuild /t:Rebuild、VCToolsVersion=14.16.27023），按原 receipt 的 31 项输入冻结当前字节到本轮隔离目录。证据内 `run-focused.ps1` 只是这轮命令/日志包装，未接入通用工具或产品。实际执行命令：

```powershell
& './立面插件/执行/I3a-fix-round1-20261103-141929/run-focused.ps1' -Phase red
& './立面插件/执行/I3a-fix-round1-20261103-141929/run-focused.ps1' -Phase green
```

包装内编译命令为 `cmd.exe /d /c ..\..\build-command.cmd`（cwd 为各轮 `input/tests`）。逐项测试调用为 `out/ElevationCoreTests.exe --case <case>`；red 只调用表内前两项，green 调用全部八项：

| case | RED 退出码 | GREEN 退出码 |
| --- | --- | --- |
| FactRotatedWindowRetainsFourCorners | 1 | 0 |
| FactElevationUsesConfirmedUnitsDirectionAndDatum | 1 | 0 |
| FactSelectorsReadActualDefinitionPoints | 未执行 | 0 |
| FactChangesLocateOnlyRequiredSubset | 未执行 | 0 |
| FactReopenRequiresFreshConsistentObservation | 未执行 | 0 |
| FactUnsupportedSourcesAndNonfiniteValuesAreRejected | 未执行 | 0 |
| FactObservationRejectsAmbiguityAndUnsupportedChanges | 未执行 | 0 |
| FactSubsetMatchNeverAuthorizesGeneration | 未执行 | 0 |

新增证据根为 `D:/Codex/建筑施工图自动化/立面插件/执行/I3a-fix-round1-20261103-141929/`：

- `baseline/`：修前两文件；`I3a-fix-round1.diff`：仅这两文件的最小修复 diff，SHA256 `A329375AAA137418CDB0663D496477362A82CFE216754A1761BB10F0BF5B59C3`。
- `red/`、`green/`：31 项实际编译 input、原始 build.log、逐 case 原始日志、实际 out 二进制/PDB、build-command.cmd 和各自 receipt.json。red 构建退出 0、两 case 均退出 1、总退出 1；green 构建及八 case 均退出 0、总退出 0。
- `green/out/ElevationCoreTests.exe` SHA256：`2F690A1A5EF37B90CE78410AA73B613A55DDD688F5FE5F40D9F634008B9B058B`。
- `handoff.json`、`freeze-manifest.json`：本轮范围/验证与字节哈希回执。31 项 live 输入与实际 GREEN 编译输入逐项相同；其中未改的 29 项仍与初版最终输入哈希一致。

本轮自审核对实际边校核不依赖绝对坐标阈值、换算不使用比例阈值、原历史/当前事实及生成资格边界未改。没有 CAD、SDK reader、codec、持久化、普通命令、新功能、子代理、提交或切支。
