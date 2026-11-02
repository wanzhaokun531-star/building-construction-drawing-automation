from pathlib import Path
import csv, hashlib, json, shutil

S = Path(__file__).resolve().parent
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()

n = read(S/'evidence/native-run-v17.json')
v = read(S/'evidence/native-preview-run-v17.json')
g = read(S/'evidence/retained-side-dimensions-v17.json')
c = read(S/'evidence/host-closure-v17.json')
assert v['error'] is None and g['dimensionCount'] == 24
assert all(x['snapshotUnchanged'] for x in c['inputs']) and not c['process']['ownProcessAlive']
assert c['final']['workerUserDesktopShowEvents'] == 0 and c['final']['workerForegroundEvents'] == 0
source = Path(n['output']); a = read(source/'native-annotations.json')
out = S/'交付/轴号及尺寸外侧优先_20261030'
out.mkdir(parents=True, exist_ok=False)
files = {
 '方案施工图_轴号及尺寸调整版.dwg': source/'方案施工图_阶段2中间图.dwg',
 '轴号及尺寸调整_A3.pdf': Path(v['PDF']),
 '轴号及尺寸调整预览.png': S/'evidence/native-preview-v17-page.png',
 'analysis.json': source/'analysis.json',
 'native-foundation.json': source/'native-foundation.json',
 'native-annotations.json': source/'native-annotations.json',
 '保存后尺寸端点核对.json': S/'evidence/retained-side-dimensions-v17.json',
 '宿主关闭及输入保护核对.json': S/'evidence/host-closure-v17.json',
 '待确认清单.csv': S/'交付/轴网尺寸中间图_20261030_确认更新/待确认清单.csv',
}
for name, p in files.items(): shutil.copyfile(p, out/name)
assert sha(out/'方案施工图_轴号及尺寸调整版.dwg') == n['DWGSHA256']
assert sha(out/'轴号及尺寸调整_A3.pdf') == v['PDFSHA256']
side_names = dict(top='上', bottom='下', left='左', right='右')
with (out/'四侧轴间尺寸对照.csv').open('x', encoding='utf-8-sig', newline='') as f:
 w = csv.writer(f); w.writerow(['侧别', '起轴号', '止轴号', '起轴坐标mm', '止轴坐标mm', '实际轴间尺寸mm', '对象句柄'])
 for d in a['dimensions']:
  vertical = d['side'] in ('top', 'bottom')
  def label(value):
   matches = [x['label'] for x in a['axes'] if x['vertical'] == vertical and abs(x['station']-value) < 1e-6]
   assert len(matches) == 1
   return matches[0]
  w.writerow([side_names[d['side']], label(d['axisFrom']), label(d['axisTo']), d['axisFrom'], d['axisTo'], d['expected'], d['handle']])
with (out/'逐轴显隐对照.csv').open('x', encoding='utf-8-sig', newline='') as f:
 w = csv.writer(f); w.writerow(['轴号', '方向', '左或下侧显示', '右或上侧显示', '因外侧优先隐藏左或下', '因外侧优先隐藏右或上', '天正本体在裁剪块内'])
 for x in a['axes']:
  w.writerow([x['label'], '竖向' if x['vertical'] else '横向', x['showNegative'], x['showPositive'], x['hiddenNegativeByOuterPriority'], x['hiddenPositiveByOuterPriority'], x['nativeNestedInClipBlock']])
note = '''# 轴号及尺寸按外侧优先调整——当前中间图

轴号先按主体中线判断本侧有无对应墙段；同侧近轴冲突时，保留墙段更靠该侧外墙的轴号。本图左侧留D、隐C，右侧C保留；A左显右隐，B右显左隐。轴位与统一编号保持。

本次按用户最新要求，四侧轴间尺寸分别以该侧最终保留轴位连续取点、重新计算间距。旧版36段改为24段；不是只删100文字，也不保留指向被隐藏C轴的左侧尺寸界线。左侧A—D为4000，D—E为1800；右侧B—C为1900，C—F为3700。四侧完整数值与轴号见CSV。

该调整仅完成当前的轴间尺寸一道。墙段/门窗洞口连续第一道、含门廊伸出范围的整栋外墙外边总尺寸第三道、正式图框和完整字体/打印线宽标准仍待完成；墙面净尺寸及总尺寸定义不因显隐变更。

61项C++规则测试通过；普通AutoCAD2020原生ObjectARX生成、保存重开通过。新增24个真实TDbDimension2，已独立读取保存后48个实际测量端点，核对其对应轴位；24个格式化显示数值及A3严格1:100 PDF已核对并看图。原213对象句柄/类型、93墙门窗公开几何、34原文字包围盒保护核对；原8方案尺寸只在输出副本隐藏。

轴号仍为容器候选：20个真实TDbAxisLabelSet中8个是模型空间根，12个在原生空间裁剪块内，共显示28端；外层块的天正直接编辑尚未验收。不能把内部TCH类型保留说成根结构完全不变。本版不再有D折线外移。

本轮无COM、Computer Use、天正业务命令或面板。只使用专用隔离桌面；测试宿主用户桌面SHOW/抢前台均0，其他CAD事件单独留作归属复核。自有宿主精确停止70，不算正常QUIT通过。6组快照保持，4个临时模块/INI精确清理，共用CP登记保持2。

17项其他待确认资料沿用原编号，Q1/Q6不单独设轴的确认保持。此图是阶段2中间成果，不是完整施工图或可安装的一键成品。旧v13/v15/v16交付保留作历史，以本目录为当前轴号与轴间尺寸结果。
'''
(out/'阅读说明.md').write_text(note, encoding='utf-8')
proof = dict(contract='bcp.side-axis-dimension-delivery.v1', nativeRun=str(S/'evidence/native-run-v17.json'), nativeRunSHA256=sha(S/'evidence/native-run-v17.json'), previewRun=str(S/'evidence/native-preview-run-v17.json'), previewRunSHA256=sha(S/'evidence/native-preview-run-v17.json'), axes=20, visibleEnds=28, dimensionCount=24, savedModelCount=257, measuredEndpointsVerified=48, modelRootNativeAxes=8, clippedNativeAxes=12, directTchEditingValidated=False, stage2Complete=False, normalQuitPassed=False, files={p.name:sha(p) for p in out.iterdir() if p.is_file()})
(out/'交付指纹与边界.json').write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(dict(output=str(out), dimensions=24, endpoints=48), ensure_ascii=False))
