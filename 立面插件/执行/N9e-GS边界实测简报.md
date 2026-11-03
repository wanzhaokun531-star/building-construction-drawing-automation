# N9e：原离屏链有限边界实测

任务6的下一诊断切片，唯一实现作者。先读本简报与 执行/N9d-GS场景关联调查报告.md；2020精确API依据见该报告。根D:/Codex/建筑施工图自动化，原v17唯一源 tests/native/text-footprint-gs/NativeGsTextFootprint.cpp 已冻结在 build/n9c-20261103-121817-090-idle-command-capture-v17-frozen-167b5fda/src。原实际e03879b7已退出：idle真实成功、主BTR及nonpersistent零回调、无RGB，三异常控制/清理/源保护通过。不要重读全部历史或重跑旧N9a/N9b/N8a。

## 唯一改动

在现有driver内增加有限只读边界观测，仍case NativeGsTextFootprint、v17一次idle调度和原3控制、main/conditional nonpersistent链。保持所有model/device/view工厂、根/样式/相机/renderer/更新与资格条件；不补addSceneGraphRoot（view.add已自动登记）、不改generic model或增新矩阵。

1. model创建后记录实际model.graphicsKernel与kernel地址；view创建后同样读view.graphicsKernel。创建/绑定后及原更新前后，有限记录view.getModelList实际数量/每个model指针以及是否包含预期model。已有getModel(root)/device kernel等字段复用，不新造完整对象框架。读回差异作为事实，不能预写为匹配；对真实不兼容不继续错误跨内核绘图。
2. 在自己model任何view.add之前，使用该kernel.addModelReactor(ownModel,observer)登记稳定地址的AcGsModelReactor；只覆盖root added/erased，两函数始终返回原默认true，记录model/root指针、与本Session预期是否一致、次数，最多32条明细。不得调用绘制/额外登记根/修改传参。回调的IO异常局部catch并记录，不穿宿主；必要观察失败不得变成绘制成功。cleanup配对移除后再销毁model；各正常/异常和unload资源保护包含新观察器，不提前销毁可能回调的对象。3异常控制沿实际同一路，不新增故障矩阵。
3. main与nonpersistent各一次，从该kernel.getConfig()读取公开getter currentDisplayDriver、driverName、getVirtualDeviceName；保留原输出和来源，明确它是singleton显示配置，非offscreen活动driver证明。禁止setter、configure/dialog、setDisplayUpdateSuppressed或内部API。2020长度参数文档含字节单位，采用足够大的零初始化WCHAR缓冲并保守传入长度：传参数字同时不超过可容纳WCHAR数及可用字节数，记录实际分配/参数长度/NUL情况，避免字符/字节歧义导致越界。空/截断/不可得如实标未知，不能默认驱动名或把初值false写成已观测加速状态。

## 实施与验证

只改上述driver和本任务README必要增量；可加本任务SDK原文/差异证据，写唯一 执行/N9e-GS边界实测报告.md。公共NativeEvidence、host/watch/bridge、TextOccupancy与其他生产src、旧冻结文件保持。现有tools/build_text_footprint_gs.ps1复用，不增新构建框架；先实际编译、自审和冻结receipt+小diff，回root后唯一CAD controller一次fresh运行。作者不得启动/控制CAD或派子代理/审查，不提交/切支/安装。

这是一项边界观测，不预设能取得RGB；实际输出全等仍零回调时准确报告，不能猜一个新参数。若主意外真正到达context/RGB，原检查继续作用，不削门以便通过。读取模型配置/根通知和清理的开发结果，不授BUILD/glyph或完整采集资格。

用户边界：原DWG/快照、当前方案内容与未保存编辑、其它CAD、共用profile/CP登记/平台DLL保持；SDK/ObjectARX C++，无COM/CUA/TCH业务命令或面板，不保存/另存成果DWG、不开关闭用户图。无配套平面/Tangent SDK不再询问。

报告先编译/冻结，实际原生后补行为事实、与v17的结构差异及可据结论；明细留文件，消息只回状态/receipt/一句测试/concerns。读实现技能cpp标准/测试按本次范围，避免为只读getter制造镜像测试；真实根通知/原异常控制/资源保护为相关验证。
