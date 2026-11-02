"""Publish bounded stage1 evidence without changing historical native receipts."""
from pathlib import Path
from datetime import datetime
import hashlib,json,shutil

PROJECT=Path(__file__).resolve().parent;STAGE=PROJECT.parent;ROOT=STAGE.parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def write(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def ref(p):return dict(path=str(p),SHA256=sha(p))

def main():
 closure_path=Path((PROJECT/'current-symbol-closure.txt').read_text(encoding='utf-8').strip());closure=read(closure_path)
 session=Path(closure['session']);quit=read(session/'normal-quit-verification.json');host=read(session/'isolation-final.json')
 report=STAGE/'坡向剖切详图索引与正常退出验证报告_20261029.md'
 assert closure['phase2Started'] is False and quit['completed'] and not quit['forcedTermination']
 labels={'slope':'坡向','section':'剖切','pointer-index':'指向索引','section-index':'剖切索引'}
 manifest=dict(contract='stage1.symbol-quit-delivery.v1',recorded=datetime.now().astimezone().isoformat(),closure=ref(closure_path),phase2Started=False,phase1FullAcceptance=False,artifacts=[],cases=[])
 rows=[];records=[]
 for n,case in enumerate(closure['cases'],31):
  kind=case['kind'];label=labels[kind];run=Path(case['run']);native=read(run/'symbol-business-native.json')
  dwg=STAGE/'交付'/f'方案图副本_{label}原生能力验证_20261029.dwg';pdf=STAGE/'交付'/f'{label}原生能力诊断_20261029.pdf'
  for source,target in [(Path(case['drawing']['path']),dwg),(Path(case['pdf']['path']),pdf)]:
   if not target.exists():shutil.copyfile(source,target)
   assert sha(source)==sha(target)
   manifest['artifacts'].append(ref(target))
  values='2% / 2%，相反箭头方向' if kind=='slope' else '31 / 32，图号S1-01，两端及方向' if kind=='section' else '7 / 8，图号S1-01，节点/折点/圈位'+('及剖视方向' if kind=='section-index' else '')
  rows.append(f"| {label} | `{native['symbols'][0]['rxName']}` | {values} | {case['business']['passed']} | {case['geometry']['passed']} | {case['preview']['passed']} |")
  manifest['cases'].append(dict(kind=kind,drawing=ref(dwg),diagnosticPDF=ref(pdf),native=case['report'],business=case['business'],geometry=case['geometry'],preview=case['preview']))
  records.append(dict(id=f'E{n}',kind=kind,result='PASS_BOUNDED_NATIVE_SYMBOL',nativeReport=case['report']['path'],nativeReportSHA256=case['report']['SHA256'],run=str(run),binarySHA256=closure['binarySHA256'],business=case['business'],geometry=case['geometry'],preview=case['preview'],COMUsed=False,ComputerUseUsed=False,phase2Started=False,scope='Two explicit stage1 fixtures on frozen scheme, actual native types/text/geometry and saved fresh-reader/PDF; not project rules or final standards'))
 total_business=sum(c['business']['passed'] for c in closure['cases']);total_geometry=sum(c['geometry']['passed'] for c in closure['cases']);total_preview=sum(c['preview']['passed'] for c in closure['cases'])
 report.write_text(f'''# 阶段1：坡向、剖切、详图索引与正常QUIT验证

更新：2026年10月29日。本轮四类符号的最小能力验证和宿主正常QUIT均已取得实际证据。仍处于阶段1，整阶段尚未验收，不进入阶段2。

## 已验证结果

| 对象 | 新增真实天正类型 | 实际读回及几何 | 文字/持久化检查 | 独立几何检查 | 原生PDF检查 |
|---|---|---|---:|---:|---:|
{chr(10).join(rows)}

合计{total_business}项文字/对象/保护/持久化检查、{total_geometry}项独立测点和方向检查、{total_preview}项原生PDF检查通过；另有10项验收器回归通过，包含故意移除箭头头部、篡改保存文字、捕捉偏移、方向反转、默认值未恢复和临时对象未擦除的拒绝检查。断言数不是需求完成率。

- 每类都从冻结方案的新副本出发，保留原213个模型对象及6个原标高公开表示，新增2个非代理天正对象，最终215个。原资源定义保留，命令自动新增的资源单列。没有将普通文字、圆或引线拼成对应天正根对象。
- 文字通过公开const explode临时表示读回，输入测点通过原生InputContext事件记录；只释放未入库返回对象。保存DWG后先销毁首个读取库，再用新库读回原件及两个符号。
- 坡向和索引使用公开创建命令后，对确切自有新对象调用`TObjEdit`，由隔离桌面内C++标准Win32控件桥设置文字。剖切使用已证的公开创建面板流程。它们是后台界面桥，不称纯无面板数值API。
- 每次试验均实测当次开始的面板默认值，完成后恢复并独立重新打开核对。对于编辑会更新创建默认值的类型，克隆本任务自有符号作为临时恢复对象，提交原值后仅擦除此临时克隆，原213及两个保留符号不擦除。全过程模型数215→216→215有证据。
- 实测原宿主`OSMODE=4133`、`OSNAPCOORD=2`会使部分脚本点受对象捕捉影响。最终版本在创建命令期间暂设OSMODE=0、SNAPMODE=0、ORTHOMODE=0、OSNAPCOORD=1，逐项读回并恢复；实际WCS测点与独立夹具一致。坡向箭头头部以实际多段线锥形宽度检查，未错误地只按顶点非共线判断。

## 正常QUIT与窗口隔离

最新会话：`{session.name}`，PID `{host['processId']}`。原生QUIT事件包含commandWillStart/Ended、docCloseWillStart、beginQuit、acrxQuitMsg、quitWillStart；监督器观察到进程退出码0、forcedTermination=false，进程和桌面句柄均已关闭。此次从排队到监督器确认退出约 **{quit['queueToSupervisorExitSeconds']:.1f}秒**。

`native.quit.done`仅表示原生命令已经排队。控制器现在继续等待独立进程退出证明，默认等待上限900秒；超时只留记录，不自动终止或误报通过。之前300秒超时的一次会话后续约726秒才正常退出，原超时记录保留。因此已证正常退出路径可行，**尚不保证快速退出或固定延迟**；未通过强制结束代替QUIT。

本会话窗口验收：{closure['hostValidation']['assertions']}项，结果`{closure['hostValidation']['result']}`；{host['sampleCount']}个采样，测试宿主在用户桌面的SHOW事件、抢前台事件及前台采样均0；其他已识别CAD的SHOW/前台事件也分别为{host['relatedCadShowEvents']}/{host['relatedCadForegroundEvents']}。输入桌面未切换，未调用SwitchDesktop，未操作用户CAD。该结论限本次有界观察，不等于全电脑永远无窗口。启动/监测使用既有原生隔离宿主辅助程序，业务操作均为ObjectARX C++；本轮无COM、无Computer Use。

## 证据和交付

- [终核JSON]({closure_path.relative_to(STAGE).as_posix()})绑定4个输入哈希、4组真实报告/验证器/保存DWG/PDF和窗口终核。最终ARX SHA256：`{closure['binarySHA256']}`；源码及二进制在该会话candidate内按哈希归档，验收器另行归档。
- [正常退出独立回执]({(session/'normal-quit-verification.json').relative_to(STAGE).as_posix()})与[native退出事件]({(session/'native-quit-events.jsonl').relative_to(STAGE).as_posix()})。
- [本轮交付清单](交付/坡向剖切索引与QUIT_交付清单_20261029.json)包含4份方案图副本和4份诊断PDF的哈希。每份副本只增加本类型两件试验对象，未合成为正式施工图。

两个冻结输入的哈希保持5BC4122C…和8C3F8727…，全部新试验回执绑定冻结方案5BC。用户工作原图本次只读观察为70554DC4…、3655782字节，文件保存时间14:19，与此前92E01E8D…不同；该原图不是本轮试验输入，不覆盖或恢复它，也不由字节差异推断建筑内容发生修改。完整当前哈希、原记录和文件角色单列在终核JSON。10个本轮专用会话已退出，其精确nonce/哈希绑定的暂存ARX/INI均归档后移除；两个数字PID已被conhost/ChsIME复用，通过创建时间及映像识别，未对这些无关进程进行任何控制。

## 图面检查与验收边界

四页均经原生AcPl输出、Poppler渲染并目视检查。坡向两箭头相反且均显示2%；剖切31/32和S1-01各两端可读；两类索引分别显示7/8及S1-01，剖切索引另有可见剖切短线。每页为A3、1:100的诊断窗口，非正式图框布局。

诊断窗口保留原图所有对象，因此坡向、指向索引和剖切页仍有原图青色线穿过部分测试位置；该现象明确保留，不能称已完成自动避让。未为改善预览删除或隐藏原建筑。绿色/黑色等颜色、字体、线宽、纸面尺寸及CTB还未按参考图作最终验收。

2%、31/32、7/8及S1-01均为明确试验值，不是本项目坡率、正式编号或有效详图引用。穿过楼梯的自动剖切选择、按区域选代表索引、固定详图库匹配、实际坡率/排向、批量避让及正式样式属于后续规则和样图；本轮不编写阶段2生成逻辑。

## 保留失败与下一步

早期空命令不能提交部分面板文字、创建命令锁存值导致滞后一件、私有桌面字段焦点被拒、指向索引范围提示遗漏、索引编辑实际标题不匹配，以及对象捕捉造成测点偏移的记录均保留。文字检查PASS而几何失败的早期样例不算完整通过。一次自有索引编辑面板等待由经PID/创建时间/桌面/实际标题和取消按钮验证的C++标准Win32工具取消，随后原生QUIT退出；没有使用COM、Computer Use或操作未知窗口。

阶段1剩余主要技术补证为：真实墙内边净距及纵向天正尺寸；LC/FDM子型与分段及编号定位；长说明/引线和图名独立组件；实例资源/实际字体；完整框图签及原生多页；失败候选整库丢弃/不发布及重开后编辑。已验证的标高、坡向、剖切、真正详图索引和正常QUIT不再整体列为“未证”，各自的正式生成规则和样式仍保留边界。
''',encoding='utf-8')
 manifest['artifacts'].append(ref(report));write(STAGE/'交付/坡向剖切索引与QUIT_交付清单_20261029.json',manifest)
 # Add current evidence; keep the original result rows and old failure receipts.
 index_path=STAGE/'全需求验证证据索引.json';index=read(index_path)
 current=[dict(id='E30',kind='NativeNormalQuit',result='PASS_ACTUAL_PROCESS_EXIT',nativeReport=str(session/'normal-quit-verification.json'),nativeReportSHA256=sha(session/'normal-quit-verification.json'),binarySHA256=closure['binarySHA256'],COMUsed=False,ComputerUseUsed=False,scope='Actual native notifications, normal exit0, no force, closed handles; no bounded fast-exit guarantee')]+records
 index['results']=[x for x in index['results'] if x['id'] not in {x['id'] for x in current}]+current
 index['latestNativeSymbolsRound']=dict(recorded=closure['recorded'],report=ref(report),closure=ref(closure_path),phase1FullAcceptance=False,phase2Started=False,COMUsed=False,ComputerUseUsed=False)
 index['externalOrchestration']='Historical E0–E24 includes COM scheduling; current E30–E34 uses dedicated non-input desktop, native file queue and Win32 controls; no COM/CUA'
 write(index_path,index)
 plan_path=PROJECT/'坡向剖切索引与QUIT_执行计划_20261029.json';plan=read(plan_path)
 for step in plan['steps']:
  if step['id'] in ('quit-baseline','slope','section','detail-index','closure'):step.update(status='complete_bounded_probe',evidence=str(closure_path),actual='Final four types text/geometry/save/PDF plus normal process exit and exact staging closure; full phase1 still pending')
 plan['latestClosure']=ref(closure_path);write(plan_path,plan)
 print(json.dumps(dict(report=str(report),manifest=str(STAGE/'交付/坡向剖切索引与QUIT_交付清单_20261029.json'),business=total_business,geometry=total_geometry,preview=total_preview,normalExitSeconds=quit['queueToSupervisorExitSeconds']),ensure_ascii=False))

if __name__=='__main__':main()
