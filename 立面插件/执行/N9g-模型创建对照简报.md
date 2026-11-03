# N9g：纯非持久根模型创建配置对照

下一诊断切片，唯一fresh实现作者。先读本简报与N9d-GS场景关联调查报告.md的最后generic model续篇。根D:/Codex/建筑施工图自动化。基线为已冻结v21 build/n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0/src。原N9f已实际确认ID/value均真正非空更新且均零回调，value实际getter=true/4traits差异仍未前拦；没有理由再调样式参数。此前所有臂仍共享manager.createAutoCADModel，接下来隔离model创建配置。

## 最小对照

只调整原conditional nonpersistent比较为两fresh同构无文字臂，均用N9f B已经实际测过的direct AcGiVisualStyle value路径、同configureStyle原trait数值/实际差异完整记录（无等价门）、同PrimitiveRoot/1024相机、默认device/kernel/view工厂、同update/观察及清理。两臂唯一待比较的配置为：

- A：manager.createAutoCADModel(kernel)，manager.destroyAutoCADModel配对。
- B：公开kernel.createModel(AcGsModel::kMain, cookie, namedGet, namedRelease)，同kernel.deleteModel配对。cookie可用目标自有db指针的公开数值标签，明确仅opaque cookie，不作为context.database已绑定的证据。两个具名函数指针非空，签名严格按2020 gs.h:1042；bNeedsValidation仅原名/原值，不能推成具体open/lock协议。

原main及3异常控制仍为既有ID/manager路线，不放宽原资格。真实main zero-WD且完全清理/ambient保护后才两臂；A保护通过后B才可执行。新B的model->graphicsKernel、view/list/root通知等按现有公开观察核，必须保留各真实结果，不预设generic有效。不同时换renderer、相机、样式、其它参数或内部API；不补显式addSceneGraphRoot。

两臂model创建后可各读一次公开model.visualStyle的ID/库与value overload（沿现有16字段观察，false记未知），用以解释factory的实际默认状态；仅getter，绝不设置model style。配置差异包含factory及其必需cookie/callback，若结果不同不能把内部具体原因猜成已确定。

## 具名callback与边界

纯nonpersistent合同规定pointer访问，不需持久ID getter；本次不实现AcDb持久适配，不打开用户/源DWG对象。namedGet/namedRelease若被GS真实调用，分别记录ID/flag或指针、线程、次数并标persistent-adapter-unavailable；不手动调用来凑证据，不返回假drawable/context，不解码raw id或猜写私有结构。

为非持久诊断提供非空callback不是宣称持久请求可用。get遇到任何实际持久请求只能记录unsupported并返回空，无公开合同可保证该情况下GS如何处理：该分支明确为本dev probe的不支持/可能宿主失败条件，不得转成普通zero-WD或合格采集。release不关闭/删除未由本provider打开的对象。若get/release均未调用，不得以“适配不可用”解释零回调。此受控开发探针不进入生产，没有用户图写入；真实宿主异常保留原始记录由唯一controller精确收尾，不吞错或填假终态。

provider上下文只可指向当前B的稳定Session，寿命包含createModel至deleteModel完成；同一模块同时只一B活跃，所有callback日志异常局部catch。model释放后才撤provider；pending/model资源仍活时拒绝卸载/不提前销毁Session，cleanup/terminal涵盖新状态。callback若异常请求或观察失败，即使得到RGB也不授资格；仍允许记录实际pixel诊断，但不能伪称完整model适配。

## 结果和实施

两臂各一次真实非空update，真实回调满足原条件才原ATIL/固定标定，保留RGB/失败。比较JSON明确model factory及配对、cookie仅标签、callback实际调用/未调用、实际样式/根/更新/清理和保护；若失败始终failed，不把调用未发生当callback正确。main原failure不撤销、全部diagnosticOnly/qualificationfalse、不运行主5×2文字矩阵。

只改现有tests/native/text-footprint-gs/NativeGsTextFootprint.cpp及README必要增量、SDK/本项diff/证据；公共NativeEvidence、build/host/watch/bridge、TextOccupancy/生产源/旧冻结制品保持。唯一报告执行/N9g-模型创建对照报告.md。先编译自审冻结receipt+小diff交root，不自行启动/控制CAD或派代理/审查、不提交/切支/安装、不重跑旧任务。作者未获root确认N9f review完成前不开工。

用户保护：纯AutoCAD SDK/ObjectARX C++，无COM/CUA/TCH命令面板或私有字段；不保存/另存图，不触碰用户CAD、源图/快照、共用profile/CP登记/平台DLL。资料缺失不再问。若新增公开API存在内部专用/Future Use限制，先报root，不临时换第三路线。
