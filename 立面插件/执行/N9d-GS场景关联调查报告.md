# N9d：GS 场景关联与宿主前提只读调查报告

2026-11-03。仅阅读现有代码、冻结记录及本机 ObjectARX 2020 SDK；本次唯一写入为本报告。未编译、启动或控制 CAD、修改源码/配置/旧证据、提交或派子代理。

## 结论与已证事实

**没有证据支持“漏调 addSceneGraphRoot 导致零回调”。2020 公开合同明确 view.add 会完成根登记。** 当前仍未查明独立离屏链零回调原因，不能只归 AcDb 持久访问，也不能归为私有桌面不能绘图。

- v17 `elevation-native-20261103-122027-e03879b7` 已真实 idle 后进入唯一 modal capture；主 BTR 和独立 nonpersistent PrimitiveRoot 均属性/WorldDraw/viewportDraw=0，更新后 view/device invalid，无实际 context/RGB。三故障清理及 ambient 保护通过；宿主已收尾。这些既有结果不需重跑参数或调度实验。
- 冻结 `evidence/native-v17-idle-capture-zero-traversal/case/gs-trace.jsonl:5–6,27` 及 `nonpersistent-control/gs-trace.jsonl:5–6,28` 已实证 associate/find、device.graphicsKernel、view.getDevice、view.getModel(root) 均匹配预期。源码 `NativeGsTextFootprint.cpp:289,325–343` 对应；不能再把 root→view→model 读回列作未观测。
- 工程 N7 使用真实 current-owned 文档及公开 REGEN（`tests/native/graphics-viewport/ViewportCapture.cpp:304`），已有两轮稳定请求、2668 非空目标 context、72 条文字请求、78/78 检查（`执行/N7-真实宿主视口采集报告.md:5,64`）。它证明既有隔离宿主路线能触发真实图形调用；没有证明 N9c 的独立 model/view/offscreen device 已接通。

## 2020 合同与结构判断

依据根：`C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit/`。以下 HTML 均直接从本机 `docs/arxref.chm` 以 7-Zip stdout 在内存读取，未写提取缓存；CHM SHA256 为 `92BB53D177903BB780329F5EBD5A72374C434F6DF4AF25CEB5449C90BB928BA2`，与既有 `tests/native/text-footprint-gs/sdk-public-contracts/source.json` 相同。

| 精确条目 / 头文件 | 合同及本案含义 |
| --- | --- |
| `AcGsModel__addSceneGraphRoot@AcGiDrawable__.html`；`inc/gs.h:545` | 原文：“AcGsView::add() calls this method to ensure that pRoot is registered as a scene graph root in this AcGsModel.” 显式登记还可使无 view 的根保持缓存。当前 view.add 已执行，未显式登记不是缺环证据；该 HTML SHA256 `552F9DE9F779DF1B29B355D7855FD0CE9007550196E8589328E61CA70D2F311A`。 |
| `AcGsModel__eraseSceneGraphRoot@AcGiDrawable__.html`；`AcGsView__erase@AcGiDrawable__.html`；`AcGsView__eraseAll.html` | 根在 view 中会隐式登记；从所有 view 移除后才失去这项缓存保持。view.erase 调用模型的 eraseSceneGraphRoot；eraseAll 移除所有 viewed drawables。现有帧前 eraseAll/readd 不要求另补手动根生命周期。 |
| `AcGsView__add@AcGiDrawable__@AcGsModel__.html` | 绑定 scene DAG 的根及处理通知的 model；子节点通过 geometry.draw 遍历；该方法 always true。因此返回 true 不是已遍历或已成像证明，但合同包含隐式根登记。 |
| `AcGsManager__createAutoCADModel@AcGsGraphicsKernel__.html`；`inc/AcGsManager.h:42–51,94–98` | 只承诺用指定 kernel 创建 model，与 destroyAutoCADModel 配对；未披露其实际 get/release callbacks、ambient/sideDB 选择或 context.database 初始化。不能由方法名补出这些保证。 |
| `AcGsGraphicsKernel__associate@Adesk__LongPtr@AcGsModel__@AcGsModel__RenderType.html`；同类 `find@Adesk__LongPtr@AcGsModel__RenderType.html`；`inc/gs.h:1314–1327` | 分别承诺 database/model/render type 关联与查询；没有根登记、安装持久对象访问回调或建立绘制 context 的承诺。find 匹配只证实该关联。 |
| `AcGsGraphicsKernel__createModel@AcGsModel__RenderType@Adesk__IntDbId@AcGsGetInterfaceFunc@AcGsReleaseInterfaceFunc.html`；`inc/gs.h:1041–1043,1217–1236` | 通用 model 的 getFunc 将 persistent IDs 转成 drawable 指针，releaseFunc 在使用结束后收尾；databaseId 是对 GS 无含义的 magic cookie，不能等同 AcDbDatabase 绑定。由同 kernel.deleteModel 配对销毁，不能混用 manager 销毁器。 |
| `AcGiDrawable__isPersistent.html` | persistent 对象属于需要 open/close 的 model；nonpersistent 是 pointer-based，GS 直接经 drawable 指针访问。因此 v17 非持久根绕过持久对象 ID 访问，却仍共用 manager model 和原离屏链；两路零回调排除了“只要避开 AcDb open/close 就能画”的充分解释，不能排除 manager model 或 device/view 链。 |

