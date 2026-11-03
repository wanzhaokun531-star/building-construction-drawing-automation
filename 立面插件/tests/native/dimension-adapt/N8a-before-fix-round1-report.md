# N8a 尺寸链适配原生小试实施报告

最终实施状态：N8a最小mm1:100小试已实际通过，整体native-case为partial；最终03f2f503包含4个支持场景、3个本方法未通过场景和15个有效反例。完整glyph与生产/当前图资格均未授予。已测源码1880dc7f保持冻结；只补报告和manifest，等待root安排一次独立fulltask review。下文较早“当前/待实测”段落均为保留的阶段历史，以本节和03f2f503真实证据为准。

## 最终真实结果与绑定（2026-11-02）

最终构建入口：

```powershell
& 'D:/Codex/建筑施工图自动化/立面插件/tools/build_dimension_adapt.ps1' -Label green_scratch_derived_binding
```

最终原生入口：AutoCAD2020/R23.1 x64隔离自有宿主，caseName=`DimensionAdaptMm100Nontrivial`，命令=`BCE_TEST_RUN`，既有七键NativeEvidence INI协议。root是唯一controller；实现者未启动或停止CAD。

| 绑定 | 事实 |
| --- | --- |
| 编译receipt | `build/n8a-20261102-230524-268-green_scratch_derived_binding-1880dc7f/receipt.json`；编译exit0，离线planner实际通过 |
| 最终ARX SHA256 | `A82B53196BCB171C12AE8B584A3D72F7CCF737C2516BCBBB13C2DF1F57463DF1` |
| 实际session | `D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261102-230616-03f2f503`，nonce `03f2f503a06840348c8edb8416a06c9f`，PID24672 |
| native-case | `case/native-case.json`；SHA `EE7D547D1BDEBC650BBF3B8D219454452A3A6D8C59D0945936030BA104F907D4`；closed partial，errors=[]；128steps=123 passed+5 unsupported（含2个故障注入验收场景），无failed，不是128全通过 |
| dimension-adapt | `case/dimension-adapt.json`；SHA `A41089859BAC87875CAEF06BDCFAEF38AAD4A726BFDA9802C8CE902955E978BA`；minimumMm100AdaptationPassed=true/allSceneNativeSetupsPassed=true/sourceAndAmbientProtected=true |
| 最终manifest | `tests/native/dimension-adapt/N8a-final-manifest.json`；逐个绑定源码/只读依赖/receipt/二进制/RED/GREEN及全部原生JSON和候选DWG，报告SHA亦写入manifest |

实现者独立再核：最终case目录388份JSON全部严格UTF-8解析成功，无NaN/Infinity；最终live编译输入逐项SHA等于receipt，ARX与实际加载moduleSHA一致；4个保存DWG磁盘SHA均等于场景记录。仅报告和manifest在实测后更新，不改已测源码。SDK rxapi.pdb缺失的LNK4099链接调试信息警告仍保留，编译0错误。

| 最终场景 | 实测结果 | 保存/独立重开声明字段 |
| --- | --- | --- |
| two_mm100_horizontal（必须成功） | supported_public_observations；非整百真实测点/混合移动、H与明确正侧基线达到目标，测值512.375/2488.75，公开format512/2489 | 460项全等；DWG SHA `3DAE431EF31E4C2EE8099527209E51F38C5C21DEB103868DA52345E9394E73BE` |
| one_mm100_vertical_large | supported_public_observations；1段、大坐标、竖向、负侧基线 | 359项全等；DWG SHA `B0887D92A8950406CF906DD1F431AA7E85E202CFF4092D4288E1252D255A2C9A` |
| six_mm100_horizontal | supported_public_observations；6段、非整百多站、负坐标平移 | 864项全等；DWG SHA `7CD8725E96A79AAE4E1E3F61C01D2264C18686F194934E0DB01F8253A8275F60` |
| twelve_mm100_vertical_large | supported_public_observations；12段、百万级坐标、竖向/负侧基线 | 1470项全等；DWG SHA `085A89488B417482B21C3850D86663108910C2314B7E4C17C8945084C62964F4` |
| two_mm50_horizontal | 本次统一transform方法unsupported；native_adaptation阶段 `measurement_does_not_match_station_projection`，并非setup失败 | 未保存，后续节点/基线适配及重开未执行 |
| two_m50_horizontal | 本次统一transform方法unsupported；同阶段同诊断 | 未保存，完整目标适配与米单位显示尚未闭环 |
| two_mm100_noncollinear | 真实法向动作已执行，但目标实际锚点未达到；本方法unsupported | 未保存，不投影覆盖锚点、不外挂Text |

