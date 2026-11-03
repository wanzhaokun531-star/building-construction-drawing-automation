# N9g 完整限定任务复核（Spec + Quality）

2026-11-03；一次独立、限定范围复核。

**No findings.** 未发现符合 review-agent 标准、由本次冻结差异引入且可从实际路径证明的可行动缺陷。

| 复核项 | Verdict | 含义 |
| --- | --- | --- |
| Spec | PASS | 实现及实际报告符合 N9g 开发诊断切片合同，真实不支持请求与原失败均保留。 |
| Quality | PASS（限定范围） | factory 配对、provider 互斥/寿命、回调异常边界、清理及卸载门禁未发现新增可证明缺陷。 |
| N9c 采集/文字资格 | 未通过 | main 仍 failed；B 为 persistent-adapter-unavailable；无 RGB、无 glyph/生产资格升级。 |

## 范围与证据绑定

已读适用的项目 AGENTS.md、N9g 完整任务复核简报、模型创建对照简报及实际报告、最小证据索引、N9d 报告中 N9f 闭合后的工厂对照合同。按 review-agent 与 autocad2020-objectarx 技能审查；后续资源 provider 设计不作为本次实现要求。

基线为 `build/n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0/src`；候选为 `build/n9c-20261103-131202-815-n9g-model-factory-v24-frozen-34b66b1a/src`，均相对于 `D:/Codex/建筑施工图自动化/立面插件`。完整读取 210 行 `n9g-v21-model-factory.diff`，定点核对相关上下文、自审冻结记录、receipt、2020 SDK 合同及本地 gs.h 回调 typedef；没有扫描共享 dirty 树。

本次只读核对下列 SHA256 与已给指纹一致：

| 对象 | SHA256 |
| --- | --- |
| v21 driver | 57985B34BE1A27EF5D4DB76836F1E0103FC72080B3416DA4E5E9BC32DCBF943F |
| v24 driver | 03E1BAFBFC77F132252B3A15B21ECCF133D40CB6AE0A1C965DB563E2FC74C623 |
| v24 receipt | C24B85974744C985602F19C4BD97B77557D5C484A73FA1338D757182FC0A16CC |
| v24 ARX | F84E1C52799F675DDD32F83C88E0B266E2C4C6839AEA02F6B5105E58D36C561D |
| 完整 diff | 971114013B36FA41CAC7E935883256551D10D296736E5CD8C7746C19E1D07733 |
| 实际 comparison | 85AAA61527E354FFCB9B757C60ACF22EE79C30D264BC0058626B5A355EFC1986 |
| B trace | 72C72AEF9241936FF9A4312465403F1A19FFC6C681EEF8F018F1E3F72CB0B687 |
| closure | 37C2B1D1534D3AC4B114BF28358970A9752F956337351970EA64282453702423 |

最小索引列出的 10 份运行证据全部重新计算哈希并匹配；包括两臂结果、main/native-case、三控制、退役及隔离收尾。运行绑定为 `D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-131409-216c3fca`，PID54164，nonce `216c3fca96874bbeb653107f7363f3f9`；native-case 实际模块 SHA 与 v24 ARX 一致。root 已完成的输入/live/module 核验不重复扩大。

## Spec 核对

- A/B 都设置 `valueStyleControl=true`，串行 fresh Session，共用原 PrimitiveRoot、几何、configureStyle 数值、默认 kernel/device/view 工厂、相机、更新与标定链。A 为 manager.createAutoCADModel/destroyAutoCADModel；B 为 kernel.createModel(kMain,cookie,具名 get/release)/deleteModel。变化包含 factory 所需 cookie/callback，不能从结果差异独断内部原因。
- 入口仍要求 main 实际 zero-WD、无帧、完整清理及 ambient 稳定；A 保护成功后才 B。main 和原三控制继续 ID/manager 路线，资格门没有放宽；没有运行主 5×2 文字矩阵，也没有新增 model style setter、addSceneGraphRoot 或其它 renderer 路线。
- 两臂 model 默认 ID 均 0、value getter 均 false；16 字段实际值保留 null/未知。view 的 direct value getter 在初始及重加后均 true；edgeStyles、edgeModifiers、viewportTransparency、lightingEnabled 四项差异完整保留，诊断臂没有据此前拦更新。每臂 comparison 的四条 device/view update 前后事件与 trace 一致；根 added/erased 各 2、身份核对通过，实际绘制三计数仍全 0。
- B 的真实 get 在 trace:27，`objectIdRaw=3014301743760`、`bNeedsValidation=false`、`threadId=20880`、`call=1`，记录 unsupported 后返回 null；release=0。记录位于 device.addView.after 之后、后续 owned 样式建立记录之前；没有调用栈证据可确定具体触发 API。raw ID 的资源角色仍未知，没有解码、打开或将其猜成样式/图层；cookie 仅是数值标签，没有冒称 context.database 已绑定。
- `render` 在 zero-WD 判定前检查 providerUnsupported；B 没有被置成普通 zero-WD。清理后再次检查 unsupported，B 为 diagnostic-failed、nextArmAllowed=false，comparison protectionVerified=false；native-case 同时保留原错误 `GS produced no real owned WorldDraw callback` 与第二错误 `nonpersistent diagnostic protection or evidence failed`。两臂 qualification=false、rawRGBPath=null、mappingVerified=false；main frames 为空、glyphComplete/capabilityUpgradeAllowed/layoutIntegrated 均 false。

