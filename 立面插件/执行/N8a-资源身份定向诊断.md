# N8a 资源身份定向诊断

2026-11-02。只读核对冻结候选、N8a简报、指定run的8份scene及SDK/官方接口语义；未改代码、编译、运行CAD、操作Git或改历史证据。本说明不是完整Task review。

## 结论

**实际失败角色已能由冻结控制流确定：首个public explode分段的 `dimensionStyle()`，尚未到 `dimtxsty()`。** 当前代码确实额外要求“源与目标各自生成的临时标准尺寸所引用的DimStyle，必须恰好构成克隆TCH根时那份SDK映射的pair”。公开接口没有给出这项一般保证。它把派生表示的资源引用纳入真实clone pair合同，存在合同前提未证的问题；但现有证据仍不足以认定该次是合法的派生资源差异，也不足以判定TCH克隆资源确实损坏。

## 精确路径与运行证据

以下行号均指冻结目录 `build/n8a-20261102-224655-614-green_matrix_v1-86f3341b/src/`，不采用作者正在编辑的源码作为结论依据。

- [DimensionAdaptDriver.cpp:114](D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-224655-614-green_matrix_v1-86f3341b/src/DimensionAdaptDriver.cpp:114)：`observe(id)`在TCH根上调用const `explode()`；132–145行从返回的临时 `AcDbAlignedDimension` 读取layer、`dimensionStyle()`和`dimtxsty()`。这些ID来自派生分段，不是从TCH根直接取得的DimStyle/TextStyle ID。145行已要求三个引用非空且归属根所在数据库。
- [DimensionAdaptGreen.inc:187](D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-224655-614-green_matrix_v1-86f3341b/src/DimensionAdaptGreen.inc:187)：`clone_seed`仅将seed根送入 `wblockCloneObjects(..., kDrcMangleName)`，得到真实SDK mapping并检查primary pair。196–197行分别在source/candidate working database作用域内对源根和克隆根独立observe。
- [DimensionAdaptGreen.inc:198](D:/Codex/建筑施工图自动化/立面插件/build/n8a-20261102-224655-614-green_matrix_v1-86f3341b/src/DimensionAdaptGreen.inc:198)：先比较声明的几何/测量/格式/纸面字段，随后顺序push：root layer → segment[0] layer → segment[0] DimStyle → segment[0] TextStyle → 下一段。`reference_map`在191行先require成功才返回JSON并push。
- 指定run的8份 `*-scene.json` 均为 `clone_setup / public_resource_reference_mapping_mismatch`，`cloneLayerMappings`恰有2项；故第三次push之前的首分段DimStyle检查抛错。不是TextStyle。该名称虽叫cloneLayerMappings，后续实际上还装DimStyle/TextStyle pair。

| 场景 | 已通过的root及首段layer | 已通过的layer线型 |
|---|---|---|
| one_mm100_vertical_large | AXIS `258 → 7A` | `16 → 5D` |
| six_mm100_horizontal、twelve_mm100_vertical_large | A-PUB_DIM `348A → 7A` | `16 → 5D` |
| two_mm100_horizontal、two_mm50_horizontal、two_m50_horizontal、two_mm100_noncollinear及measurement fault | PUB_DIM `283 → 7A` | `16 → 5D` |

以上layer定义字段相同，目标名按mangle成为`$0$...`；都未保存候选。不能把全部场景概括为PUB_DIM，也不能把measurement fault在setup阶段失败记成已验证测值拒绝。`before.comparisonJson`此前已相等，但其字段不包含base DimStyle身份/完整定义；`readResources`虽在observe内构造，失败前的before snapshot没有单独落盘。

## 公开语义能证明的边界

本机SDK `C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit/inc/id.h:47–48` 为23.1；`dbdim.h:176`、`dbmain.h:668`与`dbidmap.h:66`分别声明该getter、克隆入口和bool compute。

`explode()`生成由实体实现决定的、未入库的分解结果；其契约没有说返回对象与另一次explode结果是克隆pair，也没有说其全部资源getter对应根的持久依赖。[2020官方Explode说明](https://help.autodesk.com/cloudhelp/2020/ENU/OARX-ManagedRefGuide/files/OARX-ManagedRefGuide-Autodesk_AutoCAD_DatabaseServices_Entity_Explode_DBObjectCollection.html)。`dimensionStyle`仅表示**该dimension**引用的DimStyle记录ID；这里的dimension是返回的标准临时分段。[2020官方DimensionStyle说明](https://help.autodesk.com/cloudhelp/2020/ENU/OARX-ManagedRefGuide/files/OARX-ManagedRefGuide-Autodesk_AutoCAD_DatabaseServices_Dimension_DimensionStyle.html)。这两条2020页面是managed包装文档，用于交叉核对语义，C++可用声明以本机2020头为准。

SDK mapping代表实际克隆过程纳入的对象与翻译关系。2020官方说明允许自定义对象在克隆时转发同一mapping克隆依赖；它并未将任意公开派生结果的引用自动定义为根的依赖。[2020自定义类克隆说明](https://help.autodesk.com/cloudhelp/2020/ENU/OARX-DevGuide/files/GUID-09B54901-3BA5-4F68-AD60-D145222F1490.htm)。`compute`是按key查已有entry，跨库找不到key时返回false且value为空；不创建映射、不搜索“定义等价”的资源。[官方compute说明（2019邻版）](https://help.autodesk.com/cloudhelp/2019/ENU/OARX-RefGuide/files/OREF-AcDbIdMapping__compute_AcDbIdPair__const.html)。2020 native reference网页未能读取，未将邻版正文冒称2020实测保证。

因此，**“引用可合法解析且公开效果保持”与“引用就是源资源的SDK映射目标”是两个不同命题**。现代码191行将 `compute=false` 与 `compute=true && value!=target` 合成同一错误；只靠该错误无法断言是哪一个。也不能从临时分段有某个DimStyle ID反推TCH根私有依赖字段。

## 最小后续事实

由唯一源码作者现有diagnosticOnly分支一次性输出即可，本诊断不新增实现或运行：

1. 在require前落盘 `role=segment[0].dimensionStyle`、源/目标ID及数据库归属、打开类型/状态；分别记录compute结果、实际pair.value及其归属、isCloned/isPrimary/isOwnerXlated。失败也保留记录，避免再次丢失角色与分支。
2. 对源分段引用、目标分段引用以及存在时的mapping目标，输出记录名、当前已声明的DimStyle公开值、base text style和effective text style定义；同时保存源/克隆before完整公开snapshot。这样能分清“缺pair”“pair指向另一记录”“公开定义/有效样式确有变化”。名称受mangle影响，不能仅靠名称判身份或等价。
3. 若compute缺项而效果相同，仍需核对该资源在clone前后是否已在SDK mapping内，以及源/候选数据库默认DimStyle/TextStyle与分段选择的关系。可以证明临时分段使用了各库默认样式才称默认回退；没这份事实前只保留派生资源假设。单纯缺pair不能证明资源缺失，也不能证明它无需clone。

后续依据事实决定pair身份检查适用对象与派生表示的公开字段比较范围；保留真实SDK mapping，绝不手填等价pair。此时不支持无理由删除门禁、改TCH私有字段或给候选生产资格。简报要求的真实根、目标库引用归属、公开资源/效果及保存重开一致性仍各自有效。

冻结输入SHA256：Driver `59199689C7EC0C2756DA26A7BDC642989146B6E0F78BC7CD03A22422F1B699F4`；Green include `E1AE0FA4823B4979A1C3259C6887D6FBA0C6021D270CD3DC14671FC7D26B957D`。