生命周期上，现有代码先 view.eraseAll、移除 view，再销毁 view/model/device/kernel；非持久指针保持至 model 销毁后。此次未发现应补显式 root 登记/擦除的合同缺口。SDK 随附 `samples/database/ARXDBG/Reactors/ArxDbgGsReactor.cpp:34–53` 展示 acquire/addReactor 与 remove/release 配对，不提供该独立离屏链的成功对照。后续版本 ADN BlockView 仅为工厂配对线索，不能填补 2020 运行证明。

## 唯一下一切片：原 v17 链的最少只读边界观测

建议另行实施一次有限观测；**当前不改 model/device 工厂、根、样式、相机、renderer、更新或调度路径，不补显式根登记。** 使用既有 nonpersistent 诊断链，在创建/绑定后、原更新前后和清理处记录一次必要事实：

1. 读取 `&model.graphicsKernel()`、`&view.graphicsKernel()` 与既有 kernel/device 身份比较；读取 `view.getModelList()` 的实际模型集合。`inc/gs.h:540–541` 注释为 “Who created this AcGsModel?”；同头 `:267,452–453` 及 CHM `AcGsView__graphicsKernel.html`、`AcGsView__getModelList@AcArray_AcGsModel___@const.html` 为依据。模型 CHM `AcGsModel__graphicsKernel.html` 正文误写 AcGsDevice，保留此文档不一致，不据它重复断言设备身份。
2. 在既有 view.add/eraseAll 之前对该独有 model 配对登记有限 `AcGsModelReactor`，只观察 `onSceneGraphRootAdded/Erased(model,root)`，回调保持默认 true，不改参数、不调用绘制或其他业务；清理时先配对移除，再销毁观察器/model。依据 `inc/gs.h:1080–1083,1293–1313`、CHM `AcGsGraphicsKernel__addModelReactor@AcGsModel__@AcGsModelReactor__.html` / `removeModelReactor@AcGsModel__@AcGsModelReactor__.html` 和 `AcGsModelReactor__onSceneGraphRootAdded@AcGsModel__@AcGiDrawable__.html` / `onSceneGraphRootErased@AcGsModel__@AcGiDrawable__.html`。这是通知观察注册，不是现有公开 root 数量 getter；未收到通知须原样报告，不能自行当成根不存在。
3. 从 `kernel.getConfig()` 读取 singleton config 的 `currentDisplayDriver`、`driverName`、`getVirtualDeviceName`，保留原路径/名称/布尔输出及不可读状态。精确合同分别为 `AcGsGraphicsKernel__getConfig.html`、`AcGsConfig__currentDisplayDriver@ACHAR__@int@ACHAR__@int@bool__@const.html`、`AcGsConfig__driverName@ACHAR__@int@const.html`、`AcGsConfig__getVirtualDeviceName.html`；头 `gs.h:869–881,1004,1278`。currentDisplayDriver 查询的是当前显示驱动**配置**，布尔量合同为驱动是否具备硬件加速能力；不能称该 offscreen device 的活动 driver 或已启用加速。其长度参数按 CHM 的字节定义核实分配，不混淆 Unicode 字符数。全部只调用公开 getter，不使用 setter、configure/dialog 或内部 suppression。

