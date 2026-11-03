# N9b：文字坐标与实测占位 C++ 核心报告

日期：2026-11-03。作者状态：**DONE_WITH_CONCERNS**。本次隔离纯 C++ 计算接口已实现；最终 console 构建及 focused suite **11/11，exit=0**；复用原 N9a 的十张真实原墨迹 PNG，C++ 实测占位经未修改冻结 `full_cells` 全量完整像素单元比较 **10/10，exit=0**；35%缩结果、错误映射、遗漏映射共 **30/30** 由同一独立比较器拒绝。此处的通过只表示这十份已观察设备足迹的提取和映射；原 extentsBox 正式结果仍为 **0/10**，原 backward 单次反射诊断仍失败，不改历史。

本次不授生产 glyph、BUILD 或 Layout 资格，没有 `nativeVerified`/`glyphComplete=true` 输出，没有写任何资格状态。未启动或控制 CAD、未写 DWG、未重跑 N9a 原生探针、未执行旧77核心测试、未更改公共类型、未提交/切换 Git、未派子代理或审查。唯一源码作者按 brief 完成；供 root 安排一次完整任务审查，作者自审不冒称独立 review。

## 实际接口和算法合同

新增 `src/TextOccupancy.h/.cpp`，复用 `ElevationModel.h` 原 `Point2`、`Bounds2`、`Result<T>`，不复制几何框架。`Result::canCommit()` 在此仅表示计算值可用，不授权图纸写入。

1. `transformTextRange(TextRangeInput)` 返回四角和 AABB。调用者必须声明 `beforeDisplayBackward` 或 `afterDisplayBackward`；unknown/其它枚举拒绝。仅前者且 backward=true 时对局部 X 反射一次，后者绝不二次反射。保留负 min 和全部四角。调用者提供明确二维 localX/localY 基（表达 direction、normal 手性）及 position，再应用一次 NET modelToWorld。没有 height/xScale/tracking/oblique 参数，因这些已属于声明的范围，不能重复应用。支持有限可逆二维仿射、旋转/剪切及反射基；拒绝无效框、非有限、奇异基/矩阵或非有限/退化输出。归一化线性系数计算逆，避免直接行列式乘法溢出；无法用 double 有效表达的逆/结果拒绝。没有新增 AutoCAD 字体 API 语义断言。
2. `measureTextFootprint(TextRgbBuffer, worldToPixel, TextRenderContext)` 扫描完整输入画面。RGB 顺序、三字节、白背景不透明、正 rowStride、逐行自上向下；任一通道<255均算墨迹，含 AA。验证 width/height、rowStride、可访问字节长度 `(height-1)*stride+width*3`、所有乘法/加法溢出；行尾 padding 和额外尾字节不算像素，末行 padding 可以不存在。宽高需允许 double 精确表达整数单元边界。拒绝空白、任意缓冲边缘触墨、非有限或不可逆映射、缺失必要上下文。
3. 像素 (x,y) 为完整边界单元 `[x,x+1]×[y,y+1]`；左上为原点，y 向下。上下文 phase 标识边界原点在命名设备帧中的相位，已包含于 worldToPixel，不再相加。记录全部墨迹像素数及极值，由 `[minX,minY]` 到 `[maxX+1,maxY+1]` 的完整设备矩形逆变换四角，取四角保守 AABB；不以中心代单元、不只逆变换min/max。矩形中允许含白色空区，不是精确字形轮廓。
4. `ObservedTextFootprint` 按值携带实际 worldToPixel、device/frame/font/source 身份、DPI 和 phase。本接口只得到 **observed device footprint**；全图扫描和未碰边仅排除本缓冲区的可见裁边征兆，不能证明外部场景、clip或文字完整。没有新建跨工程资格框架或 GS 设备。

## TDD、构建和实际命令

工作目录均为 `D:/Codex/建筑施工图自动化/立面插件`。开发 Python 固定 `C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`，已有 Pillow/numpy；产品接口不依赖 Python/PDF 工具。构建冻结输入后仅编译本任务两个 console；VS2019/v141 工具目录14.16.27023、实机 cl19.16.27054、x64、C++17、/W4 /WX；无 SDK/ARX链接。

