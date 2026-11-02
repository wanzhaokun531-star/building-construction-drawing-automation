"""Publish bounded native text evidence. Never rewrite failed run results."""
from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,sys
from collect_symbol_closure import owned_process_state
P=Path(__file__).resolve().parent;S=P.parent;ROOT=S.parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def bound(p):return dict(path=str(p),SHA256=sha(p))
def fresh(p,value):
 assert not p.exists();p.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
RUN=S/'runs/quiet-plaintextreplacementnative-20261030-111342-541993';raw=read(RUN/'native-plain-text-replacement.json')
assert raw['singleReplacementPassed'] is True and raw['error']=='replacement refuses multiple or nontext components'
test=subprocess.run([sys.executable,str(S/'tests/verify_plain_text_single.py'),str(RUN/'native-plain-text-replacement.json')],capture_output=True,text=True,encoding='utf-8');assert test.returncode==0
(P/'evidence/plain-text-single-independent-validation-20261030.txt').write_text(test.stdout,encoding='utf-8')
singleAudit=dict(contract='stage1.native-plain-text-single-audit.v1',rawReport=bound(RUN/'native-plain-text-replacement.json'),rawOverallErrorPreserved=raw['error'],rawOverallRunPassed=False,singleOnlyPassed=True,checks=21,validator=bound(S/'tests/verify_plain_text_single.py'),validationLog=bound(P/'evidence/plain-text-single-independent-validation-20261030.txt'),scope='Only CDC0 TDbText to AcDbText; four literal states, identity, typography and protected212 public states. No whole batch or private field write claim.')
fresh(P/'天正文字替换_单项独立验收_20261030.json',singleAudit)
sessions=[]
for name in ['isolated-autocad-20261030-111222','isolated-autocad-20261030-112404']:
 session=S/'runs'/name;cfg=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json');final=read(session/'isolation-final.json');identity=owned_process_state(launch)
 assert identity['ownProcessAlive'] is False and final['workerExited'] is True and final['stillRunningOwnedWorker'] is False
 assert cfg['nonce']==launch['nonce']==final['nonce'] and Path(cfg['session']).resolve()==session.resolve()
 directory=Path(cfg['stagingDirectory']).resolve();assert directory.parent==Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve() and directory.name=='CodexStage1Probe_'+cfg['nonce']
 archive=session/'staging-final-archive';archive.mkdir(exist_ok=True);files=[]
 for f in [Path(cfg['stagedARX']),Path(cfg['stagedINI'])]:
  assert f.resolve().parent==directory;digest=sha(f)
  if f.suffix.lower()=='.arx':assert digest==cfg['binarySHA256']
  else:
   text=f.read_text(encoding='utf-16');assert 'nonce='+cfg['nonce'] in text and 'session='+str(session) in text
  dst=archive/f.name;assert not dst.exists();shutil.copyfile(f,dst);assert sha(dst)==digest and sha(f)==digest;assert owned_process_state(launch)['ownProcessAlive'] is False;f.unlink();files.append(dict(archive=bound(dst),stagedAbsent=not f.exists()))
 if directory.exists() and not list(directory.iterdir()):directory.rmdir()
 sessions.append(dict(session=name,identity=identity,exitCode=final['workerExitCode'],forced=final['forcedTermination'],nativeQuitConfirmed=final['nativeQuitConfirmed'],ownUserShows=final['workerUserDesktopShowEvents'],ownForeground=final['workerForegroundEvents'],relatedCadEvents=final['relatedCadEvents'],inputDesktopChanged=final['inputDesktopChanged'],files=files,stagingDirectoryAbsent=not directory.exists()))
destination=S/'交付/文字替换验证';destination.mkdir(exist_ok=False)
names=['方案副本_单个文字转原生_保持原文.dwg','方案副本_单个文字改为C09235_试验.dwg','方案副本_单个文字改为待确认_试验.dwg','方案副本_原生文字恢复原文.dwg']
artifacts=[]
for i,name in enumerate(names):
 source=RUN/f'native-plain-text-state-{i}.dwg';target=destination/name;shutil.copyfile(source,target);assert sha(source)==sha(target);artifacts.append(bound(target))
