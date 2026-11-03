# N9a 受控文字范围验证报告

日期：2026-11-03。作者状态：**DONE_WITH_CONCERNS**。测量/比较探针已实现，真实原生墨迹负例 4/4 被同一比较器拒绝；本矩阵 5 场景×2 分辨率的原公开字符串框全部未取得完整墨迹像素单元包含资格。`stringRangeComputed=true`（5/5），`renderContainmentVerified=false`（0/10），`glyphComplete=false`、`capabilityUpgradeAllowed=false`。这不是文字避让功能完成，也不是所有公开 API 无法实现的结论。

唯一源码作者从未启动或控制 CAD；独立 controller 已完成 v3/v5/v6/v7 各自新宿主执行与精确退役。最终原生 v7 完整输出及 v8 离线映射修正均已闭合；未重启 CAD 验证 v8。root 已实际查看五类144dpi原墨迹、旧叠加和最终measured叠加；288dpi核数值，未声称另做视觉审查。正式一次完整 task review 由 root 安排，作者不自行派审。

## 最终实测矩阵（严格完整像素单元，margin=0）

| 场景 | 144dpi 越界/墨迹像素 | 144dpi 最大越界px | 288dpi 越界/墨迹像素 | 288dpi 最大越界px | 完整包含 |
|---|---:|---:|---:|---:|---|
| digits | 160/880 | 0.832000 | 258/1720 | 0.664000 | false / false |
| chinese | 7/503 | 0.538929 | 4/992 | 0.161555 | false / false |
| rotated-backward | 485/485 | 162.673344 | 957/957 | 324.002191 | false / false |
| nested-nonuniform | 25/456 | 0.980752 | 36/895 | 0.799612 | false / false |
| tracking-oblique | 34/615 | 0.080777 | 56/1213 | 0.161555 | false / false |

所有原墨迹图非空且不碰设备边缘。每一个越界像素及完整单元的模型四角均留在各场景 CSV.gz，未截断采样、未改原 PDF、未缩字或跳项。表中的比较使用同次实际 PDF 校准映射；它只是有限光栅/有限校准实测，不能推断无限分辨率数学包含或向量字形一定越框。

| 真实负例 | 144dpi 越界/墨迹 | 288dpi 越界/墨迹 | 结果 |
|---|---:|---:|---|
| 原数字框以中心缩至35% | 782/880 | 1519/1720 | 两档实际拒绝、保留定位 |
| 漏非均匀净 modelToWorld | 456/456 | 895/895 | 两档实际拒绝、保留定位 |

校准先从实际无文字 PDF 的五条独立线取得原始端点，前三个非共线中心 fit，另外角点和内部点 held out；独立几何最大残差 0.0325714285714pt，小于实际 PDF 坐标单位半步 0.06pt，仅作此页独立校准检验，绝不加到范围。两档对五点的光栅中心检查最大残差分别 0.445142857143px / 0.390285714286px；数值舍入规则仍为原1px校准像素检查，不曾调宽失败阈值。fit三个点的零残差单独记，不计作独立证明。

## 支持边界与最小后续修正

已计算范围的输入仅为本次固定五个UTF-16单行字符串：数字1234567890、中文测试、旋转backward的123ABC、两级正行列式非均匀净变换的1200中文，以及tracking=1.6/oblique=0.25的12中文ABC。字高7、宽因子0.75，normal=+Z、direction按记录，raw=true/penups=true，公开extents原始min/max及全部四角/净矩阵保留。当前**保守完整像素包含的已通过域为空**；不能以“已测试”写成“已支持保守避让”。

实际 simplex 为 `C:/users/pc/appdata/roaming/autodesk/autocad 2020/r23.1/chs/support/simplex.shx`，SHA256=`30DC50FA379935E4D7EB6E60041FA070A11FBA6DEAC337A2FA9E34DCB5BFBCC1`；bigfont 为 `D:/cad2020/autocad 2020/fonts/gbcbig.shx`，SHA256=`B6D7F2C5E3057840A63433EE78A49D59FBD030C999D706F7368A14D52B2A4150`。五项 loadStyleRecBits 均1，前后绝对名保持，无替代位；这些是控制配置真实身份，不是原TCH已加载缓存身份。