| 命令 | 回执/结果 | 退出码 |
|---|---|---:|
| `./tools/build_text_occupancy.ps1 -Label red-initial` | `build/n9b-20261103-101435-700-red-initial-bf78f599/receipt.json`：build=0，测试0/3；backward、四角净变换、AA完整单元均 coordinate mismatch | 1 |
| `./tools/build_text_occupancy.ps1 -Label green-initial` | `build/n9b-20261103-101527-090-green-initial-3a2f168a/receipt.json`：3/3 | 0 |
| `./tools/build_text_occupancy.ps1 -Label focused-and-adapter` | `build/n9b-20261103-101616-933-focused-and-adapter-f8a6281c/receipt.json`：独立 focused suite 11/11，两console均编译 | 0 |
| `./tools/build_text_occupancy.ps1 -Label delivery` | 最终 `build/n9b-20261103-101858-736-delivery-b2a48216/receipt.json`：build=0，11/11，无编译警告/错误 | 0 |
| bundled Python `./tools/verify_text_occupancy.py --receipt ./build/n9b-20261103-101858-736-delivery-b2a48216/receipt.json --output ./tests/text-occupancy/evidence/native-rasters-final` | 最终10/10，30/30真实负例；`real-rasters-final.log`及`native-rasters-final/comparison.json` | 0 |

初始 RED 是可执行缺实现基线：局部框直接转发且重复两个角，backward/基/NET未转换，像素中心被误作完整边界；失败均为数值断言，不是缺符号、编译失败、空图或 helper 抛错。冻结 RED 输入和 EXE仍在该回执目录。手算不对称框 `[-2,-1]–[5,3]`，旋转到 `(10-y,20+x)` 后NET到 `(47+x-2y,46+3x+y)`，四角分别 `[47,39],[54,60],[46,64],[39,43]`；其 AABB 会揭示仅变换两个对角的错误。backward的前/后两种约定分别验证恰好一次/零次反射；另用真实N9a旋转backward的rangeMin/rangeMax、position/direction回归，声明为尚未应用反向的坐标计算，不把诊断框升级为字形包含。

focused suite还覆盖显式剪切/-normal基、未知约定、反向/NaN框、奇异/非有限基和净变换、RGB不同通道AA、带脏padding完整扫描、最后内部行墨迹、旋转剪切逆映射四角/AABB和上下文回传、空白及四条边、空指针/零宽/短stride/短长度/两类溢出、奇异/非有限worldToPixel、缺font/DPI/phase。旧生产源未改，依brief不重复旧核心suite。

第一次实图验证（`real-input-check`回执，输出目录`native-rasters-v1`）在十次图像循环后绑定旧诊断文件时 exit=1：代码误写`comparison.json`，实际为`diagnostic.json`。只读检查真实目录后修正为实际文件并提前绑定；未改旧诊断。v2完整重跑exit=0；随后为避免旧冻结目录产生Python缓存，改为对同一未改比较器字节compile/exec，最终delivery再次10/10及30/30 exit=0。失败记录见 `evidence/validation-v1-failure.json`，无v1完整comparison产物，未将该失败当作有效RED。

## 复用输入与绑定

原生源：`阶段1/runs/elevation-native-20261103-093743-84f838a5/case/text-range.json`，SHA `05254B213DC3F1B0C5D20DF1F0C352E8D12D9281DF68B9180D010800EECF6CD1`。原5场景×144/288dpi `text-range-public-measured-v8/*-ink.png` 原样复用；每张对照旧comparison的PNG SHA、墨迹计数后，开发Python仅解码为约定RGB，调用最终编译adapter。没有重新渲染、读产品PDF或把overlay当输入。

独立比较器直接执行未改冻结 `tests/native/text-range/offline-projection-v8/verify_text_range.py` 的 `full_cells`；其与live旧tools比较器SHA均 `9296A063343919A13C3A954CE855E239C706254A55063AB8B431DF21857B27F3`。映射直接复用旧comparison的`measuredProjection.worldToPageMeasured`并按dpi/72缩放；保留旧PDF/字体/五点校准绑定及有限校准边界，不重解析或重拟合。simplex/gbcbig两实际字体SHA作为C++fontIdentity，deviceIdentity绑原PopplerSHA，frameIdentity绑PNG SHA，sourceIdentity绑原生JSON SHA及场景ID。

