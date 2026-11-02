"""Publish the face-endpoint/full-axis candidate after the saved-geometry gate."""
from pathlib import Path
import csv,hashlib,json,shutil,subprocess,sys
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def require(ok,message):
 if not ok:raise RuntimeError(message)
# Run before any output directory creation or artifact copying.
subprocess.run([sys.executable,str(S/'verify_wall_following_axes.py'),'--check-only'],check=True)
n=read(S/'evidence/native-run-v32.json');v=read(S/'evidence/native-preview-run-v32.json')
gate=read(S/'evidence/wall-following-axes-release-v32-final.json');closure=read(S/'evidence/host-closure-v32.json')
require(gate['passed'] and gate['DWGSHA256']==n['DWGSHA256'] and gate['PDFSHA256']==v['PDFSHA256'],'release receipt mismatch')
require(all(x['snapshotUnchanged'] for x in closure['inputs']) and not closure['process']['ownProcessAlive'] and closure['stagingRemoved'],'host/input closure incomplete')
require(all(x['LOADCTRLS']==2 and x['action']=='read only' for x in closure['sharedRegistrations']),'shared registration state differs')
for name,h in n['binding']['sourceFiles'].items():require(sha(S/'src'/name)==h,'source changed after native execution: '+name)
candidate=Path(n['session'])/'phase2-candidate'/Path(n['binding']['module']).name
require(sha(candidate)==n['binding']['moduleSHA256'],'executed candidate binary changed')
p=Path(n['output']);a=read(p/'native-annotations.json');analysis=read(p/'analysis.json')
out=S/'交付/轴线随墙体范围布置_20261031'
require(not out.exists(),'delivery already exists; never overwrite accepted artifacts')
out.mkdir(parents=True)
items={
 '方案施工图_轴线随墙体布置.dwg':p/'方案施工图_阶段2中间图.dwg',
 '轴线随墙体布置_A2.pdf':Path(v['PDF']),
 '施工图预览.png':S/'evidence/native-preview-v32-page.png',
 'analysis.json':p/'analysis.json','native-annotations.json':p/'native-annotations.json',
 'native-foundation.json':p/'native-foundation.json',
 '全四侧尺寸与轴线出图检查.json':S/'evidence/wall-following-axes-release-v32-final.json',
 '输入保护及宿主清理.json':S/'evidence/host-closure-v32.json',
 '旧版拒绝与规则测试.json':S/'evidence/wall-following-axes-red-v32.json',
 '待确认支撑墙范围核对.json':S/'evidence/wall-following-axes-gate-role-audit-v32.json',
 '待确认清单.csv':S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv',
 '验证报告.md':S/'轴线随墙体范围布置验证报告_20261031.md'}
for name,source in items.items():shutil.copyfile(source,out/name)
require(sha(out/'方案施工图_轴线随墙体布置.dwg')==gate['DWGSHA256'] and sha(out/'轴线随墙体布置_A2.pdf')==gate['PDFSHA256'],'published artifact hash mismatch')
names={'top':'上侧','bottom':'下侧','left':'左侧','right':'右侧'}
roles={'axis':'轴位','exterior-face':'外墙外边'}
with (out/'第二道尺寸及测点.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['侧别','段号','起点类型','起点坐标mm','终点类型','终点坐标mm','尺寸mm','天正尺寸句柄'])
 for side in ['top','bottom','left','right']:
  for i,d in enumerate([x for x in a['dimensions'] if x['row']=='axis' and x['side']==side],1):w.writerow([names[side],i,roles[d['fromRole']],d['axisFrom'],roles[d['toRole']],d['axisTo'],d['expected'],d['handle']])
ends={(x['vertical'],x['label']):x for x in a['axes']}
with (out/'完整逻辑轴线.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['轴号','竖向轴线','真实轴位mm','起点X','起点Y','终点X','终点Y','负侧轴号显示','正侧轴号显示','原生线句柄','参考图原型','图层','线型'])
 for x in a['fullAxisLines']:
  e=ends.get((x['vertical'],x['label']),{});w.writerow([x['label'],x['vertical'],x['station'],*x['start'][:2],*x['end'][:2],e.get('showNegative',False),e.get('showPositive',False),x['handle'],x['referenceHandle'],x['layer'],x['linetype']])
with (out/'轴线显示分段及逻辑组.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['轴号','竖向','真实轴位mm','原生Group句柄','显示段句柄','起点X','起点Y','终点X','终点Y'])
 for x in a['fullAxisLines']:
  for z in x['pieces']:w.writerow([x['label'],x['vertical'],x['station'],x['group'],z['handle'],*z['start'][:2],*z['end'][:2]])
(out/'阅读说明.txt').write_text('本轮按关联已确认墙段首末端布置轴线，14条显示范围缩短；20逻辑轴由20原生组管理21实际显示段，文字/标高80mm留白及压底保持。4段unknown共线墙仍待确认，不自动作为主要支撑。\n'+'第二道至外墙外边保持，20条逻辑轴按已确认关联墙端控制内部显示范围。右下外墙外边—B轴为2100mm，B轴位置不变。轴号仍逐端显隐，底3、右C、左右H轴圈不恢复。\n轴圈/尺寸保留真实天正对象，当前位于原生裁剪块内，天正直接交互编辑尚未验收；贯通轴线遵循参考图本身的AcDbLine/A-DOTE/DOTE原型。\n最内道2000仍为预留灰线，完整墙段/洞口尺寸道及最终施工图、打印样式、门廊总尺寸语义未完成，属于阶段2中间图。保留17项待补资料。\nA2 PDF按1:100原生出图。本轮隔离宿主精确停止与清理完成，未将此记为正常QUIT成功。\n',encoding='utf-8')
files=[dict(name=f.name,SHA256=sha(f),bytes=f.stat().st_size) for f in sorted(out.iterdir())]
manifest=dict(contract='bcp.wall-following-axes.delivery.v1',directory=str(out),files=files,nativeRun=str(S/'evidence/native-run-v32.json'),gate=str(S/'evidence/wall-following-axes-release-v32-final.json'),sourceCandidateARXSHA256=n['binding']['moduleSHA256'],constructionDrawingComplete=False)
(out/'交付指纹.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
receipt=S/'evidence/wall-following-axes-delivery-v32.json';require(not receipt.exists(),'delivery receipt already exists');receipt.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(directory=str(out),files=len(files),DWGSHA256=gate['DWGSHA256'],PDFSHA256=gate['PDFSHA256']),ensure_ascii=False))
