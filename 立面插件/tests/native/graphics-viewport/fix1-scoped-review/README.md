# N7 fix1 原reviewer限定复核入口

仅复核首次fulltask review唯一P2的修复及必要准备协议变化，不重新开启全任务审查。生产尚未接入，glyph/clip/MText/TrueType/annotation完整性仍partial。没有新增CAD运行或额外测试重跑。

原P2精确原文（root转交）：

> 对所有持久化drawable校验数据库归属 — ViewportForward.h:44 draw仅AcDbEntity::cast判断随后无条件加入暂态白名单；ViewportCapture.cpp:147 filter和Scope同样只识别Entity。AcDbBlockTableRecord属于AcDbObject/AcGiDrawable不是Entity，因此目标compound把外部DB块表记录交geometry.draw时三处放行并转发，违nested不放行外部源子对象。实际日志5446行AcDbBlockTableRecord,persistent=true，5448却transient:BTR，证明类型分支实际使用；没有证据本轮真的foreignDB。应统一AcDbObject/持久化drawable归属，仅真正暂态进白名单，补外部BTR拒绝控制。

原完整复核包：`../review-fulltask-b293e40a/README.md`；原实现报告及本轮追加：`D:/Codex/建筑施工图自动化/立面插件/执行/N7-真实宿主视口采集报告.md`。原fulltask包不是本轮新结论，历史78之前的70通过不能覆盖本P2。

## 三个入口

1. 本索引：说明原问题、变更和验证边界。
2. `b293-to-fd78-combined.diff`：原fulltask基线到最终fd78的完整fix1组合差异，含7项实际SDK BTR控制+数量门禁、精确准备复用协议。`15018-to-fd78-ownership.diff`单独隔离在有效RED之后仅两个实现文件的归属修正；`b293-to-15018-driver-tests.diff`是前置测试/协议变化。
3. `manifest.json`：基线/有效RED/最终GREEN构建输入、二进制、命令输出、两次运行原始证据、closure/retirement/source输入SHA及本包摘要逐项绑定。`observations.json`保留7协议控制、7 BTR正负控制、实际path例子及78项结果，均来源已有raw，不新跑CAD。

## 本次修复

classifyDrawable统一判断AcDbObject::cast、database/objectId、AcGiDrawable::isPersistent/id；resident须target DB且ID一致。foreign、未知persistent或矛盾身份拒绝。只有无持久身份的真正transient可在geometry.draw同步调用期间进授权栈；Scope与filter也用同一分类。target BTR路径写真实handle，foreign BTR不得以白名单绕过。源负例只是实际SDK打开BTR并调用前置校验/filter/Scope，绝不调用source renderer。

启动协议因旧220b5729重复prepare失败而作必要调整：精确相同Config所有字段/模块与输入SHA、原PID/thread/current active ambient doc+DB+path、完整ready/target磁盘SHA、无S1OPEN/queue/capture且保护guards通过，才复用原准备。重新load_config/validate_identity，不重建Session或重写工件，保留replay trace；其他重复仍失败。重复命令的产生来源仍未证明。协议7控制单独报告，不借其替代78项归属/图形断言。

## 已有验证

有效RED15018／5bad165f：78项74pass，恰4fail（foreign child/filter/scope和owned BTR scope path），原70与其余门禁保持。GREEN fd78／5b64a6f1：78/78pass、errors=[]、status partial。有效RED保留真实失败，不用前一轮prepare already active代替。

构建命令 `立面插件/tools/build_graphics_viewport.ps1 -Label fix1-persistent-ownership-green`；fd78 receipt和build.log绑定6项Win32检查及ARX编译exit0。未重跑这些检查。协议控制7项逐次trace核读；实际闭合/退役结果由observations及报告记录，不把QUIT marker认成正常退出。卸载仍未获kUnloadAppMsg=2/commandsRemoved实测证明，不能把静态清理代码说成已原生卸载。

请限定核对原P2三处是否封堵、临时授权/目标正例是否保持，及为达到有效RED所需协议是否精确绑定。不要把本轮scope解释为完整字形、通用任意对象采集或生产接入已验收。原始证据只读，源码不再修改。
