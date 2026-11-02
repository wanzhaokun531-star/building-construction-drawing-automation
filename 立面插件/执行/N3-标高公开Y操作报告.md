# N3 标高公开 Y 操作能力探针

当前为已冻结、可交 root 原生运行的候选，尚无本候选的 CAD 执行结果。没有证明固定位置的 `setElevation(desiredValue)`。本代理未启动或控制 CAD，未安装、发布或改共享会话指针；不改 N1、N2、公共 NativeEvidence、F1 CoreTests、平面产品源码、用户 DWG 或共用配置。

## 冻结候选与验证

- ARX：`D:/Codex/建筑施工图自动化/立面插件/build/n3-20261101-183330-458-final-ed68d671/out/ElevationLevelCases.arx`
- SHA256：`83899657FC16B071764EB4C06C87887552B80CA50B4E1323F4AD35706468CCA8`
- 收据：同目录上一级 `build-receipt.json`，绑定10份完整冻结输入、对应 live 源、构建脚本、命令和日志。当前 live/frozen逐项哈希一致；源码从新目录 `tests/native/level/` 冻结。只读复用的 NativeEvidence.h/.cpp 与 live 逐字节相同，SHA分别为 `C6B083EE0346AA7CDB51090274CCCADCB2C6DE0AA92A196AD846E11C41C9EBB6`、`1B3CA34A61CC2C2DB2C42A7FDFD01C029DE7BE82EAE6CE96D9DE1C3B363A8FE2`。
- 构建入口：`tools/build_level.ps1 -Label final`。VS2019 Community实际v141/14.16.27023、Windows SDK10.0.19041、ObjectARX2020、C++17、x64、/MD、/W4 /WX，build exitCode=0。仅定向禁用SDK非UTF8注释的C4828；链接保留供应商rxapi.pdb缺失LNK4099，0编译错误。
- 同次执行 `LevelPolicyTests.exe`：`CHECKS=20 FAILED=0`，完整命令在 `build.cmd`，结果在 `policy-tests.log`，收据绑定其哈希。实际调用与原生使用相同的 numeric parser、聚合资格谓词和 N1统一 checked_read/ExecutionGuard；覆盖实际数字解析、缺数字/后缀/空白/非有限值拒绝、分支缺失、源SHA变化、硬错误、所有权/异常getter状态拒绝、硬中间快照后健康最终读取不能洗掉失败、已知空表示保留partial。未伪造TCH对象或业务setter RED。
- 前一版候选99AAD377只用于首次构建检查，不供原生运行；上述83899657是本报告唯一当前候选。

## 已实现的隔离协议

独立命令组 `BCE_LEVEL_CASES`，仅注册 `BCE_TEST_RUN`；正常卸载移除自有组。`caseName=PublicElevationY`。加载 ARX 同基名 `.ini`，继续公共 NativeEvidence 的严格 UTF16LE 七键契约：nonce/session/scheme/reference/output/caseName/expectedModuleSHA256。原有模块SHA、INI稳定性、路径/重解析点、session子路径、隔离桌面与输入SHA门禁原样复用，没有增加猜测的变量或白名单业务属性。

绑定方案SHA `595CE82FBD20C81E83A857AB569385BEFA98ADAF743300A42668E280561559C1`、参考SHA `109BDCFEA4C55219793F2B6F9D8C8B619344B51A4C73E9129378C38532B2AF4D`。参考只读SideDB固定根10838，必须实际为非代理、model owner、正确数据库的 `TDbSymbElevation`，必须有且仅有一个可解析数值的真实 `AcDbText`，初值真实文字精确为 `2.800`。不自行声明或cast猜测的TDbElevation类型；不换另一个样本凑结果。

N3独立复制N1最终稳定的有界公开读取器及两个策略头，原文件未改。读取使用真实标准 Text/MText getter、公共getGripPoints/getStretchPoints、公开explode、原生边界及样式定义；每次所有权/意外getter/C++错误经统一 snapshot 门禁拒绝。已知空 TDbMText 公开表示、bounds30/explode3等限制保留原始状态，不能填零或把省略节点当完整字形。瞬态只在明确detached且无重复/借用时回收；库内对象按open/close管理。每个独立库的workingDatabase使用RAII恢复。

