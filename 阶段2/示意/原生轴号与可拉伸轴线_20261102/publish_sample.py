"""Deliver only the verified DWG sample after owned-host cleanup."""
from pathlib import Path
import json,hashlib,shutil,datetime,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');q=read(HERE/f'qa-{v}.json');visual=read(HERE/f'visual-{v}.json');closure=read(ROOT/f'阶段2/evidence/host-closure-{v}.json');unload=read(ROOT/f'插件/evidence/unload-product-{v}.json')
assert q['passed'] and visual['reviewed'] and visual['number21AndLetterPTrialsConfirmed'] and visual['finalNumbersRestoredAndSidesCorrect'] and visual['horizontalLeadsOutsideBuilding'] and visual['titleClearOfAxisCircles']
assert unload['unloaded'] and unload['commandsRemoved'] and closure['stagingRemoved'] and not closure['process']['ownProcessAlive'] and closure['session']==m['session']==unload['session'];assert all(x['snapshotUnchanged'] for x in closure['inputs']) and all(x['LOADCTRLS']==2 for x in closure['sharedRegistrations']);assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256'];assert sha(m['source'])==m['sourceSHA256']==sha('D:/修改.dwg') and sha(m['control'])==q['SHA256']
out=HERE/'方案施工图_二层_原生轴号与可拉伸轴线确认样张_v12.dwg';assert not out.exists();shutil.copyfile(m['control'],out);assert sha(out)==q['SHA256']
r=dict(contract='bcp.direct-native-axis-dwg-delivery.v1',recorded=datetime.datetime.now().astimezone().isoformat(),version=v,DWGVersion='v12',DWG=str(out),SHA256=sha(out),source='D:/修改.dwg',sourceSHA256=m['sourceSHA256'],sourcePreserved=True,nativeAxisLabelSets=12,nativeAxisModelRoots=True,axisBlockWrappersRemoved=12,axisCADGroupsDissolved=15,visibleAxisCircles=20,lowerAxis1Added=True,axisLinesStandalone=True,lineStretchAndRestoreTestMm=123,axisCommandsVerified=['TReAxisNo:1 to21 to1','TReAxisNo:A toP toA','TObjEdit:M side control'],axisLeadControls='Separate one-off ObjectARX modern grip pair; opaque appData passed unchanged; only inner-anchor changed',titleMove=m['titleMove'],allUserModifiedDimensionsOpeningsAreasAndLayer25Preserved=True,visibleWholeTCHDimensions=19,pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],COMUsed=False,ComputerUseUsed=False,normalQUITPassed=False,stage2Complete=False,qaSHA256=sha(HERE/f'qa-{v}.json'),manifestSHA256=sha(HERE/f'manifest-{v}.json'),visualSHA256=sha(HERE/f'visual-{v}.json'),closureSHA256=sha(ROOT/f'阶段2/evidence/host-closure-{v}.json'))
(HERE/f'delivery-{v}.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
(HERE/'样张说明.md').write_text('''# 二层原生轴号与可拉伸轴线确认样张 v12

本样张以用户D:/修改.dwg为唯一绘图内容基线，保留其尺寸删改、门号位置、PUB_DIM25号色、房间面积和建筑几何。只调整本次二层样张，不将二层坐标照搬一层。

12个普通裁剪块外套已解开，保留内部真实TDbAxisLabelSet，直接位于模型空间；不炸开天正轴号本体，也不用圆和文字拼轴号。取消15个额外轴网CAD Group，独立轴线可直接选择并通过夹点或STRETCH编辑。家具原块与面积文字组保持。

补底侧1号轴号及其绿色独立连接线，号圈中心、600间隔沿用原图；其余轴号位置不变。2、3号仅上侧显示，C、E仅左侧，其余已有双侧及新增1号双侧。横向引线收回原外侧裁剪端点，通过公开操作调整实际TCH引线，不再依赖普通裁剪块；最终读取号圈中心和引线内端验证。

实际用天正TReAxisNo将1改21再恢复1、A改P再恢复A，并对中间DWG原生渲染核字；用TObjEdit的M功能控制标注侧。代表轴线端点实际拉伸123毫米后恢复，保存重开核对。轴号仍为独立单站天正集合，未宣称已把整方向所有轴号合成一个连续重排集合，也未验证所有天正轴号命令。

图名8073整体向右1000、向下1200模型毫米避让底侧轴号，内容、字体和下划线保持；这是本次试排，不设为所有工程的固定偏移。原用户19条可见整体TCH尺寸及所有测点/字点保持，原右下第二道末120也按用户文件保留。

仅DWG样张，不修改插件源码、ARX或0.1.2安装包，不表示BCP_BUILD已实现新轴网规则。普通AutoCAD隔离桌面内，使用原生命令和用户本次要求的天正轴号命令，独立的一次性ObjectARX现代夹点助手调整8处引线内端（原样传回公开appData、不解码），原有SDK保存重开验证；产品代码未改。无COM、Computer Use或业务面板。最终仅交付DWG，测试21/P/123快照及内部PDF/PNG为验证证据，不作为成果。

原DK排位备注、旧水吧等密集标注仍属于前轮未全量验收范围，本轮不称整图零碰撞。用户原件未覆盖。测试宿主正常SDK卸载后精确清理自有进程，不算正常QUIT通过。
''',encoding='utf-8');print(json.dumps(dict(DWG=str(out),SHA256=sha(out)),ensure_ascii=False))
