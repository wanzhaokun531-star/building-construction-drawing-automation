# Retired active entry point; original implementation is retained below for audit.
raise SystemExit("RETIRED_PUBLISHER: use publish_axis_facade_fix.py; four-side facade release gate is mandatory. Historical artifacts remain read-only.")
"""Package actual SDK artifacts and a diagnostic index, without CAD business rules."""
from pathlib import Path
import json,hashlib,shutil,csv,html,base64,collections,sys
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
suffix=sys.argv[1] if len(sys.argv)>1 else '-v9';native=read(S/f'evidence/native-run{suffix}.json');preview=read(S/f'evidence/native-preview-run{suffix}.json');assert preview['error'] is None and preview['formattedDimensionsVerified']==36
folder=Path(native['output']);analysis=read(folder/'analysis.json');foundation=read(folder/'native-foundation.json');annotations=read(folder/'native-annotations.json');assert foundation['sourceDimensionsHidden']==annotations['existingDimensionsHidden']==8
out=S/('交付/轴网尺寸中间图_20261030_确认更新' if suffix=='-v9' else '交付/轴网尺寸中间图_20261030');out.mkdir(parents=True,exist_ok=False)
for name in ['analysis.json','native-foundation.json','native-annotations.json','endpoint-capture.json','endpoint-supplement.tsv','方案施工图_阶段2中间图.dwg']:shutil.copyfile(folder/name,out/name)
pdf=Path(preview['PDF']);assert sha(pdf)==preview['PDFSHA256'];shutil.copyfile(pdf,out/'阶段2中间图_A3_1比100.pdf');shutil.copyfile(S/f'evidence/native-preview{suffix}-page.png',out/'阶段2中间图预览.png')
assert sha(out/'方案施工图_阶段2中间图.dwg')==native['DWGSHA256']
manifest=read(S/'inputs/frozen-scheme-v2.json');records={x['handle']:x for x in manifest['records']};rooms=[]
for r in records.values():
 if r['type']=='TDbText':
  text=next((f['rawValue'] for f in r['sourceFields'] if f['name']=='文字内容'),'')
  if text in ['卧室','厨房','卫','卫生间','楼梯','客厅','餐厅','玄关','烤火房','储物间','庭院','门廊','走廊','花池']:rooms.append((r,text))
def centre(r):g=r['sourceGeometry'];return [(g['min'][i]+g['max'][i])/2 for i in [0,1]]
counts=collections.Counter();rows=[];stable={r['handle']:r['mark'] for r in read(S/'交付/轴网尺寸中间图_20261030/交付指纹与边界.json')['pending19']} if suffix=='-v9' else {}
for issue in analysis['issues']:
 wall=issue['code']=='WALL_ROLE_REQUIRED';prefix='Q' if wall else 'K';counts[prefix]+=1;mark=stable.get(issue['handle'],prefix+str(counts[prefix]));record=records[issue['handle']];x,y=centre(record);near=min(rooms,key=lambda r:sum((a-b)**2 for a,b in zip(centre(r[0]),[x,y])))[1]
 opening=next((a for a in analysis['openingNumbers'] if a['handle']==issue['handle']),None)
 known='200厚墙；用途未定' if wall else f"原生类型{opening['observedKind']}；洞宽{opening['width']:g}，洞高{opening['height']:g}毫米"
 ask='主要墙／门垛／局部短墙等用途；不按长度猜定' if wall else ('组合门类别，是否按门连窗FDM' if opening['observedKind']=='G' else '普通窗C／推拉窗LC等类别')
 rows.append(dict(mark=mark,handle=issue['handle'],code=issue['code'],x=x,y=y,near=near,known=known,ask=ask))
with (out/'待确认清单.csv').open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['定位标记','方案对象句柄','类别','X毫米','Y毫米','最近原房名标签（仅定位）','已知信息','需确认内容','用户确认值'])
 for r in rows:w.writerow([r['mark'],r['handle'],r['code'],r['x'],r['y'],r['near'],r['known'],r['ask'],''])
