# N9c 原生离屏文字缓冲接入报告

2026-11-03，唯一源码作者，工作根 `D:/Codex/建筑施工图自动化`。依据本次 `N9c-原生离屏缓冲实施简报.md`，不把旧 Plot 观察当成 GS 支持证据。

## 当前状态

**DONE_WITH_CONCERNS：N9c未完成、采集未接通。** v17真实entry→idle-ready→唯一capture调度已成功且SCRIPT位已清；完全同v16 GS主体仍是主BTR和非持久对照零遍历、无context/RGB。原failed/qualification=false保持，已验证调度/保护不等于采集成功。源码固定v17，不再新候选或CAD操作。

候选 receipt：`build/n9c-20261103-121817-090-idle-command-capture-v17-frozen-167b5fda/receipt.json`，SHA256 `1FCA912E7C12BBBD1E6A94B3047C43BBF5D85B5472001995D33A1E5898487B2D`。

ARX：同 build 下 `out/NativeGsTextFootprint.arx`，SHA256 `776CE5E23883D73A484BC8C20D3A58E6273C40209DB9E4D9EA2ED035877E4B54`。本机编译 0 错误；唯一链接警告 LNK4099 是 SDK rxapi.lib 的 rxapi.pdb 缺失，没有修改 SDK 或关闭本项目的 /W4 /WX。PE 依赖实际包含 AcDrawBridge.dll、AdImaging.dll、acdb23/AcGe23/accore 等，无 Python、PDF 或外部转换器依赖。

## 所核本机 SDK 合同与路线

SDK 根 `C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit`。所读头及 arxref.chm 的 SHA 已记入 `tests/native/text-footprint-gs/sdk-public-contracts/source.json`。

- `AcGsManager.h`：`createAutoCADModel(kernel)` / `destroyAutoCADModel`；`createAutoCADOffScreenDevice(kernel)` / `destroyAutoCADDevice`；`createView(device)` 已加入给定设备且无 VTR；对应 `destroyView` 移除设备关系。`acquireGraphicsKernel` / `releaseGraphicsKernel` 引用计数配对；同链对象不得混 kernel。未使用会加入 GUI 的 createAutoCADView/createAutoCADViewport，也未使用只限 RapidRT 的 getOffScreenView。
- `gs.h`：`kernel.associate(dbPointer, model, kMain)` / `find` 是本次唯一尝试的库关联；只对自己新建库的键查询、记录旧值、关联与还原。未使用限当前 display kernel 的 manager.setGsModel。manager 创建模型没有 SideDB 参数，所以代码不据此假定 context 成功。
- `AcGiContext::database()` 在真实 overrule WorldDraw 中直接观察；实体 database/objectId、目标库、model find 读回均单独写日志，并独立要求一致。数据库 cookie、workingDatabase 或自己实现 context 都不作为证据。若实际 context 不符，该组合失败后停止，不能宣称全部公开路线不可能。
- 正交 view 明确 setViewport(0,0)-(1,1)、无边框、无前后 clip、相机 (75,50,100)、目标 (75,50,0)、up=Y、field=150×112.5。device 背景显式白色并读回；text 为 truecolor 黑，线宽 kLnWt000。实际 viewing/projection/screen/worldToDevice 矩阵和设置写入 trace。
- ATIL `Image::createContext(kRead, size, offset)` 返回对象由 caller delete，offset 为 upper-left；`ImageContext::getRow(y,x,count,buffer)` 读行。`RgbModelAttributes::k3Channels` 配合 `DataModelAttributes::kRedGreenBlueAlpha`；读取实际 image.dataModel().channelOrder 与 32 位像素，按 RGBA 或 BGRA 分别取三个颜色通道，输出 top-to-bottom 紧密 RGB。绝不把四通道原行当三通道。
- text/push/pop 按 N9a 已归档本机 2020 官方合同：text false 表示成功存入 primitive，push/pop true 才表示成功。新增 calibration polygon 的同版 CHM 条目亦已定点提取核对，false=成功存入、true=终止，HTML 与 SHA 归档在本次 sdk-public-contracts。没有套用通用布尔成功包装。

## 精确源码范围

仅新增以下五个源码/构建输入文件：

1. `tests/native/text-footprint-gs/NativeGsTextFootprint.cpp`
2. `tests/native/text-footprint-gs/NativeGsTextFootprint.vcxproj`
3. `tests/native/text-footprint-gs/README.md`
4. `tools/build_text_footprint_gs.ps1`
5. `tools/verify_text_footprint_gs.py`（开发离线工具）

清晰的新增源码完整 diff：`tests/native/text-footprint-gs/evidence/candidate-v17-new-files.diff`，SHA256 `C53ED85B9168D52792A068EE7F62A42E715C06981B101B21189AC4491A91623E`。最小证据索引：`tests/native/text-footprint-gs/evidence/index.json`。

ARX 直接编译同一个 `src/TextOccupancy.cpp` 的逐字冻结副本，原件与 frozen copy 均为 `28D106F22219C8CCD6C0FBC9A6C16EC48296177F6DD0A3D1252C3D9ECF5F3520`；build 入口先检查此 SHA，未改 N9b 核心或另造算法。只借用同目录纯值头 TextOccupancy.h/ElevationModel.h。沿用的 NativeEvidence.cpp=`1B3CA34A61CC2C2DB2C42A7FDFD01C029DE7BE82EAE6CE96D9DE1C3B363A8FE2`、NativeEvidence.h=`C6B083EE0346AA7CDB51090274CCCADCB2C6DE0AA92A196AD846E11C41C9EBB6`；未修改公共证据类、宿主桥、native_case_host.py/watch_native_case.py、旧 probe、旧 production 或另一个平面插件。没有 Git 提交、切支、发布、安装。

## 控制与映射实现

五个请求与 N9a 控制相同：digits `1234567890`、chinese `中文测试`、旋转0.47且 backward 的 `123ABC`、两层非均匀变换的 `1200中文`、tracking=1.6/oblique=0.25 的 `12中文ABC`；字高7、宽因子0.75。每次真实 WorldDraw 独立 loadStyleRec，完整记录位掩码及加载后路径，要求 bit1 成功且缺字/替代位均无；字体每次核 SHA。显式要求 N9a 的相同绝对路径与 SHA：simplex `C:/users/pc/appdata/roaming/autodesk/autocad 2020/r23.1/chs/support/simplex.shx` / `30DC50FA379935E4D7EB6E60041FA070A11FBA6DEAC337A2FA9E34DCB5BFBCC1`；gbcbig `D:/cad2020/autocad 2020/fonts/gbcbig.shx` / `B6D7F2C5E3057840A63433EE78A49D59FBD030C999D706F7368A14D52B2A4150`。这仍不是原 TCH 字体缓存身份。

