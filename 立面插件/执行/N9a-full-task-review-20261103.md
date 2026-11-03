# N9a 完整任务审查（2026-11-03）

## Spec compliance verdict

**✅ 在批准的“有限验证探针，可交付可信失败结论”范围内符合规格。** 已实现独立 SHX/Bigfont 原生测量与真实墨迹比较，5/5 范围计算、两档全部10项完整像素包含失败、4/4真实负例拒绝均有实际证据。没有发现需要阻止本探针结论交付的 P0/P1/P2 缺陷。此结论不授予文字避让、Stage6、生产当前图写入或撤销能力资格；backward basis 与设备像素占位仍未解决。

**⚠️ 证据/能力边界：** 本审查没有重新运行编译、原生宿主或离线整套测试，没有自行看144/288图面；图面检查复用 root 的独立144dpi记录，288dpi仅核数值。SideDB真实context、upsideDown、外层镜像、未知clip、完整MText、缺字/替代与原TCH完整glyph均未获本轮实证，不属于本探针已通过域。字体SHA错误、空图/碰边、未知basis的拒绝路径可从代码判定存在，但所提供运行记录没有这些条件逐项真实故障注入结果；本报告不把静态拒绝检查写成已执行负例。

## Quality verdict

**Approved with minor documentation correction（可接受，1项P3说明修正）。** 原生状态门、拥有权与对象生命周期、绘图清理、输入绑定和严格像素比较足以支持本次有限失败结论。没有额外实现缺陷需要修复后重跑本矩阵。下述P3不会改变原生/离线数值。

## Findings

### [P3] 将独立校准点描述改为实际的两个 held-out 点

位置：`D:/Codex/建筑施工图自动化/立面插件/tests/native/text-range/README.md:5`。

README写“two corner and one interior checks beyond the minimal affine basis”，读者会理解为最小三点拟合之外另有两个角点和一个内部点，共三项独立校核。实际v8仅五点：索引0/1/2拟合，索引3为一个角点、索引4为一个内部点，共两项独立校核（`tools/verify_text_range.py:167`、`:179`，两个measured comparison.json也均为independentChecks=2）。这是验证强度说明偏差。建议将该句精确改为：“A separate five-line calibration page supports the nominal mapping check; measured-PDF mode fits the first three noncollinear centers and checks one held-out corner and one held-out interior point.” 不需要为这项说明修改重编译或重跑CAD。

无其他可操作finding。没有把已诚实保留的backward错侧或边缘完整像素失败另列为“探针必须先全部包含”的缺陷；实现若进入后续避让功能，二者仍是必须解决的能力缺口。

## 审查输入与覆盖

- 先读实施简报`执行/N9a-受控文字范围验证待办.md`、实施报告和`tests/native/text-range/review-fulltask-v7-v8-20261103/review-context.md`。
- 按review-agent、ObjectARX2020/C++与SDD单任务审查方法，一人分段读完114128字节`full-task-from-empty.diff`全部12项新增文件。没有重读任一changed源码全文，没有git命令、编译、CAD/COM/CUA操作、派子代理或第二评审。唯一写入为本报告。
- 全文覆盖：TextRangeDriver.cpp、TextRangePlot.inc、TextRangeReplayPolicy.h、TextRangeReplayPolicyTests.cpp、OwnedTextControlFile.h/.cpp、ElevationTextRangeCases.vcxproj、README、build_text_range.ps1、capture_text_range_case.py、verify_text_range.py、N9a报告。
- 原生最终编译/运行版本为v7，ARX SHA256=`51195EEE7C025A06E87CF47C683AF532CA6C4CDEB21232700ED57EF4B85FAE80`；实际离线为v8。v8 Python没有被冒称为原生二进制的一部分。复用root已独立完成的完整性核对：209个唯一path+sha、额外binary/diff、12源live/frozen共24文件全部匹配；索引SHA256=`8C0D77853753A3662F902A2ECE523C921E34F1E25687C85664718AC525D87B02`。未重复整批hash。