旋转backward实际范围仍为min≈[0,0]/max=[30,7]，真实墨迹在插入点的负文字X方向，原四角框在正X方向，全部墨迹像素越界。这是当前basis/显示方向的实质不一致，远大于投影残差。2020 extentsBox合同仅给字符串框，setBackward/isBackward规定显示方向，未在所读条目中明确保证返回框已包含显示反向。不能凭N9的预期认定已应用或机械再次镜像。最小后续为显式定义/检验返回range basis与显示方向的关系，保留原框，单独验证需应用一次的backward局部X变换；不是隐式补镜像通过。

只读离线诊断将该唯一backward控制的局部X反射一次，复用原墨迹/同严格比较器，粗大方向失配消失但完整单元仍不包含：144dpi 95/485越界、max1.436901px；288dpi161/957、max1.576601px。诊断目录 `case/text-range-backward-basis-diagnostic-v8/` 明确 adoptedCorrection=false；没有改 native记录、原始范围或正式 public 结果，不能称修正已通过。

其余四类最大越界约0.08–0.98px，需区分PDF坐标量化、0宽发丝线、抗锯齿完整像素足迹及有限校准残差；不能用光栅单元越界直接声称数学向量字形必然越框。最小后续应为明确设备/分辨率/相位/线宽/AA条件的保守像素占位合同，或原生墨迹范围策略；不得任意加margin调到GREEN。2020官方示例本身扩大框但没有提供通用保守误差界，本任务没有复制其任意扩量。

upsideDown、外层反射、缺字/替代/TTF、竖写/装饰、完整MText、未知clip/注释上下文、原TCH完整字形和SideDB真实context均不在本轮资格内；没有用这些unsupported标签掩盖本五项检查器或包含失败。生产Layout门禁保持。


## 范围与实现

仅读取 N9a 待办、N9 公开接口调查和当前实现所需的 NativeEvidence、graphics-read/GiCapture、graphics-viewport 控制/公开转发、NativePlotProbe 及工具。使用本机 AutoCAD2020/R23.1 SDK、v141 14.16.27023、x64、C++17。应用 autocad2020-objectarx、cpp-coding-standards 和 TDD 技能；本次以真实渲染对照/故意错误候选作为有效 RED，编译失败和空图均不算 RED。没有重跑旧 N7 78 项或 N8a 3153 字段，没有改 src、生产 Layout、NativeEvidence、公共宿主桥或旧 N7，没有 Git 提交/切分支，没有派子代理或审核。

新增 `tests/native/text-range/`、`tools/build_text_range.ps1`、`tools/capture_text_range_case.py`、`tools/verify_text_range.py`。独立 ARX 的 `BCE_TEST_RUN` 仅生成自有空控制图并安装成功 OPEN＋idle 门；controller 用未修改 S1 bridge 打开精确 `session/text-range-native-job/control.dwg` 后，ARX 只在该文档排队 `BCE_TEXT_RANGE_CAPTURE`。捕获向这份自有控制文档新增 1 个 AcDbPoint；承认内存文档已修改，不冒称 ambient 零写或取得用户图写入资格；没有 DWG 保存。

局部公开 DrawableOverrule 只接受该精确持久点及控制数据库；真实 WorldDraw 的 `context()->database()` 必须等于控制数据库。公开 `geometry.text` 发出实际单行文字，传入同次真实 WorldDraw 到 `extentsBox`，记录原始 min/max，按原四角和公开 normal/direction 定位后一次使用净 modelToWorld；不重复字高/宽因子/镜像。嵌套控制通过两次实际 pushModelTransform 取得净矩阵。overrule 配对移除并恢复原全局状态；移除失败保留对象并拒绝卸载，避免悬空回调。

控制覆盖数字、中文、旋转 backward、正行列式非均匀嵌套、tracking=1.6 且 oblique=0.25。显式解析 simplex.shx/gbcbig.shx 为绝对路径并绑定 SHA，记录 loadStyleRec 完整位掩码及加载前后名称；原始日志先于缺失/替代拒绝落盘。这只是控制实际文件身份，不代表原 TCH 字体缓存身份。

