# N8a 尺寸链适配原生小试

这是已批准Task4的下一实施小任务，承接A1纯规则与N1夹点往返。不是重复N1，不是完整NativeAnnotations或当前图BUILD。先读本简报、N8调查报告（若仍未落盘则据简报及N8已知结论执行），再按具体问题读列出的接口。

## 工作区与权限

实际根D:/Codex/建筑施工图自动化，聊天空目录不迁移。独立目录立面插件/tests/native/dimension-adapt/、构建工具tools/build_dimension_adapt.ps1、实施报告执行/N8a-尺寸链适配原生小试报告.md。允许独立测试辅助值类型；暂不改生产src或已冻结N1/N2/N4/N6/N7/M1/M2/I1/I2源与共享bridge/host/watch/retire。单一实现者，不派子代理、不Git提交/切分支、不改其它平面任务。既有改动保持。历史缺件仅查D:/Codex/项目清理归档/建筑施工图自动化_20261102-181100（只读），不覆盖当前不同SHA文件。

AutoCAD2020 R23.1 x64、ObjectARX2020、v14114.16.27023、C++17。运行期业务仅公开ObjectARX C++；无COM/CUA、天正业务命令/面板、私有字段/filer解析/entmod或猜测导出。TMergeDim等平面例外不属于此任务。实现者不得启动/停止CAD，root独立controller唯一串行运行，所有测试仅自有新库/当前未命名夹具图。原参考及方案只读，不能改共享profile/登记/CP/DLL/正式包。

## 目标与边界

在独占新库内，将真实TDbDimension2从参考测点适配到明确项目目标站点和显式placement；观察公开分段、真实测量/格式文字和纸面样式。以事实判断哪些单位、比例和方向可进入下一生产接口。只做探针，不把部分成功标为所有标注可写，不升级nativeCapabilities.fieldSetter/glyphPreparation，不授当前图提交资格。

最小输入用DimensionChain/AnnotationLayout等值类型或明确对应的探针值。目标测点必须源自显式项目fixture，不取参考测值冒充项目。chain.origin、direction、stations.coordinateInput、projectedPoints与实际sourcePoints相互校核；显式每链基线/侧向偏移、同站实测锚点选择及确认来源。A1无逐链placement，不猜purpose到lane的语义。未来生产按类别/实际段数选择已批准seed，缺seed明确诊断；本探针句柄仅用于绑定可重复参考fixture。

已知N1参考1/2/6/12段fixture句柄ACB0/10837/AC83/CAF3，2段几何约399.966392407与2400.033607592而显示400/2400；不能将整数显示当原几何。N1路径tests/native/PrototypeProbe.cpp有公开segments、snapshot、grip_probes及ownership校核；按需复用理念，避免复制整个大文件。N1不是所有缩放/旋转/单位已证。优先复用NativeEvidence.h/cpp和现有构建样式。

## 必做实现

