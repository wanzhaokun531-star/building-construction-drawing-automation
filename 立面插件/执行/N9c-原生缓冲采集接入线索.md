# N9c 原生缓冲采集接入线索（尚未执行）

2026-11-03 root只读核对本机2020 SDK，供N9b后续接入；不是原生验证报告或已支持结论。N9b只从传入RGB计算占位，产品还需在ObjectARX内获取真实缓冲及对应上下文，不能运行期调用离线PNG/PDF/Python工具。

SDK根：C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit。

- inc/AcGsManager.h:92有createAutoCADOffScreenDevice(kernel)，:132有createView(device)，后者已加入指定设备；:140 destroyView配对并移除设备关联，:111 destroyAutoCADDevice。:260/:269 acquire/releaseGraphicsKernel引用计数；设备/视图/模型必须同kernel。不要把会加入GUI的createAutoCADView或createAutoCADViewport当隔离离屏创建函数。
- inc/gs.h:271/298有setViewport/setView，:326–329四种实际矩阵含worldToDeviceMatrix，:334 add(drawable,model)，:744 device.getSnapShot(Atil::Image*,offset)。快照是该设备有限范围，不证明任意外部场景完整。
- inc/gs.h:1227 kernel.createModel(type,databaseId,getFunc,releaseFunc)，databaseId仅为cookie；:1319 associate(db-pointer,model,renderType)。不能把cookie本身当实际AcGiContext数据库绑定成功。必须在真实回调中核context()->database()，禁止伪造context或用workingDatabase猜。inc/AcGsManager.h:166的setGsModel明确限当前显示kernel，不能混用另一kernel。
- 持久回调为inc/gs.h:1041 AcGsGetInterfaceFunc(IntDbId,bool)及release；inc/dbid.h:67–68提供asOldId/setFromOldId。读开/释放须与实际AcDb归属配对，不解码私有指针。
- utils/Atil/Inc/Image.h:149附近有Image(Size,DataModel*,ImagePixel,TileSize)，ImageContext.h:207 getRow(y,x,count,buffer)；RgbModel.h明确kRgba/kBgra，不能把4通道行当紧密3通道RGB。utils/Atil/Lib-x64包含AdImaging.lib与AdIntImgServices.lib，尚未链接或加载验证。

下一实际任务应先用独占新库和自己的offscreen device/view/model验证真实context、完整图形回调、空白/边缘检查、已知标定图元与worldToDevice及ATIL行方向/相位关系，再把真实RGB交同一N9b实现。只读保存矩阵、字体、设备设置、原始缓冲和结果，成功后才考虑真实TCH目标；不得把Plot/Poppler观察移给GS作为已验证结果。保持用户current/active/working文档和共有GS状态，异常路径也移除drawables并配对销毁。此处只是下一任务候选，不擅改当前N9b范围。
