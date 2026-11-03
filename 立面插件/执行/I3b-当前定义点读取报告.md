# I3b：当前定义点读取报告

I3b typed reader 已实现，root 独立 controller 实际 GREEN：32/32 passed、errors=[]、11 checkpoints，9 个读取区间均 ReadOnly passed。冻结模块及源码保持，待 root 一次 fresh 限定完整 review。实际切图未覆盖；本切片不开放 BUILD 或其它生成资格。

## 实现与范围

新增六文件：`src/ProjectFactsReader.h/.cpp`、`tests/native/project-facts/ProjectFactsFixture.h`、`ProjectFactsDriver.cpp`、`ElevationProjectFactCases.vcxproj`、`tools/build_project_facts_reader.ps1`。未改 I3a、SceneReader/AnalysisReader、既有 I1/I2/GS driver、公共 NativeEvidence/host/watch/bridge、codec/NOD 或生产命令。作者未控制 CAD、共享设置、平台 DLL、COM/CUA/TCH 业务命令、私有序列化、Git 或代理。

API `readCurrentProjectFacts(ProjectFactsReadRequest)` 接收 NativeSceneContext、明确 expected provenance 和源句柄；返回小值 `Result<ProjectFactSourceRead>`，含本次 readCompleted、实际 provenance、model owner 与源集合。项目 ID/确认单位/选择归配置层，不构造语义残缺的核心 Observation，不写 archive.bound、RuleInputs 或资格。

入口/出口核同一真实 current/active/working 库、context、实际身份与非零 expected revision。单批有中途修改/失效或不可得时整批无值；不会刷新 expected 来接纳旧选择。复用 EntityKey 非零 uint64 十六进制合同及 SceneReader 的句柄/UTF 转换；大小写、任意前导零统一，`getAcDbObjectId(...,false,...)` 不创建未知 ID stub。仅 eUnknownHandle/真实 erased ID 证明 missing，其它解析/open/身份异常为 unavailable。

每次成功 public kForRead open 由 RAII close。验证对象当前库/实际 objectId/owner/exact runtime class，仅 exact direct 模型空间 Line、AlignedDimension、Text 读取 start/end、xLine1/xLine2、normal/thickness、原 textString、公开布局字段和 hasFields。有效可见性限实体 visibility 与自有图层 on/unfrozen，不声称视口/裁剪/完整绘制。非 direct/非 exact 保留真实 owner/class 与 monostate；field 保留 hasFields 并定位，继续由核心 unsupported。没有 measurement/extents、字体加载、field evaluation、REGEN、工作库切换或磁盘产品状态读取。

## 原生证据与真实边界

测试从冻结方案只读 side DB 克隆四幅已审计 datum/title/两段楼层文字至独占宿主当前未保存图，实际 16 个 source→current mapping 可定位；产品无审计句柄或根数资格。每次读取区间独立比较 current/active/working/context、revision、DBMOD、root inventory、资源定义与 write-open/modify/append/erase/unappend/reappend/header 事件，夹具创建和修改在区间外。

最终 GREEN 原值：Text 33B 为“未保存中文立面”，position=(120,240,0)、alignmentPoint=(0,0,0)、height=3.25、widthFactor=.73、rotation=.21、oblique=.12、mirrorX=true、thickness=.5；Line 33C 定义点 (12,34,0)→(56,78,0)、thickness=2；Aligned 33D xLine 点 (10,20,0)/(30,70,0)，不以 override 或 dim-line point 替代。随后夹具主动修改两对象，revision=3 的 `ReadUnsavedEdits` 实际读到“未保存修改中文”和新 start=(123,456,0)。

9 批 DBMOD 均 1→1，9 个读取区间 ReadOnly 全过。总事件 `observedEventsIncludingFixtureEdits` 有 4 条：Line33C/Text33B 各 openWrite+modified，来自显式 fixture 编辑，因此不能称整个 case 零写。field Text 358 实际 hasFields=true；非direct 354 的 owner=351；BlockReference355/AttributeDefinition356/MText357 保留真类与空定义；erased35E/有效未知句柄分别 missing，malformed unavailable，零/旧 revision/错身份均 stale，定位准确。

root 明确接受实际切图/中途切图暂未覆盖，本项不新增时序/文档调度框架。当前入口/出口代码核验不能替代后续普通命令或整体提交的实际切图验收。未保存重开、TCH 专有 getter 或完整绘制也不在本项已证范围。未重跑旧 85/8 纯核心、I2/N9/GS 矩阵。

## 四个宿主记录（分别保留）

