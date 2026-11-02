"""Publish actual verified sample DWGs, never the product package."""
from pathlib import Path
import json,hashlib,shutil,datetime,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');q=read(HERE/f'qa-{v}.json');vis=read(HERE/f'visual-{v}.json');c=read(ROOT/f'阶段2/evidence/host-closure-{v}.json');un=read(ROOT/f'插件/evidence/unload-product-{v}.json')
assert q['passed'] and vis['reviewed'] and vis['bathroomNameAreaClearOfFixturesAndOpeningNumbers'] and un['unloaded'] and un['commandsRemoved'] and c['stagingRemoved'] and not c['process']['ownProcessAlive']
assert c['session']==m['session']==un['session'] and all(x['snapshotUnchanged'] for x in c['inputs']) and all(x['LOADCTRLS']==2 for x in c['sharedRegistrations'])
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
dwgs=[]
for f in m['floors']:
 n=f['floor'];qa=next(x for x in q['floors'] if x['floor']==n);assert sha(f['control'])==qa['SHA256'] and sha(f['source'])==f['sourceSHA256'];floorName='一层' if n==1 else '二层';out=HERE/f'方案施工图_{floorName}_卫生间面积及尺寸图层确认样张_v11.dwg';assert not out.exists();shutil.copyfile(f['control'],out);assert sha(out)==qa['SHA256'];dwgs.append(dict(floor=n,DWG=str(out),SHA256=sha(out)))
result=dict(contract='bcp.bathroom-area-and-dimension-layer-delivery.v1',recorded=datetime.datetime.now().astimezone().isoformat(),version=v,DWGVersion='v11',DWGs=dwgs,bathroomCount=2,bathroomAreaM2=[f['mergedAreaM2'] for f in q['floors']],display='5.20m² once per public bathroom',functionalRoomAreaLabels=18,removedDuplicateAreaLabels=2,movedRoomNames=2,areaBelowName=True,openingNumbersAndAllOpeningGeometryUnchanged=True,dimensionLayer='PUB_DIM',layerColor=3,layerLinetype='Continuous',layerLineweight=-3,wholeDimensionsRelayered=36,wholeDimensionsRetained=52,dimensionValuesAndGripsUnchanged=True,buildingGrossAreaUnchanged=True,pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],COMUsed=False,ComputerUseUsed=False,TCHBusinessCommandUsed=False,normalQUITPassed=False,stage2Complete=False,qaSHA256=sha(HERE/f'qa-{v}.json'),manifestSHA256=sha(HERE/f'manifest-{v}.json'),visualSHA256=sha(HERE/f'visual-{v}.json'),closureSHA256=sha(ROOT/f'阶段2/evidence/host-closure-{v}.json'))
(HERE/f'delivery-{v}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
(HERE/'修改说明.md').write_text('''# 卫生间面积、房名避让及尺寸图层确认样张 v11

两层公卫各作为一个功能房间，内部隔门不再拆成两个面积标签。原1.78及3.20两处标签合为一处5.20m²，按天正上标平方米排法显示，面积置于公卫名称下方。房名及面积移到本卫生间的空白位置，门窗自身编号和位置保持。

面积由AutoCAD AREA按周边墙内净范围重新计算，非对显示值1.78+3.20的简单相加：周边净宽2920、净深1780毫米，面积5.1976平方米。内部120厚隔墙有与1780净深对应的完整1780宽洞口（824B/8218），地面连通，因此合并净域补回原来封洞算法隔断的120×1780=0.2136平方米门洞地面。若将来内部有实体隔墙，仍扣实际墙体占地，不能泛化成所有卫生间一律取矩形。

参考图原读取清单同时含PUB_DIM和A-PUB_DIM；用户明确选无A-前缀的PUB_DIM。36条可见第三道及内部整体天正尺寸链（一层17、二层19）改至PUB_DIM，3号色、Continuous、默认线宽。外侧第一、第二道及轴号保持。尺寸测点、数值、基线、字点和直接TDbDimension2类型保持，没有炸开或套块。

每层房间面积标签10变9，原房名内容、其他面积、建筑总面积189.78/153.49、建筑及门窗全部几何保持；一层二层原有尺寸排位保存更改也保留。面积为本次计算文字，未新增字段联动。

仅修改独立DWG确认样张，不修改插件源码、835B9E0F…ARX及0.1.2安装包。普通AutoCAD隔离桌面内用AREA/MOVE/CHPROP及原有ObjectARX读回，未用COM、Computer Use或天正业务命令；不操作用户CAD。内部PDF/PNG仅作核图，不向用户交付PDF。

DK0918编号按此前用户要求保留原侧、原位置，其附近柜体/墙线接触与二层楼梯FMD长号平台线接近仍保留为既有待调整项，本轮只解除房名和面积对编号的占位，不称整图美观全量验收。
''',encoding='utf-8')
print(json.dumps(dict(DWGs=dwgs),ensure_ascii=False))