复用 NativePlotProbe 的公开 AcPl publish begin/end/destroy 管线；临时未登记 PlotSettings override，不改实际 Layout、PC3/PMP。固定 1:1 mm、A4 full bleed portrait、rotation=0、window=[0,0,150,100]、origin=[20,20]，记录实际 validated settings。临时 BACKGROUNDPLOT=0 配对恢复。输出独立 projection.pdf 和 5 份无框文字 PDF；校准页五条独立已知模型线，比较公开 settings 预测投影的五个实际位置，不用三个点拟合后的必然零残差自证。

离线固定 Poppler 144/288 dpi、白背景、任何 RGB<255（包括抗锯齿覆盖）为墨迹；像素单元为 [x,x+1]×[y,y+1]，相位=设备左上零相位、y向下。全部四角都须位于投影范围半平面；仅 1e-8 像素数值比较公差，无几何 margin。全量越界像素及每个单元的模型四角保存 CSV.gz，另输出叠加图；叠加从不作为墨迹提取输入。两档有限光栅不构成无限分辨率数学证明。

计划先 `--mode negative` 用同次真实墨迹验证 35% 缩框及漏非均匀净变换必拒绝；再 `--mode public` 检查原候选。任何真实范围越界原样保留，分析公开合同后向 root 提最小修正，不任意加 margin 或静默跳项。`glyphComplete=false`、`capabilityUpgradeAllowed=false` 始终不变。upsideDown、外层反射、缺字/替代/TTF、竖写装饰、完整 MText、未知 clip/注释上下文和原 TCH 完整字形都不属于支持域；SideDB 实际 context 后续另排。

## 已执行命令与结果

1. 首次 `tools/build_text_range.ps1 -Label capture-v1`：receipt `build/n9a-20261103-091525-876-capture-v1-34a2ed8b/receipt.json`，exit=1。实际日志保存 build.log：setLayout 要求非 const AcDbObjectId 引用；2020 VPORT setter 是 setTarget，非 setViewTarget。两错误已按实际头文件修正，未作为有效 RED。
2. `tools/build_text_range.ps1 -Label capture-v2`：receipt `build/n9a-20261103-091601-555-capture-v2-a0c74e9d/receipt.json`，exit=0、0 编译错误、1 个 SDK `rxapi.pdb` 缺少的 LNK4099。已保留此冻结候选，没有原生执行。
3. 以配置 bundled Python 对 capture/verify 两文件 AST 解析：`2 Python AST checks passed`。配置 runtime 由 load_workspace_dependencies 只读取得；系统 Python313 无 Pillow/numpy，bundled Python 有 Pillow12.3.0/numpy2.3.5。比较器复用已存在 bundled Poppler，不安装新包。
4. root 指出冻结 src 的工具相对根不应沿用 live tools 的 parents[2]；已改为向上定位同时含立面插件及阶段1/原生接口探测的现有项目根。自检同时修正 prepare 早期配置异常时 Evidence 尚空的保护，新增 load flags 失败前原始记录。将在 v3 冻结后交 controller。
5. `tools/build_text_range.ps1 -Label capture-v3`：receipt `build/n9a-20261103-091806-833-capture-v3-3bd66f42/receipt.json`，exit=0；包含上述异常保护、失败前 load flags 日志和正确冻结工具根定位。bundled Python `runpy.run_path` 在冻结 src 导入 capture/verify，各实际 ROOT 都为 `D:/Codex/建筑施工图自动化`。此冻结候选提交 controller，原生尚待执行。
6. 从 v3 冻结比较器导入 `full_cells` 的算术 sanity：内部单元、闭边界单元、越界单元和反向 winding 均按期；不是实际原生 RED。

## v3 首个原生运行及 v4 最小诊断

