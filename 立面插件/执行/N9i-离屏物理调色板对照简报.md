# N9i：离屏physical palette单因素对照

任务6下一有限原生诊断。唯一fresh作者，root确认N9h review通过后才实施。先读本简报与N9d报告末setPhysicalPalette合同续篇。根D:/Codex/建筑施工图自动化，基线冻结v27 build/n9c-20261103-133951-997-n9h-owned-resource-v27-frozen-9c7b8692/src。原N9h已实际准备/关闭92资源，但get/release均0、仍无WorldDraw/RGB；不要把它当provider返回分支已测。

## 新依据及范围

本机2020 AcGsDevice::setPhysicalPalette精确CHM条目说明物理调色板应配合离屏设备；setLogicalPalette仅提供setColor(index)/ACI映射，明确可能不同于physical。原driver只调用logical，从未physical。这不是已证根因，也不是必须初始化的文档保证；仅做一次按公开合同有/无physical配置对照。onDisplayChange是OS色深/分辨率变化通知，本任务不用。不调用内部suppression、renderer或共享配置。

## 唯一渲染配置差异

复用原main zero-WD后conditional两fresh诊断Session，**两臂均manager.createAutoCADModel/direct-value/同非持久Root**、同完整自身DB准备+原资源目录机制、同设备/view/相机/1024、同逻辑调色板数值、同几何/颜色/traits实际观察、同原device+view update及条件ATIL/固定标定。A仅logical（旧manager基线）；B在logical之后、创建任何view前，额外一次device.setPhysicalPalette，传入**与logical同一个、完全相同的256项控制palette**。不用generic模型做B，不同时换工厂或颜色。

原控制palette为probe既有值（0白，其余黑/原padding），不是AutoCAD通用ACI表；不得为让颜色/标定过关而改颜色、补某些校准色或阈值。本轮只测试额外physical调用的效果。若得到RGB但颜色不符，原标定照实失败并保留原RGB，不以画出图元直接授资格。

将原栈上logical控制palette移成稳定Session成员，使两臂logical/physical输入地址都活到device删除后；数值和调用顺序保持，记录共同生命周期延长，不把A/B表现变化归给不同输入。setPhysicalPalette无公开readback，记录scope=this owned offscreen device、count、输入引用/值策略及调用前后正常返回；不能凭void返回宣称采纳成功。两臂数据同源数值比较可据同一初始化代码，不造新hash框架/镜像测试。

N9h的fault1 generic+cache/原fault3/2/main资格不变（physical默认false）；旧generic provider代码保留不新增业务能力。更新每臂模型factory/配对字段为真实manager，不沿用kernel旧标签；B独立路径建议nonpersistent-physical-palette-control。comparison JSON用本task独立语义，包含physical请求/returned而非假readback；A不安全时B仍跳过明示。

保持所有真实callback、身份、资源/目录、buffer/映射检查；实际traits差异只在无文字诊断继续，不放宽main。两臂各一次，diagnosticOnly/qualificationfalse，原main failed不撤销；不跑主5×2文字矩阵/不宣称glyph、BUILD或N9b资格。

## 实施/验证

只改现有NativeGsTextFootprint.cpp/必要README及本项SDK原文/差异/证据。公共NativeEvidence、build/host/watch/bridge、TextOccupancy/其它生产源/旧冻结制品不变；没有新矩阵或测试框架。先编译、自审及冻结receipt小diff交root，作者不启动/控制CAD、派代理或审查、提交切支或改共享profile/硬件/平台DLL。

唯一报告执行/N9i-离屏物理调色板对照报告.md。一次fresh actual后补事实；若仅零回调，仍未知，不再用旧样式/时序组合重试。任何首次raw另留原始buffer，由root独立核图；作者不使用假context/手动worldDraw或替代绘图来补结果。

用户原DWG/快照/未保存编辑、其它CAD保持；纯SDK/ObjectARX C++，无COM/CUA/TCH业务命令/面板及私有字段，不保存或另存成果，不自动安装。
