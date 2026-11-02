# Retired active entry point; original implementation is retained below for audit.
raise SystemExit("RETIRED_PUBLISHER: use publish_axis_facade_fix.py; four-side facade release gate is mandatory. Historical artifacts remain read-only.")
from pathlib import Path
import json,csv,hashlib,shutil
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
n=read(S/'evidence/native-run-v16.json');v=read(S/'evidence/native-preview-run-v16.json');assert v['error'] is None
source=Path(n['output']);a=read(source/'native-annotations.json');rows=a['axes'];c=next(x for x in rows if x['label']=='C' and not x['vertical']);d=next(x for x in rows if x['label']=='D' and not x['vertical']);assert not c['showNegative'] and c['showPositive'] and c['hiddenNegativeByOuterPriority'] and d['showNegative'] and d['displayOffset']==[0,0,0]
out=S/'交付/轴号靠外墙侧优先_20261030';out.mkdir(parents=True,exist_ok=False)
items={'方案施工图_外侧轴号优先版.dwg':source/'方案施工图_阶段2中间图.dwg','外侧轴号优先_A3.pdf':Path(v['PDF']),'外侧轴号优先预览.png':S/'evidence/native-preview-v16-page.png','analysis.json':source/'analysis.json','native-foundation.json':source/'native-foundation.json','native-annotations.json':source/'native-annotations.json','待确认清单.csv':S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv'}
for name,path in items.items():shutil.copyfile(path,out/name)
assert sha(out/'方案施工图_外侧轴号优先版.dwg')==n['DWGSHA256'] and sha(out/'外侧轴号优先_A3.pdf')==v['PDFSHA256']
with (out/'逐轴显隐对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['轴号','方向','左/下侧显示','右/上侧显示','因外侧优先隐藏左/下','因外侧优先隐藏右/上','左/下侧源墙最外端','右/上侧源墙最外端','显示偏移X','显示偏移Y','天正本体在裁剪块内'])
 for r in rows:w.writerow([r['label'],'竖向（下/上）' if r['vertical'] else '横向（左/右）',r['showNegative'],r['showPositive'],r['hiddenNegativeByOuterPriority'],r['hiddenPositiveByOuterPriority'],r['negativeWallReach'],r['positiveWallReach'],r['displayOffset'][0],r['displayOffset'][1],r['nativeNestedInClipBlock']])
note='''# 靠外墙侧墙轴号优先——最新候选

更新规则：先按主体中线判断各侧是否有对应墙段，再在同侧近轴轴圈冲突时，优先保留更靠该侧外墙的墙轴号，隐藏另一轴号和专用短轴线。横向比较墙段向左/右的实际延伸端，竖向比较向下/上的实际延伸端，不仅按“外墙”对象分类名称判断。

本图左侧D对应墙端X=16311.890259，C对应墙端X=20511.890259，D更靠左外墙。因此左侧留D、隐C，并取消D折线外移；右侧独立判断，右C仍保留。A左显右隐、B右显左隐不变。当前20条轴线的40端轴号显示28端、隐藏12端，其中11端因本侧无对应墙，1端因外侧优先。

轴线逻辑、统一编号和建筑几何保持，36个天正轴间尺寸及原有数值不合并、不重算。图中仍有尺寸界线，尺寸文字避让与四侧三道尺寸本轮未处理。

结构仍为候选：20个真实TDbAxisLabelSet中8个在模型空间、12个在原生空间裁剪块内；选中单侧轴号外层是块，直接天正编辑尚未验收。本版无轴号显示位移和新增折线。不能把内部本体类型保留说成模型空间根对象类型完全不变。

58项C++规则测试、原生保存重开、36尺寸格式化数字、A3严格1:100 PDF及图面检查通过。原213对象句柄类型、93墙门窗公开几何、34原文字包围盒核验；原8方案尺寸仍仅在输出副本隐藏。已保存的DWG、PDF与生成证据绑定指纹。

距离同等接近外墙而又近轴冲突时，不任意删一个，保留待进一步确认的边界。阶段2全量、通用编辑、三道尺寸、正式图框及完整字体/线宽标准仍未验收。17项其他待确认清单保留，Q1/Q6不单独设轴的确认保持。

本轮无COM、Computer Use、天正业务命令或面板，只在普通AutoCAD专用隔离桌面执行，不覆盖原始与冻结DWG，不改共用启动登记。
''';(out/'阅读说明.md').write_text(note,encoding='utf-8')
proof=dict(contract='bcp.outer-axis-priority-delivery.v1',nativeRun=str(S/'evidence/native-run-v16.json'),nativeRunSHA256=sha(S/'evidence/native-run-v16.json'),previewRun=str(S/'evidence/native-preview-run-v16.json'),previewRunSHA256=sha(S/'evidence/native-preview-run-v16.json'),axes=20,visibleEnds=28,hiddenByNoSupport=11,hiddenByOuterPriority=1,clippedNativeAxes=12,modelRootNativeAxes=8,modelRootStructureUnchanged=False,directTchEditingValidated=False,stage2Complete=False,ACasePassed=True,BCasePassed=True,leftDKept=True,leftCHidden=True,DJogCancelled=True,files={p.name:sha(p) for p in out.iterdir() if p.is_file()});(out/'交付指纹与边界.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(out),visibleEnds=28,left='D retained,C hidden'),ensure_ascii=False))