每个分支均重新从同一只读源 `wblockCloneObjects` 到全新数据库，不在已变动clone上重试。核对主映射、映射全集、目标数据库、根类/owner/nonproxy及源→clone完整可观察基线相等。索引来自该次真实返回数组，保留同坐标的不同索引，不猜语义也不去重。分支为control无操作、transformBy沿Y+123，以及每个真实grip单独moveGripPointsAt沿Y+123。按现有历史5grip预计7分支，但本候选实际数量须待运行读回，不能提前写7项成功。

每分支记录实际操作返回状态、全部前后夹点/stretch/有界原生展开节点/文字/文字位置/样式/边界；数值只从真实AcDbText::textString读取，用classic locale严格解析实际字符串，记录semanticNodeIndex、原文、值、numericDelta及全部其他标准文字。没有修改瞬态文字、根文本字段、私有字段或EntMod；没有天正业务命令/面板、COM/CUA、valueName或动态系统变量猜测。dynamicAnnotationState始终为unknown且未切换。

control要求无操作前后可观察状态保持；其他分支不把数值保持当基础设施故障，也不把eOk当业务值setter成功。非eOk原样记录为不支持/错误的操作结果，同时仍尝试只读观测和保存复读，独立列operationReturnedEOk。基础设施结论仅指clone、观测、保存复读与源保护；任何异常和所有权硬错误顶层failed。

每分支保存一个从未存在的新DWG，独立新库重开，以保存目标句柄定位（不使用源句柄）；核对真实根类型/owner/nonproxy、全部可观察内容几何样式及真实数字一致，保存库model根必须为1。每个主要步骤前checkpoint flush；前后重读两源模型公开指纹、源10838严格快照和两文件SHA。健康最终读取不能清除已发生的异常。没有改写源或冻结图。

## 输出与结果读取

公共 `native-case.json` 保留共同身份字段，执行期间独占；同时创建独占的 `level-case.json` 和每分支 `level-<name>.json`，先写INCOMPLETE/failed，再在持有同一文件句柄时提交完整结果、flush后释放。顶层失败不输出成功状态。输出目录和文件均不覆盖。最终成功完成调查也最多为partial；完整私有几何和业务setter明确未证。

预计分支名control、transform-y123、grip-0-y123等；各有同名新DWG。详细JSON中须看actualText/value/numericDelta、全部文本、根grip/geometry及reopenComparison，不能只看return code。公共 NativeEvidence固定limitations仍含N1的“13段缺样本”文字，这是原样共享框架的历史局限字段，不是N3额外执行尺寸探针的声明；N3具体范围以level-case为准。

| 项目 | 本候选当前证据 | 能力边界 |
|---|---|---|
| 构建/纯策略 | exit0、20checks通过 | 未证明加载/宿主执行 |
| 源10838真实数字读取 | 代码有严格验证，待原生 | 历史2.800不是本候选实测 |
| Y变换/实际grip Y123 | 已实现隔离对照，待原生 | 不预称某索引为数值夹点 |
| 新DWG保存独立重开 | 已实现，待原生 | 不用编译成功替代读回 |
| 数值是否变化 | 待root返回真实JSON | 未知动态模式，不扩大到所有状态 |
| 固定位置任意标高赋值 | 未证明 | 即便数值变化也需单位/零点/位置分离验证 |

root在新隔离宿主串行执行后，再补实际运行目录、加载模块SHA、PID/nonce、真实分支数量与数值、保存重开、源保护、正常卸载和宿主清理证据。若观察到显示值变化，仍须另行设计不同位移量/反向/位置恢复、单位与基准验证；当前候选不立即接入产品。
# Root实际运行补记

2026-11-01新隔离宿主11380/b6fc8841实际加载83899657…CCA8候选，7个fresh-clone实验（control、整体Y+123、实际grip0至4各Y+123）均完成公开观测、测试专用DWG保存和独立重开。每项operationStatus=0、infrastructureStatus=passed；实际AcDbText在操作前、后及重开均为2.800，numericDelta=0。因此本对象在本次未知动态模式下没有通过该公开Y操作得到数值修改；不能外推所有天正模式，也不能宣称固定位置任意赋值已实现。

原生顶层partial、errors空，公开快照仍非完整私有几何。证据 `../evidence/native-level-y-b6fc8841.json` 绑定原始level-case.json、模块/nonce/PID、7输出及closure；原始会话 `阶段1/runs/elevation-native-20261101-183442-b6fc8841/`。已精确停70、清4暂存、6输入保持、无超时；QUIT被自有文档DBMOD拦截，不是正常退出。一次独立静态任务复核为No findings，20策略检查实际通过；复核没有读取本次宿主结果，以上为root运行核对。