新comparison绑定本次source/EXE/build receipt及原生JSON、每张PNG、解码RGB字节SHA/长度/布局、旧comparison、旧反射诊断、冻结比较器；携带C++四角/世界AABB/上下文/实际映射和全量核数。临时单个`input.rgb`在每次调用后精确删除，不复制旧证据库。完整矩阵保存在独立新目录，所有统计均扫描全部像素。

## 十张真实图结果

设备范围为完整单元边界，世界AABB显示6位小数供阅读；精确17位值和全部四角见最终comparison。比较器只用原有1e-8像素数值比较容差，几何margin=0。

| 场景 | DPI | 墨迹像素 | C++设备完整单元矩形 | C++世界AABB | 全部完整单元越界 |
|---|---:|---:|---|---|---:|
| digits | 144 | 880 | [254,1361]–[525,1401] | [24.853232,29.952611]–[72.658764,37.010486] | 0 |
| chinese | 144 | 503 | [258,1363]–[340,1403] | [25.558848,29.599718]–[40.023991,36.657592] | 0 |
| rotated-backward | 144 | 485 | [453,1379]–[606,1483] | [59.957663,15.483969]–[86.947502,33.834442] | 0 |
| nested-nonuniform | 144 | 456 | [350,1262]–[529,1367] | [41.788033,35.951805]–[73.364380,54.478726] | 0 |
| tracking-oblique | 144 | 615 | [262,1361]–[533,1403] | [26.264465,29.599718]–[74.069997,37.010486] | 0 |
| digits | 288 | 1720 | [509,2722]–[1049,2802] | [24.941434,29.952611]–[72.570562,37.010486] | 0 |
| chinese | 288 | 992 | [517,2727]–[680,2806] | [25.647051,29.599718]–[40.023991,36.569369] | 0 |
| rotated-backward | 288 | 957 | [907,2759]–[1212,2965] | [60.045865,15.572192]–[86.947502,33.746219] | 0 |
| nested-nonuniform | 288 | 895 | [700,2525]–[1058,2733] | [41.788033,36.040028]–[73.364380,54.390502] | 0 |
| tracking-oblique | 288 | 1213 | [524,2722]–[1065,2806] | [26.264465,29.599718]–[73.981795,37.010486] | 0 |

这十项10/10包含是“由同一实际墨迹提取其占位，再由独立算法检验坐标映射”的有限观察验证，不是用同图证明通用字体范围定理。没有将新结果覆盖旧extentsBox比较，历史原框0/10与反射诊断144dpi95/485、288dpi161/957越界仍保留。

## 独立比较器真实负例

每张实图使用C++算得世界四角缩到35%，以及用有限可逆但错误的worldToPixel（设备平移+[50,-30]）/遗漏映射（单位矩阵）再调用C++提取。后两类helper确实返回结果，独立比较器随后用实际原映射投影并拒绝；不是只测试helper抛错。以下为被拒绝的完整墨迹像素数量，各项均>0。

| 场景 | DPI | 35%结果越界 | 错误平移映射结果越界 | 遗漏映射结果越界 |
|---|---:|---:|---:|---:|
| digits | 144 | 764 | 699 | 880 |
| chinese | 144 | 425 | 479 | 503 |
| rotated-backward | 144 | 362 | 155 | 485 |
| nested-nonuniform | 144 | 332 | 176 | 456 |
| tracking-oblique | 144 | 513 | 477 | 615 |
| digits | 288 | 1489 | 793 | 1720 |
| chinese | 288 | 839 | 573 | 992 |
| rotated-backward | 288 | 716 | 167 | 957 |
| nested-nonuniform | 288 | 648 | 156 | 895 |
| tracking-oblique | 288 | 1000 | 516 | 1213 |

## 支持边界与 Layout 下一接入缺口

当前可供后续原生采集接入的只有明确二维合同下的坐标运算与特定已渲染缓冲的设备足迹提取。实际验证域是原五项simplex/gbcbig受控渲染×两档分辨率；没有原TCH缓存完整字形、任意字体、所有clip/注释场景、无限分辨率或全图出图完整性的证明。未碰缓冲边缘也可能在上游就被clip、漏画或替代；上下文身份值由调用者提供，当前核心不能核实其与真实native调用绑定。