两档固定1024×768、2048×1536使用同一模型视野，文字逐项独立全缓冲、保持边界距离。先画无文字的五个 truecolor 小方块，前三个非共线点只选择预先声明的 flipY 与 phase={0,.5} 离散候选；另两个角部/内部点只作 held-out 检验。两组分别记录残差，固定阈值均 .55像素，失败不扩大、不用文字拟合。所有候选残差、观测中心、实际矩阵与所选 affine 保留。

N9b context 的 dpiX/Y 明确按控制纸面150×112.5mm、1:1换算：w×25.4/150、h×25.4/112.5。这是控制换算，不是物理打印设备 DPI 声明。每项结果绑定 nonce/分辨率/frame/font SHA/自有对象与请求/像素边界 phase；不推断 Layout 平移后的离散足迹保持。

完整 RGB 写开发证据后直接调用 measureTextFootprint，记录墨迹数、设备完整单元框、世界四角/AABB、affine、设备/帧/字体/来源。N9b 自身 blank/edge 拒绝保持原样。未接 Layout、BUILD 或 glyphComplete。

## 保护、收尾和少量异常控制

临时 AcDbDatabase(true,true) 从未设为 workingDatabase，不切 current/active 文档、不创建窗口、不加载原 TCH 业务对象、不改其字段，也不 saveAs 源/临时 DWG。只为精确自有 AcDbPoint 的 database/objectId 接受 overrule。

主场景前有两个同 native case 的轻量真实资源异常控制，各使用独占新库及相同 setup/cleanup：模型关联后注入异常；overrule 安装后注入异常。两项期望实际抛出指定错误并验证清理、ambient 指针/DBMOD/写事件不变；它们不是另一份计算核心测试或额外宿主。v5实际进入第一项但未到注入点；v6两项完整控制仍待controller执行。

当前v8公共收尾先view.eraseAll移除drawable、移除overrule并恢复原isOverruling，关闭自己的point；若viewAttached则device.erase(view)并检查true，再同kernel.deleteView；还原自己新库原kernel关联并读回；destroyAutoCADModel与同kernel.deleteDevice；releaseGraphicsKernel；最后删除临时库。v1–v7使用manager.createView/destroyView的原路线及其真实失败另列保留。若 removeOverrule 或关联还原失败，保留 Session 与所有仍有可能回调的对象，拒绝模块卸载，不能掩盖为通过。trace 写入失败不阻止对应资源收尾。正常和异常都走同一路径。

ambient 独立小 AcDbDatabaseReactor 记录 openedForModify、modified、appended、erased、成功 header 写；另记录当前/active/working 指针和公开 DBMOD 前后。指针不变只证明没切库，写事件/DBMOD 用来补内容保护观察；不把自有库绘制懒写算成 ambient 写。原六输入哈希与精确宿主收尾由未改 controller 协议继续核验，作者没有操作用户或其它 CAD。

## 已实际执行的开发验证与失败

- api-compile-v1：`build/n9c-20261103-103718-839-api-compile-v1-5ba40f36/receipt.json` exit1。实际 ATIL ChannelOrder/PixelType 枚举使用不匹配，且 SDK half.h 的 register 触发 C5033 /WX。保留失败；不称 native RED。
- api-compile-v2：`build/n9c-20261103-103911-800-api-compile-v2-a15a7c14/receipt.json` exit0。改用本机头实际 ChannelOrder 名，只在 ATIL includes 的 pragma 范围抑制5033；本项目 /W4 /WX 保持。
- static-audit-v3：`build/n9c-20261103-104030-498-static-audit-v3-fbc6856b/receipt.json` exit1。增加 DBMOD 观察时漏 include adscodes.h；补头，失败保留，不称宿主失败或绘图 RED。
- candidate-v4：`build/n9c-20261103-104130-159-candidate-v4-f385f535/receipt.json` exit0，首次冻结编译。
- final-candidate-v5：`build/n9c-20261103-104445-736-final-candidate-v5-4fa5e91c/receipt.json` exit0。冻结后静态自审发现总 cleanupComplete 只反映主Session，若前置异常控制收尾失败，case虽failed且保留资源/拒绝卸载，但总字段可能不完整。v5只新增resourcesReleased查询并合并三个Session状态，无重试/掩盖，无GS/API/映射变更；精确小diff为 `evidence/candidate-v5-v4.diff`。已逐项重新核对 receipt 中10个 sourceFiles 及所有 live origins/frozen copies SHA。
- 离线检查器自身测试 `evidence/verifier-self-test-v1.json`：一项正例与缩框、缺映射、错映射三项拒绝通过。缩框负例同时重建内部一致的四角/AABB，确由完整非白单元覆盖失败拒绝；它只验证检查器，不证明实际 GS 或 N9b 原生采集成功。

真实 native 墨迹检查待运行：`verify_text_footprint_gs.py <case输出目录> --output <新报告路径>`。检查整个 RGB 中每一个非白像素四个单元角，经逆映射都被 C++ 世界四边形覆盖，另核精确框/计数/AABB/校准 affine、原始 RGB SHA；对每个实际帧重复三项负例。不会重跑旧 N9b 全部单测或 N9a/N8a。

## Controller 合同和未决范围

交未改 `native_case_host.py prepare <上列receipt> --case NativeGsTextFootprint`，由已有独立 controller 以 fresh nonce 串行启动/控制自有隔离宿主；`BCE_TEST_RUN` 同步完成，不需要 N9a OPEN 门或 capture helper。作者只读回 controller 发来的 binding、native-case、trace、RGB 和收尾消息。

当前实际能力仍未证。若失败，须按真实事实区分宿主 infra、模型/真实 context、GS绘制、ATIL读出或映射校准；保留日志先诊断，由 root 裁定最小修正，不改共享配置/硬件加速、profile、安全设置、平台 DLL 或假造 context。若只能完成探针而未接通，最终记 DONE_WITH_CONCERNS。即便两档通过也仅为受控字体单行/有限帧设备占位，glyphComplete=false、capabilityUpgradeAllowed=false；原 TCH 全对象/场景、图层裁切、字体资源和 Current DWG/生产集成另行推进。