1. 以独占unique_ptr<AcDbDatabase>持有每场景候选库。只从只读参考clone必要原型/依赖到新库，根必须真实非proxy TDbDimension2，直接模型根、无外套普通块/炸散替代。不向借用当前库或外部库写入，不假设wblock可事务回滚；任何不一致销毁整候选，失败不返回可用根。
2. 由实际公开explode分段的xLine1/2、dimLinePoint建立完整节点路径和方向，验证每段、共享点、baseline及测量拓扑；按几何匹配本次真实getGripPoints，不硬编码gp序号，不假设一段定义点方向与全链方向一致。重复候选/不唯一/非路径/非有限值明确诊断。每次节点移动后重取分段和grips，再匹配下一节点；移动次序避免穿越（负向位移左到右、正向右到左，必要先分阶段），移动后逐站重验。基线移动单独匹配真实baseline语义，测点不能跟随基线偏移。
3. 项目几何不因出图比例缩放。可对候选种子先做有证据的统一纸面样式变换，再移动各测点到项目坐标；最终实际测点、方向、基线以目标值为准。所有transform/move/getter返回status记录，eOk不代替结果核验。非共线actual锚点是否可适配先观测，不强行投影覆盖实际点，不支持则明确拒绝。
4. 从公开explode分段读取精确measurement、formatMeasurement与dimensionText、dimlfac/dimdec/dimscale/dimtxt及样式覆盖，区分测值/格式/override；记录实际公开文字输出（有公开可读AcDbText/MText则读取，没有则明确仍缺真实host请求）。不能用计划字符串替代native观测、不能解析TCH私有序列化。纸高/宽因子/图层/样式来源与变换前后各自核对。格式测值符合而真实TCH输出未证时保留该资格缺口。
5. 保存的仅是自有候选测试DWG；保存前公开完整观测字段/根类型/测点/grips/分段/测量/文字及已读取资源，独立重开后按同一字段集合逐项比较并报告差异，不只比较总测值/根数。不能称这些公开字段就是未知TCH全部业务字段。输入参考/方案SideDB和ambient未保存编辑的无写事件、公开状态/磁盘SHA、current/active/working/globals前后保持。生命周期按实际isPersistent/AcDbObject.database/id处理，foreign persistent绝不delete或当transient；异常路径同样恢复。

## 场景与接受策略

最小矩阵：1/2/6/12段各有不同于参考的非整百站点（可含混合左右移动）、整体非整坐标平移/大坐标、横向和90度、mm的1:100与1:50、m声明的1:50、一个实际锚点非共线控制。不要笛卡尔积泛滥；用最少场景覆盖这些维度，报告每一场景明确的目标/实际/接受判据和支持状态。mm1:100非平凡站点适配是必须成功的最小目标；其它未知API维度需实际运行，成功才资格化，失败保留事实并拒绝对应候选，不用宽松精度/额外Text/覆盖数字掩盖。

unit=m时measurement模型单位与显示mm换算分别验证；不能因几何正确就判显示正确，也不能写全局DIMLFAC影响其它对象。若没有合法公开根级方法使真实显示符合mm，应标该维度unsupported，而不是改需求或自动退回普通CAD尺寸。文字/箭头纸面标准与单位比例也分别核对。方向90度测量符号/基线侧向要由显式placement控制。

反例至少包含缺seed/错误段数、重复或倒序站点、缺placement/单位来源、不能唯一匹配的节点、源SHA不匹配和适配后测值/格式不符合时不输出；先验证故障确实到达相应阶段，setup失败不算拒绝能力。为未来接口给出支持域及最小缺项，不造全类别生产框架。

## TDD与交付步骤

- 先做独立driver、同一非平凡mm站点断言与明确未实现的adapt stub，编译冻结RED候选，报告receipt；此时停源码写入，请root安排controller真实原生RED，失败必须来自目标仍未适配而非编译/加载/文件缺失。可同时完成不依赖CAD的纯节点规划边界测试，但不能用其替代原生RED。
- root提供RED事实后实现适配，冻结GREEN候选，报告receipt及编译输入SHA，交controller运行。所有原始失败保留，不能编辑报告数据来“修”结果。每次冻结给简明receipt路径及case名，case名仅英数字下划线最长80字符，BCE_TEST_RUN正常Evidence协议，partial明确。
- 输出合法UTF-8严格JSON、closed native-case.json，具体dimension-adapt.json/逐场景观测/故障点。记录supported/unsupported/partial，glyphComplete=false、capabilityUpgradeAllowed=false。不变更共用宿主桥或重做N7。
- 实施报告列改变文件、准确RED/GREEN命令与本轮真实结果、源/binary/receipt绑定、所有权/资源观测边界、自审及未解决风险。报告完成后root派一次完整task review（spec+quality），不要自派review。不得称已完整一键生成/正式发布或自动写当前方案图。

root负责精确自有host的启动与清理；其AdDownloaderCore.dll既有平台改名状态只读记录，禁止覆盖/恢复。默认不会自动另存、关闭或重开用户当前图。