## 关键规格与质量核对

### 原生目标、范围与生命周期

- 工程显式v141/x64/C++17和2020 SDK依赖（`ElevationTextRangeCases.vcxproj:6`、`:8`、`:10`、`:11`）；build脚本冻结来源、回执、命令和SHA后构建，未引入运行期Python业务、COM或TCH业务面板。Python仅用于测试调度/离线比较。
- 公开命令注册为BCE_TEST_RUN和BCE_TEXT_RANGE_CAPTURE（`TextRangeDriver.cpp:260`）。精确健康pending replay比较完整Config、PID/thread、phase、current/active/ambient/working、原文档销毁状态、ready/target SHA和字节数（driver`:183`起、ReplayPolicy`:14`起）；健康重入不重新准备。
- 独立打开/idle gate只在精确自有控制路径、current=active=working且quiescent/inputPending=0时排队（driver`:82`起、`:108`起）。capture再次检查门及identity，使用固定自有目录的共享只读稳定哈希，添加一个自有点且不保存DWG（driver`:205`起；OwnedTextControlFile.cpp`:24`起）。这没有冒称用户图零写或生产修改资格。
- overrule仅适用于marker objectId与target database匹配的点（driver`:119`）；WorldDraw要求真实context.database=target（`:123`）。临时rule移除失败保留对象，unload拒绝释放带回调对象的Session（`:175`、`:261`）。成功、异常和卸载未完成均有Evidence终态/释放路径（`:238`起）；没有发现本次路径中的悬空rule或数据库对象delete误用。
- Plot采用独立临时AcDbPlotSettings复制/override，不写Layout（TextRangePlot.inc`:71`起）。pipeline对成功begin逐层end/destroy、失败也尝试清理（`:21`起）；BACKGROUNDPLOT RAII恢复且成功路径核回读（`:48`起，driver`:225`）。未把自有文档的1个内存点写入说成ambient完全只读。

### 字体、范围与真实变换

- 五个固定控制覆盖数字、中文、旋转backward、两级正行列式非均匀变换、tracking=1.6/oblique=0.25（driver`:54`起）。明确排除未测upsideDown和外层镜像。
- style用解析后的绝对font/bigfont路径，记录请求名、加载前后名、完整flags、路径、SHA，缺失/替代位检查和SHA复核发生在绘制前（driver`:129`起）。实际五项loadStyleRecBits均1；绝对simplex/gbcbig身份与报告一致。这里只绑定控制字体，没有声称原TCH缓存身份。
- 使用同次真实WorldDraw调用extentsBox，记录原始min/max和四角，按position/direction/normal构造局部框，取得净modelToWorld后仅乘一次（driver`:146`至`:162`）。原生结果保留backward正X框与实际负X墨迹的不一致；没有静默加镜像、字高或宽因子。
- 离线重新从原始min/max、basis、position和net核对四角（verify`:242`起至`:263`）；未将框线加入原生文字墨迹。原始text-only页和overlay是分别生成的文件（verify`:37`、`:186`起）。

### 投影、像素与负例

