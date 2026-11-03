# N7 fix1 scoped re-review — 2026-11-02

Spec compliance：原唯一 P2 **ADDRESSED**。Code quality：**Approved；No findings within this scoped re-review.**

本轮由原完整 reviewer 仅复核 b293e40a→fd78fb67 的 fix1 组合差异、原 P2 是否封堵及差异新增破坏；不是第二次完整任务审查。采用 review-agent 与 AutoCAD 2020 ObjectARX 技能。除本报告外未修改文件，未编译、运行测试套件、启动/停止 CAD、清理宿主或派子代理。

## 原 P2 逐项结论

| 项目 | 结论 | 依据 |
|---|---|---|
| geometry.draw 拒绝 foreign 持久化非 Entity | ADDRESSED | `tests/native/graphics-viewport/ViewportForward.h:8` 统一通过 AcDbObject、database/objectId、isPersistent/id 分类；`:24` 拒绝 foreign、未知持久化和矛盾身份；`:69` 在转发前调用同一校验。 |
| filter 与 Scope 不受暂态白名单绕过 | ADDRESSED | `ViewportCapture.cpp:193`、`:198` 共用分类/授权；只有 ownedResident 或已授权的真正 transient 可进入。foreign BTR 即使被测试压入指针白名单也不能通过。 |
| 暂态授权作用域及 owned 正例保持 | ADDRESSED | `ViewportForward.h:71` 记录原深度、仅 transient 压栈并以 RAII 恢复；resident 不再混入暂态栈。`ViewportCapture.cpp:204` 使用真实持久化 handle 路径。GREEN 原始请求中误标 `transient:AcDbBlockTableRecord` 的路径为0。 |
| 真实 foreign BTR 拒绝控制 | ADDRESSED | `ViewportOwnershipTests.h:3` 以只读方式打开 source/target 的实际 SDK BTR；7项控制覆盖身份、child、filter、Scope及目标真实handle路径。`:26` 模拟旧指针授权窗口，但不调用 source renderer。`ViewportProbe.cpp:55` 将各结果和7项数量门禁纳入最终报告。 |

## fixdiff 新增破坏检查

No findings。准备复用入口在 `ViewportProbe.cpp:24` 重新 load_config/validate_identity；`ViewportCapture.cpp:109` 比较 Config 的全部现有字段，`:113` 限定完整准备、同 PID/thread、原 current/active/working ambient 文档与路径、尚未开启/排队/捕获，并核 ready/target SHA及 source/ambient guards。复用不重建 Session，不重写准备工件。7项协议控制与实际重放分别记录；没有把 GREEN 的控制调用误称实际重放。

额外读取只围绕具体风险：核 `NativeEvidence.h` 的完整 Config 字段防止复用遗漏身份；核冻结输入/live源与 receipt 防止审错候选；核原始 BTR 控制/回调、prepare trace 与 closure 引用防止仅采信摘要。没有扩审未变的完整字形或卸载能力。

## 本轮只读核对结果

- 入口 manifest SHA256：`9A1077307EE64429A133DE90FFEF385E5123BB4205C2D8112DAF2203FE7FB729`；232个绑定文件逐项重算一致。19项实际编译输入的 live/frozen SHA一致，receipt exitCode=0；候选 ARX SHA为 `5B07F008DEC2615555602F9C67820A7286D4FCF96A60DEE6C0698E82468F5F66`。
- 有效 RED `5bad165f` 原始 native-case：78项、74通过、4失败，恰为 foreign child/filter/Scope 和 owned BTR scope handle path。7项协议控制通过；实际 prepare入口2次、implementation1次、replayAccepted1次。
- GREEN `5b64a6f1` 原始 native-case：78/78通过、errors=[]、partial。7项实际 BTR控制及7项协议控制通过；prepare implementation1次、实际 replayAccepted0次。原12样本自然绘制结果保持，原始回调仍有2668个context和72条文字请求。
- 两次 closure 的 binding/nativeReport/retirement/supervisorFinal 引用 SHA独立重算一致；审包包含每次4份退役归档及6项输入的绑定与核对记录。本轮未执行任何退役或源文件操作。

## 非阻断边界

两次均为明确强停70，forcedTermination=true、normalQuit=false。没有 kUnloadAppMsg=2／commandsRemoved 实测证据；本轮不把未变卸载分支重新列为修复阻断项，也不宣称已实测卸载。原重复 prepare 的来源仍未知，精确复用协议不证明其根因。完整 glyph、clip、MText布局、TrueType及注释比例能力仍 partial，生产未接入，整体立面插件未完成。