区分力：kernel 身份或模型集合异常可定位到真实关联边界；正确 root 通知可排除“没有完成登记”的猜测；显示配置可替代已加载 HDI 模块名猜活动驱动。若全部一致仍零回调，必须保持原因未知，不据此再猜一个参数。

纯 nonpersistent + generic kernel model **适合作为随后行为验证时先于 AcDb 的候选隔离方向**，但不是本报告并列推荐的第二切片。现有 owned style-ID 仍有持久访问依赖；直接改 generic model 并同时换 value-style 会混淆原因。若以后做工厂单因素比较，两臂必须先共享同一无持久依赖的 primitive/value-style 基线，分别按工厂配对销毁；2020 合同未明确允许 null get/release callbacks，不能直接猜传空。当前不实现该候选。

未知仍包括：manager model 实际持久访问适配、模型自身实际 kernel、模型根通知、配置输出及独立离屏链未更新原因；没有真实 context/RGB 或文字足迹资格。N7 的 current 图 REGEN 成功不能替代这些事实，也不改变保留当前 DWG 未保存编辑的用户目标。

## N9e 闭合后续篇：值型样式与非空根的组合尚未实测

2026-11-03。N9e `72c8e7b0` 已实证 model/view/kernel、模型集合、根通知一致，但仍零绘制回调；见 `执行/N9e-GS边界实测报告.md`。上述“下一切片”已由 N9e 完成，本续篇只回答新发现的样式传递缺口，未复审 N9e 实现。

**是，现存 N9c 实测没有覆盖“直接 AcGiVisualStyle 值型 setter 后，带非空根执行实际更新”。旧值型路线因读回差异提前退出，不能记作该路线已实测零回调。** 定位均相对 `tests/native/text-footprint-gs/evidence/`：

| 冻结证据 | 精确事实 |
| --- | --- |
| `native-v8-style-differences/case/lifecycle-2/gs-trace.jsonl:9–10` 及同目录 `result.json` | value getter 的 styleSet=true，随后因 traits 差异抛异常，faultReached=false；尚未安装 overrule、加入绘制根或进入非空 update。主 Session 未进入。 |
| `native-v9-custom-style-differences/case/lifecycle-2/gs-trace.jsonl:9–10` | 同为 styleSet=true、活动 traits 不等、立即异常；主未进入，无绘制 update。 |
| `native-v10-staged-readback/case/lifecycle-2/gs-trace.jsonl:10–17` | A/B/C value getter 均 true，活动差异仍在；12–15 行仅 emptyDevice.update/emptyView.flush，16 行 C 读回后、17 行异常。没有非空根 update。SHA256 `D0192C5BCD92DC60455086B05AE13514FF9A57C1BD809ECCFB07B84ED7D1EE4E`。 |
| `candidate-v11-v10.diff` 的 execute/render 差异；`native-v11-id-zero-callbacks/case/lifecycle-2/gs-trace.jsonl:11–21` | 旧 value style 门位于 view.add/render 前；v11 首次非空诊断 update 已换 owned ID setter，11–13/16–18 行 ID 匹配但 value getter=false，20–21 行 deviceUpdate 返回 callbacks=0。此后 v12–N9e 的非空更新均沿 ID 路线。 |

