# N9b：文字坐标与实测占位 C++ 核心

本任务接续既有 Task6，不重做 N9a。工作根 D:/Codex/建筑施工图自动化；计划 docs/superpowers/plans/2026-11-01-建筑施工图立面自动化实施计划.md，权威进度 立面插件/执行进度.md。用户已授权继续。仅一个源码作者；不提交/切换Git、不启动/控制CAD、不写DWG或其它插件。ObjectARX2020/v141/C++17，不引入运行期Python/PDF工具依赖。

## 决策及交付

N9a证明字符串范围不能直接充当完整设备像素占位。实现两个分开的、可供后续原生采集接入的纯C++计算接口：明确范围坐标约定；由实际渲染缓冲区提取完整像素单元占位。后者在指定设备、分辨率、像素相位、视图变换及渲染上下文中有意义，不能升级为无限分辨率字形或当前图完整出图资格。不得用任意margin修饰原extentsBox失败。

新增 src/TextOccupancy.h/.cpp，独立 tests/text-occupancy/ 与 tools/build_text_occupancy.ps1、tools/verify_text_occupancy.py。接口沿用已有Point2/Bounds2/Result风格，避免重复几何框架。不要修改Layout、GenerationState、NativeEvidence、N7、N9a ARX/比较器/冻结报告；新接口不输出nativeVerified或glyphComplete=true，不写资格状态。仅可顺手修正文档 tests/native/text-range/README.md 已审查P3那一句，旧冻结源保持。

1. 字符串范围转换：调用者必须显式声明范围坐标含义（尚未应用显示反向的局部框，或已应用的局部框），不能仅从backward猜extentsBox合同。未知拒绝。保留负min、全部四角、文字normal/direction或等价明确二维基、单次净模型变换；backward局部X仅在未应用约定中反射一次，已应用不得双反射。不重复height/xScale/tracking/oblique。有限二维有效仿射支持；拒绝非有限、奇异和无效基。仅宣称对明确输入合同的坐标运算正确，不新增未测试字体API语义。使用N9a旋转backward实际范围及方向作为回归；另以独立手算不对称框、旋转/剪切、净变换、负min验证。
2. 实测占位：接受有明确定义的RGB字节缓冲区及宽/高/行步长、白背景不透明画面，任何RGB通道<255算墨迹，包含AA。验证布局/长度/乘法溢出和有限可逆worldToPixel仿射；像素单位为边界[x,x+1]×[y,y+1]（像素左上相位及y向下必须明确）。扫描全部像素，记录计数及极值，空白/触及缓冲边缘拒绝（无法证实未裁切）；不凭此声称外部场景一定完整。用实际墨迹完整像素单元的设备包围矩形，逆变换全部四角得到世界四边形及保守AABB。不得将像素中心当完整单元、不得只变换min/max两点。结果携带调用者传入的渲染上下文身份及实际映射，明确为observed device footprint，需独立的完整性/字体/场景/绘制绑定后才能接Layout。上下文可用精简值结构，必要字段仅为后续使用确实会混淆的设备/帧/字体/来源标识与DPI/相位；不新造跨工程资格框架。
3. 本次不开发GS设备/渲染器或在产品里解析PDF。开发验证器将已有实际PNG解码为约定RGB，调用编译的C++命令行adapter，输出C++算得的四角；复用未改N9a full_cells 对同一真实墨迹所有完整像素单元独立检查。这是实测占位提取与坐标映射验证，不是用同图证明通用字形范围定理。保留原extentsBox及反射后诊断结果，不更改历史0/10。

## 复用输入与验证

原生运行：阶段1/runs/elevation-native-20261103-093743-84f838a5/case/text-range.json，SHA 05254B213DC3F1B0C5D20DF1F0C352E8D12D9281DF68B9180D010800EECF6CD1。

实际图像及实测投影：同case/text-range-public-measured-v8/，5场景×144/288dpi；比较器 tools/verify_text_range.py 与 tests/native/text-range/offline-projection-v8/verify_text_range.py SHA 9296A063343919A13C3A954CE855E239C706254A55063AB8B431DF21857B27F3。检查实际文件布局后直接复用已有PNG；无需重渲染或新CAD。原生PDF、字体、投影映射绑定在N9a原始记录和comparison.json中，输出新结果单独目录，不覆盖已有证据。

先读 执行/N9a-受控文字范围验证报告.md 的开头、结果和最小后续（不要全文历史重读）。原5种原范围全部未完整包含；反向框单次局部X反射消除粗大错边，但仍有边缘像素越界。原字体控制simplex/gbcbig，仅这5种受控渲染，不外推原TCH缓存。

TDD以新C++接口最少有意义失败先行：遗漏backward、只转两点、用像素中心少掉边缘，至少覆盖实际相关行为；缺符号/编译失败不是有效RED。GREEN后运行独立focused suite，以及10张真实图适配检查。用真实图结果缩成35%与错误/遗漏映射测试独立比较器会拒绝，不能只测helper抛错。图像必须非空、完整缓冲区扫描；错误缓冲/奇异映射/边缘触碰有针对性测试。无须重复旧77核心测试（旧生产源未改）；如触动公用类型则必须先报告并评估。

Python仅开发验证，使用 C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe（已有Pillow/numpy）；构建复用VS2019/v141 14.16.27023，参考现有tools/build.ps1的冻结输入方式，但只建本任务console。不要为局部任务复制整个旧证据库。新结果绑定本次源/EXE与所复用原始输入哈希即可。

唯一报告 执行/N9b-文字坐标与实测占位报告.md：实际接口、RED/GREEN命令和退出码、10张真实图计数/范围、负例、支持边界、与Layout下一接入缺口、变更文件、自审。形成一次完整任务审查所需清晰diff/基线副本和小型证据索引。作者不派子代理或审查。无Git提交，最终只回DONE/DONE_WITH_CONCERNS等、测试一句话、concerns、报告路径。
