# 来源与本地检索索引

首版整理日期：2026-09-27。这是查证入口，不是 SDK 文档全文镜像。关键决定记录来源类型、版本、文件或章节，以及是否实际运行过。

## 证据的职责

| 类型 | 能回答什么 | 限制 |
|---|---|---|
| Autodesk 2020 官方文档 | 厂商声明的支持范围和接口说明 | 新版页面不能证明 2020 行为；无法读取的正文不能标为已读 |
| 本次工程使用的 2020 SDK 头文件/属性表 | 实际可编译声明、类型、库依赖 | 文件存在不等于包完整或真伪已验证 |
| SDK 示例 | API 用法、模块组织和查找入口 | 含遗留配置和教学性简化，随附不等于本机运行通过 |
| 项目源码与运行记录 | 某配置、某二进制在某次测试的结果 | 不扩大到其他构建、宿主、业务与交互方式 |

发生差异时分别描述：例如“官方说明的补丁范围”和“本机较新 v141 补丁的有限通过记录”。实测不改写官方支持范围。

## 官方入口

- [ObjectARX 2020 Release Notes](https://help.autodesk.com/cloudhelp/2020/ENU/OARX-Readme/files/GUID-4DA730AB-7A47-4663-838B-21E71438D6C8.htm)：首版已查看。含 VS2017 版本限制、32 位支持取消、R23.1、23 系列库与类型变化。
- [2020 SDK 官方安装包](https://download.autodesk.com/esd/objectarx/2020/objectarx_for_autocad_2020_win_64_bit.sfx.exe)：来源定位，首版未重新下载或安装。需要 SDK 时先查项目已有安装。
- [ADN ObjectARX Training](https://github.com/ADN-DevTech/objectarx-training)：版本随仓库变化。首版查看时主分支面向 2027，不作为 2020 工程模板。

## SDK 根目录与版本

从 `.vcxproj`、属性表、构建脚本及项目文档取得候选路径；例如目录可能名为 `ObjectARX_for_AutoCAD_2020_Win_64_bit`，但名称本身不是证据。

读取 `inc/id.h` 的 `ACADV_RELMAJOR` 和 `ACADV_RELMINOR`；本次 2020 SDK 声明为 **23 / 1**。核对 `inc/`、`lib-x64/` 和构建实际引用路径。`_idver.h` 中构建号与宿主补丁版本另行记录。

## 本地主题索引

下列路径相对 SDK 根目录，可用 `rg` 搜索。首版阅读了相关声明、实现或指定 README；并未执行 SDK 示例。

| 要解决的问题 / 搜索词 | 首选文件 | 说明 |
|---|---|---|
| 版本 / `ACADV_RELMAJOR` | `inc/id.h` | 读内容判断 23.1 |
| 事务 / `startTransaction`、`addNewlyCreatedDBRObject` | `inc/dbtrans.h` | 区分管理器与 `AcTransaction` |
| 指定数据库 / `transactionManager` | `inc/dbmain.h` | 查目标数据库的管理器 |
| 所有权 / `closeInternal`、`release` | `inc/dbobjptr.h` | 未入库 delete，已入库 close；release 解除封装持有 |
| 实体入库 / `appendAcDbEntity` | `inc/dbsymtb.h` | 入库后不会自动 close |
| CRT / `RuntimeLibrary` | `inc/rxsdk_common.props` | `/MD` 属性依据 |
| 模块链接依赖 | `inc/dbx.props`、`inc/arx.props` | 对象库与 UI 依赖分开 |
| 命令入口、实体、图层、组 / `MKENTS` | `samples/database/ents_dg/ents.cpp`、`readme.txt` | 可读基础示例，补足返回值与失败处理 |
| 错误处理 / `Acad::ErrorStatus` | `samples/database/entswerr_dg/entswerr.cpp` | 与无完整错误处理版本对照 |
| XData / `ADDXDATA`、`PRINTX` | `samples/database/xdata_dg/readme.txt`，再定位该目录源码 | 首版仅作为入口；实现时再查注册应用名与 resbuf 所有权 |
| 事务、新对象纳管、嵌套回滚 | `samples/entity/polysamp/transact.cpp` | 按当前任务阅读完整相关函数 |
| UI 与对象模块拆分 | `samples/entity/polysamp/` | `polyui.vcxproj` 含遗留 Win32 配置，README 有旧库名，不能直接继承 |
| 跨库克隆 / `wblockCloneObjects`、`AcDbIdMapping`、`DuplicateRecordCloning` | `inc/dbmain.h`、`inc/dbidmap.h`、`inc/acdb.h` | 核对目标 owner、ID 映射与同名记录处理；`kDrcIgnore` 保留目标已有定义 |
| 坐标 / `acedTrans`、`getPointAt`、`blockTransform` | `inc/acedads.h`、`inc/dbpl.h`、`inc/dbents.h`、`inc/gemat3d.h` | 区分点与向量、二维与三维重载，以及 UCS/OCS/WCS 和累计块变换 |
| 填充平面 / `getLoopAt`、`normal`、`elevation` | `inc/dbhatch.h` | 环数据坐标与平面信息结合查证，不直接视为 WCS 顶点 |
| 范围和可见性 / `getGeomExtents`、`visibility`、`isInvisible`、`isOff`、`isFrozen` | `inc/dbmain.h`、`inc/dbents.h`、`inc/dbsymtb.h` | 核对错误状态、属性与图层标志；嵌套可见性仍需结合实例路径和宿主验证 |

示例查询（`$sdkRoot` 已定位）：

```powershell
rg -n 'startTransaction|addNewlyCreatedDBRObject' (Join-Path $sdkRoot 'inc/dbtrans.h')
rg -n 'closeInternal|release\(' (Join-Path $sdkRoot 'inc/dbobjptr.h')
rg -n 'addCommand|removeGroup|appendAcDbEntity' (Join-Path $sdkRoot 'samples/database/ents_dg')
```

先查声明，再阅读相关实现/示例和文档；不要依据符号名猜重载、返回值或可用版本。

## 离线 CHM 入口与查证状态

| 文件 | 内容入口 / 建议搜索词 | 首版状态 |
|---|---|---|
| `docs/arxdev.chm` | ObjectARX Developer's Guide；transactions、opening and closing、commands、database | 文件存在；未建立全文索引 |
| `docs/arxref.chm` | Reference；`AcDbObject`、`AcDbTransactionManager`、`AcTransaction`；坐标与克隆另查 `AcDbPolyline::getPointAt`、`acedTrans`、`AcDbBlockReference::blockTransform`、`AcDbDatabase::wblockCloneObjects` | 文件存在；按类名和方法查阅，头文件签名不等于语义正文已读 |
| `docs/readarx.chm` | Readme / Requirements / Release Notes | 文件存在；支持范围另核对官方在线 2020 说明 |

首版尝试系统 `hh.exe -decompile`，未生成可搜索内容。因此这里是**文件与主题索引**，不是已解析 CHM 正文。需要尚未查证的语义时，通过可用 CHM 阅读方式查看具体条目，或在工作区缓存合法取得的 2020 文档内容；若仍不可读，明确缺口，用头文件和示例限定推断范围。缓存和摘录不写回 SDK 或原项目。不要将整套 SDK/CHM/示例复制进技能分发包。