image=base64.b64encode((out/'阶段2中间图预览.png').read_bytes()).decode();svg=['<svg xmlns="http://www.w3.org/2000/svg" width="2400" height="1697" viewBox="0 0 2400 1697">',f'<image width="2400" height="1697" href="data:image/png;base64,{image}"/>','<g font-family="Arial" font-size="19" fill="#981b67" stroke="#981b67">']
for i,r in enumerate(rows):
 x=(74.5+(r['x']-10400)/100)*2400/420;y=1697-(11+(r['y']-12000)/100)*1697/297;dx=26 if i%2==0 else -55;dy=-20 if i%3 else 24
 svg.append(f'<circle cx="{x}" cy="{y}" r="8" fill="white" stroke-width="3"/><line x1="{x}" y1="{y}" x2="{x+dx}" y2="{y+dy}"/><rect x="{x+dx-3}" y="{y+dy-18}" width="46" height="23" fill="white" stroke="none"/><text x="{x+dx}" y="{y+dy}" stroke="none">{html.escape(r["mark"])}</text>')
svg+=['</g><rect x="8" y="8" width="650" height="34" fill="white"/><text x="16" y="32" font-family="Arial" font-size="22" fill="#981b67">PENDING INDEX ONLY: Q=wall use / K=opening category</text></svg>'];(out/'待确认位置索引.svg').write_text('\n'.join(svg),encoding='utf-8')
note='''# 阶段2轴网尺寸中间图

这是用于继续开发和核对的中间图，阶段2尚未全量验收。

已实际生成20个天正轴号集合（9竖向、11横向，共40个轴圈），编号1—9、A—L并跳过I；新增36个天正轴间尺寸。原213个对象逐句柄类型仍保留，原8个尺寸仅在输出副本中隐藏，避免新旧尺寸重复。93个墙/门窗的公开几何、34个原文字的包围盒均已核对并保存重开。新增标注保持天正类型。

组合门CD94通过公开捕捉端点、方案洞宽和唯一所属墙求得600／2400／600墙段链；全部63条局部墙链已可计算，但尚未绘制为四侧第一道尺寸。51段200墙有闭合网络依据，6段100内隔墙有完整室内包含依据；另6段200墙用途及13个门窗类别列入CSV。Q、K标记只在位置索引图中显示，尚未写入DWG。

PDF由AutoCAD2020原生AcPl打印，单页A3、严格1:100，36个格式化尺寸数值通过核对。已查看渲染，旧尺寸重复已消除；C/D近邻轴圈和100短尺寸仍重叠，待避让模块处理。当前只有轴间这一道新尺寸；三道尺寸、门廊计入整栋总尺寸、图框图签、门窗编号文字布置、最终图层/样式/字体/线宽及CTB一致性均未完成。不能作为正式施工图使用。

原房间名、图名、面积等沿用方案，未重新生成或重算。17个普通门编码已在analysis.json计算，未放置编号文字；13个未定门窗类别不猜定。还需确认的规则和资料以及开发未完成项分开记录于本轮执行报告。

本轮无COM、Computer Use、天正业务命令或面板。测试只用普通AutoCAD专用非输入桌面和冻结方案副本。宿主启动菜单资源和正常QUIT问题仍单列，不能把生成成功当成宿主生命周期通过。
'''
if suffix=='-v9':
  note=note.replace('另6段200墙用途及13个门窗类别','另4段200墙用途及13个门窗类别');note+='\n用户已确认原定位图Q1/CD9A和Q6/CFB9不单独设轴。排除规则已绑定冻结方案指纹，未推断细分构造用途；当前20个轴位均按现有规则确认。其余标记沿用原定位图，Q2—Q5及K1—K13仍待确认，共17项。\n';shutil.copyfile(folder/'axis-exclusions.tsv',out/'axis-exclusions.tsv');shutil.copyfile(S/'inputs/user-axis-exclusions-v1.json',out/'已确认轴线排除.json')
(out/'阅读说明.md').write_text(note,encoding='utf-8')
proof=dict(contract='bcp.stage2.grid-delivery.v1',nativeRun=str(S/f'evidence/native-run{suffix}.json'),nativeRunSHA256=sha(S/f'evidence/native-run{suffix}.json'),previewRun=str(S/f'evidence/native-preview-run{suffix}.json'),previewRunSHA256=sha(S/f'evidence/native-preview-run{suffix}.json'),stage2Complete=False,pending19=rows,visual=dict(reviewed=True,oldEightDimensionsAbsent=True,clippingObserved=False,nearbyCDLabelsOverlap=True,short100DimensionCollision=True,formalStyleApproved=False),files={f.name:sha(f) for f in out.iterdir() if f.is_file()});(out/'交付指纹与边界.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(out),pending=len(rows),newNativeAxes=20,newNativeDimensions=36),ensure_ascii=False))