- v8读取实际MediaBox、CropBox/Rotate和cm，拒绝本受控子集以外的明显basis/多cm变化；要求六页frame和validated settings相同（verify`:121`、`:148`）。这是固定Plot输出子集检查，不能视为通用PDF解析器或证明任意未知PDF语法都被拒绝。
- 校准前三非共线中心fit，另外一角点/一内部点held out，不把三点必然零残差当独立证据（verify`:167`起、`:179`）。actual MediaBox=[0,0,595,842]、cm=[0.12,0,0,0.12,0,0]与实测报告相同。
- full_cells逐边测试全部四个像素单元角，按winding处理，数值容差1e-8px，margin=0（verify`:65`起）。所有RGB<255含AA均提取；空图和设备边缘拒绝（`:37`起）。每个越界像素及其反投影模型四角全部写CSV.gz，overlay始终是独立产物（`:186`起）。
- shrink只缩同一范围、omit-net只移除该控制的净变换，都复用同一真实墨迹和同一full_cells比较（verify`:186`起、`:295`起）。实际shrink越界782/880与1519/1720，远大于原框160/880与258/1720的边缘失败；omit-net越界456/456与895/895，原框仅25/456与36/895。负例确实切去可见墨迹，并非仅依赖原候选本就有少量AA边缘越界而过关。
- 绑定检查涵盖build receipt、冻结源、六输入、nonce/PID/module、控制文件、五场景ID、字体身份、UTF16和原始四角（verify`:222`起）。实际运行记录明确区分v7原生和v8离线receipt/sourceSHA；比较结果带nativeReport、comparator和rasterizer SHA。

## 核对的实际证据

实际case根：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-093743-84f838a5/case/`。

| 证据 | 本次审查所确认的事实 |
| --- | --- |
| native-case.json / text-range.json | status=partial、errors=[]，prepare与controlledNativePlot passed；6页pipeline各9项状态全0；5范围、真实context及font记录齐全 |
| text-range-gate.jsonl | 自然prepareCalls=2、captureCalls=1；prepare.replayAccepted/newPreparationCreated=false；polyline false×5、text false×5，push true×2、pop true×2 |
| prepare-policy-controls及构建日志 | 10项是复制facts的纯策略控制；未把它们写成10次live command或真实线程/文档故障注入 |
| text-range-negative/comparison.json、text-range-public/comparison.json | 原名义映射在288dpi residual[-0.657480314960651,1.0984251968502576]终止；144dpi局部结果保留，未声称旧negative整轮完成 |
| text-range-negative-measured-v8/comparison.json | 两档4项真实负例拒绝，error=null |
| text-range-public-measured-v8/comparison.json | 两档10项均false，error=null，0/10完整pixel包含；glyphComplete/upgrade均false |
| offline-projection-v8/execution.json | 原名义两轮exit1；v8 negative exit0/public exit1；negative先于public；同六原PDF，未重跑native |
| native-result-closure.json / reader-retirement.json / isolation-final.json | sourceSHAsUnchanged；六输入检查记录保持；精确退役4模块/INI；workerExit70、forcedTermination=true、nativeQuitConfirmed=false；自有desktop SHOW/foreground为0（retirement字段ownedUserShows/ownedForeground）；没有把精确停止写成正常QUIT |

v8独立held-out残差：[约0,0]pt与[0.01920000000004,0.03257142857137]pt，阈值0.06pt；没有把该校准阈值加为范围margin。144/288图像校准仍采用原1px检查。旋转backward全部485/485、957/957越界；其余场景存在0.08–0.98px级完整像素边缘越界，不能据此单独推断无限分辨率向量字形必然越框。

## 具名外部定向检查与测试限制

唯一外部技术合同检查针对“AcGi不同方法Boolean语义可能被错误统一”的具体风险：读取已保留的2020 SDK四条官方HTML（sdk-public-contracts目录的polyline、text(style)、pushModelTransform、popModelTransform）。polyline/text明确false表示已存入图形，push/pop明确true成功；与driver各自判断及v7 gate原始返回一致。没有为该风险爬取其它源码、检查调用者全库或查新版SDK。

没有运行新测试：已有原始执行和代码足以回答本轮具体疑点。构建日志保留1项外部SDK rxapi.pdb缺失的LNK4099和0错误；它影响该SDK对象调试符号，不是已发现的探针行为缺陷，也没有被写成“零警告”。

已复用root对索引/源/证据完整性的独立hash检查及实际144dpi图面记录。未再次视觉复查288图、未重新验证backward诊断图；该诊断始终非采纳修正，不影响本审查采用的正式public失败结论。其它CAD related事件归属仍由controller单列，不以本探针证据替未知CAD活动作归因。
