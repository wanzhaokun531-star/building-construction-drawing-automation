# N9f：非空图元的ID/value样式传递对照

接续任务6，唯一新实现作者。先读本简报与执行/N9d-GS场景关联调查报告.md最后续篇；需要基线事实时定点查N9e报告。工程根 D:/Codex/建筑施工图自动化。当前唯一代码源 tests/native/text-footprint-gs/NativeGsTextFootprint.cpp，与冻结v19 build/n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8/src 相同。case/build工具保持NativeGsTextFootprint及现有build_text_footprint_gs.ps1。作者不得启动/控制CAD、不派子代理/审查、不提交/切支或改共享配置。

## 为什么这一项未被旧结果覆盖

N9e实际确认main/非持久两路kernel一致、视图模型集合存在、根通知正确，仍零绘制。旧v8/v9 value getter=true后立即traits异常、v10仅empty update后异常；v11以后非空更新才使用owned ID，value getter=false。所以直接value样式+非空根绘制实际未测，不能把它当又一次已证无效参数。2020两个setter支持不同输入；未保证value setter会清掉已存ID，因此必须fresh独立两臂，不能同view先ID再value。

## 最小单因素对照

在同一次host、原v17 idle调度、原3异常控制及原main失败保持的基础上，原主明确zero-WD、清理/ambient保护通过后，执行两臂各一次的无文字非持久对照。可以复用现有conditional nonpersistent-control作为A；不要另造完整driver/框架或重复旧矩阵。

- 两臂均fresh独占同构Session，自有同样的DB/marker/modelspace/持久style源（相同configureStyle值）、同manager.createAutoCADModel、同kernel/device/view工厂、相机/1024×768、同PrimitiveRoot绘原五处非文字校准、同默认renderer、同device.update及非交互view.update，保留N9e观察。两臂共享同一绘图代码/trait数值，唯一渲染配置变量为最终setVisualStyle的overload。
- A：沿当前同库AcDbVisualStyle ObjectId绑定，源关闭重开16值/op及ID/db校验保持。B：同样建立并核对持久style源用于同构，但新view只用setVisualStyle(const AcGiVisualStyle&)传入相同的configureStyle值，不先给它设置ID。记录ID getter与value getter原值、16字段/op/equivalence及observationAvailable，fresh view和两臂实例身份分别记清；不要求B返回旧ID，不猜value setter清ID。
- 这是明确diagnosticOnly/qualification=false对照。两臂的可观测traits差异完整保留；**不能让原“必须符合请求traits”的门再次在非空绘制前截断本对照**。初始/每帧两个位置均须相同处理：有安全有效资源/源身份时各执行一次原非空更新；仍保留真实callbackError、非空实际callback、buffer/映射校验等检查。main与三原控制的style资格门一律保持，不能借此使原采集流程合格。
- 不手工调用worldDraw、不改PrimitiveRoot flags/图元/颜色、没有point overrule或文字/N9b/glyph资格。真实GS回调>0且原回调检查通过才尝试一次原ATIL raw RGB及原标定；即使校准失败也保留已写原RGB和实际错误，不加margin/阈值或改颜色使其通过。真实context/database只记录不当SideDB成功。
- A完整清理及ambient保护通过后才运行B；A若只是no-WD/标定失败但清理良好，可以继续B。清理/身份失败则B不运行并记具体原因。两臂root指针保持到view/model销毁，所有diagnosticSessions参与terminal资源/unload保护。外部调度、主failure和原native-case失败保留；任一臂诊断画出东西也不撤销主failed/qualificationfalse，不跑5文字×2主矩阵。

## 结果及范围

结果各有独立trace/result，附一个简洁style-transport比较JSON：两臂执行/错误、requested运输方式、真实样式getter/根通知/回调/原RGB/映射、清理及保护；不以数字地址复用称共享同一实例，不把ID vs value差异扩大为所有SideDB或所有GS结论。若B出图而A没有，只能首先证明这两个实际配置分支表现不同；properties实际采用不同要一并说明，不能宣称两臂已采用完全相同样式。

允许修改上述driver、本任务README必要增量及SDK/差异/本任务证据；唯一报告执行/N9f-样式传递对照报告.md。公共NativeEvidence/bridge/host/watch、TextOccupancy、其它生产源及旧冻结文件保持。不再复核N9e或重跑N9a/N9b/N8a。编译自审后冻结receipt+小diff交root，再唯一controller一次fresh；未实际运行不得预写样式/回调/清理通过。无需为低影响metadata造镜像单元测试；真实对照和原异常/保护覆盖为必要验证。

用户仍要求纯AutoCAD SDK/ObjectARX C++，无COM/CUA/TCH业务命令/面板或私有字段；不改原DWG/快照、用户CAD/共享profile登记/平台DLL，不保存或另存成果图，不自动安装。无配套平面和Tangent SDK不再问。