controller 创建 binding `evidence/native-case-binding-0d3bb560.json`、session `阶段1/runs/elevation-native-20261103-091901-0d3bb560`、自有 PID44840。S1OPENPROBECONTROL 实际 openStatus=0、error=null；native-open.json 的 current/mdiActive 均为精确控制路径。worker-state 的同 serial 实际 commandStarted=true、commandEnded=true、failed=false、phase=idle、cmdActive=0、inputPending=0、nativePending=false；健康 heartbeat、无 modal。约70秒仍只有 prepare-ready、空target和持锁 native-case，没有 PDF 或 text-range.json；controller 安排原helper按180秒超时后依协议退役。该失败不是有效文字 RED，更不表明公开 extents/渲染不可用。

按 systematic-debugging 技能先只读对照原 N7 成功门及桥原生状态。确认 v3 缺少全部 gate/idle 诊断，因此不能区分 openEnded、ownsCurrent、isQuiescent 等条件，不能猜根因或删 `args->status()==kCmdStatusNone`。另一个可确证独立缺陷：Session 持有 NativeEvidence 的 share=0 输出句柄，在终态 finish 后漏掉原 N7 的 evidence.reset()；这解释 native-case 的读取锁，不能解释无 PDF。

root 明确同意最小 v4。只改 TextRangeDriver.cpp：追加共享只读、flush 的 gate JSONL，记录真实 globalName/group/status、documentBecameCurrent、prepareDoc 销毁事件、current/active/working/doc/database 原始地址、路径/预期/各bool及idle变化；诊断不删除/放宽门。终态释放 Evidence，unload-before-capture 写明确失败；绘图、比较器、helper、项目及 build 脚本逐字保持 v3。独立新宿主，不重放旧 prepare 或命令。

`tools/build_text_range.ps1 -Label gate-diag-v4`：receipt `build/n9a-20261103-092345-529-gate-diag-v4-e2bfe704/receipt.json`，exit=0。对两个回执按冻结输入文件名逐项比较，仅 TextRangeDriver.cpp SHA 改变；完整 scoped diff 为 `tests/native/text-range/gate-v4-v3.diff`。仍待 controller 真实 gate 证据。

controller 停后读到 v3 真实 native-case.json：`status=failed`，steps 仅 prepare passed，`errors=["already prepared"]`，没有 text-range/PDF；作者随后也只读确认同原始 JSON。controller 未重放 BCE_TEST_RUN/prepare，helper 只做 OPEN。由代码可以确证第二次 prepare 命中 `require(!session)` 后进入 catch，移除 gate；这足以解释随后捕获不执行。重复进入的来源仍未知，不归因桥或 AutoCAD。此后选择先评估原 N7 的 attested prepare replay/no-op，不能盲删重复拒绝或 reset 重做。v4尚未原生执行；建议将已知根因最小修正和入口计数合并为唯一下一轮 v5，待 root 决定。

root 裁定 v4 不单独运行，采用 v5 精确健康 pending no-op。范围仅本 N9 探针：fresh load_config/validate_identity 后比较完整 Config 身份元组；同 PID/thread；有完成的 pending 准备/Evidence/三种 hook，无error，未 OPEN started/ended，未 queued/capturing/finished/target/marker/callback/output；同原 current=active=prepareDoc，working/currentDB=ambient，原文档未销毁且路径相同；ready/空target SHA 与字节数不变。仅该精确健康状态接受，不新建准备或重做；其它身份、已 OPEN、已 capture、其它文档/线程或制品变化均拒绝。实际 prepare/capture 入口次数单独记录在共享 gate 日志；不将纯策略控制计作实际入口或自然重复再现。

新增 TextRangeReplayPolicy.h 的值语义策略及 10 个独立 C++ 控制（健康pending、身份篡改、wrong thread、已OPEN、capture完成、queued、其它current、原doc销毁、目标hash和字节数错误）。全部通过复制 facts 检验，绝不逐项改变 live Session 或重放命令。原生 prepare 也保存一份基于当次真实 facts 的同类值控制结果，与实际入口次数分开。`tools/build_text_range.ps1 -Label pending-replay-v5` 冻结回执为 `build/n9a-20261103-092644-707-pending-replay-v5-452a9268/receipt.json`，exit=0、0错误、1个相同SDK PDB warning；实际执行输出 `10 value-only replay policy controls passed; no live session mutation or command replay`。

