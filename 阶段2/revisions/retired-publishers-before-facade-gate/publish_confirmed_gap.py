from pathlib import Path
import csv,hashlib,json,shutil
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
n=read(S/'evidence/native-run-v22.json');v=read(S/'evidence/native-preview-run-v22.json');g=read(S/'evidence/confirmed-gap-layout-v22.json');c=read(S/'evidence/host-closure-v22.json')
assert v['error'] is None and g['witnessStart']==1000 and g['dimensionOffsets']==[2000,2800,3600]
assert all(x['snapshotUnchanged'] for x in c['inputs']) and not c['process']['ownProcessAlive']
source=Path(n['output']);a=read(source/'native-annotations.json')
out=S/'交付/尺寸线2000_界线起点1000_20261030';out.mkdir(parents=True,exist_ok=False)
items={'方案施工图_2000_1000调整版.dwg':source/'方案施工图_阶段2中间图.dwg','尺寸线2000_界线起点1000_A2.pdf':Path(v['PDF']),'尺寸线2000_界线起点1000预览.png':S/'evidence/native-preview-v22-page.png','native-annotations.json':source/'native-annotations.json','native-foundation.json':source/'native-foundation.json','analysis.json':source/'analysis.json','间距及实际测点核对.json':S/'evidence/confirmed-gap-layout-v22.json','输入保护及宿主清理.json':S/'evidence/host-closure-v22.json','待确认清单.csv':S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv'}
for name,p in items.items():shutil.copyfile(p,out/name)
assert sha(out/'方案施工图_2000_1000调整版.dwg')==n['DWGSHA256'] and sha(out/'尺寸线2000_界线起点1000_A2.pdf')==v['PDFSHA256']
with (out/'尺寸取点与显示起点对照.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['侧别','用途','尺寸线距墙mm','界线靠墙显示起点距墙mm','实际起测坐标','实际止测坐标','实际尺寸mm','真实天正对象句柄','原生裁剪容器内'])
 for d in a['dimensions']:w.writerow([d['side'],'轴间' if d['row']=='axis' else '主体包络样张',2800 if d['row']=='axis' else 3600,1000,d['axisFrom'],d['axisTo'],d['expected'],d['handle'],d['nativeNestedInGapBlock']])
note='''# 尺寸线2000、界线起点1000——当前试排v22

以各侧最凸出外墙为0，最内墙段/洞口道预留线位于2000，中间轴间道2800，外道3600，相邻800。尺寸界线只从距墙1000处开始显示，真实测量点仍在墙边/真实轴位，28个尺寸数值保持。

1000空白段内与尺寸界线重合的短轴线也只裁剪显示，不移动原生轴号的内端几何。总尺寸线至轴圈近侧净距600，当前R400，所以轴圈近边4200、中心4600、最外缘5000。可见整体范围从1000到5000仍为4000，沿用此前含轴圈的总显示跨度试排口径；它不是可见直线笔画长度4000，最终定义仍待用户据图确定，不冒称4000要求已最终验收。

本版24轴间尺寸、4主体外墙包络样张尺寸均为真实TDbDimension2，嵌套在28个原生裁剪容器内；20个真实TDbAxisLabelSet也全部在原生裁剪容器内，显示28端。外层选中对象为AcDbBlockReference；容器内直接天正编辑未验收。原生对象本体类型保留，不等于模型空间根类型保持。

最内2000处四条浅灰线只是墙段/洞口道预留基线，完整该道尺寸尚未生成。17700/18400外道仍为主体外墙包络样张，整栋门廊/平台边界语义未正式验收。三道完整尺寸、完整参考样式及阶段2全量仍未完成。

v22原生生成保存重开、实际28个关联裁剪字典、56个保存后尺寸测量端点、28个显示数值及基线、40轴圈和20轴号裁剪范围核对。A2严格1:100 PDF已渲染并查看；上/左文字裁切已修正。旧v19几何检查通过但图面文字被裁切，未交付；v20直接移动轴线起点带动圈，被保护拒绝，没有输出DWG。v21关闭原图名图层导致原文字范围保护失败，也拒绝输出；v22保留原文字图层后原213及127项公开几何/文字核对通过。上述失败原据均保留。

265模型根：原213、20轴号容器、28尺寸容器、4辅助线。原213句柄类型、93墙门窗公开几何和34文字包围盒核对；8原尺寸仅输出副本隐藏。扩大核对范围后，原图框/表格会进入窗口，故仅在输出副本关闭A-PUB_TITLE、A-PUB_TAB两个已识别图框/表格图层，并保存重开核实。原图名图层保留，通过打印窗口避开其图面范围。源图和冻结文件不改。正式图框尚未处理。

只普通AutoCAD2020专用隔离桌面+ObjectARX C++原生接口，无COM、Computer Use、天正业务命令/面板。原生裁剪字典与显示位置均用2020 SDK读写，不猜写天正私有字段。6组快照和共用CP登记保护、4暂存文件精确清理；自有宿主精确停止70，不算正常QUIT。17其他待确认项保持，Q1/Q6不再询问。
''';(out/'阅读说明.md').write_text(note,encoding='utf-8')
proof=dict(contract='bcp.confirmed-gap-delivery.v1',nativeRun=str(S/'evidence/native-run-v22.json'),nativeRunSHA256=sha(S/'evidence/native-run-v22.json'),previewRun=str(S/'evidence/native-preview-run-v22.json'),previewRunSHA256=sha(S/'evidence/native-preview-run-v22.json'),witnessStart=1000,dimensionPositions=[2000,2800,3600],axisCircleNearGap=600,axisCircleCentre=4600,axisCircleOuter=5000,axisVisibleSpanTrial=4000,axes=20,axisContainers=20,dimensions=28,dimensionContainers=28,innerGuides=4,modelRoots=265,directTchEditingValidated=False,threeRowsComplete=False,wholeBuildingTotalSemanticAcceptance=False,normalQuitPassed=False,stage2Complete=False,files={p.name:sha(p) for p in out.iterdir() if p.is_file()});(out/'交付指纹与边界.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(out),witnessStart=1000,dimensionPositions=[2000,2800,3600]),ensure_ascii=False))