复核本机 2020 CHM 四条合同：`AcGsView__setVisualStyle@AcGiVisualStyle__.html`（SHA `C7FA50650A0E782EDDB6785D39923657FDD9B31475D3BC75FE13003872A2C0A0`）承诺设置值型样式；`AcGsView__setVisualStyle@AcDbObjectId.html`（SHA `DFDC8692B38B44CC631D06C52496050D6ACC363F8833C6F90239EEDB5E4165D7`）要求有效既有 style ID、model 加入 view 后调用。`AcGsView__visualStyle@AcGiVisualStyle__@const.html` 返回是否已设置样式及值输出，`AcGsView__visualStyle@const.html` 返回样式 ID。合同没有承诺跨 overload 必须互相转换，也没有保证 requested traits 原样读回、延后 update 才生效，或 value setter 会清除原 ID。value getter=false 不能单独证明 ID 方案没有设置样式；true 且 traits 不同也不能证明曾经尝试非空绘制。

**现在更有区分力的唯一下一行为切片是 style transport A/B，先于 generic model。** 两臂均使用原 manager.createAutoCADModel、原 kernel offscreen device/view 工厂、同一 PrimitiveRoot 实现与几何、同一库样式源及严格 copyTo 回读、同相机/尺寸/trait 请求值和 N9e 观察。均在 root/model 加入 view 后绑定；A 仅调用 owned ID setter，B 仅调用 direct value setter，初始及原 erase/readd 后按相同位置各绑定一次。不得同时换 generic model、renderer、style 数值或更新方式。

为避免旧 ID 残留，推荐同一宿主内两臂各自 fresh 同构 Session，前臂完整清理后创建后臂；“同 model/设备”指同工厂与配置，不以销毁后数字地址复用冒充同实例。若强制复用同一个 view 先 ID 后 value，公开合同无法排除残留或顺序效应，结论必须带此限制。

两臂都限定 diagnosticOnly=true、qualification=false、无文字；完整保留 getter 返回及 traits 差异，**差异不在这次纯诊断中阻断唯一一次非空更新**，否则又重现 v8–10 的覆盖空白。只允许原 device.update / 非 interactive view.update，真实回调先决条件、身份/资源/ambient 保护及 RGB 原样保存规则保持；不据此接入文字矩阵或升级资格。真实有回调后才能读原 RGB，先保存再检查标定。若 B 有绘制而 A 没有，只定位到这两条样式绑定路径在该链的行为差异，仍不把请求 traits 等价或完整样式采用自动判为通过；两臂均零则此 style transport 改动没有解决该链，原因仍未知；仅 A 有绘制则保留相反结果。该切片是在填补从未执行的组合，不重试 v10 已排除的空更新时序假设。

## N9f 闭合后续篇：通用 kernel model 工厂对照

2026-11-03。据 root 提供的 N9f/v21 闭合事实，fresh ID/value 两臂已执行非空更新且均零 WorldDraw；value 臂 getter=true、四项实际 traits 差异未前拦，root/kernel/list 边界正常。因此值型样式组合空白已填补；本续篇不再推荐调整样式，不复审 N9f 代码。

**以 N9f B 为共同基线，仅比较 manager model 与通用 kernel model 有公开合同依据，但尚无运行成功保证。** 最小声明如下（本机 2020 `inc/gs.h:1041–1043,1227–1234`）：

```cpp
using AcGsGetInterfaceFunc = AcGiDrawable* (*)(Adesk::IntDbId objectId,
                                              bool bNeedsValidation);
using AcGsReleaseInterfaceFunc = void (*)(AcGiDrawable*);
// 通用臂：
model = kernel->createModel(AcGsModel::kMain, diagnosticCookie,
                            diagnosticGetInterface, diagnosticReleaseInterface);
// 与之配对：kernel->deleteModel(model);
```

此处是签名说明及候选调用，不是待复制的实现。`Adesk::IntDbId` 为 `Adesk::LongPtr`（`inc/adesk.h:115`），须保持 x64 宽度。cookie 可取稳定的非 AcDb 诊断上下文 token，不将它解引用为数据库；createModel 合同明确它只是 GS 无含义、可经 AcGi 查询返回的 magic cookie。原 kernel.associate/find 若保留，仍只是独立的 ownedDb/model/kMain map，不把 cookie 或 map 等同持久访问适配。