v5 ARX SHA256=`E79F739D3650AF64AED046E3E7AEABC0BE99FFF8FF9325D0552A10466ED9B107`。与 v3 逐项回执比较：仅 TextRangeDriver.cpp 和 build_text_range.ps1 改变，新增两个 replay policy 文件；TextRangePlot.inc、verify、capture helper、vcxproj、NativeEvidence 两文件逐字相同。driver完整差异 `tests/native/text-range/pending-replay-v5-v3.diff`。v3真实运行失败作为门状态修正前证据，不是文字墨迹 RED；v5须由controller新宿主串行执行。

controller 随后创建 v5 fresh binding `evidence/native-case-binding-d97278c1.json`、session `阶段1/runs/elevation-native-20261103-092848-d97278c1` 并已隐藏启动；作者只等结果，不操作宿主。若新运行只实际 prepare 一次将如实记录，不宣称自然重复已再现。

## v5 原生事实与 v6 仅自有打开文件读取修正

v5 controller helper 实际 exit=0，native-case 实际 status=failed。真实 gate 日志记录自然 prepareCalls=2、健康 replayAccepted（newPreparationCreated=false），随后精确 OPEN started/ended、pathMatches/ownsCurrent=true、idle.queue status=0，captureCalls=1（seq56进入、57失败）。这证明精确 pending no-op 和终态释放实际有效；没有重放 prepare。重复来源仍未知。实际错误为 `SHA input open failed: .../text-range-native-job/control.dwg`，尚未 Plot、无文字 PDF，不算有效文字 RED。原 NativeEvidence 错误没有 GetLastError，v5 实际 Win32 错误码未知，不能事后宣称32。

只读定位原 N7 的 OwnedControlFile.h/.cpp 及 ViewportCapture 的前后调用，确认其已为打开的自有控制图使用独立固定路径、只读访问、共享读写的稳定哈希入口。root 同意复用此算法到本 N9 范围，新增 OwnedTextControlFile.h/.cpp，仅命名空间/函数/include名和固定 text-range-native-job 路径适配。逐字反向替换比较与原 N7 完全相同，旧 N7 不改也不重跑。

v6 完整保留：GENERIC_READ；先以 FILE_SHARE_READ 诊断并立即记录实际 GetLastError（不据猜测定因），再 FILE_SHARE_READ|FILE_SHARE_WRITE 正式读；父目录和文件非 reparse；句柄 finalPath 精确匹配固定自有控制路径；预期字节数；读前后 volume/fileIndex/bytes/creation/write/attributes 稳定；完整 SHA；不比较可因读取改变的 accessTime；跨 capture 前后身份/字节/时间保持。两处 control.dwg 检查改调用此入口，原两个 source、ready、未打开 target 仍保持 NativeEvidence 原严格读取，不改公共代码。绘图/比较器/replay策略不变。

`tools/build_text_range.ps1 -Label owned-read-v6`：receipt `build/n9a-20261103-093231-375-owned-read-v6-c9432856/receipt.json`，exit=0、10 replay值策略控制通过。ARX SHA256=`629D08E5783A53D590372E0DD96D4FB6E2D610943A63E6FBEAF8C7B7141F8BBC`。driver差异 `tests/native/text-range/owned-read-v6-v5.diff`；仅 driver/vcxproj/build脚本和新增两个自有读取文件改变，TextRangePlot.inc、verify、helper、replay两文件、NativeEvidence逐字相同。待独立controller fresh v6实际 Win32读取与Plot。

## v6 实际文件共享与原生 primitive 返回合同修正 v7

controller v6 binding=`evidence/native-case-binding-fbac8c33.json`，session=`阶段1/runs/elevation-native-20261103-093339-fbac8c33`、PID34620，helper exit=0、native closed failed。真实日志 seq7 prepareCalls=2/replayAccepted/newPreparationCreated=false；seq33 strictOpenObservation 为 success=false/error=32/share=1；seq34 GENERIC_READ/share=3 正式打开 success=true/error=0；seq37 readBytes=13149/readCalls=2/stableIdentitySizeCreationWriteTime=true/matchesPreparedSHA=true。因此这次才有实际 Win32 32 的事实，不回填成 v5 错误码。自有打开文件读取修正已实际通过，未扩重跑原 N7 算法。