4个通过场景共3153个声明字段逐项比较全等；包括真实非proxy直接模型TDbDimension2根、测点/夹点/分段/测量/format/override、已读纸面/字体宽因子/图层及实际资源集。数值仍为原定1e-6模型单位，没有放宽精度。不是未知TCH全部私有字段完整性或完整制图/纸面glyph验收。

1:50保留具体原始差异：`two_mm50_horizontal-observation-1.json`中transformBy(0.5)返回0，首段xLine几何长度约199.983196203655，但measurement仍399.9663924073101；dimlfac约1.9999999999999996，dimtxt148.75000000000006，将该实际measurement直接输入formatMeasurement返回`\\A1;800`（另一段返回4800）。m1:50的0.0005变换返回0，首段几何约0.199983196204而measurement399.96639240731105、dimlfac约2000、dimtxt0.14875、format799933（另一段4800067）。这里只证明本次统一变换后“measurement即模型几何投影”前提不成立；未证明所有公开方法不支持1:50或米单位，也没有取得真实TCH宿主文字输出。未临时改公式、乘除以猜测因子或改需求过测。

非共线控制：公开moveGripPointsAt实际接收目标 `[22612.75,6437.875,0]`、offset `[-1887.6586075923406,37.25,0]`，返回0；立即读取的实际中节点约 `[22612.749999999996,6400.625000000001,0]`，法向37.25未达到，仍在原测点线。原通用诊断文字为`node move changed another actual measurement anchor`，覆盖“全部预期锚点不符”，但本次raw不足以声称另一锚点发生不当移动；具体拒绝依据是目标自身法向锚点未匹配。失败快照完整保留。

源保护最终实际通过：`source-and-ambient-after.json` postObservationComplete=true、errors=[]、currentActiveWorkingPreserved=true、allPreserved=true；原方案、原参考与含未保存Text控制的ambient各7类write计数全部0，safe公共state、未保存控制及全局值保持。工作scratch懒创建写事件在逐scene `*-source-scratch.json`及fault/binder控制文件单独记录，未抹去或重置为源零写。旧94c641fd原reference真写失败仍在下面历史记录和原raw中保留。

15个有效反例均到达明确阶段：5个request缺项/站点异常、1个源SHA绑定、实际错误段数、实际grip后重复匹配故障、实际缺seed句柄、4个直接derived binder控制，以及独立候选完整适配后measurement/format验收反例。4个binder分别实际命中 `derived_declared_definition_mismatch`、`derived_symbol_key_relation_mismatch`、`derived_symbol_wrong_database_direction`、`derived_reverse_binding_inconsistent`；measurement与format分别在对应acceptance阶段命中，不是setup提前失败。全部不保存/不返回可用候选根。

源与资源身份合同：原SideDB只承担safe公共身份/owner/key快照和持续无写监测；危险getter在owned scratch。真正root/Layer/Linetype仍使用真实SDKclone pair；派生DimStyle/TextStyle使用公开分段角色、exact类、owner/table getAt、明确单层数字mangle key、完整声明shape与双向一致性递归核对。缺pair仍原样记录false，不伪造mapping。显式纸面变换后的资源新建只在自有库观测，1:50本次拒绝，不用其结果扩大支持域。

root实际收尾绑定见session的 `native-result-closure.json`、`reader-retirement.json`、`isolation-final.json`：精确70退出，4暂存退役，6源SHA保持、active0；明确normalQuit=false，不称正常QUIT通过。宿主/共享环境由root控制，本实现未改profile/CP/登记/DLL或冻结bridge。

## 完整改动文件与自审

唯一实现范围的6个构建输入/工具文件：

1. `立面插件/tests/native/dimension-adapt/DimensionAdaptDriver.cpp`
2. `立面插件/tests/native/dimension-adapt/DimensionAdaptGreen.inc`
3. `立面插件/tests/native/dimension-adapt/DimensionTopology.h`
4. `立面插件/tests/native/dimension-adapt/DimensionTopologyTests.cpp`
5. `立面插件/tests/native/dimension-adapt/ElevationDimensionAdaptCases.vcxproj`
6. `立面插件/tools/build_dimension_adapt.ps1`

