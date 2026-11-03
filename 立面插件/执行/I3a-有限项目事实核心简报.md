# I3a：有限项目事实的值模型与失效核对核心

本项是已批准实施计划任务3/4/7中“记录单位、实际零点及人工选定立面范围”的第一实现切片。先读本简报；必要背景为执行/I3-实际配置绑定调查报告.md，其伴随NOD存储只是后续建议，本项不实现存储/命令。工作根D:/Codex/建筑施工图自动化，保留现dirty feature分支与其它任务文件，无提交/切支。

## 目的与范围

现有I2只保存单位，ProjectConfig.zeroLevel字符串不表达实际定义点。新增独立纯C++ ProjectFacts.h/.cpp，以typed subset表达确认事实并核对新观察；不复制/放宽完整ObservedScene/ProjectArchive绑定。无CAD调用、磁盘身份、字形范围或代替完整生成资格。本项完成不代表用户已能运行零点配置命令。

仅实现以下一组紧密功能，接口命名可依现有风格决定并在报告说明：
- 有限source支持域：直接模型空间exact AcDbLine、AcDbAlignedDimension作为点来源；exact无field的AcDbText作为标题/楼层说明。类型化持有必要公开定义值，勿把canonicalGeometry字符串当typed subset。记录实际root handle、模型空间owner handle、exact类、direct-root、有效可见性及读取身份/revision出处。Line保留两端、normal/thickness；AlignedDimension保留两xLine定义点/normal（不需measurement、显示数字、dimstyle、字形）；Text保留原串、position/alignmentPoint、normal、rotation、height、width、oblique、两对齐及两mirror/thickness/hasFields。未来SDK reader填这些值，本项不推测源图或实现SDK读取。
- 用户确认事实包含persistentProjectId、已确认mmPerInputUnit及其来源、标题/楼层来源、明确的point selector（line start/end或dim xLine1/xLine2）、实际datumMeters及确认来源、明示高度方向、人工选择平面窗口。零点必须从所选typed定义点导出，不采用文字下缘、邻近拾取点或参考项目数字。首域仅XY二维立面，所有点/方向/数值有限且平面一致，向量非零；不要限定未来项目+Y或mm1。
- 窗口可用UCS平面origin/两正交basis及实际尺寸来表达并导出四角，或保存实际四角并严格验证矩形；旋转XY窗口不能退化成WCS min/max包围框。明确局部selector匹配类，未知/派生TCH/嵌套/field/隐藏/不支持平面返回定位诊断，不能默认有效。
- 核对历史有限事实与一次新的typed观察：项目ID/单位来源与比例、源存在/exact类/owner/可见性、所需subset定义值变化，返回相应currentSubsetMatches/stale/unsupported/orphan事实状态与定位。重开后的旧运行期identity/revision只作历史出处，不能自行判当前有效，也不能只因runtime identity变化便拒绝真正重新读到的相同依赖。新观察的同批identity/revision必须一致。句柄按已有规范识别（大小写/前导零同一，非法/溢出拒绝）；不靠固定样本句柄/根数。
- 只核有限subset。尺寸文字/样式变化不属于本datum subset；其它建筑编辑不被冒称整体未变。有限匹配永不设置archive.bound、CommandQualification、RuleInputs或generationAuthorized=true，不注册输出或生成归属。无独立generation框架/新builder。

## 验证和边界

围绕真实行为增加少量必要C++测试：明确四种getter选择及错误selector、非+Y/平移/单位/非零基准、旋转窗口四角、源点/标题/可见性/owner/单位变化定位、同句柄重新读取与历史/缺失/无效观察、非有限/不支持来源拒绝、有限匹配不产生生成资格。测试不要镜像实现或重复旧77项大矩阵。复用tests/TestSupport.h与CoreTests工程；新增测试/注册及必要build输入列表即可，勿另建测试框架。测试先有效RED（业务断言失败，不以缺符号/编译失败冒充），再GREEN；最终一次现有build.ps1正常自检若自动全跑，可复用该结果，不再重复。编译v141/C++17、/W4 /WX，代码清楚可维护。首个核心不提前实现codec、NOD writer、交互、AnalysisReader改造、SDK/生产native build或额外故障框架。

## 交接

唯一fresh实现作者，只改ProjectFacts、必要tests/工程/build输入列表及本项证据；不得派子代理、审查、操作CAD、改共享profile/平台DLL、源码无关文件或Git状态。开始前冻结受影响基线用于小diff。自审后冻结所有实际编译输入/二进制和RED→GREEN原始日志，交root小diff文件和精确路径。唯一报告执行/I3a-有限项目事实核心报告.md，说明哪些接口已经实现、哪些SDK读写/存储/普通命令仍未接入。不要求临时用户确认，也不写已确认单位/零点的新问卷。
