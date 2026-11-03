原唯一 P2：**ADDRESSED**。本轮修正新增破坏：**No findings.**

本次仅复核 43394e7c→055bdf65 的 `MaterialTileDriver.cpp`、`MaterialTileFixtures.h` 修正及其直接证据，不重开完整 M2 审查。原问题是保存重开只有总面积/线数跨边界比较，全部片共同改变材料字段可漏检。

规格确认：保存前在独占输出库建立按真实持久 root handle 对应的基线；独立重开后、重新 evaluate 之前读取材料字段。基线包括每根颜色、图层引用、图案名称/类型/比例/方向/相位、全部定义与 dash、数据库单位/线型比例，以及实际引用标准资源的完整公开 DXF 字段、真实 handle/owner/extensionDictionary、XData 和 Xrecord 策略。接受条件现在同时要求根清单、完整字段映射严格相等及原面积/线数检查通过，没有依赖重开库第一片作为跨库基线，也没有新增忽略字段或浮点容差。原 P2 的要求已落实。

代码质量确认：新增采集只在测试夹具中使用公开 SDK，按实际引用去重遍历标准资源，记录结构 owner 身份而不沿其反向展开整个无关数据库；未知类、读取失败和预算超限会使验证失败。统一变色负控制仅修改独立重开内存库，逐根检查实际颜色改变，不保存回 DWG。新增正常对照和反例均使用同一 `acceptSavedMaterialFields()`；RED→GREEN 差异仅为接入完整字段/根清单比较及诊断版本标记。本轮未发现可证明的新增缺陷。

实际证据：

- 8e1f62fc/d4306e4d RED：73 项中 72 通过，errors=[]，唯一失败为 `SaveReopenUniformRootColorChangeRejected`。8/8 根真实变色，16 项字段差异，面积 58200000、线数 115873 均保持，旧接受条件仍返回 true。属于有效行为 RED，不是夹具设置失败。
- 055bdf65/96d3d8d0 GREEN：73/73、errors=[]。五正常场景 root handles 完全一致，前后完整字段数分别为 9088/9088/13616/13616/9088，各场景实际字段数组相同、差异为 0、accepted=true。同一 8 根变色产生 16 项差异，面积/线数保持而 accepted=false，savedBack=false，直接封闭原漏检。
- 原 67 项保留并在本次原生报告中通过；typed10、4 个实际命中故障、8 项资源冲突及五场景原生覆盖/接缝/67 源定义检查未被替代。生产 core 和其余 14 份编译输入保持 433 指纹，只有两份测试文件改变。
- 已读取冻结构建命令与输出：AutoCAD 2020 SDK、v141 14.16、x64、C++17；ARX、无诊断宏生产对象和 API 编译合同均完成，receipt exitCode=0，唯一警告为 SDK rxapi.pdb LNK4099。16 份当前 origin/compiled copy SHA 无差异。

绑定核对：manifest SHA256 `8BF51C979FE2F295B72820E772CB48DAC0CF3220237F3A6A01DA1DFA39BC24A3`；完整 fix diff SHA256 `9DD2F0A8AF12AD92E1EC7F601866FEC6969CBAF5A447B87F509E1D38C8F26747`。manifest 内 43 项绑定文件均匹配。GREEN 原始报告 SHA256 `CBEE831ED532E08780E6C9381C7383CE8DF7CBB6748C148E1916662E51D10D56`，ARX SHA256 `648B9D2857E491E45ECAECEA15BD355BD9822A420FC8202B1D97CDE2376CA72C`。本轮仅复用并核对已有命令、输出和原始证据，没有重新构建、运行测试套件或启动 CAD。

本轮范围外事项：无新增 finding。055 没有重跑 Plot；433/af1e6b04 的自然 Plot 与三处目视仍仅作为相同生产 core 当时的历史图面证据，不能称为 055 新 driver 的出图。duplicate /PageMode 警告、内部 QA 用途和 CTB 未验收边界保持。96d3d8d0 的退出为 70/forcedTermination=true/normalQuit=false，不能称正常退出。

结合原完整审查，本轮关闭其唯一 P2，M2 在既定范围内的规格与代码审查通过。范围仍为轴向矩形及严格内部不交矩形孔、公开标准资源、独占新库准备；未增加 CurrentDoc writer、任意复杂轮廓或 TCH 私有完整性资格，不代表整个分支或整个立面插件已完成。
