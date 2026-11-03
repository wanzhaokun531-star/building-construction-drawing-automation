# N8a完整任务复核的约束与裁定

此包为首次独立full-task review（spec+quality），不是merge/whole-branch review。六个源码/工具均是本任务新增，full-task.diff给出全文；没有本任务Git提交。其它既有改动不属于本任务。正文需求见原N8a简报，实施报告为作者主张，需核对diff与原生证据。

实施计划Global Constraints中与本次直接相关的原文：

- 目标沿用本项目的 AutoCAD 2020 R23.1 x64、ObjectARX 2020及既有 v141 工具链。
- 运行期业务使用 ObjectARX C++，不引入 COM、Computer Use、天正业务面板或私有序列化字段写入。
- 成果直接生成到当前方案 DWG 的选定立面中，保留该图其他内容；配套平面和参考图只读。
- 命令成功后当前文档保持正常待保存状态，由用户照常保存；插件不自动另存、关闭或重开当前图。
- 失败或取消时，恢复到本次命令开始前的文档状态，包括用户此前未保存的编辑。
- 调整出图比例不能缩放建筑；默认1:100，纸面文字和符号标准按实际参考类别核准。
- 参考全部源数据保持只读；测试只修改自有隔离宿主中的可写副本，不操作用户 CAD、修改共用登记或复用其他任务宿主。
- 不改平面插件正式包、发布指针或已验收案例规则。

N8a原简报范围原文：

> 只做探针，不把部分成功标为所有标注可写，不升级nativeCapabilities.fieldSetter/glyphPreparation，不授当前图提交资格。

> mm1:100非平凡站点适配是必须成功的最小目标；其它未知API维度需实际运行，成功才资格化，失败保留事实并拒绝对应候选，不用宽松精度/额外Text/覆盖数字掩盖。

> 保存的仅是自有候选测试DWG；保存前公开完整观测字段/根类型/测点/grips/分段/测量/文字及已读取资源，独立重开后按同一字段集合逐项比较并报告差异，不只比较总测值/根数。不能称这些公开字段就是未知TCH全部业务字段。

执行中的具体裁定（完整出处执行进度.md对应本轮）：

1. 独立新库，不并改Line-only PreparedBatch/InPlaceWriter，不获得当前图TCH提交资格。
2. 真实explode返回exact AcDbAlignedDimension三空(objectId/ownerId/database)但isPersistent=true。AcDbObject持有以公开cast、三空、非self/duplicate/支持类型接管/delete；unknown或任一非空仍拒绝/不delete。SDK dbobjptr.h closeInternal及N1三空为依据。Gi绘图授权不随之变化。
3. 原始reference/scheme持续monitor，只读安全common identity/table owner+key/声明表定义。危险TCH/font/explode/grip观测在每场景独占source scratch（同SHA文件另读）；不得warm原源再清零monitor。scratch懒写单列，不混称源零写。
4. 真实root/layer/linetype SDKpair严验。公共explode派生DimStyle/TextStyle的outer pair缺失时不伪造map：以实际root pair、对应几何分段角色、双方exact类/数据库/symbol表owner/getAt(actual key)、一层$数字$前缀加完整source key、声明定义及递归TextStyle、双向一致性验证derived binding；有真实pair仍必须匹配。实际标量修改、同定义异key、错向/逆向拒绝控制用于验证它；其覆盖不是全DimStyle变量或全部生产资源图。
5. 异常后置尽力采集保护状态/单项读取错误，保留primary错误；缺after或实际source写不能记保护passed。早期94c641fd写失败、两轮无效RED均保留；有效RED是014cca66，正式最终是03f2f503。

最终1880/03f2f503事实由root独立核过：module/receipt及8编译文件/live一致；4个mm100支持场景保存重开3153声明字段差0；123 passed+5 unsupported并非128全过。其它三维度本方法未通过；mm50原observation显示几何长度约199.983而measurement约399.966、dimlfac约2、formatMeasurement直接输入该measurement得800。SDK measurement/cache/格式语义及真实宿主文字仍须后续核验；该日志没有证明所有公开方法不支持50。报告与代码的表述/资格是否准确也是本次复核内容。

复核只读，不改源码/Git/归档/共享环境，不启动CAD、不重跑已有原生/纯测试，不派子代理。按具体现有疑点需要可读官方2020 SDK和直接依赖；新复核报告写执行/N8a-full-task-review-20261102.md。每项找出可操作缺陷/准确文件行和影响，分spec与quality给结论。整个插件、NativeDimensions生产服务、完整glyph或下一阶段不在已完成主张内。