随后 native errors=`["calibration polyline failed"]`，只有 projection.pdf2044字节，无文字PDF/text-range.json。作者将实际 PDF 用既有 Poppler144dpi 只读渲染，得到12个非白像素、bbox=[164,1514,176,1515]，并实际查看第一横线裁图。证据 `tests/native/text-range/v6-polyline-return-evidence/`；原 PDF SHA=`30E4F440A1C26C9E217919BCF2D24F81EFFAF60D0D476EB3848B24086F763A86`。root 也独立实际查看原页，只一条横线；不是5线校准，也不是文字墨迹 RED。

直接读取本机2020 `arxref.chm` 的 polyline(点数组)、text(style)、pushModelTransform(matrix)、popModelTransform 四条官方 HTML。合同明确 polyline/text 的 false=primitive成功存入图形、true=终止；push/pop 则 true=成功。v6 的 `require(g.polyline)` 错将成功的 false 当失败，其真实已存第一条线与合同一致。v7 针对同机制所有受影响入口修正 polyline/text；push保持true成功，pop按自己的true成功记录。逐方法记录原始 rawReturn，不建立通用 Boolean 成功包装器。官方2020条目和CHM SHA索引存 `tests/native/text-range/sdk-public-contracts/`，不拿线上其它版本替代2020合同。

`tools/build_text_range.ps1 -Label primitive-contract-v7`：receipt `build/n9a-20261103-093625-872-primitive-contract-v7-ec19072f/receipt.json`，exit=0，10 replay值控制通过；ARX SHA=`51195EEE7C025A06E87CF47C683AF532CA6C4CDEB21232700ED57EF4B85FAE80`。对 v6 冻结输入逐项 SHA比较仅TextRangeDriver.cpp改变，绘图pipeline/比较器/自有读取/replay策略及其它输入保持；差异 `tests/native/text-range/primitive-contract-v7-v6.diff`。v7 待controller fresh运行，不重用失败宿主或capture。

## controller 两阶段执行约定（只供有唯一控制权者）

以最终冻结 receipt 运行未改 `tools/native_case_host.py prepare <receipt> --case ControlledTextRange`，沿用其源码/bridge/6 输入 SHA 核验。controller 在已有私有桌面/隔离 profile 协议启动自有新宿主。等待 `case/prepare-ready.json`；case 状态应为 prepared。然后用 bundled Python 执行冻结 `src/capture_text_range_case.py <binding>`；工具仅推进已绑定宿主，不启动/停止，不削弱 S1 bridge 的命令过滤。完成应有 `case/native-case.json` status=partial 且 errors=[]、`case/text-range.json`、6 份 PDF。该状态只是原生输出具备，尚未包含验证。

源码作者取得 binding 后先用同一冻结 `src/verify_text_range.py <binding> --mode negative`，再 `--mode public`，各输出目录 create-new 防覆盖失败证据。真实失败/公差/范围结果及 root 实际看图、正式完整 review 和必要 scoped 修正将在此续记。


## v7 完整原生输出与 v8 离线投影实证

v7 binding=`evidence/native-case-binding-84f838a5.json`，session=`阶段1/runs/elevation-native-20261103-093743-84f838a5`，PID52252。controller helper exit0；native-case partial/errors=[]，prepare/controlledNativePlot passed，projection+5文字PDF及text-range.json完整。gate实际prepareCalls2/captureCalls1，replayAccepted newPreparationCreated=false；rawReturn为polyline false×5、text false×5、push true×2、pop true×2，均与2020合同相符。原control文件读取前后稳定/SHA相同；自有文档确实在内存新增1点，不保存DWG。overruling和BACKGROUNDPLOT实际恢复。

原冻结v7比较器依序执行 `--mode negative` 再 `--mode public`，两者都在288dpi校准残差[-0.657480314960651,1.0984251968502576]终止（exit1），各自旧输出目录完整保留。negative只有144dpi的两负例观测，不能把整轮称完成；public也只完成初始144dpi五项。没有调宽1px阈值。

