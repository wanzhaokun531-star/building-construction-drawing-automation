# N9c：ObjectARX 原生离屏文字缓冲接入

接续Task6的N9b计算核心，目标是在AutoCAD2020内部用公开GS/ATIL生成真实像素缓冲并调用同一TextOccupancy.cpp，运行期不依赖Python、PDF或外部转换器。不是完整字形或生产排版资格。本次唯一源码作者与独立宿主controller分工；作者不得启动/控制CAD，冻结候选交root派controller。工作根D:/Codex/建筑施工图自动化，不提交/切支、发布/安装，不碰另一个平面插件、旧冻结制品和用户CAD。

## 最小实现

只新增 tests/native/text-footprint-gs/ 及独立 tools/build_text_footprint_gs.ps1。如需要离线读原始RGB生成检查图，可新增 tools/verify_text_footprint_gs.py（开发工具）；不要复制/修改N9b的算法，ARX必须链接其同一src/TextOccupancy.cpp及必要纯值类型。冻结构建输入及回执兼容未改native_case_host.py/watch_native_case.py，case名为NativeGsTextFootprint。原生入口BCE_TEST_RUN同步执行；无需N9a的OPEN门或Plot流程。公共NativeEvidence、宿主桥和其它生产src保持。

1. 用独占自有空AcDbDatabase和确定的持久控制对象，尝试公开GS离屏device/view/model关联。参考执行/N9c-原生缓冲采集接入线索.md与执行/N9-文字范围公开接口调查报告.md的GS段，具体签名以本机2020SDK为准。新库不得设置为workingDatabase，不切current/active文档，不创建GUI视图或可见窗口，不改共享GS配置/硬件加速、profile/安全设置或平台DLL。所有新对象仅在自己的临时库中，结束销毁，不写入/保存源DWG。
2. 真实WorldDraw的context()->database()、实体database/objectId及GS模型绑定必须分别记录，目标数据库一致后才作为SideDB文字采集成功。不得手写假context、借用/强转回调、靠cookie或workingDatabase猜已绑定。可以采用manager.createAutoCADModel或kernel.createModel公开路线，但先选择一个证据支持的最小组合；若失败，保留实际诊断再由root裁定最小下一改动，禁止循环蒙试全局设置。
3. 控制沿用N9a已明确的5个单行请求（数字、中文、旋转backward、非均匀嵌套、tracking/oblique），同字高/宽因子/字体绝对路径与SHA，独立实际loadStyleRec校验，不直接继承Plot结果。只接受精确自有对象的overrule；真实text/push/pop布尔语义参考N9a已归档2020官方合同，不能把false成功当错误。新增范围仅为受控字体/单行，不加载原TCH业务对象或改其字段。
4. 为自己的device建立正交view，明确背景白/墨迹黑、视口无框、线宽及实际设备/视图矩阵。先独立无文字标定画面（至少3个非共线点及2个独立位置）验证API worldToDevice、ATIL行顺序/通道顺序/像素相位的对应；不要用文字墨迹拟合映射，不随失败任意扩大误差。固定两档合理像素尺寸（推荐1024×768、2048×1536，同模型视野），两档都保留原始RGB和映射，后续可用于查裁切/分辨率影响。无需声称物理打印DPI；N9b context.dpiX/Y按明确的控制纸面换算设定并说明，不把像素尺寸假称真实打印设备DPI。
5. 每项文字独立缓冲且远离边界，真实GS更新后用ATIL读取像素；根据官方通道定义转换为紧密RGB，不能把RGBA/BGRA原内存当RGB。调用N9b measureTextFootprint，记录墨迹计数、设备完整单元框、世界四角/AABB与帧/设备/字体/来源。原生全缓冲和结果落盘仅为开发证据，不是保存成果DWG。独立离线检查C++四角覆盖全部非白完整单元；故意缩框和遗漏/错误映射必须被同检查器拒绝；无须重跑N9b全部单元测试或旧N9a/N8a。
6. 资源生命周期使用对应factory/destroy配对：移除drawable/overrule和所有持久引用、销毁view/model/device、恢复自有库的关联、释放kernel后再销毁库。异常路径同样收尾。不要卸载仍有回调的模块或把泄漏掩盖成通过。当前/active/working及原6输入哈希保持；冻结原始源只读。给少量有意义异常路径验证，不另造全局监控框架。

## 实测与范围

作者先完成SDK核对、编译及静态自审，报告冻结receipt。root交独立controller以fresh nonce运行一次。controller沿用已核隔离桌面、profile和精确自有进程收尾，无共享profile/CP变更，不操作用户/其他CAD；本case同步完成，不用capture helper。如宿主环境失败，则明确infra，不能把它当绘图范围失败。原生API或context失败记录为实际未通过；不冒称全部公开方法不可能。

本任务不必通过强行拓宽接口或删除检查；但达到实际可用缓冲与映射才称采集能力完成。只能完成探针而未接通时报告DONE_WITH_CONCERNS与事实，root继续处理真实缺口。两档通过仅为实际有限设备占位，不授无限分辨率glyphComplete、原TCH字体缓存或生产BUILD资格。后续完整对象/场景、图层/裁切、字体资源及Current DWG集成另行推进。

唯一报告 执行/N9c-原生离屏缓冲报告.md，包括所核SDK合同、精确新文件、构建回执、真实输出与失败、保护/资源收尾、有限支持域和接入缺口。一次完整review所需清晰diff和最小证据索引在tests/native/text-footprint-gs内。禁止作者派子代理/审查；不制造Git提交。完整报告写文件，消息仅回状态、候选/测试一句、concerns及报告路径。
