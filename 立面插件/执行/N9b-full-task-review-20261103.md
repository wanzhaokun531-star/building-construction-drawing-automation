# N9b full task review — 2026-11-03

## Spec Compliance

- **❌ Issues found：1 项 P2。** 明确坐标约定、单次 backward、完整像素单元和 observed-only 边界均按简报实现；但数值失真后的退化四边形没有被拒绝，见下方唯一发现。
- **⚠️ Cannot verify from diff：** 原生缓冲采集、真实设备/字体/对象绑定、上游 clip 与场景绘制完整性、Layout/既有资格门接入尚未实现。它们是简报明确留给后续的工作，不计本任务遗漏。`src/TextOccupancy.h:59` 明示成功观察不建立完整性或 Layout 资格；`tests/text-occupancy/README.md:34`、`:43` 保留同一边界。

## Strengths

- `src/TextOccupancy.cpp:51` 分开处理两种显式范围约定，未知枚举拒绝；局部 X 反向只应用于 beforeDisplayBackward，文字基与 NET 各应用一次。`tests/text-occupancy/TextOccupancyTests.cpp:14`、`:22`、`:41` 包含单次反射、独立手算四角和 N9a 实际旋转 backward 输入。
- `src/TextOccupancy.cpp:73` 验证 RGB 长度、stride、尺寸溢出、映射与上下文，扫描全部实际像素，包含任一通道的 AA；空白与四边触墨拒绝。完整单元矩形逆变换全部四角，映射和上下文按值返回；没有资格状态或 DWG 写入路径。相关 focused 测试位于 `tests/text-occupancy/TextOccupancyTests.cpp:71`、`:96`。
- `tools/verify_text_occupancy.py:123` 使用冻结 `full_cells` 独立检查实际图像的所有墨迹单元；`:129`、`:137` 分别检查缩至 35% 和错误/遗漏映射的真实错误结果。历史 extentsBox 的 0/10 没有被本次结果替换。
- `tools/build_text_occupancy.ps1:10` 冻结当前所需输入，只构建两个 console；7 个新增文件职责清楚，未把原生采集或生产资格框架加入本任务。独立文字、复合符号/尺寸和材料表占位的全局目标没有被冒称为已全部完成；本 diff 没有缩放建筑、修改出图比例或写入当前方案 DWG 的路径。

## Issues

### Critical (Must Fix)

无。

### Important (Should Fix)

**[P2] 在返回成功前拒绝浮点舍入造成的退化四边形 — `立面插件/src/TextOccupancy.cpp:48`**

`finish()` 只检查四角有限以及 AABB 两轴均有正跨度；位于斜线上的点集也满足这项检查。例如 `localRange=[(0,0),(4,0.1)]`、`localX=(1,1)`、`localY=(1,-1)`、`position=(1e16,1e16)`、单位 NET 和 `afterDisplayBackward` 全部是有限合法输入，基行列式为 -2，但逐步 binary64 运算把四角变成 `[(1e16,1e16),(1e16+4,1e16+4),(1e16+4,1e16+4),(1e16,1e16)]`。AABB 宽高仍为 4，所以 `transformTextRange()` 在 `:69` 返回成功，尽管文字范围已经塌成两个点构成的线段。相同检查还用于 `measureTextFootprint()` 的逆映射结果（`:111`）；例如中心有一个墨迹像素的 3×3 图与 `worldToPixel={0.125,0.125,-2.5e15,5,-5,0}` 可产生相同类型的塌缩。这使“可用四边形”包含数值上无法表示的输出，也与当前 `TEXT_*_NUMERIC` 所述非退化合同不符。应以平移后的边向量等稳定方法验证实际返回四边形仍有二维面积，失真时返回现有数值错误；补充大坐标平移及逆映射塌缩的 focused 回归，不应靠扩大占位 margin 掩盖。

### Minor (Nice to Have)

无其它可行动发现。

## Assessment

**Task quality：Needs fixes。** 正常尺度路径、有限观察边界及验证工具符合本次范围；修复上述成功返回退化几何的问题后才宜通过此任务门禁。本发现不否定现有 11/11、10/10 或 30/30 的已测场景结果。

## 审查范围与证据

- 本次是唯一一次完整 task review，同时覆盖规格符合性与代码质量。按指定空基线完整读取 `tests/text-occupancy/evidence/new-files.diff` 的 7 个新增文件；分三段显示以避免截断，没有另读改变文件，没有用整个 dirty 工作树取 diff，没有运行 Git 命令。
- 已读任务简报、作者报告及 `evidence/index.json`。root 提供的 10 项 SHA 绑定核验作为控制者已核证据接受；本审查没有把作者自述替代实现检查。
- 读取现有最终 `build/n9b-20261103-101858-736-delivery-b2a48216/{receipt.json,build.log,focused-tests.log}`：构建/测试 exit=0，11/11，未见编译警告或错误。读取初始 RED 的 `focused-tests.log`：3 项均为 coordinate mismatch、0/3，符合有效行为失败。读取现有 `evidence/real-rasters-final.log`：10/10 实际图、30/30 错误结果拒绝，保留历史 0/10。没有重跑这些测试。
- 仅为两个具体风险读取未改依赖：① `Result::canCommit()` 是否暗含生产资格：检查 `src/ElevationModel.h:49`，它仅检查值存在且无 blocking issue，当前 README 已澄清计算成功的意义；② 独立像素检查是否确实检查完整单元：检查冻结比较器 `tests/native/text-range/offline-projection-v8/verify_text_range.py:65` 的 `full_cells`，确认逐像素四角和逐边半平面判断，未放宽原 1e-8 像素数值容差。
- 对唯一数值发现只做无文件写入的 binary64 算术演示，得到 2 个不同角点、AABB 检查仍为 true、基行列式 -2；没有将这项演示冒称为实际 C++ 回归执行。首次 PowerShell 算术演示因数组表达式解析失败，随后使用 bundled Python 的内存内算术确认；未影响任何源码、证据或现有测试。
- 本审查未启动 CAD、未写 DWG、未修改源码/Git/证据，也未派子代理；唯一写入是本报告。当前方案保留其它内容、调整比例不缩放建筑、完整符号/尺寸文字及材料表占位的生产级验收，需要后续接入任务提供实际 DWG 与原生完整性证据，不能由这次静态 review 或十张有限观察推定。
