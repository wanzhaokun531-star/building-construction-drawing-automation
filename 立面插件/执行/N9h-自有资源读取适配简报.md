# N9h：满足实际GS资源请求的公开自有对象适配

任务6下一实现切片，唯一fresh作者。先读本简报与N9d调查报告末尾resource-cache续篇；基线v24 build/n9c-20261103-131202-815-n9g-model-factory-v24-frozen-34b66b1a/src，N9g已经一次完整review PASS。工程根D:/Codex/建筑施工图自动化，仅现有NativeGsTextFootprint.cpp/必要README、本项SDK/差异证据和报告。不要重读17版长历史/重审已闭环N9e/f/g。

## 已出现的真实断点

N9g B kernel/value模型真实get一次（raw3014301743760/validationfalse/main thread），返回空/unsupported，未请求release。该raw并非dbCookie/marker/modelspace/后来style；类型未知，不解码或硬编码此值。原main failed且无RGB不能被这次探针改判。现在需按公开API提供真实自有AcDbObject→AcGiDrawable读取能力，动态识别角色并满足合法请求；不再调图形参数或palette。

## 公开目录与生命周期

1. 两fresh诊断臂仍A manager/value、B kernel/value，同图元/设备/相机/trait数值/更新；新增共同的完整自有DB准备与资源catalog。为避免kForRead pin阻塞后续样式写入，**两诊断臂先完成marker及owned style等所有显式DB写入，核16字段/source，再建立只读目录和read-open缓存，之后才创建model/view**。main及其它原控制仍原时序。新比较与v24相比包含准备顺序/cache就绪变化，结果不能只归因provider；两臂本次内部仍同构。
2. 只枚举本Session新建自有空DB：九种SymbolTable本体及records（Block/Layer/TextStyle/Linetype/View/UCS/Viewport/RegApp/DimStyle），NOD字典路径/值及字典派生类可按公开迭代递归。以**真实已取得的AcDbObjectId.asOldId**建key，记公开角色路径/class/ID/database及指针。visited/dedup处理多路径/循环，不设样本句柄/任意数量上限或读取用户/ambient对象。AcDbObject公开继承AcGiDrawable，可返回真对象指针。只缓存成功kForRead、同ownDb、对象ID/指针一致者；不可读/未知目录如实保留不可用，不能伪造完整性。
3. 每次本provider成功open必须在最终配对close；驻库对象不能delete。Iterator本身按SDKdelete。重复目录访问不能丢额外open引用。cache中已read-open真对象地址稳定，存活到model实际delete完成后；之后核实际close返回值逐个释放，再销毁db。close失败保留未释放项/Session、拒绝卸载，不报告all released或先删DB。
4. named get只锁内存目录、精确匹配已知key并返回对应真AcGiDrawable*；不从raw调用setFromOldId/反推handle/打开未知或foreign对象，不调用AcDb API去猜角色。记录实际get的raw/flag/thread、matched角色/class/真实ID/返回指针，unsupported记录具体原因。bNeedsValidation只记原值，不编造验证协议。release只管理本provider借用计数/对应指针，不关闭或delete不是自己取得的对象；底层一次read-open引用继续pin到deleteModel之后。get/release均局部catch、同步数据安全；model删后才撤provider，所有cache/owner/observer资源进入terminal和unload保护。
5. 若engine未调用get，不能声称动态验证资源适配；若仍未知请求保留unsupported，不假成功。若成功匹配，报告确切角色和公开class，不用“猜测是图层”。不得以同一次数字地址复用称同实例。原callback/RGB/固定标定条件保持，真实callback后才快照，已写raw在标定失败仍保留；diagnosticOnly/qualificationfalse、无文字或主5×2，不授BUILD/glyph/完整采集资格。

## 最小异常验证与主流程边界

复用原after-model-association故障点：**将本轮fault1 Session改为generic+directvalue+上述完整catalog/cache**（仍同预期注入字符串），以实际验证cache ready/provider/model已创建后异常清理。旧manager fault1已有冻结结果复用、不冒称本轮再测；fault3/fault2 manager路径及原main保持。不新造故障框架/大矩阵。控制记录cache实际read-open/关闭数、错误/未释放项、provider撤销及ambient保护；不把未发生的get/release计作已测。

本轮A/B normal paths验证实际借用/释放与资源角色。计数与持有read-open引用分开：底层引用按自身open配对，不为每次借出重复close。GS不发生release等未观察行为如实报告；不编造归还完成。模型已delete才收cache这一实际生命周期须明确，不借cookie证明context绑定。

准备顺序调整与catalog/cache自身写入仅限自有内存DB，绝不改用户current/active/working。原main的failed不被诊断成功撤销；若首次取得RGB，仍仅有限nontext诊断。公共NativeEvidence/host/watch/bridge、TextOccupancy及其它生产src/旧冻结制品不改。

## 实施交接

复用现有build工具及NativeGsTextFootprint case。唯一报告执行/N9h-自有资源读取适配报告.md，写setup写入完成→catalog pin→model→actual get/return/release→delete model→close cache→db释放的事实链。先编译/自审/冻结receipt及相对v24小diff交root；作者不启动/控制CAD/派代理或审查/提交切支/改共享配置。唯一controller一次fresh后补原生事实，不重复旧矩阵或为镜像metadata造测试。

SDK依据以本机2020头/CHM，AcDbObject/表/字典公开open-close契约为准。未知资源/失败分支不能偷换成已成功适配；有具体API/生命周期阻碍及时报root，别擅改第三路线。

用户保护：SDK/ObjectARX C++，无COM/CUA/TCH业务命令面板或私有字段，不写原DWG/快照/用户CAD，不保存/另存图，不动共用profile/CP登记/平台DLL或自动安装。无SDK/配套平面不再问。
