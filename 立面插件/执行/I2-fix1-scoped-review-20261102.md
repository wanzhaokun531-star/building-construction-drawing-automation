# I2 fix round 1 定向复核（2026-11-02）

No new findings.

**原 P2：ADDRESSED（代码与离线覆盖）。** 原三个普通命令捕获结果未参与验收的问题已修正；fix1.diff 未发现新的可操作破坏。**新驱动原生接受证据：待执行。** 这不是 fix1 已通过原生51项的结论。

## 逐项复核

| 原问题/要求 | 状态 | 证据与结论 |
| --- | --- | --- |
| SDK status 未门禁 | ADDRESSED | recordPrinted 将实际 result.status 与实际 RTNORM 传入 acceptPrintedCommand；成功值没有替换为合成测试的17。函数要求 status == successStatus。 |
| readOnly 未门禁 | ADDRESSED | 直接传递原 printedCommand 的 result.readOnly，判定要求 true；原监测集合及比较逻辑未削减。 |
| restored 未门禁 | ADDRESSED | 直接传递 result.restored，判定要求 true；原 strict logger 恢复逻辑未改变。 |
| 三处实际 expect 未消费完整结果 | ADDRESSED | UnitConfigDriver.cpp 新第459行 ConfigInvalidatesOrdinaryIssuePages、第512行 SwitchInvalidatesOrdinaryIssuePages、第515行 InfoDisclosesUnitOnlyScope 均直接使用 recordPrinted 返回值。原文字判据保留并作为 expectedText 参与同一合取。false 进入既有 expect → failed step / state.pass=false 链路。 |
| 报告未显示实际接受因子 | ADDRESSED | 同一调用记录 status、successStatus、readOnly、restored、expectedText、accepted 六字段及原 transcript/SHA，再返回同一个 accepted；没有另造用于报告的通过值。 |
| 缺少针对漏判的离线负例 | ADDRESSED | 同一实际 header 的合取入口覆盖三标签各5情形：健康、坏status、写入、恢复失败、文字错误。每个异常单独存在而预期文字正确时均拒绝，错误文字仍拒绝。离线数据明确为合成观测，无 native API 执行声明。 |

## 离线证据及新差异

读取完整 fix1.diff、manifest、实施报告 fix1 追加段，以及 test-acceptance.cmd、静态接入检查脚本、RED header、RED/GREEN原始日志和接入检查输出；没有重开完整 I2 或整分支审查。

原始 offline-red.log 为 TOTAL=15 FAILED=9；恰为三种状态漏判×三个标签，健康和错误文字判据仍符合旧行为。offline-green.log 为 TOTAL=15 FAILED=0。manifest记录相应 exit1/exit0。测试 main 对 failed != 0 返回1，保存的 cmd 向上传播该退出码，与日志和记录一致。RED header 明确还原原来的仅 expectedText 行为；GREEN 使用实际修复 header，离线编译路径直接指向工程内的 PrintedCommandAcceptanceTests.cpp，其 include 使用同目录 header。三个标签共用同一函数；真实三处接入另由所审 diff 和 native-wiring 检查共同确认，未把标签循环冒充三次 CAD 执行。

源码改动仅四文件：驱动、host-neutral 接受 header、离线测试 cpp、build 输入冻结清单。接受函数为 noexcept 的直接四条件合取，没有外部状态；产品及原捕获/恢复逻辑不变。build 增加冻结两份离线源，未将离线 main 加入原生 ARX 编译清单。未发现本次改动引入新的执行、事务、Undo、日志恢复或产品行为变化。

独立哈希核对全部通过：52项绑定文件、4份变更快照、2份 before 与原 receipt 对照、33份编译 origin 和33份编译副本。内嵌 candidate 与实际 receipt 完全一致，30份 unchangedPriorInputs 均保持；正式/驱动二进制哈希也匹配，构建 receipt exitCode=0。

新候选：`i2-20261102-193640-098-review-fix1-printed-command-acceptance-5ee43611`。

- 产品 SHA256：`85356995913E0E461C06F02D945E54395A976B13D798FAD57882EDFE78581250`。
- 驱动 SHA256：`8C169AFF37C4218D59C970A0E44C48A1DF8712CB36AE13FEB19D270F04C6899C`。
- receipt SHA256：`4A417CFA75670091F1518A713B97BDFFB031163107DEEA2C0447AC82B6373689`。

## Verdict 与待执行边界

**规格/代码 verdict：本次 P2 修复符合原审查要求，三处漏判已消除。质量 verdict：本定向复核通过，无新增 finding。** 离线覆盖足以证明该接受逻辑拒绝原来漏放的三类观测，并结合实际调用接入确认修复没有停留在未使用 helper。

**原生验收待执行：** 平台当前禁止新 CAD；本次未运行新候选。放行后仍需普通 CONFIG后ISSUES、切图后ISSUES、INFO 的实际结果，让新增六字段进入原始 capability，并验证对应 expect 接受真实成功、只读、恢复和文字条件。旧 f1f997b8/d01 的51/51仍是已经核实的历史实测；不能移作5ee43611新驱动已过的证据，也不因当前等待而否定或声称缺失旧证据。

## Out of scope

完整 I2/整分支重审、产品功能扩展、旧 logger 故障根因及既存平台行为均不在本轮。未重跑离线测试/全suite、编译、启动或暂存 CAD、修改代码/源档案/归档/注册表、派子代理、提交或推送；本轮唯一写入为本定向报告。