对应候选代码定位：`NativeGsTextFootprint.cpp:257` 默认样式观察；`:382` unsupported/zero-WD 分流；`:414` B 工厂配置；`:484` 两 fresh 诊断臂；`:493` 清理后失败判定；`:502` 串行比较；`:520` 主入口条件及原失败保留。

## Quality 核对

- provider 在 createModel 调用前、受 providerMutex 保护注册；重复活跃 provider、孤儿调用或边界失败会拒绝新 B。Session 由 `vector<unique_ptr<Session>>` 持有，vector 扩容不移动 Session 本体；上下文不依赖栈临时对象。
- callback 对 owner 查找、调用计数、unsupported/观察失败及孤儿计数使用同一 mutex；trace 使用独立 mutex。现有新增路径只出现 provider→trace 的嵌套锁顺序，没有看到 trace→provider 的反向持锁路径；GS create/delete 调用不在 providerMutex 临界区内。边界原子标志及局部 catch 保留日志/边界失败，C++ 异常不会穿过这两个 noexcept callback；没有把该处理宣称为原生宿主异常恢复保证。
- get 仅记录原参数并返回空，没有伪造 drawable/context、手调 callback、猜验证协议或打开对象。release 仅记录，不 close/delete 非本 provider 取得的指针；未调用的 release 不算正确性验证。A 明确为自定义 provider 未安装，manager 内部 callback 状态未知。
- 清理按实际工厂分支销毁 model；provider 在 deleteModel 返回、model 清空后才撤销。PrimitiveRoot、db 及 callback 记录器在 model 销毁期间仍活着。B trace:79–81 为 deleteModel.before/after→withdrawn，:82–83 才销毁 PrimitiveRoot，:88–89 才销毁 db，:92 resourcesReleased=true。creation 返回空或抛出后的清理分支也保留注册状态直至清理；销毁失败不会提前撤 provider。
- resourcesReleased 含 providerRegistered；卸载先查 provider 活跃/孤儿/边界失败、running 与 pending dispatch，随后各 Session 清理成功才 clear/reset。未完成清理时保留 Session，不先销毁 callback owner。定位：候选 `NativeGsTextFootprint.cpp:129–167`、`:416`、`:600`。

## 实际保护、收尾与限制

三异常控制均到达原故障点、清理完整、ambient 不变并 passed。A/B 清理均完整，ambient 指针保持，DBMOD 均为 1→1，五类写事件均 0。B 的 comparison protectionVerified=false 包含真实 unsupported 失败，不能据此称 ambient 已修改或资源泄漏。

closure/retirement/isolation-final 一致记录 explicitStopRequested=true、forcedTermination=true、workerExitCode=70、normalQuit=false；不是正常 QUIT，也不套用旧 session 的 forced=false。4 个精确暂存文件退役、6 项输入核对记录保持；自有 SHOW/前台均 0，relatedCadEventsNeedReview=false。

复用冻结构建 exitCode=0 与此次唯一真实运行证据；本复核没有重构建、启动 CAD、跑旧测试/全套测试、派子代理或二审，仅新增本报告。公开合同没有保证 GS 接受 get 返回 null；本次未在该分支发生 fatal 不能补成此保证。未测的 release、销毁期间 callback、日志/锁边界异常路径仅作静态核对，不宣称动态覆盖。持久适配是本 task 明确排除的能力，不列为漏项；本诊断闭合不代表通用模型纯指针链、callback 适配、N9c 采集或 BUILD 产品能力已通过。