只读原PDF得到全部6页MediaBox=[0,0,595,842]，没有CropBox/Rotate，全部cm仅[0.12,0,0,0.12,0,0]。原名义297mm页高=841.8897637795pt，与实际842pt有舍入差；实际线端点也在整数PDF坐标格，并非理想毫米的无限精确投影。0.12是实际PDF坐标单位的point变换，不称300dpi设备证据。完整原端点/坐标/fit/独立残差见case/text-range-projection-analysis.json。

root同意最小纯离线v8：读取实际page box和全部cm，要求各文字页与校准页的实际basis、validated settings同一；校准五线建立明确标measured的WCS→page，前三fit、另两独立验证。矩阵=[[2.8344,0,56.556],[~0,-2.8337142857142834,785.3771428571428]]；held-out角点残差≈0，内部点[0.0192,0.0325714285714]pt。仅绑定本次相同PDF/plot settings，未知basis/不同box/rotation/多cm拒绝，不冒称通用PDF解析或普适投影。

v8冻结在 `tests/native/text-range/offline-projection-v8/`；receipt记录来源v7与sourceSHA=`9296A063343919A13C3A954CE855E239C706254A55063AB8B431DF21857B27F3`。AST核对full_cells/compare/bound_inputs/project/render五函数与v7完全不变；只修映射及添加独立输出标签，不改严格完整像素单元/margin0。v8负例exit0、public exit1（真实越界保留，error=null），两档全矩阵及独立校准都完成。未重编译ARX或再开CAD，全部6原PDF SHA仍与原native报告逐项相同。

实际完整离线命令、输出状态/退出码及各结果SHA在 `tests/native/text-range/offline-projection-v8/execution.json`。比较器使用configured bundled Python及已存在Poppler，四轮均先negative后public。最终结果分别为case/text-range-negative-measured-v8/comparison.json与case/text-range-public-measured-v8/comparison.json；原失败目录case/text-range-negative及case/text-range-public保留。原生/离线摘要见tests/native/text-range/N9a-outcome.json。

## 图面、保护与交接

最终同原像素裁图和独立叠加为case/text-range-visuals-measured-v8/five-controls-144-native-and-public-overlay.png，以及该目录五项独立ink-crop/overlay-crop；无框墨迹输入和叠加文件分开。作者实际查看原生第一线、五类原墨迹及两版叠加；root已实际查看五类144dpi原墨迹/旧叠加/最终measured版，并逐项读取10public/4negative计数；288dpi仅数值核验，未冒称另视觉审查。root独立记录为case/root-visual-qa.json，SHA256=`99116E654314EF0A6955D82C62A01133B3FFA854EE2236A644C40FC9D27FEEBA`。

v7 controller native-result-closure.json记录sourceSHAsUnchanged=true，4个暂存模块/INI精确退役，reader-retirement.json的6输入SHA保持。isolation-final.json记录workerExitCode70/forcedTermination=true/nativeQuitConfirmed=false、ownUserDesktopShow=0/ownForeground=0、inputDesktopChanged=false、monitorError=null/supervisorErrors=[]。这是精确停止，不冒称正常QUIT；其它CAD related事件单列需归属复核，不将其活动归为本宿主，也未触碰用户/未知CAD。

最终图面root已查看；后续由root安排唯一一次完整task review；审查本任务需求/全部新增代码/有效校准/失败保留，不只审最后几行。review包提供最终原生冻结源、v8离线冻结源、从empty的最终新增源码全文diff、receipt/命令/全证据索引。作者未派审、未Git提交或切分支，未改公共宿主桥/NativeEvidence/旧N7/src/生产Layout；报告追加不重编译或重跑native。

完整复核包已冻结：`tests/native/text-range/review-fulltask-v7-v8-20261103/`，入口review-context.md；全文full-task-from-empty.diff，12份最终新增源码/文档，以及实际native-v7/offline-v8源快照和receipt/commands/evidence-index。旧失败仅索引原路径，不复制历史大目录。