依据仍为 CHM `AcGsGraphicsKernel__createModel@AcGsModel__RenderType@Adesk__IntDbId@AcGsGetInterfaceFunc@AcGsReleaseInterfaceFunc.html`、`AcGsGraphicsKernel__deleteModel@AcGsModel__.html`、`AcGiDrawable__isPersistent.html`：getFunc 将 persistent IDs 转 drawable，releaseFunc 收尾，nonpersistent drawable 则直接通过指针访问。合同未明确允许空函数指针，故使用具名、非空、签名严格一致的 get/release 函数。**bool 唯一查得的公开说明是 `gs.h:1042` 的 “does id need validation?”；CHM createModel 未展开 bool，未找到独立回调条目或随附示例。不能把 true/false 编成未经证实的 open、锁定、已验证或安全跳检协议。**

最小诊断回调只记录原始 ID/bool 或释放指针及计数；两种 bool 均不访问、打开或转换任何 AcDb，不手工调用 drawable 绘制、不伪造 context。get 真被请求时置 `persistentAdapterUnavailable=true`，没有可返回的持久 drawable，返回 nullptr；release 不 delete/close 未由本适配器取得的指针。日志异常不得穿过回调。**2020 合同未明确保证 GS 安全接受 get 返回 nullptr，因此“发生请求后如何继续”不是已证安全或完整适配；必须作为不可用路径保留，不能以该臂证明纯指针链已受测成功。** 未调用 get 时保持 unavailable=false，不能把不存在的 persistent 请求当作零回调解释。释放回调意外到来同样保留异常访问事实，不猜对象归属。

比较应使用同一宿主中串行 fresh 的两个同构 Session：M 沿 manager.createAutoCADModel/destroyAutoCADModel，K 仅改 kernel.createModel/deleteModel；其它均为 N9f B 的 direct value setter及真实读回、同一 PrimitiveRoot 图元、默认设备/view 工厂、相机、观察和原更新。两臂样式差异仍只记录，不前拦非空无文字诊断；不删原资源、身份或 ambient 保护。这里“纯 nonpersistent”只指加入 GS 的 root/图元，不宣称原初始化完全没有 AcDb；原 style 源审计等保持共同基线。

生命周期保持：回调函数及其稳定上下文先于 createModel 存活，直到 deleteModel 返回后才失效；createModel 本身若触发回调也须可记录。先 view.eraseAll，配对移除 view/模型观察器并销毁 view，恢复原 association，再按各自工厂销毁 model；PrimitiveRoot、cookie/回调记录器至少活过 model 销毁，device/kernel 最后按原配对释放。回调可能在销毁期间发生，不能提前清空唯一上下文。两个 Session 不并行，不让无 userdata 参数的 callback 将事件错记到另一臂。

解释边界：K 无持久适配请求且有真实绘制、M 仍零，只支持这两个 model 创建/适配路径的实际行为差异，不证明 manager 实现缺陷或全部 AcDb 不可用；两臂在无请求时仍零，则本工厂替换没有解决该链，原因继续未知；K 请求了 persistent ID，则该诊断适配器不覆盖真实请求，应单列 adapter-unavailable，不把随后零回调归为通用 model 不支持非持久根。若仅 M 能绘制，保留相反结果。所有臂仍 diagnosticOnly/qualification=false，不授予文字、SideDB 或生产采集资格。本次仅追加只读结论，没有实施、编译或运行该候选。

## 补充合同：physical/logical palette 与 display-change

仅作为后续解释依据，不修改当前 N9g、不建议立即增加调用。2020 本机 `inc/gs.h:726–728,738–741` 及同版 `arxref.chm` 原文已核：

