# I3b：当前内存实际定义点/文字的SDK只读适配

任务3/4/7的下一切片，必须等I3a最终冻结且完整限定review通过才派实现作者。根D:/Codex/建筑施工图自动化。依赖src/ProjectFacts.h/.cpp的最终值接口，需求背景执行/I3-实际配置绑定调查报告.md；不重读所有历史，不实现伴随存储或普通命令。

## 唯一目标

把当前NativeSceneContext内用户指定的模型根读取成I3a所需FactSource typed subset。采用新增ProjectFactsReader.h/.cpp，复用现SceneReader身份/句柄/UTF转换；不从磁盘DWG读取产品状态，不解析canonicalGeometry或public-filer私有数据。API单次批量读取，接收预期当前identity/revision及明确源句柄；返回这次实际read provenance、模型空间owner与各源真实subset/定位诊断。项目ID、确认单位和用户选择仍由配置层提供，不在reader捏造；它不确认楼层、窗口成员或生成资格。

- 入口及返回前核同一有效current/active/working文档库与context、非零revision及本次预期；中途修改/切图则不返回可用于匹配的完成批次。不悄悄更新expected接纳旧选择。未保存新图也可读。
- 只支持direct模型空间exact AcDbLine/AcDbAlignedDimension/AcDbText。公开kForRead打开并验证owner/class/对象库；不处理block实例、派生TCH、MText/Attribute。记录真实类和unsupported原因，不让不支持或缺失的一件伪装成其它件；可明确证明缺失与读取不可得要区分。句柄大小写/前导零按I3a规范统一，不反构造未知raw ID。
- 只读取I3a已定义字段。点来自start/end或xLine1/xLine2，不取measurement/extents/显示文字框，不触发REGEN/字体加载/field evaluation，不setWorkingDatabase/切库/改全局。Text用原textString、公开布局字段及hasFields；field来源由核心保持unsupported。有效可见性明定为direct模型实体visibility及所属layer on/unfrozen，与既有AnalysisReader支持域一致；不声称视口裁剪或完整绘制可见性。
- 必要原始getter都是公开只读标量/点，不需把整个current scene克隆后求measurement。若实际某getter不只读，则停止并将该项未知，不调用更大有副作用链补数。每次成功read-open责任清晰配对close；不在产品图建临时实体或改资源。
- 本API返回的是有限typed观察，不是ObservedScene/full signature，不写archive.bound/generation资格/RuleInputs/输出归属。SDK读取成功和I3a匹配都不让BUILD开放。

## 实际验证

新增最小独立native测试driver、工程及build脚本，可复用现NativeEvidence/唯一native_case_host.py协议（不改公共Evidence/host/watch/bridge）。合理冻结当前依赖源，不修改已完成I1/I2/GS driver或旧制品。作者不得控制CAD。遵循已用TDD流程：先写新native测试与可链接fail-closed reader骨架，编译冻结后交root独立controller一次fresh RED；root记录业务断言失败后才实现reader，再冻结最终一次fresh GREEN。每版各一次，总两宿主；不把构建失败、基础设施/调度失败当RED。没有额外参数矩阵或重跑既有能力。

测试在独占私有宿主的当前未保存fixture内：从冻结方案只读克隆为测试内存或自建少量支持源，加入未保存中文文字；之后所有读取基于这份current memory。核真实当前四个已审计datum/title/多段楼层文字（可用evidence/current-project-datum-audit.json作测试定位，不硬编码进产品）。另外最少必要反例覆盖当前未保存点/文字修改确实被新读取得、当前版本0/过期/切图拒绝、隐藏/非direct/非exact/field/缺失的准确定位。核心纯值旋转/单位公式已验证，不在native重复整套矩阵。对每次只读区间核真实current/active/working库、DBMOD、对象/资源修改/append/erase/header写事件与fixture根inventory保持；不把正常创建fixture写入混进只读观察。需要保存重开只作测试且不当成本切片产品功能，本切片本来无需保存。

可按原生驱动需要做最少真实证据输出，不能为了测试给产品API加通用故障框架/权限白名单。没有all-pass脚本标志替代源getter原值。若某反例必须进一步支持界面/切图时序才可验证，准确报未覆盖并交root裁定，不调用COM/CUA/天正命令面板。

## 交接

唯一fresh实现作者，不派子代理/审查、不操作CAD/共享设置/平台DLL/其它任务或Git。冻结小diff、编译输入live+copy哈希、模块及一份唯一报告执行/I3b-当前定义点读取报告.md交root。构建/自审与实际实测分开；root controller实测后回填报告，再一次限定完整review。本项不实现CFG交互/NOD/codec/BUILD，不重跑原77/85纯核心或I2/N9旧任务。

