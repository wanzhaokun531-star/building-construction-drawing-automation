# R1 fix round1：4项P2的作者修复与证据

2026-11-01。范围为R1 scoped复核提出的四项P2；基线是 `build/f1-20261101-172847-104-r1-finite-green-final-8e89a51a/` 冻结输入。本轮未运行CAD、未派子代理，未修改N1/native、阶段一、平面模块或fixture。当前为作者修复完成、等待scoped复核的候选，不将作者测试结果称为No findings。

## 四项修复

1. **有限bounds派生无穷切分边界导致不收敛递归。** 实际反例为两个带标题的分区，X分别为-1e308和+1e308。旧代码的span和gap可溢出为inf，计算inf边界后全部成员仍进入left，直到C00000FD栈溢出。现在检查派生span、width、score和boundary均有限，boundary还必须严格位于空白带内部；递归前独立验证两侧非空且都严格小于原集合。无法满足时整组保留为可定位歧义，不硬分。标题局部坐标的派生bounds也先检查有限性，不将非法占位交给切分。
2. **大方向向量让楼层几何出现NaN并漏报冲突。** 新增按方向最大绝对分量缩放后再归一化的纯函数，避免直接求 `hypot(DBL_MAX, DBL_MAX)` 溢出。LevelAssociation的点差、投影及毫米换算逐步检查有限性。方向为(DBL_MAX,DBL_MAX)、上下差(0,3000)时，稳定得到2121.3203435596426 mm，与文本3900 mm明确冲突。点差±1e308无法表示时geometryMm保持未读取，声明值、三个来源及定位诊断保持，不写NaN/inf或伪零。
3. **有限轴站位相减为inf仍返回可接受轴链。** 接受任何spacing前检查有限且正值；站位-1e308/+1e308得到非有限间距时返回无value的AMBIGUOUS_FEATURE，canCommit=false。轴方向同样使用稳定归一化，正常反向4/3/2/1及3000/4000/3000原断言保持。
4. **Text反射只改rotation/正height而丢手性。** Primitive2增加默认false的textMirroredX/Y，并定义文字基矩阵为 `R(rotation) * diag(signX,signY)`。相似变换下，以变换后的未反转X基确定新rotation，保持已有X符号，并按det符号累计Y镜像；这使X、Y两个实际文字方向都保持，支持已有镜像、两次反射及反射后嵌套90度旋转。文本非相似变换不能由当前字段表达时明确INVALID_TRANSFORM；缺少文字元数据不能绕过此拒绝，缺旋转的反射也不会猜0。两镜像位都进入规范几何签名，签名版本升级bce-geometry-v2；旧v1签名不能冒充含镜像信息的有效确认。真实fixture文字无镜像，保持默认false，未修改夹具补假数据。

## 实际RED→逐项GREEN