(destination/'阅读说明.md').write_text('这些DWG是阶段1文字能力试验副本，不是施工图成果。\n\n只把方案中句柄CDC0的独立“庭院”文字转为AutoCAD原生AcDbText；第一份保留原文，其余为改字及恢复试验。其他212个对象的已核查公开状态保持。原始DWG和冻结输入未覆盖。\n\n未证明全部天正文字可转换；复杂文字保留方案仍待运行，轴号、尺寸、标高及索引的天正整体类型要求不变。\n',encoding='utf-8')
report=S/'天正文字转原生文字验证报告_20261030.md'
report.write_text('''# 天正文字转原生文字：阶段1验证

**已验证单个独立文字可以替换为AutoCAD原生AcDbText，再用SDK修改内容。** 这是用户明确允许文字类型转换后的路线；不是天正私有文字字段写入成功。阶段1全量仍未通过，未进入阶段2。

## 本轮授权与实际方法

用户明确答复“允许文字改为 AutoCAD 原生文字”。文字可采用AcDbText/AcDbMText；本轮实际验证AcDbText。墙、门窗、楼梯及轴号、尺寸、标高、索引等复合对象的整体类型要求没有变更。

仅用ObjectARX C++及AutoCAD2020 SDK：公开const explode提取唯一临时AcDbText显示实体，原生clone保留其字体、字高、宽度、旋转与位置，继承独立文字根对象的通用显示属性，再按SDK官方示例用handOverTo替换。ObjectId/句柄CDC0保留；随后setTextString改字。handOverTo请求保留XData和扩展字典，但未据此宣称天正全部私有关系均已验收。

未调用COM、Computer Use、天正业务命令或面板。普通AutoCAD专用配置在独立非输入桌面运行，SECURELOAD保持1。

## 已完成的单对象闭环

输入绑定冻结方案SHA256 `5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E`。真实run为`quiet-plaintextreplacementnative-20261030-111342-541993`，二进制`1CE534D06E997122B444EEC0110A1D07B4F7A088349C72DD3327455E92446CD4`。

- 原生TDbText“庭院”转为AcDbText，句柄CDC0、所属模型空间保留，模型仍213个对象。
- 原文庭院→C09235→待确认→庭院四态均单独保存，并用全新数据库读回。
- 每态文字内容、Native Text字体样式绑定、插入点、对齐点、字高、宽度系数、旋转、法向及显示属性保持；其余212个对象的已核查公开状态保持。
- 独立单项验收21/21通过。原生PDF出图前后各A3/A2/A1的既有验收通过；选定CDC0“庭院”的A3局部渲染7830字节差异为0，已目视核对。

这只证明本样本的局部显示保持。整页渲染仍有约5000字节差异，位置在该文字局部之外，不能称整张图像素一致。天正私有名义字高与AutoCAD原生显示文字的参数表达不能直接混用，应保留经过核查的Native Text显示参数。

## 必须保留的失败和边界

该v1命令在四态成功后继续尝试34个TDbText全量转换，遇到多个显示部件即保护性拒绝；**原始整体结果18PASS/4FAIL及错误原文均保留**。单对象验收是对已完成部分的另行审计，没有将原run改为整体通过，也没有交付全34转换成果。

v2增加“只转换唯一临时纯Text，其余保留原TCH并列清单”的筛选，已编译为`D3A2A769A587860DA83DCC7E4639F083689E325B928647673E8393006D6D5ABD`，但新会话在worker就绪前被“选择项目”启动业务窗阻断，**没有执行v2，不能计通过**。未操作该面板、未修改全局应用自启动登记。其来源未最终归因，注册表中的其他启动应用仅作排查线索。

此前公开EntMod试验及两个DXF试验也未通过：EntMod和单文字DXF往返导致自有宿主异常退出；整图DXF导入报天正墙体缺少组码10并发生错误阻塞。失败均归档，三个入口已从日常runner模式中移除。它们不影响已证实的原生文字替换路线，也不证明所有天正接口永远不支持改字。

本轮单文字宿主虽记录beginQuit/acrxQuitMsg/quitWillStart，900秒内仍未观察正常进程退出；明确核准后只停止自有宿主。新启动阻塞宿主也精确停止，不能把这两次退出计为正常QUIT通过。测试窗口在用户桌面的SHOW/抢前台事件均为0，所有暂存模块按nonce/哈希归档清理，未结束用户CAD。

## 交付与后续使用

可打开[保持原文的验证DWG](交付/文字替换验证/方案副本_单个文字转原生_保持原文.dwg)。同目录另有C09235、待确认和恢复原文的试验副本，均不是正式施工图成果。

施工图新增独立文字可按已授权规则采用标准Native Text；本轮未验证复杂多部件文字、AcDbMText转换或复合天正符号内文字。后续仍需对照参考图核全部字体与纸面样式，不能用本次文字结果代替轴网编号、尺寸关联和其余阶段1验收。

一次最终只读C++审查未发现有证据支持的P1/P2问题。新增独立验收器及其反例测试另有本机记录；完整离线回归311项通过，断言数不表示需求完成率。
''',encoding='utf-8')
notice='''<!-- plain-text-native-authorization-result-20261030 -->
**2026-10-30 最新文字结论：** 用户明确允许独立文字改为AutoCAD原生文字。SDK原生TDbText→AcDbText的单对象四态替换/保存读回已闭环，独立21项通过，选定庭院文字PDF局部像素一致；其余212对象公开状态保持。原v1后续34批量拒绝的4FAIL保留，v2筛选批量因启动窗阻塞未执行，本轮正常QUIT回归未通过。仅独立文字载体约束更新，复合天正对象仍保持；阶段1全量未通过，未进入阶段2。见[文字替换报告](D:/Codex/建筑施工图自动化/阶段1/天正文字转原生文字验证报告_20261030.md)。

'''
for f in [S/'全需求能力覆盖矩阵.md',S/'阶段1全需求验证结论.md',S/'执行记录.md']:
 text=f.read_text(encoding='utf-8-sig');assert 'plain-text-native-authorization-result-20261030' not in text;f.write_text(notice+text,encoding='utf-8')
