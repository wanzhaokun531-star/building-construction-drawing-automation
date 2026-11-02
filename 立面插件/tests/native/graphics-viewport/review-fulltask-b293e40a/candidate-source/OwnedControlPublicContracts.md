# N7 自有当前文档读取与延后捕获的公共合同

仅适用于 `config.session/viewport-native-job/control.dwg`。scheme/reference 继续使用未改动的 NativeEvidence 严格 SHA 读取。

Microsoft 的 [CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) 合同要求：新句柄的共享模式也必须兼容已有句柄的访问。`FILE_SHARE_WRITE` 允许已有写访问共存，并不为本次 `GENERIC_READ` 句柄授予写权限。新读取不请求写/删除权限，不共享删除；先记录旧严格试开的现场错误码，再用READ|WRITE共享只读句柄读取。旧9c4d日志未保存GetLastError，因此不能追认该次错误为32。

[GetFileInformationByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle) 提供的卷号和文件索引用于身份对比；读取前后及整个capture前后核对身份、大小、creation/lastWrite、attributes，SHA必须等于prepared目标，累计ReadFile字节数必须等于prepared大小。lastAccess也原样记录，但读取可能影响访问时间，因此不用于稳定性断言；这不建立对未知私有内存的证明。

2020本机 `core_rxmfcapi.h:453–464` 与同名CHM条目公开注册/移除OnIdle回调；`AcApDocument::isQuiescent` 明确指没有活动command/LISP/ARX命令。N7的公开CommandStack reactor观察S1OPEN成功结束，然后只在idle且文档quiescent、inputPending=0、current=active、workingDB属于exact自有目标时排队一次固定捕获命令。文档becameCurrent事件只记录候选，不再立即排队。该改动不声称所有S1OPEN内部命令执行都违反SDK，只移除这次已观察到的未证明安全嵌套窗口。

idle hook有配对移除；移除失败拒绝cleanup/模块卸载。没有MFC对象或UI调用；SDK头中未用到的CPoint坐标声明只用标准C++前向声明满足编译。
