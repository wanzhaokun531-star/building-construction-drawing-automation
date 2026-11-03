# N8a fix round1：拒绝验收与正式源SHA控制

状态：离线RED→GREEN及fresh focused原生实测已完成；原P2的修复证据已具备，等待原reviewer scoped重审，未自标review完成。

## 2026-11-03 fresh focused真实结果

root唯一controller实际执行 `DimensionAdaptSourceBindingFocused` / `BCE_TEST_RUN`，使用原07d5冻结ARX，未重新编译。session `D:/Codex/建筑施工图自动化/阶段1/runs/elevation-native-20261103-090208-428e9c88`，nonce `428e9c88dabf4953a755da537e0c33c5`，PID9468。

| 证据 | SHA256与结果 |
| --- | --- |
| case/native-case.json | `2E25BCE06514F1664A52B92ED5D51C309FA28CC8A2E8E2C6BA4B91D221E5A957`；closed partial/errors=[]，4 passed |
| case/rejection-focused.json | `A2F83486D421A42AFDE873D7093B3CD244BBD8917B07C9830F5AC8338621FD90`；3个helper控制均成立，指针/globals/原SHA保持 |
| case/formal-source-sha-rejection.json | `6F2AA5DE96B13923F5B9FA9915B526A7883873EBE7DF27BFE57A08C240477D76`；原未改NativeEvidence.validate_identity实际拒绝reference source SHA mismatch |
| native-result-closure.json | `209DC57EF2D782A2C5A049C9EBE7D575B41329F7B1B23A3DD722057C89FB6E28`；受控精确收尾，normalQuit=false |
| reader-retirement.json | `5C8833F0E778580DCB30C2290280B7A0349C5DA8A0CEFD9BE6710AFD2955453F`；4退役、6源SHA保持、own SHOW/foreground0 |

实现者回读全部上述原始JSON并核SHA/模块绑定：实际moduleSHA=`BC0EEB29B77CE4C16F950BFE1CD0B8709EFC572760D38CDD20D3787EBD730885`，等于07d5 binary；原始NativeEvidence.h/.cpp与1880字节相同。

坏源是case输出内独立新文件 `rejection-focus-bad-reference.dwg`：复制1704526原字节后追加明确标记，SHA `0ABFD1534F7C9CD83C065E3593CE0A106CA828984113E2273EE6568FF820195F`。原reference SHA仍 `109BDCFEA4C55219793F2B6F9D8C8B619344B51A4C73E9129378C38532B2AF4D`。健康Config先通过原正式身份入口，坏Config只换reference路径，实际到 `NativeEvidence.validate_identity.reference_source_SHA`，diagnostic为`reference source SHA mismatch`，不是测试自抛sentinel。

正常operation返回且name==expected时，helper实际抛`fault_was_not_rejected:source_sha_mismatch`；真正预期operation异常可接受，module SHA错误阶段被拒绝。focused未运行尺寸矩阵，未改03f2原raw，未将14个旧有效+1个新正式入口控制冒称旧03f2的15项原本全有效。四个mm1:100场景与3153字段仍引用1880/03f2证据。

完整scoped复核包：`tests/native/dimension-adapt/review-fix-round1-07d5a532-20261103/`。`fix-round1-complete.diff`含Driver/Green/build_dimension_adapt.ps1相对1880的实际diff，和3个新增RejectionAcceptance.h/RejectionAcceptanceTests.cpp/RejectionFocus.inc全文；6个变更/新增review文件，11个07d5/8个1880冻结输入快照。原仅2文件scoped.diff和预冻结focused-manifest原样保留，不覆盖。`source-index.json`记录未改核心，最终`review-index.json`追加真实native/closure/retirement与最终报告绑定。独立`N8a-fix-round1-final-manifest.json`链接最终包，旧manifest不覆盖。

本次整理仅文档与证据复制/索引，无源修改、重编译、CAD控制、Git或review代理派发。实际宿主由root退出码70结束，不称正常QUIT通过；fulltask gate由原reviewer scoped结论决定。

## 以下为冻结前历史记录（原样保留）

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
