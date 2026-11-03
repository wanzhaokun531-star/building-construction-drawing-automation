# N9d：GS场景关联与宿主前提只读调查

当前任务属于原实施计划任务6。先读本简报，不需重读全部历史计划。工程根 D:/Codex/建筑施工图自动化；只读代码/SDK/现有证据，唯一写入为 执行/N9d-GS场景关联调查报告.md。不编译/启动或控制CAD、不修改源码/共享配置/旧冻结证据，不派子代理、不提交Git。

## 当前真实边界

N9c的最新冻结代码 tests/native/text-footprint-gs/NativeGsTextFootprint.cpp；实际v17 session 阶段1/runs/elevation-native-20261103-122027-e03879b7。三异常清理控制通过；真实idle后唯一modal capture已证，主BTR根和独立nonpersistent PrimitiveRoot两路均属性/WorldDraw/viewportDraw计数0，device/view更新后valid=false，没有实际context或RGB；指针/DBMOD/写事件保护通过，宿主已清理。此事实不能证明所有GS不可能，也不能只归AcDb持久遍历。无需复跑。

本机SDK C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit，宿主AutoCAD2020/R23.1 x64，v141。2020头/CHM为实际依据，后续版本示例只能作线索。现有公开合同归档在 tests/native/text-footprint-gs/sdk-public-contracts；如需提取其它条目复用既有本机提取方式。setDisplayUpdateSuppressed已核明确For Autodesk internal use only，不能采用或重复尝试。原已试的样式值/视图时序/renderer/启动脚本变体在 执行/N9c-原生离屏缓冲报告.md；只按需要定点查，不重复全部实验。

## 要解决的问题

1. 当前 manager.createAutoCADModel + kernel.associate(ownedDb,model,kMain) 只证明map读回；它是否同时建立该model的场景根及持久对象访问来源？定点核 AcGsModel::addSceneGraphRoot/eraseSceneGraphRoot、AcGsView::add/eraseAll、AcGsManager::createAutoCADModel 和 kernel.createModel/get/release callbacks 的2020公开合同、生命周期和现有示例。模型根注册与视图绑定是否缺一环，不能只按方法名猜结论。
2. 比较真实工作的工程内图形捕获或SDK示例，找最小结构差异。已读ADN BlockView后续版本只作线索，不用其当前图/全局renderer/交互调用直接覆盖本任务。
3. 对测试宿主默认图形链仍未知的事实，列一个最少的只读观测方案：如公开AcGsConfig currentDisplayDriver/driverName/getVirtualDeviceName、kernel/model/device身份、实际模型scene root登记等；必须核公开合同，不把已加载HDI模块名当活动驱动。禁止调用setter或内部接口。
4. 给出一个有区分力的下一验证切片及理由；若证据不足明确未知，不新猜参数或一次改多个部件。评估是否需要先用纯nonpersistent + generic kernel model区分manager模型适配与device链，再接AcDb。此为候选设计，不自行实现。

## 保护范围与结果

用户目标仍是当前方案DWG内生成、保留未保存编辑、未来不同项目；无自动保存/新成果DWG/关闭重开，无剖面/图框图签。无配套平面、无Tangent SDK不再询问；调查不碰TCH私有字段/命令面板/COM/CUA、平台DLL、共用profile/CP登记或其它CAD。

报告应简短：已证事实、精确SDK原文出处、架构差异或排除依据、唯一下一切片、未知项。回消息只给结论及报告路径。不要为每个接口制造新审计框架或全目录扫描。