所有路径相对 `D:/Codex/建筑施工图自动化/立面插件/`，各目录都有冻结input、build.log、tests-all.log和receipt.json。各次使用相同命令形态，只替换Label：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File '立面插件/tools/build.ps1' -Target CoreTests -Label <label>
```

| Label | build目录 | 实际输出/退出 |
|---|---|---|
| r1-fix-round1-red | `f1-20261101-173650-731-r1-fix-round1-red-b631f744` | 编译0警告/错误；原25项PASS；楼层、轴间距、Text镜像三项断言FAIL；最后空白带反例C00000FD，testExit=-1073741571 |
| r1-fix-round1-gap | `f1-20261101-173727-823-r1-fix-round1-gap-85e0ba42` | `PASS OverflowingGapDoesNotRecurse`；`Executed 29, failed 3`，退出1 |
| r1-fix-round1-level | `f1-20261101-173759-641-r1-fix-round1-level-397f1a55` | 楼层与空白带反例PASS；`Executed 29, failed 2`，退出1 |
| r1-fix-round1-axis | `f1-20261101-173817-036-r1-fix-round1-axis-84b48431` | 楼层、轴间距与空白带PASS；仅Text镜像FAIL；`Executed 29, failed 1` |
| r1-fix-round1-green | `f1-20261101-173852-117-r1-fix-round1-green-12cc9da9` | Text两基方向、已有镜像、嵌套反射/旋转及Y镜像签名PASS；`Executed 29, failed 0` |
| r1-fix-round1-text-missing-red | `f1-20261101-174000-482-r1-fix-round1-text-missing-red-5ce041da` | 补X镜像签名及缺元数据反例；缺rotation/height时旧路径绕过非相似拒绝，`MirroredTextPreservesBothBasisDirections`实际FAIL，29项失败1 |
| r1-fix-round1-final | `f1-20261101-174017-436-r1-fix-round1-final-9a0485e9` | 最终v141 14.16.27023/C++17 /W4 /WX，0警告/错误；`Executed 29, failed 0`，退出0；未知case退出2 |

初次RED另从该冻结EXE逐条运行，补充日志为 `red-case-<case>.log` 与 `red-individual-cases.json`：

```powershell
& '<173650构建目录>/out/ElevationCoreTests.exe' --case HugeLevelDirectionKeepsFiniteCandidate
& '<173650构建目录>/out/ElevationCoreTests.exe' --case OverflowingAxisSpacingIsRejected
& '<173650构建目录>/out/ElevationCoreTests.exe' --case MirroredTextPreservesBothBasisDirections
& '<173650构建目录>/out/ElevationCoreTests.exe' --case OverflowingGapDoesNotRecurse
```

实际退出依次为1、1、1、-1073741571。gap反例的旧代码属于进程级栈溢出，不伪称常规catch断言失败；用例将Windows错误弹框关闭，未操作其他进程。逐条日志在初次receipt生成后补录，不伪称包含于旧receipt.outputs。

四个新case：OverflowingGapDoesNotRecurse、HugeLevelDirectionKeepsFiniteCandidate、OverflowingAxisSpacingIsRejected、MirroredTextPreservesBothBasisDirections。原25项保留，包括原15项F1、真实741条逐成员TSV真值、四立面两剖面、空间平移/转向、完整剖面保留及真实轴反序；没有降低原窗口候选角色或视图归属断言。

## 相对172847的冻结增量

最终构建目录：`build/f1-20261101-174017-436-r1-fix-round1-final-9a0485e9/`。

`fix-round1-vs-172847.diff` 由Python difflib逐文件比较两个冻结input生成，仅四个文件变化；没有从当前工作目录拼一个不同版本冒充增量。最终14个冻结输入SHA又与当前文件逐项核对，无不匹配；fixture和其manifest/expected TSV均未改变。

| 文件 | 172847 SHA256 | fix round1 SHA256 |
|---|---|---|
| src/ElevationModel.h | AC353AB736DD3016BC3FC327E467E6303842AC85D8D649CD2F81698924613F12 | F96148EFDA1813CE1457267FD0A3A3DAB9CEAC00CE962AF9405763123D3D2374 |
| src/Recognition.cpp | A54A78335F5F4DC2B2F5C0436D0039A2B050DEA80381146C7F430AF650B59407 | 1067FE1F95A24836B1FA4A3E5CEF1EAE7A725DC97F4BDA3D45FCFF4DDC379BC1 |
| tests/CoreTests.cpp | E3FB02DAC5CE431921794F2071C9CD63E8111DDD8692B3A403B1D55CF9DD0BBB | EAB0B8E613123263C822ADC7861B163FA58A01617C5B17121A732B0553933D70 |
| tests/RecognitionTests.cpp | CEDDC19D1DE11EB3C42979D1AA04F9B1BA7BD0B5E72A8595C16DCD7EA8579375 | 41EB3B418995C5EC0371F0E754C4FEF6DCB30E3E80B6DADB028F4361B8B98085 |

基线receipt SHA256：`EA41F2DAE177D26C4CA5F4E1AE4620C67431F70FD0D8C54D65ACED88959CC14A`。

最终receipt SHA256：`FA7C5B454AC9F624BC153BAFEB5D6342769CC6DB71B981C440E1C569570F12F7`。

增量diff SHA256：`1ED57FFAECB47C5D726E33073590624F548ECEE350FB5CF0F310E776AF8D15BB`。

补充审计 `fix-round1-audit.json` SHA256：`3071BEB51EB9437B06FDFAAC5A0D2D2001AAA5D5EE073007CBDF71BFF7139DA3`。它绑定baseline/final receipt、diff、分析JSON、最终14个输入及退出码。此audit、diff和异cwd补充日志在最终receipt后生成，audit中明确标记，未修改旧receipt来追补输出。

## 同一analyse结果保持

最终 `out/recognition-analysis.json` 仍来自真实fixture测试中**用于全部断言的同一次analyse返回值**。源DWG SHA仍 `595CE82FBD20C81E83A857AB569385BEFA98ADAF743300A42668E280561559C1`；四立面两剖面成员数159/139/133/136/65/109，741唯一成员，174剖面成员保留，12楼层候选，PLAN_REQUIRED保持。未从期望TSV驱动生产分类。

分析JSON SHA256仍为 `45CB6EF230143278479176DA9F982612D1AE2C7DFE816300DEC3F188782C3ECD`，与172847逐字节相同；此JSON没有输出内部几何签名字串，因此签名升级不改变这个分析文件的内容。镜像签名变化由专门行为断言覆盖。

从 `C:/Windows/Temp` 再运行最终冻结EXE的 `--case FourElevationsExcludeTwoSections`，`Executed 1, failed 0`；分析JSON SHA前后相同，证明fixture加载仍独立于cwd。见 `tests-unrelated-cwd.log` 与补充audit的unrelatedCwdCaseExit=0。

结论只限上述四项修复及纯值测试。实际SDK SceneReader、完整门窗语义、原生轴网关联和DWG写入仍不在本轮验证范围；当前候选等待scoped复核。