with (ROOT/'AGENTS.md').open('a',encoding='utf-8') as f:f.write('\n\n'+notice)
spec=ROOT/'docs/superpowers/specs/2026-10-28-建筑施工图平面自动化需求基线.md'
with spec.open('a',encoding='utf-8') as f:f.write('\n\n## 2026-10-30 文字载体授权更新\n\n用户明确允许需要修改内容的独立天正文字改为AutoCAD原生AcDbText/AcDbMText，并保持字体、位置及图面效果。此前所有新增标注必须为TCH的要求在独立文字部分据此更新。复合轴号、尺寸、标高、索引等整体对象类型要求不变。当前仅单个TDbText转AcDbText已完成能力验证，复杂文字和MText转换尚未验证。方案定稿原则及不进入阶段2的限制不变。\n')
prior=read(P/'纯原生阶段1_输入模块与退出终核_20261030.json');inputs=[]
for row in prior['inputs']:
 result={'id':row['id']}
 for key in ('source','snapshot'):
  file=Path(row[key]['path']);digest=sha(file);result[key]=dict(path=str(file),SHA256=digest,unchangedComparedToPrior=digest==row[key]['SHA256'])
 assert result['snapshot']['unchangedComparedToPrior'];inputs.append(result)
closure=dict(contract='stage1.plain-text-replacement-closure.v1',recorded=datetime.datetime.now().astimezone().isoformat(),singleOnlyPassed=True,batchPassed=False,phase1FullAcceptance=False,phase2Started=False,sessions=sessions,earlierFailedThreeSessionClosure=bound(P/'天正文字替换_失败试验终核_20261030.json'),inputs=inputs,sourceSpecUpdateAuthorized=True,rawOverallFailurePreserved=True)
fresh(P/'文字替换_输入模块与退出终核_20261030.json',closure)
indexFile=S/'全需求验证证据索引.json';index=read(indexFile);ids={r['id'] for r in index['results']}
cases=[('E53','PublicEntModFailed',S/'runs/quiet-publictextpatchnative-20261030-103913-900619/quiet-job-receipt.json','FAILED_OWN_HOST_CRASH'),('E54','WholeDxfFailed',S/'runs/quiet-publicdxftextnative-20261030-104713-927527/native-public-dxf-text.json','FAILED_TCH_WALL_IMPORT'),('E55','SingleTchDxfFailed',S/'runs/quiet-textonlydxfnative-20261030-105430-763667/quiet-job-receipt.json','FAILED_OWN_HOST_CRASH'),('E56','NativePlainTextSingle',P/'天正文字替换_单项独立验收_20261030.json','BOUNDED_SINGLE_PASS_ORIGINAL_BATCH_FAIL_PRESERVED'),('E57','NativeTextPdfCrop',P/'evidence/plain-text-single-target-crop-20261030.json','SELECTED_TEXT_CROP_EXACT_WHOLE_PAGE_NOT_EXACT'),('E58','NativePlainTextV2',S/'runs/isolated-autocad-20261030-112404/isolation-final.json','STARTUP_BLOCKED_NOT_EXECUTED'),('E59','NativePlainTextClosure',P/'文字替换_输入模块与退出终核_20261030.json','ALL_OWN_HOSTS_STOPPED_NOT_NORMAL_QUIT')]
for key,kind,file,status in cases:
 assert key not in ids;index['results'].append(dict(id=key,kind=kind,evidence=bound(file),status=status,route='AutoCAD SDK native only; no COM/CUA/TCH business commands or panels'))