## Controller 调度事实

root转述controller确认：v4只做只读预检，未prepare/launch，无nonce、stage或宿主；因此v4没有原生运行证据。root已逐项核对v5冻结输入、receipt/ARX与v5-v4小diff，已交controller以fresh nonce实际运行v5一次，结果见下。v6仅按root明确裁定修正后重新冻结。

## v5 实际失败、终态与 v6 最小修正

v5 session `阶段1/runs/elevation-native-20261103-104556-ae74343c`，PID29716。controller已闭合精确stop70（非正常QUIT），4暂存退役、原6输入保持、自有active0；因报告被独占句柄锁住，使用独立 `locked-report-closure-audit.json`，不能冒充标准watch closure。原native-case关闭后SHA为 `8A2545B84BD1FAAEC8E99811B0FBA53D65FFF562D49335658D1BD8CDA7B810C6`，status=failed、steps=[]、errors=[ambient identities changed]；这是旧异常汇报覆盖主因的事实，未改旧文件。原输出及controller收尾审计逐字只读快照与SHA索引在 `tests/native/text-footprint-gs/evidence/native-v5-failure/`。

实际 `case/lifecycle-1/result.json` 的observed为 `2D kernel unavailable`，passed=false、cleanupComplete=true。其真实baseline DBMOD=1→1、ambientUnchanged=true、五类写事件均0。原trace证明manager非null后，以唯一k2DDrawing能力请求调用acquireGraphicsKernel而实返null，随后销毁自有库；还未创建model/device/view/overrule，没有真实WorldDraw、ATIL或文字。公开acquire接口没有错误码或细化失败原因；因此这里只能认定该宿主该能力请求未取得kernel，不能认定硬件加速、隔离桌面故障或全部公开GS方法不可能。

主execute因前置控制失败未进入，主Session的current/active/ambient仍null、dbmodBefore仍默认0；其 `gs-result.json` 里的ambientPointersUnchanged=false和DBMOD0→1来自未初始化值比较，不能证明实际切库/改图。实际lifecycle观察是前述1→1。原gs-result仍保留 `real GS resource lifecycle fault control failed: 2D kernel unavailable`，frames=[]。

另核公共Evidence::finish只rewind/write/flush，原NativeEvidence::create_new使用share0；v5全局Session保留s.evidence直到模块卸载，成功/异常终态都会持续持有独占报告句柄。此为本探针终态持有期错误，不修改公共NativeEvidence。

root据上述实际失败裁定v6仅修三类问题，精确差异 `evidence/candidate-v6-v5.diff`：

1. 固定单次k3DDrawing请求，记录descriptor要求、返回kernel及isCompatibleWith(3D/2D)；没有fallback循环。本机2020 `gs.h` 示例及 `samples/database/ARXDBG/Reactors/ArxDbgGsReactor.cpp:35–36` 均请求k3DDrawing。acgivisualstyle.h明确k2DWireframe使用2D GS、k3DWireframe使用3D GS，因此只对自有view设k3DWireframe并用value getter读回type，XY控制仍沿原正交相机；不改共享字典或GS配置。这是有据的下一候选，尚非3D kernel实际成功证据。
2. ambientInitialized显式区分观测适用性；主execute未进入时主ambient字段记null及not-observed-main-not-entered，不以默认零值推断改动。lifecycle自身真实baseline和faultReached单独保留。主原failure先写终态错误，额外ambient判断只在真实初始化后适用。
3. 所有终态finish后释放Evidence；异常catch通过移走unique_ptr保证即使finish再次异常也关闭报告。潜在回调Session资源仍保留到安全cleanup/unload，二者生命周期分离；不删除资源失败检查。额外终态IO/cleanup异常亦先保留已捕获的执行主因。

v6 receipt已重新核对10项冻结sourceFiles及所有live origins/frozen copies；核心SHA与公共NativeEvidence保持。尚未重跑CAD，等待controller确认旧宿主闭合后以fresh nonce实际执行v6。

## v6 实际致命异常与 v7 纯定位候选

v6实际session `阶段1/runs/elevation-native-20261103-105246-a7877155`，PID33336。controller已精确闭合自有进程，自有active0、4暂存退役、6输入保持；不是正常QUIT。原错误窗文本由controller只读取出、未点击：`致命错误: Unhandled Access Violation Reading 0x0000 Exception at 1352F0D7h`。faulting module未知，不根据该地址猜模块。原 `owned-fatal-dialog-readonly.json`、`crash-closure-audit.json` 与原case文件逐字快照索引在 `evidence/native-v6-fatal/index.json`。唯一native-case是未commit的INCOMPLETE骨架，主trace空；不补写或回填任何终态成功/失败字段。

lifecycle1实际取得kernel，compatible3D=true、compatible2D=false，模型关联读回成功；指定after-model故障注入实际到达，正常完成关联还原/model销毁/kernel释放/自有库销毁，result为passed=true、cleanupComplete=true、ambient DBMOD1→1且五类写事件0。这是实际有限资源控制成功，不是文字采集成功。

lifecycle2同样取得3D kernel，创建自有view后最后trace为requestedType=5、styleSet=true、actualType=10。本机acgivisualstyle.h与2020 CHM枚举明确5=k3DWireframe、10=kCustom；getter公开合同只说明输出style及是否已设置，没有同一内建type标识保证。故type10不直接证明wireframe属性错误；v6严格type断言确会抛出C++异常，第二故障注入/正交setView/overrule安装尚未到达。该普通C++异常随后进入cleanup，但原日志只在调用完成后写，不能据最后style行认定具体崩于eraseAll、marker.close还是destroyView等调用；不得删断言后直接重试掩盖异常清理缺陷。

root明确裁定v7纯定位，保持v6 API、次序、严格type断言和算法，仅在catch进入cleanup前记录原C++错误，并在view.eraseAll、removeOverrule/restoreOverruling、marker.close、destroyView、关联还原及find、model/device销毁、kernel释放、数据库析构/ambient reactor移除等实际调用前后通过同一write_flushed立即FlushFileBuffers。没有SEH吞异常、共享设置变更或新增矩阵。精确diff `evidence/candidate-v7-v6.diff`。v7实际编译exit0，10项冻结输入和live/frozen副本逐项核对；待controller用同case fresh nonce一次定位。

## v7 真实定位与 v8 最小配对/样式修正

