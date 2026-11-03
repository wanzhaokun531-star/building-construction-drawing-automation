# N9e 完整任务复核：Spec + Quality

只读复核当前N9e有限边界观测增量，非整个分支或N9c采集能力验收。先读执行/N9e-GS边界实测简报.md和执行/N9e-GS边界实测报告.md。报告作者正在补实际段，root派审时会确认完成。唯一复核报告写执行/N9e-full-task-review-20261103.md；不改任何源码、索引、Git或旧证据，不派代理/二审。

## 准确比较范围

共享工作树有其它平面任务，禁止git全树diff/扫描/重跑。Git HEAD cb0b5837e4c0941f71c443948b8d425280d5aa75没有本任务commit；本任务冻结源而非虚构commit：

- Base：build/n9c-20261103-121817-090-idle-command-capture-v17-frozen-167b5fda/src/（原v17）。
- Candidate：build/n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8/src/。
- 完整小diff：该v19 build目录的 n9e-v17-boundary.diff。一次阅读该diff及其中上下文；只在具体函数被截断、或有具名风险时定点补读源码/SDK/现有调用者。不要重审旧500行主体或N9a/N9b/N8a。
- receipt SHA1921AE812258CE0B3291E820AE0CCE18B1B2A35A30962031D991A8CD23543861，ARX24884E607FA9B727DE2FB72F8AE6CA1A95354E64469EB30F5BC305422664EE1D。同目录有self-review及v18→v19唯一getConfig空分支校正差异，v18没有原生执行；root已核10构建输入live/frozen/module和exit0。

## 实际验证与结论边界

唯一运行 阶段1/runs/elevation-native-20261103-124216-72c8e7b0，PID22936；作者报告/最小快照索引将给完整定位。raw native/gs-result为failed，原GS produced no real owned WorldDraw callback必须保留。N9e只要求看见此前缺失边界，不要求消除N9c绘制失败或授资格；不能因此拒绝正确的观测实现，也不能以观测完成冒称采集完成。

实际main/diag各model/view kernel匹配、模型集合0→绑定后1，两个rootAdded/两个Erased、exact model/root及默认true、无日志失败，随后仍零图形回调。singleton显示配置gdi15.hdi/GSHEIDI/hardware能力false，不能冒称offscreen活动driver或根因。原三异常控制与清理/保护通过；只有编译/实际观察是本task新增结果，旧纯C++核心不重跑。

closure C9329C25B9908855212123B1623EDC974F0DCEE2C3B6C5DEA3FC23525C8E07FD；70精确强停，normalQuit=false、4暂存退役、6源保持、自有active0。其它CAD一次前台事件单列relatedReview=true，未操作，不伪造全局全零或正常QUIT。没有生产安装、用户DWG改写、COM/CUA/TCH命令/私有字段。

## 要审的内容

按任务brief核所有新增观测事实/行为和范围，兼看callback不抛异常、返回默认true、稳定observer生命周期/正常与异常移除及unload保护、字符/字节缓冲长度与截断未知、配置不可得不新增绘制前提、原参数/资格保持、记录解释准确。禁止将开发探针检查扩成产品准入。SDK供应商PDB LNK4099已知且非本源编译警告，勿要求修改/下载供应商文件。

先给Spec与Quality各自verdict，列全部可行动发现（文件行/具体触发/影响）。不信作者自评，核实际差异/结果；但不为确认其通过重复构建或跑CAD。仅在具名疑点无既有证据回答时才最小focused检查，并写理由。纯调查N9d/此前N9c17版不在新代码范围，不能重新派整项审查。
