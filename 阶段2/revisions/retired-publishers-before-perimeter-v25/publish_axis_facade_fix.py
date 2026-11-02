"""Publish only after a fresh, independent geometry/measurement release gate."""
from pathlib import Path
import csv,hashlib,json,shutil,subprocess,sys
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def require(ok,message):
 if not ok:raise RuntimeError(message)
# This must run BEFORE creating the delivery directory or copying any DWG/PDF.
subprocess.run([sys.executable,str(S/'verify_axis_facade.py'),'--check-only'],check=True)
n=read(S/'evidence/native-run-v23.json');v=read(S/'evidence/native-preview-run-v23.json');gate=read(S/'evidence/axis-facade-release-v23-final.json');closure=read(S/'evidence/host-closure-v23.json')
require(gate['passed'] and gate['DWGSHA256']==n['DWGSHA256'] and gate['PDFSHA256']==v['PDFSHA256'],'release receipt does not bind current output')
require(all(x['snapshotUnchanged'] for x in closure['inputs']) and not closure['process']['ownProcessAlive'] and closure['stagingRemoved'],'host or input protection closure incomplete')
p=Path(n['output']);a=read(p/'native-annotations.json');analysis=read(p/'analysis.json');old=read(Path(read(S/'evidence/native-run-v22.json')['output'])/'native-annotations.json')
oldAxes={(x['vertical'],x['label']):x for x in old['axes']};newAxes={(x['vertical'],x['label']):x for x in a['axes']}
out=S/'交付/轴号显隐根因修复_20261030';out.mkdir(parents=True,exist_ok=False)
items={'方案施工图_外轮廓轴号修正版.dwg':p/'方案施工图_阶段2中间图.dwg','轴号显隐修正版_A2.pdf':Path(v['PDF']),'轴号显隐修正版预览.png':S/'evidence/native-preview-v23-page.png','analysis.json':p/'analysis.json','native-annotations.json':p/'native-annotations.json','native-foundation.json':p/'native-foundation.json','全四侧出图阻断检查.json':S/'evidence/axis-facade-release-v23-final.json','旧版漏判根因.json':S/'evidence/axis-facade-v22-root-cause.json','输入保护及宿主清理.json':S/'evidence/host-closure-v23.json','旧轴网发布入口阻断.json':S/'evidence/legacy-publishers-retired-v23.json','待确认清单.csv':S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv'}
for name,source in items.items():shutil.copyfile(source,out/name)
require(sha(out/'方案施工图_外轮廓轴号修正版.dwg')==gate['DWGSHA256'] and sha(out/'轴号显隐修正版_A2.pdf')==gate['PDFSHA256'],'published DWG/PDF hash mismatch')
poly=analysis['wallOuterBodyContours'][0]
def face(station,vertical,negative):
 hits=[];i=0 if vertical else 1;j=1-i
 for p,q in zip(poly,poly[1:]+poly[:1]):
  if abs(q[i]-p[i])<1e-6:
   if abs(station-p[i])<1e-6:hits.extend([p[j],q[j]])
  elif min(p[i],q[i])-1e-6<=station<=max(p[i],q[i])+1e-6:hits.append(p[j]+max(0,min(1,(station-p[i])/(q[i]-p[i])))*(q[j]-p[j]))
 return min(hits) if negative else max(hits)
with (out/'全四侧轴号显隐对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['轴号','侧','旧版显示','新版显示','本侧墙端坐标mm','本侧外轮廓坐标mm','墙端距轮廓mm','判定原因','墙体句柄'])
 for logical in analysis['axisCandidates']:
  if not logical['confirmed']:continue
  key=(logical['vertical'],logical['confirmedLabel']);before=oldAxes[key];after=newAxes.get(key)
  for negative,flag,reach in [(True,'showNegative','negativeWallReach'),(False,'showPositive','positiveWallReach')]:
   edge=face(logical['coordinate'],logical['vertical'],negative);distance=abs(logical[reach]-edge);shown=bool(after and after[flag]);side=('下' if negative else '上') if logical['vertical'] else ('左' if negative else '右');reason='本侧外轮廓有真实墙端支撑' if shown else '墙端未接到本侧外轮廓' if distance>100+1e-6 else '保持已确认分侧规则或外侧优先'
   w.writerow([key[1],side,before[flag],shown,logical[reach],edge,distance,reason,'/'.join(logical['walls'])])
with (out/'四侧尺寸链对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['侧','用途','起测轴位坐标mm','止测轴位坐标mm','实际尺寸mm','天正尺寸句柄'])
 for d in a['dimensions']:w.writerow([d['side'],d['row'],d['axisFrom'],d['axisTo'],d['expected'],d['handle']])
note='''# 轴号显隐根因修复v23

本次不是按轴号3做单点隐藏。根因是旧版只按主体中线初分侧，且仅当轴圈相距小于800才比较靠外程度；底部3与4轴距1400，未进入近轴规则。3轴下侧已认可墙端在20541.263552，轴向投影的局部外墙外表面为16441.263552，相差4100，显然不是墙厚范围内的端部连接。

新规则在四侧统一增加轴向外轮廓支撑检查：沿轴与真实凹形主体外轮廓相交，分别取负/正方向最外表面；依据真实200墙半厚100及1e-6数值精度核对该侧墙端。这里100来自墙厚/2，不是新设工程容差。保留中线初分侧及外侧优先规则；投影未知时阻止生成，不用猜测补轴。顶部3有局部外轮廓支撑，保留；底部3隐藏。

全图同时纠正右C及左右H，共4个旧显示端。A左/B右、D左及其他有效端保持。全局逻辑轴20及编号不变；C/H两端均不需外显，不生成外部轴号集合；实际18个真实天正轴号集合、24个可见端，均在原生裁剪容器内。

尺寸链与最终可见轴位共用数据。底部改为1、2、4、6、8、9，尺寸1100、4200、3600、5000、3600；2—4之间按真实轴位量得4200，不保留指向隐藏3的尺寸界线。其余三侧同样重建，共20个轴间+4个主体包络样张尺寸，48个实际测量端点已保存后读回。测点仍在真实墙边/轴位，1000显示留空、2000内道预留、2800轴间、3600外道及600圈近边净距保持。

防复发有三层：70项纯规则测试（新增四向支撑、凹轮廓、厚度边界、无投影和绕序案例）；10项固定冻结方案回归（含底3隐/顶3显及4200）；独立Python出图门禁不调用C++谓词，再算所有显示端的几何支撑并核对保存后尺寸取点、隐藏轴引用和间距。门禁已对旧v22真实拒绝4处，对新v23通过。发布程序在创建交付目录、复制DWG/PDF之前强制重新执行门禁，并绑定实际DWG/PDF哈希，未通过不能发布该批。

普通AutoCAD2020原生ObjectARX生成保存重开259根：原213、18轴号容器、24尺寸容器、4辅助线。原213句柄类型、93墙门窗公开几何和34文字包围盒核对。A2严格1:100 PDF已渲染并看图：底3无圈/短轴线/尺寸界线，顶部3保持；所有数值和文字完整。独立只读代码复核无可证明新增缺陷，并实际复测70+10及门禁旧版拒绝/新版通过。

保留真实TCH本体不等于外层根类型保持；直接天正容器编辑仍未验收。最内2000灰线只是预留，完整墙段/洞口道、整栋门廊/平台总尺寸语义、完整参考样式及正式图框仍待完成。4000仍按1000到圆最外缘5000的含轴圈总显示跨度试排，未终核为直线笔画长度。17其他待确认资料及Q1/Q6原确认保持。

本轮无COM、Computer Use、天正业务命令/面板，只本任务专用隔离桌面和冻结5BC副本。源DWG与6组快照保持，共用CP_ARX/CP_NET为2不改，自有宿主凭证核准后精确停止70并清理4暂存ARX/INI；不把它算正常QUIT通过。阶段2全量仍未完成。
''';note+='\n\n6个旧轴网/DWG发布入口已在执行和导入时直接阻断，避免绕过新门禁；原源码归档、历史成果保留。无生成标注的原方案分析展示入口保持原样。\n';(out/'阅读说明.md').write_text(note,encoding='utf-8')
manifest=dict(contract='bcp.axis-facade-fix-delivery.v1',nativeRun=str(S/'evidence/native-run-v23.json'),releaseGate=str(S/'evidence/axis-facade-release-v23-final.json'),releaseGateSHA256=sha(S/'evidence/axis-facade-release-v23-final.json'),DWGSHA256=gate['DWGSHA256'],PDFSHA256=gate['PDFSHA256'],logicalAxes=20,nativeAxisSets=18,visibleEnds=24,axisDimensions=20,totalSamples=4,savedEndpoints=48,modelRoots=259,bottom3Hidden=True,top3Retained=True,releaseGateMandatoryBeforeCopy=True,threeRowsComplete=False,directTchEditingValidated=False,normalQuitPassed=False,stage2Complete=False,files={x.name:sha(x) for x in out.iterdir() if x.is_file()});(out/'交付指纹与边界.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(out),gatePassed=True,bottom3Hidden=True,top3Retained=True),ensure_ascii=False))