另有本实施报告和最终 `tests/native/dimension-adapt/N8a-final-manifest.json`。`NativeEvidence.h/.cpp`只是只读编译依赖，源SHA与冻结副本相同；生产src、其它冻结探针/共享工具和正式包无本任务写入。`执行/N8a-资源身份定向诊断.md`由root另派只读audit编写，不冒称本实现者文件或fulltask review。

自审已逐项对照简报：独占候选/失败销毁、真实TCH整体与直接模型根、动态节点路径与重取夹点、非穿越调度、独立基线、显式fixture来源、实际native measurement/format、派生资源严格绑定、声明字段完整重开和源/ambient保护均有对应最终raw；反例到达阶段和拒绝控制均有效。所选mm1:100 1/2/6/12、H/V/非整平移/大坐标可进入下一生产接口的进一步设计讨论；这不是任意段数/类别、任意units/scales/非共线的全域证明。

剩余最小缺项：本方法1:50 measurement/scale语义与米单位显示需要另一个有针对性的公开API试验；非共线实际锚点需要合法不同机制或明确拒绝政策；缺任意类别/段数seed、逐链placement仍必须诊断。所有场景天然host text请求均未取得，public dimBlock没有可读Text/MText，glyphComplete=false、capabilityUpgradeAllowed=false，nativeCapabilities.fieldSetter/glyphPreparation不升级。当前图提交、全部标注生产服务及正式发布未实施。等待root一次完整独立spec+quality review；实现者未自派review，未进行Git操作。

## 当前1880dc7f：独立source scratch与严格derived binding

335a实际94c641fd诊断位于 `D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261102-225502-94c641fd/case/`，root提供report SHA `5893CBACD98791C803BDA8D516FC72C94F7DBE6801855C477615E455AEA75188`。raw已回读：两段DimStyle source12202 `_TCH_ARCH&&100`→target86 `$0$_TCH_ARCH&&100`，TextStyle source34A1 `_TCH_DIM_T3`→target85 `$0$_TCH_DIM_T3`。exact资源类、实际DB归属、声明定义对应一致，但两类sdkCompute=false、pair.value=null，均不在outer TCH root clone map。不能手工assign它们为SDKpair。

实际动态节点/基线操作均status0，两段measurement512.3750000000036/2488.7499999999982，format512/2489，真实站点及baseline达目标；这只证明diagnosticOnly API观测，未保存、未资格化。

335a终于取得完整after：postObservationComplete=true，errors空，currentActiveWorkingPreserved=true；scheme/ambient写事件均0，公共state/globals保持。但reference openedForModify13/modified9/appended5，DimStyle表4→5，新增12202 `_TCH_ARCH&&100`，allPreserved=false。旧source写保护失败原样保留，不迁为passed。root外部70/4退役/6SHA保持/active0记录独立。

