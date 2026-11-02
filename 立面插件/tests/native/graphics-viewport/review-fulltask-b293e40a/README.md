# N7 首次完整任务独立复核包

此包供首次fresh独立fulltask review；不是追加小修复审，也没有预填No findings。禁止为了复核启动CAD或修改源码。无需Git，无Git不初始化。最终冻结源和实测模块均为b293e40a，ARX SHA256 `6B18ACE07B03AE2DCC4ECE736A7C891DC9D1DF80D5B9E60A3B0301B6126A48D6`；真实2b412dba/PID5568，70/70原生断言通过但partial。

## 复核范围和入口

- `stub-to-final.diff`：从805dc0d3最初stub到最终的全部N7实现、测试、build脚本和协议变化，不只看最近文件共享修正。
- `candidate-source/`：实际编译的Probe/Capture/Forward/Fixtures/Trace/OwnedControlFile及单测/项目/契约；NativeEvidence仅只读依赖，已核与stub相同。GraphicsPolicy/ControlPolicy绑定冻结N5来源，未改原N5。
- `external-controller/capture_viewport_case.py`：root作者的外部orchestrator副本，SHA和本次viewport-host-advance.helperSHA相同；包括身份/ready schema/目标复制/只派S1OPEN/最终closed报告等。其实现也在本次完整接口链审查范围，但本代理未修改它。
- `manifest.json`：实际源输入、模块、SDK公共头/CHM合同、设计/计划/N7简报、真实运行及各失败历史、6保护输入、4退役归档和共享宿主证据逐文件SHA绑定。绝对路径指向保留原始证据，未重写raw。共享桥/监测代码不属于N7修改。
- `actual-text-requests.json`：72条两轮真实公开文字回调，带原始JSONL行号、原文/UTF-16/长度/style/font/geometry/matrix，不能仅用count充当正确性。
- `actual-context-file-observation.json`：2668context摘要、全部真实viewport记录和读取/idle时序。`verification-output.json`为包整理时独立重算输出，不能取代raw。
- `立面插件/执行/N7-真实宿主视口采集报告.md`最新段落列出12样本精确文字和计数、实际共享失败32/新共享成功、source/ambient/target事件分离、强停70及能力边界。

## 请重点查

1. 是否只接收宿主自然World/ViewportDraw，不伪造context/view；所有公开几何/traits/context合法原样转发；异常后注册与全局状态及OnIdle/Doc/Command生命周期是否完整。
2. isApplicable只命中自有target及同步暂态draw白名单；source/ambient生命周期不关闭/保存/重读回滚，workingDB RAII及事件监测正确；外部源子对象不得因nested而放行。
3. prepare-ready atomic close、最终NativeEvidence独占跨两阶段、exact session-owned path/nonce/PID/root12/SHA绑定；S1结束后idle/quiescent自动只排队一次，queue不放宽公共白名单。
4. OwnedControlFile只读权限+兼容已有writer sharing的官方依据、最终路径/重解析点/身份/size/time/SHA校验；source严格SHA未变。两次哈希和已知对象事件不冒充未知私有内存快照。
5. 完整12控制断言与实际回调值；stub曾12个0==0 false pass的修复实效；当前正计数forward匹配。TCH采到字符串不等于glyph/排版正确。
6. Evidence七INI、失败关闭及卸载、源6SHA、4退役归档、closure强停70/normalQuit=false绑定，禁止拿QUIT marker推正常退出。
7. MText/full glyph/clip/TrueType/字体fallback/annotationScale仍partial、生产未接入；任何潜在缺陷按实际可触发路径提出，不为通过结果作豁免。

## 已有执行和历史

构建命令 `立面插件/tools/build_graphics_viewport.ps1 -Label owned-file-idle-frozen`；冻结目录build/n7-20261101-221056-470-owned-file-idle-frozen-b293e40a，6 Win32检查通过、ARX exit0。命令与build.log均由manifest绑定。有效单测RED30e4ce2e为6项3失败。无须重编译覆盖既有产物。

原生596805fa：stub70项13pass57fail，12个空转发默认值是假阳性；b9becee9：prepare already active、无采集，调用来源未确定；9c4d780f：prepare1/capture1，S1OPEN未结束时capture，strict SHA openfail未记GLE；2b412dba：公开真实draw/context/text+两轮稳定，70/70门禁通过，partial保持。旧失败原始文件不改写。

整理命令：`python 立面插件/tests/native/graphics-viewport/review-fulltask-b293e40a/assemble_review.py`。只读已有原始证据和冻结源，写本包摘录/副本/diff/manifest/output；不启动CAD、不接触生产代码。manifest本身SHA位于verification-output.json。返回意见交root，由root决定一次作者修复与必要再验证。