index.update(recorded=datetime.datetime.now().astimezone().isoformat(),phase1FullAcceptance=False,phase2Started=False,latestTextScope='User permits native text; single CDC0 migration proven, batch and MText not proven.');indexFile.write_text(json.dumps(index,ensure_ascii=False,indent=2),encoding='utf-8')
artifacts.extend([bound(report),bound(P/'天正文字替换_单项独立验收_20261030.json'),bound(P/'文字替换_输入模块与退出终核_20261030.json'),bound(P/'evidence/plain-text-single-target-crop-20261030.json'),bound(P/'evidence/plain-text-final-offline-20261030.txt')])
manifest=dict(contract='stage1.native-plain-text-delivery.v1',recorded=datetime.datetime.now().astimezone().isoformat(),scope='single independent text only; no whole phase1 or batch success',singleOnlyPassed=True,phase1FullAcceptance=False,phase2Started=False,artifacts=artifacts)
fresh(S/'交付/文字替换验证_交付清单_20261030.json',manifest)
main=S/'本轮交付清单.json';m=read(main)
for row in m['artifacts']:
 path=Path(row['path'])
 if path.exists():row['SHA256']=sha(path)
m['artifacts'].extend(artifacts+[bound(S/'交付/文字替换验证_交付清单_20261030.json')]);m.update(recorded=datetime.datetime.now().astimezone().isoformat(),phase1FullAcceptance=False,phase2Started=False,latestTextReplacement='bounded native AcDbText migration passed; batch/normal quit not passed');main.write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(report=str(report),deliveredDwg=len(names),singleChecks=21,originalBatchFailuresPreserved=4,newSessionsClosed=len(sessions),allSixSnapshotsUnchanged=True,phase1FullAcceptance=False),ensure_ascii=False))