接Layout仍缺：实际原生渲染/缓冲采集入口和准确设备映射；设备/分辨率/phase/视图/AA/线宽/字体及源对象/帧绑定；独立的字体、场景、clip和绘制完整性证据及既有资格门接入。以上未开发，也不能用本次console结果替代。`Layout`、`GenerationState`、`NativeEvidence`、N7与N9a ARX/比较器/冻结报告和资格状态均未改。

## 变更、审查包与自审

仅新增下列7份源/测试/工具/说明，以及指定唯一报告和小型`tests/text-occupancy/evidence/`索引/结果。没有修改已有`tests/native/text-range/README.md`，可选P3措辞修正留给原作者。开始时已有他人未提交的AnalysisCommands/NativeMaterialTiles变更，本作者均未碰。

| 新增文件 | SHA256 |
|---|---|
| `src/TextOccupancy.h` | `D3EC0CE9C4B6E4E651A3E0B195B687FD0BEF92E65005B515F4BC4F85C3E60340` |
| `src/TextOccupancy.cpp` | `7EBDEBF6F188F0560D32411B3B3BB6E4C4E191DE26D30739A582DC63776142F0` |
| `tests/text-occupancy/TextOccupancyTests.cpp` | `365B6C3625EB2684E0DFE25345B4067971EAB37CC5C6A6AA0C629DAEDB579580` |
| `tests/text-occupancy/TextOccupancyAdapter.cpp` | `AA0E1907259049FF44F5894577B2C6AF54CEF6E805765D5C4EF28C52AB3B8938` |
| `tests/text-occupancy/README.md` | `9C22AB58F625F7E60D1174D7C75B5DADF7A86A0A48524B7BB84325738FDB574C` |
| `tools/build_text_occupancy.ps1` | `060C35592457A5059BD5683EA9B15048AD92F5F13056FA1EA71AFD279E7C777D` |
| `tools/verify_text_occupancy.py` | `08AB016FC9E367FADD40FBAEC575AC14F36C0AD824F0DD9ADDE0B752459FE812` |

清晰单次review包入口：`tests/text-occupancy/evidence/index.json`；`new-files.diff`逐文件与空基线比较；`empty-baseline.txt`表示这些文件此前不存在；`baseline.json`为开始前105份受保护既有文件指纹（全部生产src、旧文字探针/冻结源、旧tools比较器、旧报告和进度），`protected-check.json`确认105/105原字节保持、旧探针树没有新文件。最终构建冻结input及receipt保存实际编译源码和测试，不需拷贝旧证据库；最终索引与comparison指向同一delivery回执/EXE。

作者自审逐项核对：范围约定必须显式；局部X反向仅一次；全部四角及负min；仅一个NET；不重复字体比例；全缓冲RGB及AA；溢出/非有限/奇异拒绝；空白/碰边拒绝；完整单元/全部逆变换角；元数据原样回传；实图非空；独立full_cells原SHA；30个实际错误结果确实拒绝；旧资格/0/10保持。没有发现本次功能未决缺陷；仅上述接入/证明边界仍未完成，不能称生产避让完成。

自审发现importlib首次在旧冻结比较器目录新增一个`__pycache__/verify_text_range.cpython-312.pyc`（开始指纹中没有），旧源SHA始终未变。自动审批拒绝了同时清理该文件和空目录的计算路径命令；随后更窄的原生PowerShell `Remove-Item -LiteralPath`仅精确该单文件成功exit=0，未递归、未切shell，空目录保留。最终验证器compile/exec避免再次生成缓存，核验旧树无新增文件。详见`cache-restoration.json`。该自动审批拒绝已向root报告；没有遗留待用户批准动作。

## Fix round1/5：拒绝浮点舍入后的退化四边形

