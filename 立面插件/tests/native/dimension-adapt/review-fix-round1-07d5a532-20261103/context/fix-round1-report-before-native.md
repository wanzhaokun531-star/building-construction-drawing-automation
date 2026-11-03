# N8a fix round1：拒绝验收与正式源SHA控制

状态：离线RED→GREEN已完成，focused候选已冻结，等待root controller原生实测；未声称review发现已闭环。

唯一发现来自 `N8a-full-task-review-20261102.md` P2。旧helper可捕获自己的未拒绝sentinel而误接受；旧源SHA反例只另比较全零SHA，未测正式入口。旧“15个有效反例”主张撤回为14项原有证据+1项待修正式控制，旧raw和旧manifest均保留，修前报告精确备份SHA45124EB0EB6B2E5ABCE051DC8DA8EBE8F71D6B5540C5C068E169E1407B850ABE。

命令：

```powershell
& 'D:/Codex/建筑施工图自动化/立面插件/tools/build_dimension_adapt.ps1' -Label fix1_rejection_helper_red -CaseName DimensionAdaptSourceBindingFocused
& 'D:/Codex/建筑施工图自动化/立面插件/tools/build_dimension_adapt.ps1' -Label fix1_rejection_focused_green -CaseName DimensionAdaptSourceBindingFocused
```

RED receipt：`build/n8a-20261102-232547-961-fix1_rejection_helper_red-fa4e2a6c/receipt.json`，编译成功后实际测试exit1，错误为helper accepted normal return when name equals expected。GREEN receipt：`build/n8a-20261102-232650-961-fix1_rejection_focused_green-07d5a532/receipt.json`，编译/原拓扑/新拒绝测试exit0。新测试三个分支：正常返回且name==expected被拒绝，operation真正预期异常可接受且原消息保留，错误阶段异常不可接受。

helper修正只catch operation；未拒绝失败在catch外。源SHA控制与正式矩阵共用formal_source_sha_rejection，原NativeEvidence.validate_identity完全只读不改：先健康身份控制，再将参考字节复制到本case/output独立文件并追加显式坏SHA标记；克隆Config只换reference，必须由原validate_identity抛出reference source SHA mismatch。所有原始输入只读；focused不调用尺寸适配/资源/保存重开，不创建CAD候选DWG，坏文件只是明确命名的字节故障夹具。

core-preservation JSON与scoped.diff绑定1880/07d5：适配、资源binding、场景和roundtrip/finally核心逐字未改，五个相关冻结文件SHA保持。变更仅helper、源SHA控制调用、focused分派、构建新增测试输入。07d5 ARX SHA BC0EEB29B77CE4C16F950BFE1CD0B8709EFC572760D38CDD20D3787EBD730885。

focused caseName=`DimensionAdaptSourceBindingFocused`，BCE_TEST_RUN，沿用正常Evidence协议。当前没有新native结果；尚不能将正式SHA缺口标为已通过。root唯一controller启动/收尾，之后交原reviewer scoped重审。原1880/03f2四场景与3153字段保持独立证据。