- `AcGsDevice__setPhysicalPalette@AcGsColor__@int.html`（SHA256 `926F9173A19D00A101B4D5B370E676AC6C0A3AB87769B4E39BF142A1A3F58FCF`）：“This function specifies a physical palette. It should be used in conjunction with off-screen devices.” 参数为组成 palette 的 AcGsColor 数组及条目数。同版 `!!MEMBERTYPE_Methods_AcGsDevice.html` 方法列表有相同说明。这是明确的离屏配合用途依据；未给初始调用次序、默认物理 palette 状态、指定颜色/数量、缺省时是否停止遍历或所有设备都必需的保证。
- `AcGsDevice__setLogicalPalette@AcGsColor__@int.html`（SHA256 `EA52485DABD35374827CA019D7807C77D19F0745E74A23E35B47549C41CD3A87`）用于告知客户端在 `AcGiSubEntityTraits::setColor()` 以 palette index 指色时采用的逻辑 palette；AutoCAD 使用 ACI 映射。合同明确逻辑 palette 可能并非底层硬件实际使用的 physical palette（硬件也可能没有 palette）。因此当前 driver 只有 setLogicalPalette 的事实不能视为已经设置 physical palette；反过来也不能从未显式 physical 设置推出根本不会绘图。当前图元用 truecolor 的事实亦不足以自行证明 physical palette 不适用，合同没有写此豁免。
- `AcGsDevice__onDisplayChange@int@int@int.html`（SHA256 `6DC44918A71E9C16928D0950348D63C73CAE86C1A38B14446034A926A9ACF329`）是向 device 通知 **Operating System Color Depth 和/或 Screen Resolution 已改变**，参数依次为新的 bits-per-pixel、X/Y resolution。不是按名字推测的离屏画布尺寸初始化，也没有“离屏设备首次创建必须调用”的合同；不能将待截取 1024×768、猜定色深或 DPI 当成真实 OS display-change 数据。画布尺寸的 onSize 是另一通知。

据 root 提供的检索状态，2014 Autodesk ADN 员工 Balaji Ram 的论坛回复及 CPP.zip 附件仅为线索；附件源码没有读取，普通 HTTP 已遇 Cloudflare 挑战并停止。本报告未重试、绕过或引用附件中的任何调用次序。2018 在线方法列表不替代上述 2020 本机原文。当前 physical palette 缺省状态、设备实现是否需要该配合、其与零回调是否有关仍未知，不据此变更 N9g 或追加 renderer/宿主试验。

## N9g 实际请求后续篇：已知自有资源的真实只读 provider

已定点读取 `阶段1/runs/elevation-native-20261103-131409-216c3fca/case/nonpersistent-kernel-model-control/gs-trace.jsonl`：20/21 行 createView，24/25 行 device.add 成功，27 行 get 请求 `3014301743760`、bool=false、返回 null；28 行才记录新建 owned style。该 raw 值不等于已记录的 cookie、marker、modelspace BTR 或后来创建的 style。这里只证明此区间确有持久资源访问缺口，尚不知道具体触发调用及资源角色；不得由数值邻近、地址偏移或 bool=false 猜角色。非持久 root 不保证 model 所需的其它资源也非持久。

**用公开枚举所得 ID 和真实 read-open 指针构成有限 provider 有依据，可在同一后续切片中识别请求角色并满足已覆盖的请求，不需先单开一次只观察角色的试验。** 2020 精确依据：