2026-11-03，唯一完整 task review `执行/N9b-full-task-review-20261103.md` 提出一项 P2：“在返回成功前拒绝浮点舍入造成的退化四边形”。该发现成立：上文原11/11和作者原自审未覆盖大坐标使合法输入的实际输出角点塌缩；原 `finish()` 只检查AABB两轴正跨度，不能排除斜线上点集。此附录更新本次状态：**修复完成，定向13/13、最终候选实图10/10及30/30，均exit=0，待root转原reviewer限定复核**。没有安排第二次完整审查或由作者派审。

修改只涉及 `src/TextOccupancy.cpp` 的共用 `finish()` 和 `tests/text-occupancy/TextOccupancyTests.cpp` 两份源文件。保留原四角有限及AABB检查，随后以实际已返回的四角依次构造相邻边向量；先相减移除世界大原点，再按每条边最大绝对分量归一化，检查四个相邻转向均有限、非零且同向。重复角、共线角或失去凸四边形面积的数值失真均拒绝。该方法避免直接用绝对世界坐标做鞋带面积所产生的大数相消，同时避免叉积乘法溢出；未增加数值门槛或几何margin，未改变渲染提取、范围反射、资格状态或原始墨迹。两入口继续使用既有 `TEXT_RANGE_NUMERIC` / `TEXT_PIXEL_NUMERIC`，不新增诊断体系。

新增两个实际C++回归覆盖同一机制：

- 范围 `[(0,0),(4,0.1)]`、localX=(1,1)、localY=(1,-1)、position=(1e16,1e16)、单位NET、afterDisplayBackward：输入有限可逆，但实际角点舍入到斜线上，必须以 `TEXT_RANGE_NUMERIC` 拒绝。同一大坐标平移将localRange最大Y改为4后，四角仍有二维面积，成功返回。
- 3×3 RGB仅中心墨迹、worldToPixel=`{0.125,0.125,-2.5e15,5,-5,0}`：逆映射输出塌缩，必须以 `TEXT_PIXEL_NUMERIC` 拒绝；同大坐标把线性第二行改为 `{0.125,-0.125}` 的可表示完整单元正常通过，未一概拒绝大坐标。

| 命令（工作目录仍为立面插件） | 冻结回执和实际输出 | 退出码 |
|---|---|---:|
| `./tools/build_text_occupancy.ps1 -Label fix1-red` | `build/n9b-20261103-102838-575-fix1-red-d6246965/receipt.json`，旧b2a48216核心字节未改+新增回归；build=0，11/13；两项分别失败为 `large translation returned a collapsed range` / `inverse mapping returned a collapsed cell` | 1 |
| `./tools/build_text_occupancy.ps1 -Label fix1-final` | `build/n9b-20261103-102855-993-fix1-final-3a0f0c94/receipt.json`，build=0，13/13；无编译警告/错误 | 0 |
| bundled Python `./tools/verify_text_occupancy.py --receipt ./build/n9b-20261103-102855-993-fix1-final-3a0f0c94/receipt.json --output ./tests/text-occupancy/evidence/fix-round1/native-rasters-final` | 仅对最终候选重核一次现有十张PNG；10/10 observed足迹包含、30/30错误结果拒绝；完整输出 `fix-round1/real-rasters-final.log` | 0 |

原N9a/native、PNG、比较器、工具、公共类型及其它生产源保持。没有重新渲染、CAD、DWG写入、扩大场景矩阵、改变margin、Git提交/切支或子代理。原实图表的墨迹计数/设备范围与世界四角均保持；最终新comparison包含本次新核心/EXE和原输入绑定。历史字符串框仍0/10，接Layout及glyph/BUILD资格缺口保持。

独立fix审查包入口为 `tests/text-occupancy/evidence/fix-round1/index.json`。`scoped.diff` 精确比较b2a48216冻结输入与这两份修复源；RED/最终构建各自保留冻结input、EXE、日志及回执。修改本报告前已完整备份到 `fix-round1/report-before-fix.md`，SHA与原 `evidence/index.json` 绑定的原报告SHA一致；旧index及旧diff/所有结果保持，不覆写其历史哈希。新fix索引另外绑定当前追加报告、原review、原index、原报告备份、两源前后SHA、RED/GREEN日志、最终EXE/实图comparison。初轮105保护指纹中104份仍逐字保持；唯一变化为root正常更新的 `执行进度.md`（root已确认，本作者未写），不属于两份修复源码diff，不回滚该进度记录。