root批准最小修正方案后，现正式冻结：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-230524-268-green_scratch_derived_binding-1880dc7f/receipt.json`，构建命令build_dimension_adapt.ps1 -Label green_scratch_derived_binding，编译及planner exit0。case仍DimensionAdaptMm100Nontrivial。live输入SHA已复核与receipt相同，源码暂停等待controller。

生命周期修正：原reference/scheme SideDB持续挂monitor，只采声明的safe common identity与symbol owner/key状态，不调用getGripPoints/explode/font式观测，不warm源、不重置monitor。每场景重新从同SHA会话文件读一个明确可写owned source scratch，关闭输入流后全程挂独立scratch监测；危险公开getter/字体/派生表示及seed clone都在scratch作用域，scratch懒写完整另报，不计作源零写。仅必要seed/dependency进入独占candidate。fault actual snapshot及binder控制也有各自owned scratch，绝不对protected reference做explode。

真实SDK合同仍严验root主pair/owner以及Layer/Linetype映射。公开explode DimStyle/TextStyle另建标明非SDKmap的derived binding：两侧必须是exact公开记录类，属于对应scratch/candidate；owner必须对应实际DimStyleTable/TextStyleTable，owner表getAt(实际key)必须回同ID，并保留前后proof。真实SDKpair若存在必须吻合observedTarget；无pair时仅接受一层`$数字$`前缀+完整source key，不能递归剥前缀或任意同定义A→B。源/目标去数据库局部id/name后的全部本探针已声明shape精确相等；DimStyle的TextStyle引用递归独立binding；跨分段source→target和target→source一致。保留sdkCompute/pair原值和两侧完整definitions，不将derived correspondence冒称私有TCH字段身份映射。

新增直接binder拒绝控制，均在自有独立候选且不保存：实际setXScale修改已声明标量→直接binder拒绝→实际还原核对；复制同定义标准TextStyle到真实新表key→拒绝错名；实际源/目标ID调换→拒绝数据库错向；在实际匹配记录上显式逆向registry故障→拒绝逆向不一致。这些控制必须真正到达binder阶段，copyFrom/setup失败不算拒绝通过。

diagnosticOnly恢复false，重新执行7矩阵和原故障场景。源保护只声明safe getter字段集与连续全库write事件，不冒称对原源执行危险完整读回；完整公开分段/资源/样式和保存重开字段观测全部在owned工作库进行。当前支持/unsupported和保存重开实际结果仍待controller。

## 当前335a1673有界资源与节点诊断

首GREEN86f3341b实际session `D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261102-224722-0627b51b/case/native-case.json` SHA `9B1A1FA784FBA430EEAE81030C21C102942D479FCA5904C7A58515F9FC198092`：8次scene（7矩阵+首measurement故障场景）均在clone_setup报public_resource_reference_mapping_mismatch，setupPassed=false、actual=null、operations=[]，未执行动态适配；必须成功的mm1:100也未通过。只见root和首段两个PUB_DIM283→7A，以及Continuous16→5D映射已通过；原错误混合compute=false与pair.value不同，且错误发生前未写具体DimStyle角色/定义。root及只读audit静态定位到首segment.dimensionStyle检查，但静态定位不代替真实身份事实。

该运行后续fatal fault断言post_adapt_fault_did_not_reach_native_setup在最终source/ambient-after写入前抛异常，仅before存在，因此这次源/ambient公共内存保护不能称已通过；root外部报告70收尾、4退役、6磁盘SHA保持、active0独立保留，不拿它补算缺失的内存后置观测。

root授权小有界诊断：当前receipt `D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-225437-978-resource_mapping_bounded_diagnostic-335a1673/receipt.json`，构建命令build_dimension_adapt.ps1 -Label resource_mapping_bounded_diagnostic，exit0及planner通过，case仍DimensionAdaptMm100Nontrivial。源码已冻结等待controller。

diagnosticOnly=true，仅首两段场景。公共explode的每段dimensionStyle/effectiveTextStyle分别记录source和observedTarget资源的role、class、id/owner、database归属、全部本探针已声明definition；SDKcomputeFound、pairkey/value、isCloned/isPrimary/ownerTranslated与目标是否匹配分开。原真实TCH root clone的完整AcDbIdMapping条目单独记录，明确公共explode派生表示的资源引用不自动等于root实际SDK依赖。四个角色都写closed JSON，不在首个不符时丢弃其余诊断；不assign/伪造mapping，不删除资源门禁。

在完全独占新库内，仍保留资源拒绝结果，然后按root明确授权继续一个diagnosticOnly动态节点分支，采集真实transform/getGripPoints/move/explode测值。诊断永不保存、不发布、不将requiredMinimum通过，不冒称完整资源资格。若适配API失败，其原始操作快照与diagnosticApiOutcome保留，primaryResourceError独立记录。

新增ProtectionAudit finally：挂在源/方案/ambient monitor与完整基线生命周期内，正常返回或C++异常退出均独立尽力读取三库公共状态、globals、两磁盘SHA和current/active/working，after输出每项观察错误/是否完整/实际write计数。某一项getter失败仍继续其余项；未取得或不匹配不报allPreserved；后置异常作为SECONDARY_PROTECTION_AUDIT_FAILURE，不覆盖场景首要错误。场景源seed后置观测失败亦另列sourcePostCheckError，保留原primary错误。其真实运行效果仍待本候选controller验证。

## 当前阶段：有效RED已取得，GREEN矩阵v1已冻结待实测

以下历史段落按发生顺序保留；当前状态以本节为准。有效RED由root独立controller执行：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261102-223238-014cca66/case/native-case.json` SHA `2AD1E6D882F6F7055C444CD8070FACE77B40B0E505AA6755BD415ABD610D66F9`；`dimension-adapt.json` SHA `070835DF91709805C5EEADC492724C22028699672CD50C3D726CAB0BDEFE3C8E`。nativeSetup passed，cloneGeometryMeasurementFormatPaperEqual=true；实际测值399.966392407/2400.033607592，targetProjectStations和baseline明确失败，stub unimplemented。四次实际三空分段所有权分类通过。root报告70收尾、4退役、6SHA保持、active0。此RED来自目标未适配，前述setup/编译失败均未代替它。

