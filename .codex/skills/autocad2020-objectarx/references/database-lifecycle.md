# 数据库对象、事务与迁移

接口以实际 2020 SDK 的 `dbmain.h`、`dbsymtb.h`、`dbtrans.h`、`dbobjptr.h` 为准。以下说明提供决策顺序，不是未经验证即可粘贴的完整命令模板。

## 明确谁负责对象

| 状态 | 管理方式 | 常见错误 |
|---|---|---|
| `new` 出来，尚未入库，也未交给其他所有者 | 本地负责 `delete`；可使用作用域守卫或合适智能指针 | 失败分支泄漏 |
| 已成功加入数据库，未交给事务管理 | 数据库拥有对象；按打开方式 `close()` | 默认 `unique_ptr` 析构删除数据库对象 |
| 通过事务打开或已成功纳入事务的新对象 | 由该事务完成关闭/提交/回滚管理 | 又 `close/delete`；事务结束后继续使用指针 |

`AcDbObjectPointerBase::closeInternal()` 在 `objectId().isNull()` 时 delete，否则 close。它是所有权辅助工具，不能消除判断责任：转交事务后，应使用 `release` 解除本地封装的持有，避免两个管理者重复清理。根据具体封装核对构造、open、create、release 的实际签名。

`appendAcDbEntity()` 成功后不会自动关闭实体；此时所有权已经变化。使用默认 `unique_ptr` 暂管未入库实体时，必须在成功入库的边界解除默认删除责任，并给“入库后、纳管前”失败设计关闭及回滚路径。不要等函数末尾才考虑它。

## .NET 到原生接口对照

| .NET 习惯 | 2020 原生处理 |
|---|---|
| `Database.TransactionManager.StartTransaction()` | 选定目标 `AcDbDatabase`，取得其 `transactionManager()`，调用管理器的 `startTransaction()` |
| `Transaction.GetObject(...)` | 使用 `AcTransaction::getObject(...)`，检查 `Acad::ErrorStatus`、输出指针和类型 cast |
| `BlockTableRecord.AppendEntity(...)` | `AcDbBlockTableRecord::appendAcDbEntity(...)`，检查返回值与产生的对象 ID |
| `Transaction.AddNewlyCreatedDBObject(...)` | **管理器**的 `addNewlyCreatedDBRObject(...)`；不在 `AcTransaction` 上臆造同名方法 |
| `Commit()` / Dispose 回滚 | 管理器的 `endTransaction()` / `abortTransaction()`；只结束本作用域启动的层级 |

不要直接用全局当前数据库替代已选定的目标数据库。模型空间、当前空间、图纸空间是不同业务目标，迁移时保留原语义。跨文档或后台数据库任务要核对当前上下文、所需文档锁和 API 限制；不要把前台命令示例直接推广到任意线程。

## 一次写入的检查顺序

1. 明确数据库、目标空间/表记录和宿主上下文。读取原命令规则，不擅改实体类型或坐标变换。
2. 启动事务，检查返回值，建立只负责本次事务层级的退出保护。嵌套事务不应由内层结束外层。
3. 用所选事务取得对象；检查状态、空指针和实际类型。打开模式由操作决定，不默认全部写打开。
4. 创建新对象并设置目标数据库默认值与业务属性；未入库时所有失败路径仍由本地负责销毁。
5. 入库成功后明确转移所有权，再调用管理器纳管。入库失败和纳管失败分别清理；后者需关闭尚未纳管的数据库对象，并回滚此次修改。与选用封装的实际行为交叉检查。
6. 纳管成功后撤销本地关闭/删除责任。后续失败走事务回滚；成功走 end。检查终止状态，不把调用过 end 等同于已成功提交。
7. 事务结束后不复用原对象指针。跨阶段保存 `AcDbObjectId` 并在正确数据库、有效生命周期内重新打开；跨 DWG 的 handle/ID 不能当全局唯一对象引用。

命令入口、回调与宿主边界按项目约定处理异常，避免 C++ 异常越过宿主 ABI。异常保护与 `Acad::ErrorStatus` 检查都不能相互替代。

## 示例与验证

- 基础入库与关闭：`samples/database/ents_dg/ents.cpp`。
- 错误处理对照：`samples/database/entswerr_dg/entswerr.cpp`。
- 管理器纳管及嵌套事务：`samples/entity/polysamp/transact.cpp`。
- XData：从 `samples/database/xdata_dg/` 查当前签名、注册应用名和分配/释放配对；不能把所有 SDK 指针都当 `delete` 对象。

按改动选择正常完成、取消、错误/异常、回滚、保存重开、重复执行和多文档等检查。修正所有权的改动至少验证受影响的失败路径。事务成功仅证明此次数据库修改流程，不自动证明尺寸、比例、配筋、标注等业务正确。
