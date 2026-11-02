from pathlib import Path
import csv,hashlib,json,shutil
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
n=read(S/'evidence/native-run-v18.json');v=read(S/'evidence/native-preview-run-v18.json');g=read(S/'evidence/perimeter-spacing-trial-v18.json');c=read(S/'evidence/host-closure-v18.json')
assert v['error'] is None and g['nearCircleGap']==600 and g['outerCircleOffset']==4000
assert all(x['snapshotUnchanged'] for x in c['inputs']) and not c['process']['ownProcessAlive']
source=Path(n['output']);a=read(source/'native-annotations.json')
out=S/'交付/外围间距试排_20261030';out.mkdir(parents=True,exist_ok=False)
items={'方案施工图_外围间距试排.dwg':source/'方案施工图_阶段2中间图.dwg','外围间距试排_A3.pdf':Path(v['PDF']),'外围间距试排预览.png':S/'evidence/native-preview-v18-page.png','native-annotations.json':source/'native-annotations.json','native-foundation.json':source/'native-foundation.json','analysis.json':source/'analysis.json','间距实测核对.json':S/'evidence/perimeter-spacing-trial-v18.json','输入保护及宿主清理.json':S/'evidence/host-closure-v18.json','待确认清单.csv':S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv'}
for name,p in items.items():shutil.copyfile(p,out/name)
assert sha(out/'方案施工图_外围间距试排.dwg')==n['DWGSHA256'] and sha(out/'外围间距试排_A3.pdf')==v['PDFSHA256']
with (out/'轴号间距对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['轴号','侧别','本侧显示','圆心距墙mm','总尺寸至圆近边净距mm','圆最外缘距墙mm'])
 for r in g['ends']:w.writerow([r['label'],r['side'],r['visible'],r['centreOffset'],r['totalLineToNearCircle'],r['outerCircleOffset']])
with (out/'尺寸排布对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['侧别','用途','线距最凸出外墙mm','实测尺寸mm','句柄'])
 for d in a['dimensions']:w.writerow([d['side'],'轴间' if d['row']=='axis' else '主体包络试排',1800 if d['row']=='axis' else 2600,d['expected'],d['handle']])
note='''# 外围间距试排v18

用户已确认总尺寸线到轴圈靠建筑一侧圆边净距600毫米；对4000起止点要求先试一次。当前模板实测圆半径400毫米，因此：最内道预留1000、中间轴间道1800、最外道2600、圆近边3200、圆心3600、圆最外缘4000，均为模型毫米。相邻两道尺寸间距800。

本次4000按外墙至圆最外缘的总外伸试排，没有额外圈外尾线；不是一根可见直线笔画长4000。此解释是试排选择，尚待用户据图确定，不把它记作用户已明确的线段起止定义。

24段轴间尺寸按各侧保留轴位重建，数值和分段沿用v17，仅线位改至1800。外道新增4个真实天正主体外墙包络尺寸，横向17700、竖向18400，线位2600；用于本次排布样张，正式整栋总尺寸仍待完整门廊/平台边界语义验收。

1000位置的四条浅灰直线只是最内墙段/洞口道的预留辅助基线，尚未标墙段/洞口尺寸。不能将图中两道实尺寸加辅助线说成外围三道尺寸全部完成。1000预留线可与后续正式生成的该道尺寸替换。

20个真实TDbAxisLabelSet仍显示28端：8个模型空间根、12个在原生空间裁剪块内；左D显/左C隐，右C显，A左/B右保持。本次通过2020 SDK公开现代夹点沿轴向移动真实天正端部，不解码appData、不改圈径、不替换成普通轴号图元。单侧轴号外层仍为块，直接天正编辑未验收。

原生生成、保存重开及A3严格1:100 PDF已核对看图。原生核对40个轴圈的保存几何；另从保存DWG独立检查16个模型根轴圈及墙端锚点、56个尺寸测量端点、28条尺寸基线和4条灰色辅助线。600净距、3600圆心、4000圆最外缘均通过。纯规则61项回归通过；本轮作为试排，不宣称生产方案全量验收。

输出265模型根：原213保持句柄类型、20轴号本体/容器、28天正尺寸、4条辅助线。原93墙门窗公开几何和34文字包围盒保护核对；8个原尺寸只在输出副本隐藏。17项其他待补资料保留，Q1/Q6不设轴不再询问。

本轮只普通AutoCAD2020专用隔离桌面+ObjectARX C++，无COM、Computer Use、天正业务命令或面板；自有宿主用户桌面SHOW/前台0。宿主精确停止70，不算正常QUIT通过；6组快照保持，共用CP两项2不改，4个暂存ARX/INI精确清理。原始图不覆盖。完整三道尺寸、整栋边界语义、容器编辑、最终字体/线宽、正式图框及阶段2全量仍待完成。
''';(out/'阅读说明.md').write_text(note,encoding='utf-8')
proof=dict(contract='bcp.perimeter-spacing-trial-delivery.v1',nativeRun=str(S/'evidence/native-run-v18.json'),nativeRunSHA256=sha(S/'evidence/native-run-v18.json'),previewRun=str(S/'evidence/native-preview-run-v18.json'),previewRunSHA256=sha(S/'evidence/native-preview-run-v18.json'),trial=True,axisLine4000Interpretation='wall-to-outer-circle-edge;no-extra-tail;not-stroke-length',nearCircleGap=600,axisDimensions=24,bodyEnvelopeTotals=4,innerGuides=4,modelRoots=265,threeRowsComplete=False,directTchEditingValidated=False,wholeBuildingTotalSemanticAcceptance=False,normalQuitPassed=False,stage2Complete=False,files={p.name:sha(p) for p in out.iterdir() if p.is_file()});(out/'交付指纹与边界.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(out),trial=True),ensure_ascii=False))