GREEN新增私有文件 `tests/native/dimension-adapt/DimensionTopology.h`、`DimensionTopologyTests.cpp`、`DimensionAdaptGreen.inc`；driver继续独立BCE_TEST_RUN，工具仍只编译本目录和只读NativeEvidence。无生产/冻结探针/共享宿主修改。

GREEN构建命令：

```powershell
& 'D:/Codex/建筑施工图自动化/立面插件/tools/build_dimension_adapt.ps1' -Label green_matrix_v1
```

当前冻结receipt：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-224655-614-green_matrix_v1-86f3341b/receipt.json`；case `DimensionAdaptMm100Nontrivial`；BCE_TEST_RUN。已实际编译exit0并运行离线planner测试通过。编译输入逐项SHA包含在receipt的sourceFiles/inputOrigins中，冻结后live输入已复核保持。保留1条SDK rxapi.pdb缺失的LNK4099调试信息链接警告。

离线拓扑测试先创建stub并实际失败：`build/n8a-20261102-223750-743-topology_stub_red-ffeff1e1/build.log`末尾`N8A_TOPOLOGY_TESTS_FAILED: path not implemented`；随后实现后`build/n8a-20261102-223810-608-topology_green-45f5abee/receipt.json` exit0。覆盖分段乱序/方向反转、实际grip乱序、负向左到右/正向右到左、重复边/分叉/重复或倒序站点、缺失或重复grip、非有限值、竖向路径。不拿纯规划测试替代上述原生RED。

GREEN机制：从实际explode xLine1/2建完整有序邻接路径，验证n段n+1节点、每段唯一、baseline共线和测值对站点投影；源整体方向来自几何最远端点后规范化。统一transformBy只按显式原型mm1:100与目标单位/出图比例调整种子纸面及刚体方位，项目锚点随后逐个移动到显式项目坐标；每一步重新getGripPoints/explode，按实际节点唯一匹配grip，核其余节点保持。基线只匹配实际分段baseline上两端语义grip，单独法向移动，并核测点不跟随。

每个实际transform/move状态、匹配的动态grip索引、目标锚点和offset记录到场景operations；每次实际操作后独立closed observation JSON记录完整声明快照。exact detached AcDbAlignedDimension的接管规则沿用5bfcc715及实际诊断，isPersistent继续保留观测。

最小矩阵（均从独立新候选库clone；不构造笛卡尔积）：

| 场景 | 显式项目站点 | 方位/原点/基线 | 单位比例与判据 |
| --- | --- | --- | --- |
| two_mm100_horizontal | 0,512.375,3001.125 | H，12000.375/-3400.625，offset+800.5 | mm1:100，必须实际成功；每站点/段measurement/format/纸高及重开字段 |
| one_mm100_vertical_large | 0,1876.625 | V，1000000.375/2000000.625，offset-700.125 | mm1:100，非整平移/大坐标/负侧基线 |
| six_mm100_horizontal | 0,135.375,630.125,1250.875,1999.25,3201.5,4450.625 | H，-45000.375/16000.625，offset+900.375 | mm1:100，六段与多节点调度 |
| twelve_mm100_vertical_large | 0,120.375,555.625,1110.875,1875.125,2720.375,3650.625,4700.875,5861.125,7130.375,8510.625,10000.875,12020.375 | V，2356789.125/-1456789.375，offset-1100.25 | mm1:100，十二段/大坐标 |
| two_mm50_horizontal | 0,710.625,3245.875 | H，14200.125/17200.375，offset+600.25 | mm1:50，种子统一纸面比例0.5后项目测点不缩放 |
| two_m50_horizontal | 0,.512375,3.001125 | H，12.000375/-3.400625，offset+.40025 | m1:50，实际measurement模型m；dimlfac/format显示mm另验，无合法根级入口不改分段/不外挂Text |
| two_mm100_noncollinear | 同必须成功两段站点，实际中锚点法向+37.25 | H，22100.375/6400.625，offset+800.5 | 真正尝试公开node move后读回；不提前投影覆盖实际锚点，measurement或联动不合则unsupported |

GREEN源保护：方案/参考只读SideDB在准备后挂database reactor，记录全部openedForModify/modified/appended/erased/unappended/reappended/header事件；前后同声明公共根/资源状态和磁盘SHA。ambient只允许controller新建未命名空夹具，测试setup先建立独立AcDbText未保存控制，确认DBMOD非0，再挂监测和保存基线；控制的内容/位置/alignment/高/宽/旋转/样式及当前全局DBMOD/DIMLFAC/DIMSCALE/DIMTXT/DIMDEC/INSUNITS/LTSCALE/CLAYER/TEXTSTYLE/DIMSTYLE前后比较。该控制Text是保护fixture，不是尺寸文字替代物。current/active/working仍外层校核，Working RAII异常恢复。

声明资源集：实际根/分段引用的Layer、Linetype、TextStyle、DimStyle的明确getter集，以及候选四符号表中同一声明字段；记录源/目标Layer映射/定义/名称和实际DimStyle/TextStyle引用映射。effective与base样式的已读变量差异单列，区分override/measurement/format。维度文字只读现有公共dimBlock AcDbText/MText，有则记录；缺则明确host请求缺口。不调用自然宿主draw，nativeTextHostRequest=null、glyphComplete=false、capabilityUpgradeAllowed=false。

保存前必须通过真实锚点、测值、format、纸面高及字宽/字体源检查。只保存owned candidate测试DWG；保存前root完整快照+数据库和已读资源写JSON；独立readDwgFile重开后按同一字段集展平成JSON路径逐标量比较并输出每字段before/after/equal，数值统一1e-6模型单位，其它字符串/状态/字段路径精确比较。不是只比较根数或总测值，不宣称未知TCH全字段完整。任何未通过的候选不返回可用根；重开失败的DWG只是测试证据，支持状态拒绝。

负例：缺seed、实际不存在seed句柄、实际错误段数、重复/倒序站点、缺逐链placement/单位来源、实际grip快照注入重复后的唯一匹配拒绝、源SHA错绑定；另用独立候选完整适配后分别注入错误measurement验收oracle和format验收oracle，必须到对应native阶段、拒绝且不保存。setup失败不算这些拒绝能力。

本轮整编曾因AcDbSymbolTable派生newIterator参数不协变、缺adscodes/acutmem头及C4456局部名遮蔽而exit1；保留4370febd/b9cb7230构建记录，按SDK签名改用公开基类符号表iterator并补头/改局部名后编译通过。97e4a548为中间成功编译候选，86f3341b为当前冻结版本；未伪称旧失败已成功。

当前剩余：controller实际GREEN矩阵、各支持/unsupported判定及源/ambient/roundtrip结果；随后root安排一次独立task review。尚无GREEN原生结果，不能称小试完成、完整生成、正式发布或当前图提交已授权。

## 最新候选5bfcc715：按实际诊断修所有权setup

实际诊断文件 `D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261102-222913-7d6a8f4a/case/explode-ownership-1.json` SHA `A8E627A1F20A23BB0AD93B032A95CE2465446A9F940D15EB080BD68491274E83` 已回读：两个返回均为exact AcDbAlignedDimension，AcDbObject/Entity cast均true，objectId/ownerId均null，databaseNull=true，但isPersistent=true。e11因此closed failed，仍是setup诊断失败，不计有效RED。root报告7d6a宿主70/4退役/6SHA保持/active0。

root据SDK2020 `dbobjptr.h:564–580`及已通过N1三空生命周期合同作出本次明确裁决：本探针AcDbObject explode返回的接管/删除使用cast+objectId/ownerId/database三空+非self/duplicate和实际类型；isPersistent保留原值观测，不作为本AcDb生命周期额外否决。不是普遍改写AcGi draw可用性规则，不授权N7 draw。

已仅修setup分类/cleanup，限定exact AcDbAlignedDimension。unknown、任意id/owner/database非空、self、duplicate仍借用且拒绝、不delete；接管对象由unique_ptr释放。stub/项目断言保持未实现和相同目标。

最新冻结receipt：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-223203-576-red_proven_detached_segments-5bfcc715/receipt.json`；构建命令build_dimension_adapt.ps1 -Label red_proven_detached_segments；exit0；case仍DimensionAdaptMm100Nontrivial。原始失败及中间候选均保留。源码再次暂停，尚待本候选真实运行。

