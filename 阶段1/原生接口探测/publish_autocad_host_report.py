"""Publish bounded stage1 evidence after native hosts have exited and cleanup passed."""
from pathlib import Path
import datetime,hashlib,json,shutil
P=Path(__file__).resolve().parent;S=P.parent;ROOT=S.parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def bound(p):return dict(path=str(p.resolve()),SHA256=sha(p))
def save(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
def link(p,label):return '['+label+']('+str(p.resolve()).replace('\\','/')+')'
def main():
 closure=P/'AutoCAD宿主迁移_输入与模块终核_20261029.json';d=read(closure);assert d['completed'] and all(x['stagingCleanup']['directoryAbsent'] for x in d['sessions'])
 stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S');recorded=datetime.datetime.now().astimezone().isoformat()
 latest={}
 for item in d['cases']:
  if 'currentValidation' in item:latest[item['mode']]=item
 positive=read(Path(d['negativeProtection'][-1]['path']));assert positive['completed'] and positive['preserved']
 lines=[]
 labels={'AutoCadHost':'普通 AutoCAD，无天正组件','AutoCadSupport':'专用配置的组件/安全核对','AutoCadSupportedHost':'AutoCAD 加对象支持组件','SchemeNative':'方案实际墙端点生成/编辑天正尺寸','BusinessReadNative':'直接实体 list() 的业务参数读取','FamilyNative':'18类对象家族与拆克隆','StylesNative':'资源表样本核对','SafetyNative':'事务及资源保护','GeometryNative':'标准几何能力控制样本','CandidateDiscardNative':'候选库验证、拒绝覆盖及成功发布','ReopenDimensionNative':'已保存天正尺寸重开、移动、读回及恢复','RxDiscoveryNative':'公开 RX 属性只读发现'}
 for mode,label in labels.items():
  case=latest[mode];v=case['currentValidation'];lines.append(f"| {label} | {v['passed']}/{v['assertions']}，失败{v['failed']} | `{Path(case['run']).name}` |")
 output=S/'交付';output.mkdir(exist_ok=True);dim=Path(latest['ReopenDimensionNative']['run']);deliveries=[]
 for name,target in [('dimension-forward.dwg','AutoCAD原生尺寸_线位试移_阶段1.dwg'),('dimension-restored.dwg','AutoCAD原生尺寸_线位恢复_阶段1.dwg')]:
  source=dim/name;dest=output/target
  if dest.exists():assert sha(dest)==sha(source)
  else:shutil.copyfile(source,dest)
  assert sha(dest)==sha(source);deliveries.append(dict(**bound(dest),source=bound(source),scope='Stage1 native TCH dimension fixture; not a completed construction plan'))
 rx=read(Path(latest['RxDiscoveryNative']['nativeReport']['path']));sampled=[x['metadata'] for source in rx['sources'] for x in source['samples']];classes=sorted({x['rxName'] for x in sampled});owned=[v for x in sampled for v in x['members'] if str(v.get('owner','')).startswith('TDb')];assert not owned and len(sampled)==22 and len(classes)==16
 normals=[x for x in d['sessions'] if x['workerExitCode']==0];assert len(normals)==7
 report=S/'AutoCAD原生接口迁移与继续验证报告_20261029.md'
 report.write_text('''# AutoCAD 原生接口迁移与继续验证报告

本轮已在普通 AutoCAD 2020 的专用测试配置中接通已安装天正对象组件，完成原生读取、实际尺寸克隆与夹点编辑、保存重开、候选文件保护和公开 RX 属性发现。**“仅用 AutoCAD SDK、不调用天正业务命令或面板”尚未证明可覆盖全部生成需求；阶段1全量仍未通过，未进入阶段2。**

## 技术边界与输入

- 业务访问为 ObjectARX C++；调度为专用非输入桌面的原生文件队列。无 COM、无 Computer Use、未调用天正业务命令或业务参数面板。直接实体 `list()` 是公开虚函数调用，不是天正 LIST 命令。
- 普通 AutoCAD 不加载天正组件时，两图各有143个代理实体；加载已安装组件后为0代理，方案213、参考618模型对象。代理缓存或原类名不计为真实天正对象访问。
- 使用用户授权的 `CodexS1Native_6f6df19579f3` 测试配置，SECURELOAD 保持1。只对该配置增加已安装 SYS23x64 的信任和支持路径，保留供后续阶段1复用；未直接改日常配置。初次复制配置前后源配置指纹一致，终核另记录当前关键字段，不能据此声称其他进程造成的全配置变化也不存在。
- 该安装会自动初始化多项天正 UI/公共模块，虽菜单为 acad，本轮不能称为“纯净对象解释器部署”或不依赖天正。仅在专用桌面关闭已核准的“日积月累”启动提示；未点击安全提示、许可提示或天正业务面板。首次支持路径中的 SYS 会使启动遇到签名提示，随后仅在专用配置移除该路径，保留 SYS23x64；没有降低安全策略。
- 写入只在阶段1新副本/输出；实际输入仍是冻结方案5BC4122C…与参考8C3F8727…。两图1单位=1mm。原路径两份DWG仅只读记录当前哈希及保存时间，不恢复、不覆盖、不推断建筑内容或制图标准有变更。6组快照保持；原路径方案现为D92251DF…（16:18保存）、参考现为245CB01E…（16:49保存），均与冻结字节不同。另4组源资料保持，差异独立保留，不称全部原文件未变。

## 本机实证

下列为当前独立验收器对各自原生报告的复核数，不能相加当完成率，也没有把所有业务重新标成最后一版 ARX。

| 测试 | 实际检查 | 运行 |
|---|---:|---|
'''+ '\n'.join(lines)+'''

尺寸测试以方案真实墙 CD83 的端点生成3600天正尺寸。新对象 D0D3 为 TDbDimension2；已保存夹具在新库中打开后，GP0 线位移动−123mm、保存读回，再恢复并再次保存读回，原213对象公开状态保持。独立测点未移动。

读回3600来自天正对象公开临时分解表示中的原生 AcDbAlignedDimension 测量与格式化 API；临时表示未入库且已释放，根对象没有替换。格式化原文为 `\\A1;3600`，原生 MText 解析后为3600。**它不是本轮实际图形字形/PDF渲染验收。** 旧泛型文字读取器遇到 AcDbAlignedDimension 不支持，原38项中10项失败保留；v2专用读取68项通过。

候选库三例分别是“保存后故意验证失败不发布”“既有发布目标拒绝覆盖”“验证通过后原子移动并新库读回”。另真实复现并修复预检查拒绝后误删已有候选文件的问题：CREATE_NEW 独占预留、本次所有权清理，新反例确认已有字节保留、拒绝分支没有发布、源对象保持。这不是任意崩溃或所有故障的完整原子性承诺，也没有把旧事务内存问题改判通过。

## 纯原生业务字段的缺口

公开 AcRxMemberQueryEngine / AcRxProperty::getValue 实测方案8类、参考14类，共22个代表对象、合计16种天正实体类。每个对象列出45或52项继承的通用属性；普通 AcDbLine 正控制列出57项。没有发现由 TDb 类声明的业务属性，也未发现编号、标高值、索引编号等业务字段的通用 RX 入口。该试验为只读，33项通过不代表业务 setter 通过。

实测类别包括墙、门窗/组合门窗、文字、尺寸、标高、楼梯、图名、轴号、柱、引注/箭头、剖切、组合符号、管线和管件。结果只说明本机这些样本的公开 RX 通道没有提供所需业务字段，**不证明天正不存在其他厂商公开接口**。

因此，旧四侧轴网/连续编号、文字、标高、坡向、剖切及索引的天正命令桥成功记录仍保留为历史路线证据；不能替代本轮“不用天正业务命令”的原生业务写入证明。后续关键依赖是能核实的天正公开 C++ 业务接口/头文件或厂商接口说明；现有 AutoCAD SDK 通用属性和几何 API 本身不能作为全部业务写入已实现的依据，不猜写天正私有序列化字段。

## 未通过项及阶段边界

- 家族试验在新宿主仍复现4项失败：单独门/窗克隆后的表示厚度变化。整份定稿方案保留路线不需拆重建这些构件，但失败记录保留。
- 事务试验仍复现2项失败：abort 后内存孤儿 LINE 仍可 read-open；模型和保存恢复通过。候选库隔离只补证上述有限情形。
- 完整参考样式、实体局部覆盖、字体实文件、CTB/STB、全图布局/图框、最终纸面实测、自动避让和全部工程选位仍未验收。资源表61项及标准几何控制样本不替代这些项目。
- 房间净边界及纵向尺寸语义、门窗类别/分段语义与全部参数写入还需对应样本或公开接口验证。本轮未改变定稿方案、不开发增量同步、未进入阶段2。

## 宿主退出、无打扰观察与安全清理

9个专用 AutoCAD 会话中，7个真实原生 QUIT 正常退出0；2个早期签名安全提示阻塞会话只在精确身份核准后停止本任务自有宿主，保留失败。正常退出耗时较长，原生通知、进程退出和监督器终态均独立留证，不把“已排队QUIT”算通过。

'''+ '\n'.join(f"- `{Path(x['session']).name}`：{x['normalQuit']['seconds']:.1f}秒，退出0，未强制结束。" for x in normals)+'''

本任务宿主在用户桌面的 SHOW 和抢前台观测为0；部分会话记录到其他既有CAD进程的窗口活动，严格验收标为 REVIEW_REQUIRED，未自动归因于用户、其他任务或本任务。因此只声明本任务宿主的观测边界，不声称所有桌面活动都已无打扰。未切换用户输入桌面、未操作用户CAD。

全部9个会话均已退出，按 nonce、映像、创建时间核准；PID复用不当成旧宿主。唯一暂存 ARX/INI 精确归档后移除，任务临时目录为空并删除；没有递归清理安装目录或删除其他插件。

## 交付与核对入口

'''+ '\n'.join('- '+link(Path(x['path']),Path(x['path']).name) for x in deliveries)+ '\n\n- '+link(closure,'输入、会话、原生报告与模块终核')+'\n- '+link(S/'AutoCAD原生续测_独立复核记录_20261029.md','独立复核和修复记录')+'''

两份 DWG 是能力夹具，不是正式施工图。276项离线回归通过。完整阶段1结论仍为部分能力通过、全部生成需求未闭环。
''',encoding='utf-8')
 backup=P/'evidence'/('before-autocad-status-'+stamp);backup.mkdir()
 marker='<!-- autocad-native-host-20261029 -->'
 note=f'''{marker}
**2026-10-29 最新技术约束及结果：** 普通 AutoCAD 专用测试配置 + 已安装天正对象组件 + ObjectARX C++，本轮无COM/Computer Use、无天正业务命令或面板。0代理、原生尺寸2189项、保存夹具重开68项、候选发布27项及已有文件保护反例通过；22个代表对象/16类的RX发现33项只见继承通用属性，未发现业务字段通用写入入口。家族4项、事务2项失败在新宿主复现。9会话中7个原生QUIT退出0，2个早期安全阻塞精确停止并保留失败；自有宿主用户桌面SHOW/前台均0，其他CAD活动单列REVIEW_REQUIRED。源和6组快照终核、暂存精确清理完成。旧命令桥成功不替代本轮纯原生业务写入验收，阶段1全量未通过，未进入阶段2。详见 {link(report,'本轮报告')}。

'''
 for path in [S/'全需求能力覆盖矩阵.md',S/'阶段1全需求验证结论.md',S/'执行记录.md']:
  shutil.copyfile(path,backup/path.name);text=path.read_text(encoding='utf-8-sig')
  if marker not in text:
   at=text.find('\n')+1;text=text[:at]+'\n'+note+text[at:];path.write_text(text,encoding='utf-8')
 agents=ROOT/'AGENTS.md';shutil.copyfile(agents,backup/'AGENTS.md');text=agents.read_text(encoding='utf-8-sig')
 if marker not in text:agents.write_text(text+'\n\n## 2026-10-29 普通AutoCAD专用配置与原生续测\n\n'+note+'用户明确授权仅修改专用测试配置；SECURELOAD保持1，日常配置不直接改。对象支持来自已安装完整天正组件，会自动加载多项UI模块，不称纯净对象解释器。仅关闭专用宿主已核准的“日积月累”启动提示，不点击安全/许可提示或操作业务面板。原始方案仍只作只读观察，冻结5BC继续固定输入。\n',encoding='utf-8')
 indexpath=S/'全需求验证证据索引.json';shutil.copyfile(indexpath,backup/indexpath.name);index=read(indexpath);ids=set(x['id'] for x in index['results'])
 for offset,(mode,label) in enumerate(labels.items(),35):
  identifier='E'+str(offset)
  if identifier in ids:continue
  case=latest[mode];v=case['currentValidation'];index['results'].append(dict(id=identifier,kind=mode,label=label,route='ordinary AutoCAD + installed TCH support + public ObjectARX, no TCH business commands',run=case['run'],nativeReport=case['nativeReport']['path'],nativeReportSHA256=case['nativeReport']['SHA256'],assertions=v['assertions'],passed=v['passed'],failed=v['failed'],result=v['result'],binarySHA256=case['binarySHA256'],sourceFilesAtRun=case['sourceFiles'],receipt=case['receipt'],controllerAtRun=case.get('controllerAtRun'),controllerArchiveGap=case.get('controllerArchiveGap'),validation=v,phase2Started=False))
 if 'E47' not in ids:index['results'].append(dict(id='E47',kind='PreexistingCandidateProtection',run=positive['run'],nativeReport=positive['nativeReport'],nativeReportSHA256=positive['nativeReportSHA256'],binarySHA256=positive['binarySHA256'],result='PASS_EXPECTED_PREFLIGHT_REJECTION',expectedNativeAttemptRejected=True,assertions=5,passed=5,failed=0,evidence=d['negativeProtection'][-1],scope='Five actual driver checks; expected native case rejection is not a successful full native publication-suite run'))
 index['latestAutoCadNative']=dict(recorded=recorded,report=bound(report),closure=bound(closure),phase1FullAcceptance=False,phase2Started=False,publicRxBusinessSetterEstablished=False,profile=d['profile'],inputs=d['inputs'],sixGroupInputAudit=d['sixGroupInputAudit'],negativeProtection=d['negativeProtection']);index['phase1FullAcceptance']=False;index['phase2Started']=False
 index['inputWarning']='Historical92E/7055 observations retained in older records. Latest working originals: schemeD92251DF and reference245CB01E, observed read-only; all current tests remain bound to frozen5BC/8C3 and all six snapshots unchanged. No architectural or standard-change attribution/restore.'
 index['externalOrchestration']='Historical evidence includes COM and TCH command bridges. Latest ordinary-AutoCAD round uses private desktop/native file queue and public ObjectARX; no external COM/CUA/TCH business commands. Vendor internal startup behavior is not claimed absent.';save(indexpath,index)
 manifest=output/'AutoCAD原生续测_交付清单_20261029.json';save(manifest,dict(contract='stage1.autocad-native-delivery.v1',recorded=recorded,phase1FullAcceptance=False,phase2Started=False,report=bound(report),closure=bound(closure),artifacts=deliveries,independentReview=bound(S/'AutoCAD原生续测_独立复核记录_20261029.md'),latestBinary=d['latestCandidate'],offlineRegression=d['offlineRegression']))
 globalpath=S/'本轮交付清单.json';shutil.copyfile(globalpath,backup/globalpath.name);globaldata=read(globalpath)
 globaldata.update(recorded=recorded,phase1FullAcceptance=False,phase2Started=False,observedCurrentOriginalSHA256=d['inputs'][2]['SHA256'],observedCurrentReferenceSHA256=d['inputs'][3]['SHA256'],inputScope='Current frozen5BC/8C3 tested inputs; working original DWGs observed read-only, no architectural/standard-change inference. See latest ordinary-AutoCAD closure.',latestAutoCadNativeDelivery=bound(manifest),priorManifest=bound(backup/globalpath.name))
 paths=[Path(x['path']) for x in globaldata['artifacts']]+[report,closure,manifest,S/'AutoCAD原生续测_独立复核记录_20261029.md',*[Path(x['path']) for x in deliveries]]
 globaldata['artifacts']=[bound(x) for x in dict.fromkeys(paths) if x.exists()];save(globalpath,globaldata)
 ledger=P/'AutoCAD宿主迁移_执行计划_20261029.json';data=read(ledger);data.update(recorded=recorded,boundedContinuationComplete=True,phase1FullAcceptance=False,phase2Started=False,currentStop='Native ordinary-AutoCAD continuation documented; required TCH business setters not exposed by observed RX channel, public vendor interface remains unproven.',closure=bound(closure),report=bound(report),delivery=bound(manifest));save(ledger,data)
 print(json.dumps(dict(report=str(report),delivery=str(manifest),evidenceIds='E35-E47',phase1FullAcceptance=False,phase2Started=False),ensure_ascii=False))
if __name__=='__main__':main()
