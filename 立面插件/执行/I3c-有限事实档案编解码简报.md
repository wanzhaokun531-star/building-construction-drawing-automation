# I3c：多立面有限事实的历史档案编解码

本项继承已批准总计划任务3/7及I3a/I3b的有限事实边界。先等I3b完整限定review通过再派实现，工作根D:/Codex/建筑施工图自动化。它只提供后续同DWG自有字典需要的纯值字节编解码，本项不写字典/不接普通命令。I3a/I3b源与既有G2 codec不改。

## 决定及接口边界

新增ProjectFactsCodec.h/.cpp。历史档案包含同一个persistentProjectId及任意数量的历史ProjectFactConfirmation记录（每个记录按title.rootHandle稳定区分，等价句柄不重复）。每条保存I3a所有确认输入、typed source及原始出处；**不存ProjectFacts中可从确认输入重算的datumPoint/单位化方向/windowCorners，不保存readCompleted或任何当前有效/生成状态**。项目ID须一致；各记录可以来自不同历史identity/revision，不能为编解码重新写成当前身份。原有记录的不同单位确认可保留为历史，下一次真实核对再按当前单位决定有效性。

接口例如encodeProjectFactsArchive/decodeProjectFactsArchive，值类型命名明确Stored/Historical。编码及解码对每条调用现confirmProjectFacts仅作结构/有限数值支持域校核，不能称产生了新用户确认；不要捏造当前Observation或用SDK读取作codec前置。解码只回历史确认输入，后续服务必须用真实reader新读后再checkProjectFacts，不能凭解码成功给currentSubsetMatches/Archive.bound/RuleInputs/BUILD资格。

版本化、带magic、长度前缀且有界的确定性字节格式即可，复用既有G2 codec的合理模式但不改其实现/原schema。明确未知版本、尾随垃圾、截断、无效count/enum/数字/UTF8/项目ID/重复标题handle拒绝，字符串支持中文/换行/分隔符原样往返；finite double须精确roundtrip（max_digits10+classic或等价可移植做法），拒绝NaN/Inf。不执行文本内容或将其作为命令。容量可沿用既有4MiB/16384项级别，必须先核长度/剩余字节再分配，不靠声明count直接巨额reserve；限制只服务档案资源安全，不把样本视图数量/句柄/固定材料角色当业务白名单。

## 必要验证/实施

唯一fresh作者、TDD：围绕多立面（每条多Text楼层依赖）、四getter/所有typed字段、中文/特殊文本、极小/极大但I3a可接受数值、不同历史identity/revision、等价句柄重复/项目ID错配、损坏/未知版本/尾随/恶意长度和解码永不授当前资格写少量真实接口测试。有效业务RED→GREEN；复用现CoreTests/TestSupport/工程及build输入列表，不新建框架。可用I3a fix的冻结命令+focused方式，仅跑新增codec cases；最终需按本改动验证，不重复旧85/8或native32。原生字典事务/Undo/命令行为不是本纯codec任务的测试责任。

仅新增codec、测试文件，修改必要CoreTests注册/项目/build输入列表与本项证据；不改ProjectFacts/Reader、SceneReader、AnalysisCommands、G2/UnitConfig、生产SDK项目、公共host/watch/Evidence或平台文件。无CAD、SDK、共享profile/全局变量、COM/CUA/TCH业务命令/面板、子代理/审查或Git操作。

开始前冻结受影响既有三文件；自审后冻结实际输入/二进制、RED/GREEN日志及小diff交root。唯一报告执行/I3c-有限事实档案编解码报告.md，写清只历史codec已实现，同图持久化/兼容/事务/普通CONFIG/完整生成仍未接入。root一次限定完整review后才继续writer工程。