v7 session `阶段1/runs/elevation-native-20261103-105830-44227fc6`，PID25168，controller已精确stop70、4暂存退役、6输入保持（非正常QUIT）。原trace和错误窗、一次模块表、closure审计逐字快照在 `evidence/native-v7-destroyview-fatal/index.json`。模块低32位地址范围只能给出accore.dll候选，不足以认定faulting module；保持未知。lifecycle1实际完整释放。lifecycle2原异常明确记录为 `own view visual style readback differs`；随后view.eraseAll、rule.reset、marker.close的before/after均完整，最后 `destroyView.before` 没有after并fatal，故定位为这次 `manager.destroyView(view)` 调用内，而不是前面的close或绘制。主未进入，唯一native-case仍是未commit骨架，不作终态回填。

root裁定v8完整替换view工厂配对：同一个已取得kernel的 `createView(nullptr,false)`，明确 `device.add(view)` 成功后viewAttached=true；收尾先erase全部drawables，再device.erase(view)要求true并清attached，最后同kernel.deleteView。2020 gs.h及对应CHM明确deleteView的这两个移除前置条件，device.add/erase均true=成功；不混manager创建/kernel删除，也不改model或device各自原有配对。erase失败即保留资源并拒绝卸载，调用前后flush定位日志保持。这只是一条有据替代候选，尚不称已修复实际收尾。

类型5/10继续记录为metadata。保留getter styleSet检查，另逐项记录并比较16个公开绘制属性的AcGiVariant值及operation：face lighting model/quality/color mode/modifiers/opacity，edge model/styles/modifiers/color/opacity/width，display styles/brightness、draw order、viewport transparency、lighting enabled。颜色记录method/index/RGB，其他按类型记bool/int/double/string/null；任何属性或operation差异会保留并拒绝，不是删除所有读回检查。没有任意容差或全局style字典写入。

原after-model和after-overrule异常控制保持；新增faultPoint3：view已create并成功挂到device，但未设置viewport/style/camera就注入指定C++异常，要求同一路cleanup完整释放且ambient稳定。顺序1→3→2，仍同一case/一个fresh host，不扩大两档五文字矩阵。这直接回归v7暴露的早期view异常清理阶段，不能只用完整初始化后的路线掩盖。

v8唯一源码差异见 `evidence/candidate-v8-v7.diff`，实际编译exit0、核心与公共证据类未改、10冻结输入逐项匹配。源码再冻结，等待controller一次fresh nonce原生观测。

## v8 实际回归、样式差异与 v9 有限显示合同

v8 session `阶段1/runs/elevation-native-20261103-110603-e44f5dbc`，PID43980，controller已标准watch收尾，4退役/6保持/自有active0，无fatal。原文件及 `native-result-closure.json` 逐字快照索引 `evidence/native-v8-style-differences/index.json`。control1和新control3均实际faultReached/passed/cleanup/resourcesReleased=true，ambient DBMOD1→1、五类写事件0；尤其新control3证明kernel.createView/device.add后尚未style/camera的真实异常清理路径成功，不能只报正常初始化后清理。control2因样式属性差异拒绝、注入点未达，但资源清理完成。主未进入，gs-result正确记录ambient not-observed/null、frames=[]、cleanupComplete=true；native-case终态failed保留主因且已关闭可读，原报告锁/未初始化ambient修正由此实际验证。

16项有6项真实不等：faceLightingQuality 2→0、faceColorMode0→1、faceOpacity .6→1（均kSet）；edgeStyles值4同但kSet→kEnable；edgeModifiers0→8(kEdgeColorFlag)；edgeColor从kNone/index257变kByLayer/index256。其余10项值/op相同，含faceLightingModel=kInvisible。2020 SDK明确kInvisible不绘faces，前三面参数对本域非活动；但edgeColorFlag会覆盖drawable颜色，不能忽略，kEnable也不能在无已知base时当作kSet。因此v8拒绝不是单纯type metadata噪声，更不能直接移除所有读回。

root批准v9用自有custom value style定义有限XY线稿合同，全部通过公开setTrait且明确kSet：face lighting model=kInvisible、无face modifiers；edge model=kIsolines，visible|obscured mask，edgeModifiers=0（禁color/width/jitter/overhang等额外修饰），opacity1、width1；背景display mask、brightness0、无lighting/viewport transparency，固定draw order。inactive edge color仍显式设ByLayer并记录，但edgeModifiers必须严格等于0，从而不得把该色用作drawable truecolor覆盖。样式/context身份改为 `XY-custom-invisible-faces-object-color-edges-v1`，不再自称严格内建k3DWireframe。

保留同16项请求/读回值、operation与原始equal差异；只有当请求和读回的faceLightingModel都为kInvisible且operation都为kSet时，face lighting quality/color mode/opacity三项标为inapplicable（SDK：faces are not drawn），不宣称原始全项等价。其余活动值及operation严格相同，否则仍拒绝；尤其edge modifiers、mask的kSet不得宽放成kEnable。此候选是否实际被view采用尚待实测。没有改共享字典、factory配对、三异常控制、两档五文字矩阵或.55校准阈值。

v9已实际编译exit0，10冻结输入/live副本逐项匹配，小diff为 `evidence/candidate-v9-v8.diff`，源码冻结交controller一次fresh nonce。

## v9 实际活动差异与 v10 一次时序诊断

v9 session `阶段1/runs/elevation-native-20261103-111210-3ff2980c`，controller在服务中断后恢复同session的标准watch，未重开宿主；已stop70、4退役6保持、自有active0，无fatal。原报告/trace/closure逐字快照 `evidence/native-v9-custom-style-differences/index.json`。实际closed failed；control2仍未到overrule，主未进入、frames=[]。

v9四个活动项不符：edgeStyles请求5/kSet→读回4/kEnable；edgeModifiers0→8；viewportTransparency false→true；lightingEnabled false→true。其它值相同，实际整套读回与v8相同，故不能继续猜属性值或把这些当作custom身份差异。保留原失败，当前不具备已核采用custom合同的view。