- `inc/dbmain.h:1646,1655–1666,1728` / CHM `AcDbObject.html`、`AcDbObject__close.html`：AcDbObject 公开继承 AcGiDrawable，有公开 objectId/database/close；驻库后对象由数据库负责删除，客户端只配对关闭自己打开的引用，不能 delete 驻库指针。故不是只有 AcDbEntity 才能作为 provider 的真实 drawable。
- `inc/dbmain.h:373–390,429–431`；`dbsymtb.h:954,1405–1428` / CHM `AcDbSymbolTable__newIterator@AcDbSymbolTableIterator__@bool@bool@const.html`、`AcDbSymbolTableIterator__getRecordId@AcDbObjectId_@const.html`、`AcDbSymbolTableIterator__getRecord@AcDbSymbolTableRecord__@AcDb__OpenMode@bool@const.html`：公开取得符号表、枚举真实 record ID、kForRead 打开记录；iterator 由调用方 delete。
- `dbdict.h:48–85,167` / CHM `AcDbDatabase__getNamedObjectsDictionary@AcDbDictionary__@AcDb__OpenMode.html`、`AcDbDictionaryIterator__objectId.html`、`AcDbDictionaryIterator__getObject@AcDbObject___@AcDb__OpenMode.html`：公开字典路径、当前 entry ID 和只读打开对象；打开失败不可用默认指针。`dbsymtb.h:58–60` / `AcDbSymbolTableRecord__getName@AcString__@const.html` 可取资源名称；`dbid.h:67` 的 asOldId 仅用于已取得的真 ID→数值 key，绝不从 callback raw 反构造 ID。

最小角色目录可限于该自有空库已存在的九类 SymbolTable 本体及记录（Block、Layer、TextStyle、Linetype、View、UCS、Viewport、RegApp、DimStyle），以及 NamedObjectsDictionary 的字典路径和资源 entry，含既有 visual-style/material 等默认资源。字典节点可经公开 AcDbDictionary::cast 识别后继续枚举，记录名称路径、真实类名及 ID；按真实 ID 去重、保留角色别名，不遍历实体几何/扩展私有字段、不读 ambient 库、不硬编码本次 raw 值或预判请求是某种 style。缺失、擦除、读取失败及目录覆盖范围均保留，不称已覆盖所有 GS 资源。

每个可借条目必须在 callback 外成功 kForRead 打开并验证 `id.database()==ownedDb`、`object.database()==ownedDb`、`objectId()==id`、未擦除且可读，再缓存确切 AcGiDrawable 指针；一个唯一 ID 只持有本 provider 自己的一次成功 open。callback 只做数值精确查表、原 ID/bool/角色/命中与借用计数记录，返回该真指针；两种 bool 不映射未经公开的验证协议。未知、目录命中但未安全打开、失效或不属于本库者仍 unsupported/null，不 setFromOldId、不开任意对象、不借 ambient 指针。

release 只匹配本 provider 实际借出的 drawable 指针并扣减借用计数，不关闭长期 pin、不 delete、不代替其它组件 close；未知释放或计数下溢单列失败。缓存及 callback 上下文跨越全部 view/model 使用和 deleteModel；模型真正销毁、回调不再可达后，对每次成功 open 各 close 一次，检查返回值，最后才销毁 ownedDb。原已打开 marker/BTR 若被纳入，应明确另有一次本 provider 的 read-open 引用，或明确借用原所有者至 model 销毁，不让两个清理器关闭同一次 open。借用计数不平衡须报告，不能因此提前关闭尚被 GS 使用的对象或把已释放资源假报通过。

须处理一个实际顺序约束：原 createOwnedStyle 稍后要将同库 visual-style 字典以 kForWrite 打开并写入；不能先把该待写容器长期 kForRead 钉住而自造打开冲突。角色目录和可借指针是两层：待写容器先只收集角色/ID、关闭枚举引用，保持既有写入完成后才 read-open 纳入缓存；请求若先落到未安全打开的容器，明确记录已知角色但 unavailable，不在 callback 内升级/写库。已存在且不再修改的资源记录可提前 pin 并实际服务。若选择把全部 owned DB 写入前移到 provider 冻结前，须把时序改变列为事实；原请求可能因此消失，不能仅据消失声称已满足旧请求。

下一切片保持原 generic model/direct value/图元/设备/更新不变，只补这个有限真实资源 provider 与其生命周期；角色与真正返回 drawable 的命中在同次 get 中记录。若请求命中且有真实绘制，只证明本次有限资源适配接通；若命中后仍零绘制，则不能再用这次已满足的 ID 请求解释零回调；新未知 ID 仍表明覆盖不足，不归为 renderer 或所有公开路径失败。本次仅只读追加依据，没有实现、运行 CAD 或提出 palette 试验。