## 所有权setup失败与当前诊断冻结（优先于下述旧候选）

3534候选实际运行session：`D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261102-222423-7cc12dd8/case/native-case.json`，SHA `4D46A0642ACFBF77FB7DBFC4016262B171C494C1D27F8885D1D8772D3AAD1B23`。steps/capabilities空，checkpoint2为before-read-only-reference，错误为foreign persistent explode return rejected without delete；尚未到目标适配断言，明确是setup失败，不是有效RED。root报告自有精确70收尾、4退役、6源SHA保持、active0。

系统诊断发现旧错误文本将isPersistent/database/objectId三条件混在一起，未记录各实际值，不能据此认定原因。N1 `PrototypeProbe.cpp:575`以AcDb objectId/ownerId/database三空认定detached；本机SDK2020 `dbobjptr.h:564–580`的对象指针析构用objectId区分delete/close，`dbmain.h:1663–1666`说明database返回对象所在库。但AcDbObject/AcGiDrawable官方isPersistent文档均称true为database-resident/AcGsModel open-close，故目前不能只凭N1实现或错误字串否定本轮isPersistent；必须取真实条件诊断。[Autodesk官方AcGiDrawable说明](https://help.autodesk.com/cloudhelp/2018/ENU/OARX-RefGuide/files/OREF-AcGiDrawable__isPersistent.html)。

当前只诊断候选：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-222842-648-red_ownership_flags_only-e11d49dc/receipt.json`；构建命令为build_dimension_adapt.ps1 -Label red_ownership_flags_only，exit0；caseName保持DimensionAdaptMm100Nontrivial。

每次explode在拒绝前写closed `explode-ownership-N.json`，记录每个返回值cast、class、objectId/ownerId、databaseNull/databaseMatchesSource、isPersistent、AcDb三空与deleteOwned；先对AcRxObject作AcDbObject/Entity公开cast并排除null/self/duplicate。保留原!isPersistent保守接管/删除门槛，未知/foreign不delete。明确detached且!isPersistent的对象通过unique_ptr持有，getter或异常退出均释放，避免析构时重扫未分类raw返回。

中间 `n8a-20261102-222813-437-red_acdb_ownership_diagnostic-c46ea160`曾编译，但最新root要求先保持isPersistent门槛只取事实，因此未交controller、不采用。源码再次暂停，等待诊断；stub和目标断言始终未改。

## 当前采用的RED冻结（替代下述8673未实测候选）

root在启动前指出跨库JSON包含layer名称，正常kDrcMangleName使该相等断言不成立。因此8673只保留构建记录，未安排CAD；未把它计原生RED。

已按root授权只修测试setup，保持stub及项目目标断言不变：将几何/measurement/format/override/dimlfac/dimdec/dimscale/dimtxt单独比较；真实AcDbIdMapping核对根和各分段LayerId，分别比较公共图层颜色/真彩/线宽/开关/冻结/锁定/打印字段及线型引用的实际映射，并保留source/target名称、定义、ID和mapping状态。不改用kDrcIgnore；纸面参数只声明当前已读字段，不冒称公共资源全集完整。

当前receipt：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-222356-830-red_setup_mapped_layers-3534cf0c/receipt.json`。

当前构建命令：

```powershell
& 'D:/Codex/建筑施工图自动化/立面插件/tools/build_dimension_adapt.ps1' -Label red_setup_mapped_layers
```

exit0；caseName仍为`DimensionAdaptMm100Nontrivial`；所有实际live编译输入已核与receipt相同。源码再次暂停，等待controller。

## 改动范围

- 新增 `tests/native/dimension-adapt/DimensionAdaptDriver.cpp`：独立BCE_TEST_RUN driver、真实10837两段TDbDimension2克隆、独占unique_ptr候选库、公开分段/夹点/测值/format/override读取、显式项目fixture断言及明确未实现adapt stub。
- 新增 `tests/native/dimension-adapt/ElevationDimensionAdaptCases.vcxproj`。
- 新增 `tools/build_dimension_adapt.ps1`：复制实际输入到独立build/src，SHA绑定后用AutoCAD2020 SDK/v14114.16.27023/C++17构建；复用既有NativeEvidence.h/cpp但不修改。
- 本报告。

未改生产源码、冻结N1/N2/N4/N6/N7/M1/M2/I1/I2、共用bridge/host/watch/retire、profile/登记/CP/DLL和正式包。未进行Git操作；未启动/停止CAD。

## RED命令与绑定

构建命令：

```powershell
& 'D:/Codex/建筑施工图自动化/立面插件/tools/build_dimension_adapt.ps1' -Label red_stub
```

首轮构建 `build/n8a-20261102-222146-609-red_stub-16cefd28/receipt.json` exit1：局部辅助函数near与Windows SDK near宏冲突，编译器显示函数标题语法错误及调用点类型错误；仅重命名为points_close后再构建。该编译失败不是原生RED。

冻结receipt：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-222201-998-red_stub-8673e6da/receipt.json`。

二进制：`D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-222201-998-red_stub-8673e6da/out/ElevationDimensionAdaptCases.arx`。

SHA256：`F00E4BA046F37B78383FD7C87863B78C79A15E5AC5EE101A9E3D617AAD4CADF3`。

实际编译exit0，0编译错误，1条SDK rxapi.lib缺少rxapi.pdb的LNK4099链接调试信息警告。receipt内逐个sourceFiles/inputOrigins记录精确编译副本SHA；冻结后已复核四个live输入均与receipt相同。

controller caseName：`DimensionAdaptMm100Nontrivial`；原生命令：`BCE_TEST_RUN`；同名INI仍使用既有七键NativeEvidence协议，不增加或改写共用host契约。

明确目标fixture：origin `[12000.375,-3400.625,0]`，direction `[1,0,0]`，coordinateInput `[0,512.375,3001.125]`，实际锚点及projectedPoints均为 `[[12000.375,-3400.625,0],[12512.75,-3400.625,0],[15001.5,-3400.625,0]]`；逐链baselineY `-2600.125`，侧向偏移 `800.5`；mm与1:100均来自显式fixture；独立预期段长 `[512.375,2488.75]`。目标不取参考显示整数或实际源测点。

有效RED必须先证实真实clone/根类型/2段3节点/公开观测通过，进入实际目标测点断言后因stub保持原状，生成 `TARGET_STATIONS_NOT_ADAPTED`。输出为closed `native-case.json`、`dimension-adapt.json`及原子checkpoint。任何加载、文件、identity、clone或观测setup失败均不算此RED。

## 所有权与观测边界

源SideDB与候选库独立unique_ptr持有；候选clone必须直接模型根、真实非proxy TDbDimension2，任何失败整库销毁，不输出候选DWG或可用根。explode返回只有实际非persistent、database空、objectId空的对象才delete；foreign persistent绝不delete。公开getter在对应源或候选working DB scope执行并恢复。

RED记录公开xLine1/2、baseline、grips、measurement/status、formatMeasurement/status、dimensionText/status、dimlfac/dimdec/dimscale/dimtxt及layer。真实host文字请求未取得，glyphComplete=false、capabilityUpgradeAllowed=false。这不构成未知TCH全部业务字段完整性声明。

RED仅以参考seed前后同字段快照及参考/方案磁盘SHA保护源；current/active/working指针保持，未设置globals。完整无写事件reactor、ambient未保存编辑公共状态、完整资源指纹及保存独立重开逐字段比较属于后续GREEN驱动尚未实现部分，不能提前计通过。

## 自审与待决

1. 已冻结后发现一个须由实际controller判定的setup疑点：clone前后JSON含layer名称，kDrcMangleName可能改名，严格JSON相等可能在目标断言前拒绝；若发生应记录setup失败，修driver setup后再取有效RED，不能改失败报告或冒称适配拒绝能力。
2. 其它1/6/12段、混合方向、整体平移/大坐标、90度、mm1:50、m1:50及非共线控制矩阵尚未实现/运行。格式/真实字形、纸面样式、合法根级单位显示转换均未资格化。
3. 未自派review；待有效RED、GREEN实现与controller实测后，由root安排一次独立完整task review。

GREEN命令/receipt与实际结果：尚无。此处只记录当前真实状态，不称Task4完成、正式发布或当前图生成可用。
