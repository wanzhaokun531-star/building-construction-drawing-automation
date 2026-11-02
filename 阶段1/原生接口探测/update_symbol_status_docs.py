"""Refresh current status while retaining dated historical failures and manifests."""
from pathlib import Path
from datetime import datetime
import hashlib,json

P=Path(__file__).resolve().parent;S=P.parent;R=S.parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def write(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
MARK='<!-- symbols-and-quit-20261029 -->'
summary='''本轮E30–E34补证已完成：坡向、剖切、指向索引和剖切索引均保留真实天正对象，302项文字/保护/保存读回、80项独立几何、92项原生PDF及10项验收器回归通过；默认值和坐标输入环境均实测恢复。最新专用宿主原生QUIT正常退出0、未强制结束，耗时308.2秒；窗口严格验收49PASS、775采样，测试宿主及相关CAD的用户桌面SHOW/抢前台均0。属于本次最小夹具，不等于正式样式、自动选位或阶段1全覆盖。详见[专项报告](坡向剖切详图索引与正常退出验证报告_20261029.md)。未进入阶段2。'''

def insert_latest(path):
 text=path.read_text(encoding='utf-8-sig')
 if MARK not in text:
  head,tail=text.split('\n',1);text=head+'\n\n'+MARK+'\n'+summary+'\n'+tail
 return text

matrix=S/'全需求能力覆盖矩阵.md';text=insert_latest(matrix)
text=text.replace('本次文档更新只读复核既有原始文件并运行独立Python验收器，未操作CAD。','前次文档更新只读复核既有证据；本次另在专用非输入桌面完成新增原生试验，见E30–E34。')
replacements={
'| G02 ':'| G02 坡向与坡度 | T18 / 有限值、方向及持久化通过 | 真实TDbSymbArrow，2%文字与相反方向 | E31两箭头实际公开文字、WCS测点、锥形头部宽度及保存新库/PDF；默认值与宿主捕捉环境恢复 | 正式坡率/排向/区域来源及参考样式未验；2%仅能力值，不是工程默认 | 复用TArrow＋TObjEdit＋获准隔离Win32桥；区域算法后阶段 | 坡率、排向、区域标高阶段3/4，缺参待确认 |',
'| G03 ':'| G03 楼梯剖切 | T19 / 有限端点、方向、编号及持久化通过 | 真实TDbSymbSection，31/32及S1-01 | E32两端实测、正反剖切方向、实际编号图号、新库及原生PDF通过 | 穿楼梯自动选线和工程编号/参考样式未验 | 复用TSection公开面板流程；自动选线后阶段 | 特殊楼梯和方向规则阶段3 |',
'| G04 ':'| G04 详图索引/代表部位 | T20 / 两类真索引技术夹具通过，图库待资料 | 真实TDbSymbIndexPointer，详图号和目标图号 | E33/E34分别经TPOINTINDEX/TSECTINDEX创建，TObjEdit实际7/8、S1-01，节点/折点/圈位/剖视短线、W/P及默认恢复通过 | 未验证有效图库目标、分区域代表位置或避让；同RX类型由创建命令及实际剖切短线区分 | 复用已证命令＋获准后台控件桥，不用组合说明替索引 | 固定详图库、适用条件及有效图号阶段3；试验值不用于项目 |',
'| 天正标高/坡向业务值 /':'| 天正标高/坡向业务值 / T17–T18 | E29标高往返、E31坡向实际2%及相反方向、W/P均已通过 | 本组最小技术补证已完成；正式区域、来源、计算和样式按所属阶段，不再整体登记未证 |',
'| 剖切与真正详图索引 /':'| 剖切与真正详图索引 / T19–T20 | E32剖切及E33/E34两类真实索引的编号/图号/测点/方向及W/P已通过 | 本组最小技术补证已完成；穿梯选线、区域代表和有效图库目标属于后阶段 |',
'| 失败隔离/必要关系与重开编辑 /':'| 失败隔离/必要关系与重开编辑 / T38–T39 | E30正常QUIT、E28及本轮49项窗口检查、既有有限W通过 | 仍需失败候选整库丢弃/不发布及重开后新对象公开改值并恢复；保留内存孤儿FAIL，不新增方案修改同步 |',
}
lines=text.splitlines()
for i,line in enumerate(lines):
 for prefix,value in replacements.items():
  if line.startswith(prefix):lines[i]=value;break
text='\n'.join(lines)+'\n'
text=text.replace('## 阶段1剩余8组技术补证','## 阶段1技术补证清单（6组仍待，2组本轮补齐）')
if '| E30 正常QUIT' not in text:
 pos=text.index('**E0的接口来源须单列：**')
 table='''| E30 正常QUIT | [退出回执](runs/isolated-axis-20261029-154336/normal-quit-verification.json)及同目录49PASS窗口终核 | native QUIT事件＋真实进程exit0、无force、句柄关闭；308.2秒；775采样own/related SHOW及前台均0 | 不保证快速退出；旧70及300秒超时记录保留 |
| E31 坡向 | [真实报告](runs/quiet-slope-20261029-154346-195615/symbol-business-native.json) | 真实TDbSymbArrow两件、2%、相反方向、73业务/15几何/23PDF；W、恢复默认值/捕捉环境 | 明确试验值；非正式坡率/区域/样式 |
| E32 剖切 | [真实报告](runs/quiet-section-20261029-154435-453099/symbol-business-native.json) | 真实TDbSymbSection两件、31/32、S1-01两端、正反方向；75业务/15几何/23PDF | 穿梯自动选线未实现；非正式图号 |
| E33 指向索引 | [真实报告](runs/quiet-pointerindex-20261029-154402-285592/symbol-business-native.json) | 真实TDbSymbIndexPointer两件、7/8和S1-01；77业务/23几何/23PDF | 固定详图库与有效目标未绑定 |
| E34 剖切索引 | [真实报告](runs/quiet-sectionindex-20261029-154431-408405/symbol-business-native.json) | 同真实TDbSymbIndexPointer类型，由TSECTINDEX创建及实际剖切短线区分；77业务/27几何/23PDF | 区域代表选位/避让/参考完整样式仍未验 |

'''
 # Keep these rows inside their own table, not outside the earlier table.
 text=text[:pos]+'| 新证据 | 原始入口 | 本次通过 | 边界 |\n|---|---|---|---|\n'+table+text[pos:]
matrix.write_text(text,encoding='utf-8')

conclusion=S/'阶段1全需求验证结论.md';text=insert_latest(conclusion)
replace2={
'| 天正标高/坡向业务值／':'| 天正标高/坡向业务值／T17–T18 | E29标高往返＋E31实际2%相反坡向、W/P及默认恢复 | 最小技术补证已完成；正式来源/区域、坡率、计算及参考样式待所属阶段 |',
'| 剖切与真正详图索引／':'| 剖切与真正详图索引／T19–T20 | E32剖切、E33指向索引、E34剖切索引实际类型/值/方向/测点及W/P | 最小技术补证已完成；穿梯选线、区域代表和固定详图库待所属阶段 |',
'| 失败隔离/必要关系与重开编辑／':'| 失败隔离/必要关系与重开编辑／T38–T39 | E30正常QUIT、49项窗口检查及有限保存保护 | 仍需整库候选丢弃/不发布和重开后编辑/恢复；旧内存孤儿FAIL保留 |',
}
lines=text.splitlines()
for i,line in enumerate(lines):
 for prefix,value in replace2.items():
  if line.startswith(prefix):lines[i]=value;break
conclusion.write_text(('\n'.join(lines)+'\n').replace('## 剩余8组阶段1技术补证','## 阶段1技术补证清单（6组仍待，2组本轮补齐）'),encoding='utf-8')

for path,title in [(S/'执行记录.md','# 最新实证：2026-10-29 坡向/剖切/索引与正常QUIT'),(R/'AGENTS.md','## 最新实证：2026-10-29 坡向/剖切/真索引与正常QUIT')]:
 text=path.read_text(encoding='utf-8-sig')
 if MARK not in text:
  local=summary if path.parent==S else summary.replace('](坡向剖切详图索引与正常退出验证报告_20261029.md)','](阶段1/坡向剖切详图索引与正常退出验证报告_20261029.md)')
  text+='\n'+title+'\n\n'+MARK+'\n'+local+'\n\n冻结方案5BC和参考8C3未变。原路径方案本次观察70554DC4…，14:19保存，非本轮测试输入；只记录文件差异，不覆盖恢复、不推断方案建筑修改。全部能力值为夹具，不能当项目参数。10个专用会话已退出，精确暂存ARX/INI归档并清理；PID复用按创建时间/映像识别，不操作无关进程。\n'
  path.write_text(text,encoding='utf-8')

# Preserve the previous recorded manifest before refreshing its current paths.
mp=S/'本轮交付清单.json';m=read(mp);archive=P/'evidence/阶段1交付清单_符号补证更新前_20261029.json'
if not archive.exists():archive.write_bytes(mp.read_bytes())
m['recorded']=datetime.now().astimezone().isoformat();m['priorManifest']=dict(path=str(archive),SHA256=sha(archive))
m['observedCurrentOriginalSHA256']='70554DC491164990145FB46E0DD3F54F7B2E69A9739675EC7FAAC8B80B73EF37'
m['inputScope']='All current capability probes use unchanged frozen5BC. Current user working original7055 is observed only, with bytes/last-save separate; no architectural-change inference or overwrite.'
targets={str(S/'坡向剖切详图索引与正常退出验证报告_20261029.md'),str(S/'交付/坡向剖切索引与QUIT_交付清单_20261029.json')}
existing={e['path'] for e in m['artifacts']}
for path in targets-existing:m['artifacts'].append(dict(path=path))
for entry in m['artifacts']:
 path=Path(entry['path'])
 if path.is_file():entry['SHA256']=sha(path)
m['unitRegression']='Current symbol acceptance 10 tests passed; historical regression scopes retained in prior manifest'
m['temporaryModules']='Current symbol-round10 owned hosts exited; exact nonce/hash-bound S1Quiet ARX/INI archived and removed; PID reuse checked without process control'
m['latestRound']=dict(report=str(S/'坡向剖切详图索引与正常退出验证报告_20261029.md'),evidenceIndex=str(S/'全需求验证证据索引.json'),business=302,geometry=80,preview=92,hostAssertions=49,phase2Started=False)
write(mp,m)
print(json.dumps(dict(updated=[str(matrix),str(conclusion),str(mp)],remainingTechnicalGroups=6,phase2Started=False),ensure_ascii=False))