本机2020 setter/getter合同没有首次update后才生效的保证，太早读回只是待证假设。只读查阅的[Autodesk 2014图形变更说明](https://blog.autodesk.io/graphic-changes-in-autocad-2015/)只支持kernel/style入口背景；旧2015 zip链接实际403，未读取。Autodesk ADN维护的[2022重构BlockView](https://github.com/ADN-DevTech/BlockView)之[离屏源码](https://raw.githubusercontent.com/ADN-DevTech/BlockView/master/BlockViewCommands.cpp)先设置device尺寸、创建view/style、挂device、空update，后加model/drawable/camera。该后续版本仅为初始化线索，不当作2020实测或延迟生效合同。来源与限制记于 `sdk-public-contracts/style-timing-sources.json`，未复制其共享设置或renderer调整。

root批准v10保持同一custom setter及所有值，完整记录A立即setter后/B明确1024×768 onSize及原正交相机后/C无drawable自有device首次update+view.flush后三组16项原始trait值/op/差异。A/B仅标diagnostic、gateApplied=false，不称通过；C gateApplied=true，活动合同仍严格核值/op，失败正常终止，绝不吞edgeColorFlag等差异。若C真实满足，才继续原control2/主5×2流程，以免时序假设成立后还多开一宿主；并不提前认定采集支持。

v10仅此分阶段读回变动，无重复setter/属性循环，无renderer、共享GS、安全/profile或矩阵/阈值变化。实际编译exit0，10冻结输入/live副本匹配，小diff `evidence/candidate-v10-v9.diff`；源码冻结，待controller一次fresh nonce。

root补充的只读区分点已加入同v10候选：在任何getter前，单独构造默认AcGiVisualStyle并完整snapshot其16项值/op，明确actualOrigin=default-object-before-any-getter；A/B/C则标getter-output。没有往readback塞哨兵或改变setter/流程，尚不认定getter未改输出。首次v10编译receipt `build/n9c-20261103-111714-486-staged-style-readback-v10-0fd62254/receipt.json` exit0保留；补此读取日志后的上列最终v10receipt亦exit0、10冻结输入复核通过。

## v10 实际排除时序假设与 v11 唯一持久样式ID路线

v10 session `阶段1/runs/elevation-native-20261103-111940-506b9b2d` 已标准watch4退役/6保持/自有0。原trace SHA `D0192C5BCD92DC60455086B05AE13514FF9A57C1BD809ECCFB07B84ED7D1EE4E`，快照索引 `evidence/native-v10-staged-readback/index.json`。D默认对象→A真实变化6个字段（face model/quality/color/opacity、edgeStyles operation、edgeColor），所以不能称getter没改输出。A/B/C全部16值/op完全相同，4个活动差异仍在，所以等待device初始化/首次empty update的假设亦不成立；不再重复时序。仍closed failed，无主文字或原RGB。

下一接口有本机2020明确依据：AcDbVisualStyle的copyFrom(const AcGiVisualStyle*)/copyTo(AcGiVisualStyle*)公开映射；db.getVisualStyleDictionary(kForWrite)仅打开自己库字典；AcGsView的ObjectId setter要求有效既有styleId，且同版CHM明确应在AcGsModel已加入该view之后调用。getter返回该view设定style ID。合同条目与头SHA补充在本次sdk-public-contracts。

root批准v11只走此ID路线：把原custom值copyFrom到新AcDbVisualStyle，入同一自有SideDB字典 `N9C_OWNED_XY_CUSTOM`，关闭后重新打开copyTo，严格核对全部16原值/op（不是只核活动项）；记录style.database/objectId/ObjectId.database均为目标库。然后view.add(精确自有marker,同model)成功后setVisualStyle(ownedStyleId)，读回精确id/database，并保留实际value getter的16项观察。render每次erase/readd后都按同前置条件重新绑定并核ID及活动合同，既无重复文字变换也不遗失model前置条件。

持久style由自己库字典管理、post后close，重开读完close，没有跨清理持有对象指针；view删除后清styleBound，再保持model/device/kernel配对清理，最后销毁自己的db/style字典并清styleId。持久样式和view的关联不活过该库，不动ambient字典/working库/共享GS。

若ID绑定与DB样式严格回读均成功，但view活动值仍不满足，则有限诊断分支明确可达：在lifecycle2自己的Session内安装精确overrule后、原after-overrule故障注入之前，最多尝试一次无文字calibration update、真实WD/context和ATIL原RGB；记录diagnosticOnly=true、qualification=false、faultPoint=2及真实sessionOutput、callback/mapping错误。原RGB先落盘再检查标定，颜色/映射不符继续保留，不转成采用成功。随后仍抛style mismatch，使resource faultReached=false/passed=false；主Session未进入记NA，不跑五文字矩阵、不调用N9b形成合格文字结果。若真实context不匹配，原拒绝仍阻止继续绘制，回调错误如实记录，不能伪造缓冲成功。若活动合同真正满足，才按原三异常控制和主5×2采集继续。

v11已同工具链实际编译exit0，10项冻结输入/live副本匹配，核心与公共证据类保持；小diff `evidence/candidate-v11-v10.diff`。尚未运行，源码冻结待controller一次fresh nonce。

## v11 实际持久ID绑定、零回调与 v12 BTR入口

v11 session `阶段1/runs/elevation-native-20261103-112931-c6b6091b` 已标准watch stop70/4退役6保持/自有0。原报告/trace/diagnostic/closure逐字快照 `evidence/native-v11-id-zero-callbacks/index.json`。实际自有AcDbVisualStyle关闭重开copyTo全16值/op一致，view ObjectId及database精确相同。值型getter实返false，它没有返回可用AcGiVisualStyle；旧日志的default输出不能当作已采用的错误属性。v11旧文件原封保留，此处更正解释，不补算成功。

有限诊断实际在lifecycle2可达：精确overrule已安装，view erase/readd后重新核ID，device.update/flush正常返回，但真实WorldDraw callbacks=0、callbackError为空，未读取ATIL/无RGB，diagnostic结果qualification=false；随后正常完整cleanup、ambient1→1零写。原control2 fault未达/failed，主未进入NA。不是文字或glyph RED，也还未证明真实context数据库。

2020 AcGsView::add合同输入是scene graph根，children经AcGiGeometry::draw遍历，典型AutoCAD根为modelspace BTR；该方法总返回true，所以add=true不能证明实际遍历。ADN BlockView公开示例亦加BTR，仅作入口依据，不据此断言直接实体不受支持。root批准v12唯一根改同自有SideDB的modelspace BTR，并核它只有精确原marker一个child；原point保持为唯一overrule目标，不加载其它实体或业务对象。保持同model/kernel/样式ID配置与工厂配对。BTR读引用在erase所有drawable后关闭，数据库仍最后销毁。

有界只读诊断仅记录前64次精确目标isApplicable、setAttributes基类原flags和viewportDraw，累计计数仍完整；不改flags、不加RegenDraw，viewportDraw转调基类。记录自有view visible/valid、device valid、getDevice/getModel、viewport/pointInView/worldToDevice。2020只有setDisplayUpdateSuppressed而无相应公开getter，明确其观察为null；不把isValid称update enabled，不调用show、不改抑制/global配置、不新增刷新路线。原invalidateAll保持。

样式事实和资格入口亦按公开合同修正：ID路线的证据为有效同库持久AcDbVisualStyle源16值/op关闭重开严格回读、精确view ID/db绑定，每帧重新核来源。value getter=false时observationAvailable=false，所有actual/op/equal字段null，观察未知，不再将默认输出制造差异或强制要求此overload转出ID配置；这是本驱动添加且无SDK保证的门槛，不是采集所必需。getter=true则仍核活动值/op，真实矛盾拒绝并只准原有限diagnostic分支。此配置不声称值型getter通过或全面renderer一致；实际黑/白/彩色标定、映射、WD真实context与后续占位像素各自严格验证，缺任何一项均失败。若control2真实到达注入点再正常运行原三资源控制及主5×2，不额外增加文字矩阵。

v12已实际编译exit0，10冻结输入/live副本匹配，核心和公共NativeEvidence保持；小diff `evidence/candidate-v12-v11.diff`。源码冻结，等待controller一次fresh nonce。

## v12 实际主流程零遍历与 v13 普通view更新候选

v12 session `阶段1/runs/elevation-native-20261103-114052-48923b9f` 已标准watch4退役6保持/自有0，无fatal，原文件快照 `evidence/native-v12-btr-zero-traversal/index.json`。三控制均实际故障到达、passed/cleanup/resourcesReleased=true、ambient DBMOD1→1零写。主流程已进入，BTR唯一根归属及精确device/rootModel正确；view visible=true、markerPointInView=true，viewport=[0,0,1023,767]。然而device.update前后viewValid/deviceValid均false，所有isApplicable/attributes/WorldDraw/viewportDraw计数均0，无RGB。不能再归为仅overrule未命中、view隐藏或直接叶节点入口已证不支持；实际是未见GS遍历完成。

本机2020 CHM普通device.update合同说传播到所有owned views并force correct picture，故未证明无参版本必异步。普通view.update说明非interactive时会把queued graphics变换/栅格化至device；view.flush则明确仅用于interactivity。SyncBehavior两个重载尽管在头中声明eSync，但同版CHM均For Future Use，不在本候选调用。onRealizeForeground/BackgroundPalette只是GUI焦点时实现palette，无离屏初始化保证；manager.createAutoCADOffScreenDevice条目只承诺创建离屏device，没有需要可见window或idle的合同，不能把当前失败归因同步命令/隔离桌面。相关普通device/view update、flush及两个Future Use overload共5个原字节HTML与SHA独立归档 `sdk-public-contracts/update-contracts-v13/index.json`。

root批准v13保持一次普通device.update并独立记录后状态/回调，再读取自有view.isInteractive；只有真实false才调用一次普通view.update并记录后状态/回调。若意外interactive，准确拒绝本非interactive测试合同，不调用interactivity控制。删除原非interactive不适用的flush。这是有公开合同依据的直接view更新一次有限候选，不称已修复或普通device版异步。原BTR/model/style、相机/尺寸/三资源控制/5×2矩阵及所有context/标定门保持；不触Future Use eSync、palette/show、renderer、suppression、硬件或共享设置。仍零回调或错误context都严格失败。

v13实际编译exit0，10冻结输入/live副本匹配，唯一源码变更为driver，小diff `evidence/candidate-v13-v12.diff`；源码冻结交controller一次fresh nonce。

## v13 实际更新路径未遍历与 v14 自有device工厂配对

v13 session `阶段1/runs/elevation-native-20261103-114539-74058ce5` 已标准watch4退役6保持/自有0。原文件快照 `evidence/native-v13-direct-view-zero-traversal/index.json`。device后全部回调0，实际isInteractive=false，直接view.update后仍全部0且view/device invalid。无context/RGB；主ambient DBMOD1→1、五类写事件0、cleanup=true。入口时序方向到此停止，此事实不扩大为所有GS/API失败。

本机2020 kernel.createOffScreenDevice明确支持离屏绘制，deleteDevice要求先移除所有view；getter可读this device实际renderer。setDeviceRenderer合同只作用this AcGsDevice，kSoftware说明不依赖GS配置采用软件加速，是自有对象可配置入口，不等于改全局硬件加速；但当前实际renderer原值尚未读取，直接选软件仍是新选择假设。kFullRender在同版文档是高端renderer，不可把样例的software注释当成kSoftware。4个请求的2020工厂/delete/RendererType/setter原字节条目及补充getter共5项SHA归档 `sdk-public-contracts/device-contracts-v14/index.json`。

[ADN后续BlockView离屏源码](https://raw.githubusercontent.com/ADN-DevTech/BlockView/master/BlockViewCommands.cpp)实际选kernel.createOffScreenDevice并在结尾kernel.deleteDevice，非此前manager包装器；其2022重构版本只提供工厂配对线索，不是本机2020通过证据，也不复制其FullRender/Gouraud、交互或current图处理。

root批准v14唯一行为改变：同已取得kernel.createOffScreenDevice创建device并同kernel.deleteDevice销毁，view先erase/delete，model仍沿manager配对；3控制和main均共用此实际路线。记录device.graphicsKernel与精确kernel一致、getDeviceRenderer原值；不调用renderer setter、不同时切model、无fallback。原BTR/style/相机/尺寸/更新路径/严格context和像素门都保持。清理原有manager device调用完整移除，不混factory。

v14已实际编译exit0，10冻结输入/live副本匹配，核心/公共证据保持；小diff `evidence/candidate-v14-v13.diff`。源码冻结交controller一次fresh nonce。

## v14 实际默认renderer与 v15 自有software device

v14 session `阶段1/runs/elevation-native-20261103-115032-d019a224` 已closed failed并有标准watch closure（explicit termination，normalQuit=false，sourceSHAsUnchanged=true）；原文件快照 `evidence/native-v14-kernel-device-default-zero/index.json`。主trace实际device.graphicsKernel==expectedKernel，rendererOriginal=0(kDefault)。device和非interactive直接view更新后全部回调仍0、view/device invalid，无新context/RGB。完整device工厂替换没有解决，不能把未遍历扩大为所有GS失败，也不能声称普通无参更新已证异步。

root批准v15唯一新行为：在自己的新kernel offscreen device上、任何view创建之前，一次setDeviceRenderer(kSoftware)，立即getter要求实际等于kSoftware，分别记录old/requested/actual和viewExists=false。依据2020 RendererType和setter原文：软件加速作用this device、不受GS configuration设置影响；不调用kernel.getConfig或全局硬件设置，不是关闭用户硬件加速，也不归因于硬件故障。没有fallback/枚举循环或kFullRender，不同时更改model/view/BTR/style/update、字体、相机尺寸、三控制或所有context/像素门。device/context身份增加software-device，后续观察仅绑定此实际设备路径。

v15已实际编译exit0，10冻结输入/live副本匹配，核心/公共证据不改；小diff `evidence/candidate-v15-v14.diff`。尚未运行，源码冻结交controller一次fresh nonce；若软件请求本身不被实际getter采用或仍无回调/context，保留真实失败。

## v15 软件请求未采用与 v16 非持久正对照

v15 session `阶段1/runs/elevation-native-20261103-115428-2b9e8739` 已标准watch70/4退役6保持/自有0。原文件快照 `evidence/native-v15-software-not-adopted/index.json`。control1通过；control3在建view前实际old0/request1/actual0，严格拒绝软件请求，故原fault3未达、主未进入。此轮不能描述为又一次零回调/软件设备绘图失败，更不能据此选别名、枚举fallback或归因硬件。renderer/入口/样式参数尝试到此停止。

root批准v16恢复v14同一默认kernel-device/manager-model配置，删除v15软件请求和software身份；主BTR/3控制及既定参数不变。仅主明确zero-WD标记、总callback0/frames空、主及3控制resourcesReleased=true且ambient稳定后，才新建同case独立诊断Session `nonpersistent-control` 一次，不另启host。其它主因（context/字体/style/资源/infra失败等）不触发。

新增短小PrimitiveRoot直接继承公开AcGiDrawable：isPersistent=false、id=null、subSetAttributes=kDrawableNone，真正由GS调用subWorldDraw并仅绘原同几处非文字calibration；subViewportDraw只计数。2020 isPersistent文档明确非持久对象是pointer-based，由GS经drawable指针直接访问，区别持久AcDb的open/close适配。该版声明没有公开gsNode/setGsNode义务，不伪造缓存。SDK声明及isPersistent/id/subWorldDraw/subSetAttributes原文SHA归档 `sdk-public-contracts/nonpersistent-contracts-v16/index.json`。没有手工调用worldDraw、伪造context或安装point overrule。

诊断沿同device/model/view、同相机/1024尺寸/自有style及实际getter条件，只换GS根为该稳定指针对象；真实callback>0且无回调错误才读ATIL原RGB和原标定。context/database只记录真实回调，不要求或伪装其为SideDB资格；不请求文字，不调用N9b形成采集资格。diagnosticOnly=true/qualification=false、原main failure始终保留；结果即使得到原RGB亦不使native-case成功。若非持久能画而BTR不能，才定位持久遍历适配；若也不能，只说明本默认链仍未完成，不能泛化所有GS。

生命周期先完整清理主，再建新Session；PrimitiveRoot地址保持到view移除/销毁和model销毁之后才reset。diagnosticSessions保留实际资源并参与总cleanupComplete与unload拒绝，不能因主已清理就假报所有资源已释放。诊断另有ambient指针/DBMOD/写事件观察，异常仍走同cleanup。两个观察回调(subSetAttributes/subViewportDraw)日志局部catch并置callbackError，不让IO异常穿过GS边界，返回原flags/void；subWorldDraw原catch保持。该冻结前边界校正无新矩阵，首次v16编译receipt `build/n9c-20261103-120241-208-nonpersistent-positive-control-v16-d8c37d47/receipt.json` exit0保留，最终小修后上列receipt亦exit0。

最终v16共10冻结输入/live副本核对，核心/公共NativeEvidence未改，小diff `evidence/candidate-v16-v15.diff`，边界小修 `evidence/v16-callback-boundary.diff`。源码冻结交controller一次fresh nonce，不重跑旧N9b/N9a。

## v16 实际闭合检查点：采集未接通

v16 session `阶段1/runs/elevation-native-20261103-120532-36fd1d3d`，PID18476，controller已标准watch精确stop70、4暂存退役、6输入保持、自有active0；不是正常QUIT。closure原SHA `00E79891EF7B27DE37882B630A9CF33747EA10934E82C01D96122089FAC4DE4B`。原主/诊断trace、result、三故障控制、closure/retirement及一次自有模块清单逐字快照与SHA索引 `evidence/native-v16-nonpersistent-zero-traversal/index.json`。未改任何原输出。

主流程实际failed，原错误 `GS produced no real owned WorldDraw callback` 保持，qualification=false、frames=[]、nonpersistentControlAttempted=true。主和独立nonpersistent-control都attributes/WorldDraw/viewport=0；可见、非interactive，device/model归属正确，更新后view/device仍invalid。诊断原result SHA `A0D5D4C98CE7E0F72BA44260FEF5DE541F7C95475D9CC86A1C1BE610D9C7A8BA`，diagnosticOnly=true/qualification=false、RGB=null、mappingVerified=false、sideDbSuccessClaimed=false。没有任何真实context数据库观察或原始像素，不能称采集能力完成/文字或glyph RED。原RGB独立全单元覆盖检查及实际帧负例亦未能执行；离线checker自身正负例不能替代它们。

三资源故障控制真故障/清理通过；主与诊断cleanupComplete=true，ambient DBMOD均1→1、五类写通知均0。诊断资源参与实际终态与unload，原主failure没有被诊断状态覆盖。作者始终未启动/控制CAD、无子代理/审查/提交/切支，也未重跑旧N9b/N9a。

一次自有已加载模块清单 `owned-process-modules-v16.json` SHA `35ABD821EFA4AFC141CCA88222321087E2CADC0EE1D426484389EBEDD373B81F`，controller记录373项，含acgs/OGSGraphics/OGSDevices/heidi15/d3d11/D3D10Warp及gdi15/null15/paint15/hlr15等。这仅为loaded inventory，不能证明活动driver，不能归因为null15或其它单个模块。非持久正对照亦未遍历，故当前不能只归为AcDb持久遍历适配问题，更不能断言所有公开GS路径失败。

本次交付状态DONE_WITH_CONCERNS，采集本身未完成；源码/receipt仍为上列最终v16。后续只可在新独立任务中核宿主GS初始化及默认链有效性等事实，本轮不提出/编新参数候选，不做全局或GUI实验。有限设备占位、完整TCH/字体缓存/Current DWG和生产BUILD资格均未授予。

## v17 新增启动命令上下文验证（尚未实测）

root只读boot确认本case一直在启动SCR尾直接调用BCE_TEST_RUN；S1QUIETSTART只装timer/reactor并恢复对象依赖，不证明此时SCRIPT已结束。2020 CMDACTIVE官方页只规定0无命令/4脚本/64ARX等位义，没有SCRIPT抑制GS的保证。这是新增宿主启动命令时机假设，不是再次尝试v10/v13的view内部更新时序，原v16失败和模块名不明因的结论保持。

root批准v17只新增一次最小idle-hook调度：BCE_TEST_RUN load/validate身份，固定nonce/PID/thread/current/active/working，写entry快照并登记acedRegisterOnIdleWinMsg；不创建GS/库或终态Evidence。实际idle callback在同线程/原doc/db下，只有CMDACTIVE0、CMDNAMES空、isQuiescent=true、inputPending0才记录唯一idle-ready，置queued后sendStringToExecute固定BCE_GS_CAPTURE（false,false,false）。无sleep/定时框架、逐idle日志、OPEN/REGEN/切库/显示或bridge/watch改动；eOk仅排队，queued后不重排。重复精确入口只记no-op，不再注册/排队。

正式modal capture再核fresh完整Config、nonce/thread/doc/db身份、确已唯一排队、SCRIPT位4清零、无额外待输入且为document command context；capture自身ARX active属于正常，不要求自身CMDACTIVE0。先记录capture事实，再成功移除idle hook，才运行原v16 GS主体。body逐字比较（仅函数改名runGsCore）通过，证明见 `evidence/v17-gs-body-identity.json`，没有改device/model/root/style/相机/标定/5×2或非持久对照。idle及hook移除异常局部捕获，错误不穿宿主；移除失败不运行GS、仍保留模块。

读得到的 `dispatch-trace.jsonl` 和 `dispatch-state.json` 给出entry/idle-ready/capture三时刻CMDACTIVE/CMDNAMES/quiescent/inputPending/current/active/working/线程及排队/错误阶段；waiting-idle不作逐次日志。pending hook/已排队未完成capture拒绝卸载。真正capture才建立并终态释放NativeEvidence；若未进入capture，native-case不生成假GS业务结论，由controller按实际dispatch状态限定等待并作infra收尾。现有watch可等待报告不存在，源码未改。

2020注册/移除bool签名、sendStringToExecute、inputPending(0无字符/-1错误)、isQuiescent(无command/LISP/ARX)原条目与声明SHA在 `sdk-public-contracts/idle-dispatch-v17/index.json`；既有NativeQuietWorker只读借其quietIdle门，未改/调用其内部状态。v17首次实际编译缺acutmem.h而exit1，receipt保留，不当native RED；补头后编译及最终冻结均exit0。10冻结输入/live副本匹配，小diff `evidence/candidate-v17-v16.diff`；等待controller一次fresh nonce，作者未启动CAD。

## v17 实际闭合终态：调度已证，N9c仍未完成

v17 session `阶段1/runs/elevation-native-20261103-122027-e03879b7`，PID32760，controller已标准watch精确stop70/4退役/6保持/自有0（非正常QUIT）。closure SHA `4E05F2CAC8B60A5EE9A15AD814DD64CD9BDD10D30ACC7EA9C064B19779BD01DD`；dispatch trace SHA `DE2EEE0CD1861B74105DCFEA3A954B421DAFADFB9C9ED505D828DB212945377E`。本版必要原件逐字快照与SHA索引 `evidence/native-v17-idle-capture-zero-traversal/index.json`；未改原结果。

真实三时刻均为同PID/thread39160及同current/active/working库身份：

| 时刻 | CMDACTIVE | CMDNAMES | inputPending | quiescent | applicationContext |
| --- | ---: | --- | ---: | --- | --- |
| entry | 5（含SCRIPT位4） | BCE_TEST_RUN | 0 | false | false |
| idle-ready | 0 | 空 | 0 | true | true |
| capture | 1（无SCRIPT位） | BCE_GS_CAPTURE | 0 | false（自身命令） | false |

另一次真实BCE_TEST_RUN入口CMDACTIVE1/inputPending10，精确pending no-op，没有重建/重排。最终entryCalls2、captureCalls1、idleReady/queued/queueAccepted=true、queueStatus0，captureStarted/Finished=true、hookAttached=false。gsCoreCompleted=true仅指原GS函数实际执行并返回，不代表native成功。这是实际调度证据，不能只把eOk作执行证明。

原native和gs-result仍failed/qualification=false，原错误 `GS produced no real owned WorldDraw callback` 保持。主BTR与独立非持久正对照的属性/WorldDraw/viewport回调均0，RGB=null、mapping=false；没有context库观察或文字足迹。三资源故障控制、主和诊断清理/ambient保护保持（DBMOD1→1、零写通知）。因此在本次已证后SCRIPT、无输入pending、正式命令上下文条件下仍未遍历，排除“本case仍在启动SCRIPT中执行”这一特定条件作为充分解释，不等于找到根因或全面排除宿主图形链问题。不能仅归AcDb持久适配，不能把已加载模块名当活动driver/原因。

最终只读核定的2020 `AcGsDevice::setDisplayUpdateSuppressed(bool)` 条目Description原文为“For Autodesk internal use only.”，不是Future Use但明确内部专用，无参数表/false具体语义或公开this-device初始化保证。头中的成员归属只证明接收对象，不能补出公开行为；缺getter只意味着原状态未知，不是未采用的理由。本轮因内部专用合同明确不调用、不编候选，原HTML SHA `6F058C7D1ABF972AB5A70FC432ED08FCA74118EF56053D325DE2A67910A8B236`，依据索引 `sdk-public-contracts/suppression-not-used-v17.json`。

最终状态DONE_WITH_CONCERNS，N9c采集未完成；可交付的是冻结探针、真实调度/资源保护及未接通事实。v17源码/receipt不再改，不新增测试/参数候选、CAD或全局/GUI实验，未重跑旧完成任务。后续只能独立核图形内核实际工作链及宿主默认链有效性；本轮没有可用真实缓冲、独立实际像素覆盖验证或任何BUILD/glyphComplete/完整TCH资格升级。