| 记录 | 原始 session | 业务结论与收尾 |
|---|---|---|
| 夹具失败 | `elevation-native-20261103-143315-5e06e911` | errors=[fixture append:21]、steps=[]、checkpoint1；不是 RED。4退役/6保持/own0，exit70、forced=true、normalQuit=false。原件保留。 |
| 有效业务 RED | `elevation-native-20261103-143743-07a2fef5` | 32项中9 ReadOnly passed/23业务failed、errors=[]、9批无值+FACT_READER_NOT_IMPLEMENTED、11 checkpoints。之后才实现getter。4退役/6保持/own0，exit70、explicitStop=true、forced=false、normalQuit=false。 |
| GREEN前启动中断 | `elevation-native-20261103-144536-c3c34ac9` | ready前约16秒exit1，无ready/workerstate/native-case，未运行watch；不是业务GREEN失败。独立 pre-ready-infra-closure-audit.json，无伪造标准closure。4退役/6保持，explicitStop=false/forced=false/normalQuit=false；原因未知，stdout/stderr/heartbeat原件保留。 |
| 同冻结候选唯一启动复验 GREEN | `elevation-native-20261103-145021-1b7be28c` | 32/32 passed/errors[]，11 checkpoints。4退役/6保持，own SHOW/foreground/readErrors=0；exit70、explicitStop=true、forced=true、normalQuit/nativeQuit=false，不称正常QUIT。 |

上述 session 绝对根均为 `D:/Codex/建筑施工图自动化/阶段1/runs`。第一次夹具错误中，SDK2020 acadstrc.h 将21定义为 eIllegalEntityType；原代码非法把 AcDbAttribute append 至模型空间，原日志未带class，不能单凭旧异常唯一定位。定点改为合法 direct AcDbAttributeDefinition 作为 Text 派生 exact 拒绝正控制，并给 append 异常补 exactClass/owner/status；reader 保持骨架直至有效RED。在线官方 [appendAttribute owner合同](https://help.autodesk.com/cloudhelp/2027/ENU/OARX-RefGuide/files/OARX-RefGuide-AcDbBlockReference__appendAttribute_AcDbObjectId__AcDbAttribute_.html)、[appendAcDbEntity对AttributeDefinition的支持](https://help.autodesk.com/cloudhelp/2027/ENU/OARX-RefGuide/files/OARX-RefGuide-AcDbBlockTableRecord__appendAcDbEntity_AcDbObjectId__AcDbEntity_.html)与本机2020签名核对；编译依据本机2020头。

最终 GREEN relatedCadEventsNeedReview=true 保留：原 log 20条均属其它 PID35036（create2/destroy2/foreground4/hide6/show6），SHA `0471D4610FF93DDD54D1AEA1A7756211011D42031D032602C3D501F2C1A33C89`。本任务未操作该PID，不能把这些事件归责 own，也不能抹去 review 标记。

## 冻结、复核与审查入口

最终 GREEN [receipt](D:/Codex/建筑施工图自动化/立面插件/build/i3b-20261103-144226-082-green-39397c2a/receipt.json)，SHA `C0AF93E4CECEB83CE645A716DA38ED627211FD23F0F99DC15E1E24D0C4DA9211`；模块 SHA `FC2D9320C3A7B741B3D25267A0D0A4E992E1158CCB5117AB0A7BB9CCA2D377E4`，case=`CurrentProjectFactSources`。最终 native SHA `3DC1028889F2309EFA94695D314496AED476253EBFC4E433EAB8DEEF0FFC60B4`，closure SHA `02D36EF375129360EBC2103FA1054FA969F0A975CBF25FC03E4A7EB5508B81FE`。

实际构建 SDK2020/R23.1、v141 14.16.27023、x64、C++17，0错误、既有缺 rxapi.pdb 的 LNK4099 链接1警告。已重核 receipt.sourceFiles、live+compiled copy、实际模块及6组保护DWG mismatch=0；启动中断后未改代码、模块、脚本、配置或重编译。相对有效RED仅reader.cpp真实实现及driver总事件字段改名，32断言/fixture保持。

I3a保护哈希：ProjectFacts.h `B2467B244B8DF3287ADFFAECEE3D5038DFA1656C5D39313120EBB0F3A329F52D`；ProjectFacts.cpp `D9209C4C8183F1462B0A6B2A9D4ED5B1B61B8D5B464BC890600B66123F39D3E4`。

聚合审查包：[review-i3b-20261103](D:/Codex/建筑施工图自动化/立面插件/tests/native/project-facts/review-i3b-20261103)。`current/` 是当前六个新增文件的逐字冻结副本；`scope-stat.json` 给范围/行数/hash；01初始新增六文件diff、02夹具修复diff、03最终reader-green-vs-valid-red.diff以及04当前完整新增六文件diff均齐。审查整个I3b新增范围，不重审旧核心或重跑旧矩阵。最小实际证据索引 [evidence-index.json](D:/Codex/建筑施工图自动化/立面插件/tests/native/project-facts/review-i3b-20261103/evidence-index.json) 绑定四宿主原件、构建/模块、收尾、related log与完整审查输入。
