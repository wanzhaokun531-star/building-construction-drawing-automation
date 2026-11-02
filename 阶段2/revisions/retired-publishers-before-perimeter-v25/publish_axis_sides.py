# Retired active entry point; original implementation is retained below for audit.
raise SystemExit("RETIRED_PUBLISHER: use publish_axis_facade_fix.py; four-side facade release gate is mandatory. Historical artifacts remain read-only.")
"""Package the verified native DWG and independently regenerated native PDF."""
from pathlib import Path
import json,csv,hashlib,shutil
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
n=read(S/'evidence/native-run-v13.json');v=read(S/'evidence/native-preview-run-v15.json');sides=read(S/'evidence/axis-side-result-v13.json');assert v['error'] is None and v['sourceNativeRunSHA256']==sha(S/'evidence/native-run-v13.json')
source=Path(n['output']);analysis=read(source/'analysis.json');annotations=read(source/'native-annotations.json');out=S/'交付/轴号分侧显隐与避让_20261030';out.mkdir(parents=True,exist_ok=False)
files={'方案施工图_阶段2轴号显隐版.dwg':source/'方案施工图_阶段2中间图.dwg','轴号分侧显隐与避让_A3.pdf':Path(v['PDF']),'轴号分侧显隐与避让预览.png':S/'evidence/native-preview-v15-page.png','analysis.json':source/'analysis.json','native-foundation.json':source/'native-foundation.json','native-annotations.json':source/'native-annotations.json','轴号显隐核对.json':S/'evidence/axis-side-result-v13.json','待确认清单.csv':S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv','已确认轴线排除.json':S/'inputs/user-axis-exclusions-v1.json'}
for name,p in files.items():shutil.copyfile(p,out/name)
assert sha(out/'方案施工图_阶段2轴号显隐版.dwg')==n['DWGSHA256'] and sha(out/'轴号分侧显隐与避让_A3.pdf')==v['PDFSHA256']
with (out/'逐轴显隐对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['轴号','方向','左/下侧显示','右/上侧显示','显示错排X毫米','显示错排Y毫米','本体位于裁剪块内','天正本体类型'])
 for a in annotations['axes']:w.writerow([a['label'],'竖向（下/上）' if a['vertical'] else '横向（左/右）','显示' if a['showNegative'] else '隐藏','显示' if a['showPositive'] else '隐藏',a['displayOffset'][0],a['displayOffset'][1],a['nativeNestedInClipBlock'],'TDbAxisLabelSet'])
note='''# 轴号按侧显隐及避让候选结果

按用户确认的主体外墙范围中线分左右/上下，已确认墙段跨中线保留两端，仅在一侧则保留该侧。A保留左侧、隐藏右侧；B保留右侧、隐藏左侧。20条轴线统一编号不变，40个原端部轴号保留29个、隐藏11个。隐藏同时作用于该端天正轴圈和专用短轴线。

左侧C/D用显示错排和折线连接处理，D显示位置向左800、向上800模型毫米；折线起点仍在原D轴定位。墙体及轴线取值不跟随圈移位。圈径和文字仍继承模板。本轮尺寸链保持，图中部分绿色短线为尺寸界线，未随轴号隐藏。

这是供图面检查的候选结构：20个真实天正TDbAxisLabelSet本体保留，其中9个仍在模型空间、11个位于AutoCAD原生空间裁剪块内，单侧显隐由裁剪实现。D的容器另含一条原生折线。选中单侧轴号时外层是块，直接使用天正轴号编辑工具的体验尚未验收，不能认为原对象根结构及编辑入口完全相同。

DWG原生保存重开、裁剪字典/变换/几何、原213对象句柄类型及93墙门窗公开几何/34文字包围盒核对通过；模型空间269对象，新增36个天正轴间尺寸保留。原8个方案尺寸仍在输出副本中隐藏。原生A3单页1:100 PDF已渲染查看，轴圈无相互重叠，轴尾不穿过其他可见轴圈；36个尺寸格式化数值复核通过。

当前只对这张样本完成此项效果验证。两端均需显示又同时需要独立错排的其他轴号情况尚未实现；三道尺寸、尺寸文字避让、正式图框和全部制图标准仍未完成，阶段2全量未验收。17项其他待确认资料继续保留。原始方案和冻结快照不覆盖。

本轮仅ObjectARX C++公开SDK，普通AutoCAD专用非输入桌面；无COM、Computer Use及天正业务命令或面板。
''';(out/'阅读说明.md').write_text(note,encoding='utf-8')
receipt=dict(contract='bcp.axis-side-delivery.v1',nativeRun=str(S/'evidence/native-run-v13.json'),nativeRunSHA256=sha(S/'evidence/native-run-v13.json'),previewRun=str(S/'evidence/native-preview-run-v15.json'),previewRunSHA256=sha(S/'evidence/native-preview-run-v15.json'),axes=20,visibleEnds=29,hiddenEnds=11,nativeClipContainers=11,directTchEditingValidated=False,rootStructureUnchanged=False,stage2Complete=False,visualReview=dict(reviewed=True,axisCirclesOverlap=False,originalTableFragmentInPDF=False,ACasePassed=True,BCasePassed=True,dimensionTextAvoidanceComplete=False),files={p.name:sha(p) for p in out.iterdir() if p.is_file()})
(out/'交付指纹与边界.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(out),visibleEnds=29,hiddenEnds=11),ensure_ascii=False))
