"""Publish checked DWGs only, after exact isolated-host cleanup."""
from pathlib import Path
import json,hashlib,shutil,datetime,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');q=read(HERE/f'qa-{v}.json');visual=read(HERE/f'visual-{v}.json');closure=read(ROOT/f'阶段2/evidence/host-closure-{v}.json');unload=read(ROOT/f'插件/evidence/unload-product-{v}.json')
assert q['passed'] and visual['reviewed'] and unload['unloaded'] and unload['commandsRemoved'] and closure['stagingRemoved'] and not closure['process']['ownProcessAlive']
assert closure['session']==m['session']==unload['session'] and all(x['snapshotUnchanged'] for x in closure['inputs']) and all(x['LOADCTRLS']==2 for x in closure['sharedRegistrations'])
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
dwgs=[]
for f in m['floors']:
 n=f['floor'];qa=next(z for z in q['floors'] if z['floor']==n);assert sha(f['control'])==qa['SHA256'] and sha(f['source'])==f['sourceSHA256'];name='一层' if n==1 else '二层';out=HERE/f'方案施工图_{name}_门窗分类编号确认样张_v10.dwg';assert not out.exists();shutil.copyfile(f['control'],out);assert sha(out)==qa['SHA256'];dwgs.append(dict(floor=n,DWG=str(out),SHA256=sha(out)))
result=dict(contract='bcp.opening-category-sample-delivery.v1',recorded=datetime.datetime.now().astimezone().isoformat(),version=v,DWGVersion='v10',DWGs=dwgs,numberedCounts=m['totals'],nativeInternalNumbers=40,changedNumbers=27,allNumberedOpeningsDirectTDbOpening=True,noOpeningInAnyCADGroup=True,wholeTCHDimensionChains=52,allSourcePhysicalGeometryDimensionsAreasAndNamesPreserved=True,floor2LatestSavedDimensionEditsPreserved=True,notesAdded=2,pendingPositions=['F1:81E4/DK0918','F2:81EC/DK0918'],pendingNumericEncoding=['F1:824B/1780','F2:8218/1780'],pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],COMUsed=False,ComputerUseUsed=False,TCHCommand='TChWinLab (public numbering only)',normalQUITPassed=False,stage2Complete=False,manifestSHA256=sha(HERE/f'manifest-{v}.json'),qaSHA256=sha(HERE/f'qa-{v}.json'),visualSHA256=sha(HERE/f'visual-{v}.json'),closureSHA256=sha(ROOT/f'阶段2/evidence/host-closure-{v}.json'))
result['visualPendingPositions']=visual['pendingPositions']
(HERE/f'delivery-{v}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
(HERE/'修改说明.md').write_text('''# 门窗分类编号确认样张 v10

本次只改独立DWG样张，未修改插件代码、ARX或0.1.2安装包。

采用用户新类别：室内门M、室内门洞DK、外立面防盗门编号类别FMD、外立面窗LC、外立面推拉门LM。卧室、厨房、卫生间入口优先用M，不因位于外墙改号。宽高数字后缀沿用已确认值；未改门窗实际构造、宽高、洞口类别或开启方式，FMD类别编号本身不证明防盗性能。

40处真实天正门窗内部编号已保存重开、SDK公开LIST逐项核对；27处变更、13处M保持。门窗直接位于模型空间，不套块、不加普通CAD组，仍由天正门窗自身显示编号。52条可见整体TCH尺寸链、面积及房名保持。

二层基于用户已保存的v9冻结副本，保留上侧尺寸链新增2240及另外两处尺寸排位调整；一层原样张未被覆盖。

用户明确两处DK0918保持原侧、先标待调整。因此这两处原编号位置保留，图下备注明确列为待调整，尚未消除其附近公卫文字、墙柜等密集排布。二层楼梯入口FMD0927仍接近/接触楼梯平台线，作为样张排位复核项，不称全部零碰撞。两处1780宽洞口的数字编码尚未确定，编号仍空白。旧水吧与其他既有密集标注问题不在本轮宣称全量美观验收。

仅交付DWG。内部原生PDF/PNG用于核图，不作为用户交付。无COM或Computer Use；TChWinLab为已获用户授权的公开天正门窗编号命令。宿主最终SDK卸载后精确停止自有进程及清理，不算正常QUIT验证通过。
''',encoding='utf-8')
print(json.dumps(dict(DWGs=dwgs),ensure_ascii=False))
