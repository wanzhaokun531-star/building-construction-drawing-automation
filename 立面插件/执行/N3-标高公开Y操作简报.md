# N3 requirements：原生标高Y操作的隔离能力验证

对应任务1/4公开接口能力缺口。用户没有天正SDK；P1本机官方帮助给出动态标高随移动更新线索。此探针不使用天正业务命令/面板、COM/CUA/私字段/EntMod，也不切换动态状态或猜系统变量/PE valueName。不改N1 final、N2、用户图或共用配置。不承诺固定位置setElevation已解决。

先读执行/P1-公开接口调查.md、N1-原生原型报告.md及2020SDK moveGripPointsAt文档。拥有新增tests/native/level/目录、tools/build_level.ps1、执行/N3-标高公开Y操作报告.md。只读复用稳定NativeEvidence.h/.cpp（不可修改），可复制N1的公开安全观测/生命周期方法到新目录但不得改原文件或引入私有方法。不启动CAD，不派子代理；root统一新隔离宿主执行。

提供独立ARX与BCE_TEST_RUN（独立command group BCE_LEVEL_CASES），同NativeEvidence七键INI/模块SHA/nonce/输入哈希/隔离桌面/输出目录严格验证，caseName PublicElevationY。现框架prepare允许自定义caseName，只需驱动验证本case。结果兼容native-case.json共同身份字段/独占文件完成才释放，并有自有level-case.json。初始/失败不输出成功状态，所有原图/快照hash保持。

从真实参考SHA109BDC…AF4D只读输入打开根10838（P1/N1原型4）的实际TDbElevation，核对非代理/owner/model，公开展开找到真实AcDbText的'2.800'，先记录所有真实夹点/几何/文字完整性。每个实验都从同一源fresh clone到独立准备数据库，以wblockCloneObjects公开映射核对真实类型/依赖/所有权；建立独立workingDatabase RAII并恢复。若图根或观测与预期不符，拒绝不换另一个凑结果。

实验为control无操作、transformBy沿Y+123、按实际读取到的每个grip分别moveGripPointsAt沿Y+123。不要用多次试错修改同一clone或猜grip语义。每次记录实际返回、前后所有grip/所有可读文字/文字位置/边界，保存仅测试新DWG并独立重开再读原生类型/真实文字。原图每步重读保护，异常/所有权硬错误使顶层failed；glyph/几何已知不完整明确partial，不能漏掉错误后判通过。任何输出文件不覆盖，且来源/模块/源码绑定。

评价分开：操作执行/保存复读/源保护可通过；业务效果列observed values及numericDelta，数值不变不是基础设施故障，但只表示该对象/该操作/当前状态未实现所需数值修改。若确有变化，必须确认真实符号位置和全部文字变化，记录是否只移动文字或方向；不把变化直接宣布成固定位置任意赋值setter。未来prepare还需基准/位置分离验证，不能立即接产品。

复用纯策略最小测试或新增策略测试验证状态聚合、缺真实数字/源SHA变化/硬错误拒绝、只读数值比较不改字符串冒充setter。探针属于能力调查，不要求伪造失败业务算法；构建实际执行退出0并记录SDK链接警告。冻结构建输入/ARX/SHA/命令/报告；真实结果由root提供后填写能力矩阵。无Git，不触碰共享会话指针/登记/安装/正式包。
