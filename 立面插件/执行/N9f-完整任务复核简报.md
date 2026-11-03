# N9f一次完整限定任务复核

先读N9f-样式传递对照简报.md与N9f-样式传递对照报告.md，核Spec+Quality；报告N9f-full-task-review-20261103.md。只读源码/证据，唯一写复核报告，不派子代理/二审、不跑CAD/重构建或全套测试、不git扫描共享dirty树。需要时读review-agent/ObjectARX2020技能。

只比较冻结v19→最终v21：base build/n9c-20261103-124108-755-n9e-gs-boundary-observation-v19-frozen-7958f6f8/src；candidate build/n9c-20261103-125502-105-n9f-style-transport-v21-frozen-b12d7cf0/src；完整148行diff为v21目录 n9f-v19-style-transport.diff。同目录self-review、receipt绑定源码及固定原函数分段。receipt 2DE9716ACDCB39682D626EF32115C1EA9D273B797107BD79E0DF522A03F2184F；ARX DBECB9814CDAECCEE8B89D472F6F037C65D4165925BFAEFE8094CEC52EB6FF5D。v20未native。不要重新审已完成N9e/v19或旧N9c17版。

actual session 阶段1/runs/elevation-native-20261103-125656-6a479330；执行/N9f-实际证据索引.json有18文件及必要行。原main failed保持，A(ID)/B(value)各fresh Session都实际到非空device/view update，均零WD/RGB；B四traits差异记全且诊断gate=false，不是metadata提前退出。结果是对照完成、N9c依然失败，不能拿未取得像素作为本task未实现，也不能放宽成采集通过。

要核：两臂同构/唯一setter变量与实际样式不等解释、value仅诊断可达/主和控制门保持、A安全清理后B执行及未知/错误前臂的跳过记录、真实callback/buffer/映射条件、稳定资源/卸载保护、JSON事实及原failure不撤销。若需看完整函数，仅为截断hunk/具名风险定点补读，说明范围。只在已有证据不能回答具体疑点时最小focused检查；不重复源码全树/运行矩阵。

实际closure95FFFBE4193FCB7B0BE18DB712FE1F10DD3F8ADE3909F35ED0B39865B5353000。特别：exit70、explicitStopRequested=true但forcedTermination=false，normalQuit=false/nativeQuitConfirmedfalse；不要以旧模板写强停true或正常QUIT。4退役6输入保持/own0，其它47676未碰、relatedReviewfalse。唯一SDK LNK4099为既有供应商PDB信息非本源警告，不要求改供应商文件。

分别给Spec/Quality verdict和所有可行动发现，精确文件行/触发/影响；报告未触发分支不当动态覆盖。本task不授任何完整字形/BUILD/生产资格，不以probe元数据创建新产品门槛。
