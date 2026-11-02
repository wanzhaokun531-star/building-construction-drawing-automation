"""One-off sample numbering contract. Does not modify the plugin."""
from pathlib import Path
import json,hashlib,importlib.util,re,collections
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
spec=importlib.util.spec_from_file_location('native_public_fields',ROOT/'阶段2/示意/门窗内部编号_20261102/public_fields.py');pf=importlib.util.module_from_spec(spec);spec.loader.exec_module(pf)
def inside(p,vertices):
 x,y=p;result=False
 for a,b in zip(vertices,vertices[1:]+vertices[:1]):
  if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:result=not result
 return result
previous=ROOT/'阶段2/示意/门窗编号取消额外成组_20261102';delivery=read(previous/'delivery-v148.json');qa=read(previous/'qa-v148.json');baseline=read(previous/'manifest-v148.json');areas=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-placement-final.json');generation=ROOT/'阶段1/runs/stage2-product-20261101-143628'
assert all(sha(ROOT/p)==h for p,h in baseline['sourceFiles'].items())
plan=dict(contract='bcp.opening-category-numbering-sample.v1',pluginCodeChanged=False,pluginBinarySHA256=baseline['pluginBinarySHA256'],sourceFiles=baseline['sourceFiles'],userRules={'M':'室内门，卧室/厨房/卫生间入口优先且不因位于外墙而改号','DK':'室内门洞','FMD':'其余外立面平开门按本次防盗门编号类别试排','LC':'外立面窗户','LM':'外立面推拉门'},roomPriorityConfirmed='User: 卧室、厨房、卫生间入口统一用 M',numericEncoding='Keep existing confirmed width/height digit suffix; unresolved 1780mm suffix remains pending',floors=[])
for df in delivery['DWGs']:
 n=df['floor'];q=next(x for x in qa['floors'] if x['floor']==n);source=Path(df['DWG']);previewJob=Path(q['previewJob']);fieldRead=ROOT/f'阶段2/示意/门窗内部编号_20261102/sdk-read-v148-floor{n}.json'
 if n==2:
  saved=read(HERE/'saved-v9-audit-v149.json');assert not saved['publicFieldChanges'] and set(saved['geometryChanged'])=={'8018','8460','8475','8478'};source=Path(saved['frozenCopy']);assert sha(source)==saved['sourceSHA256'];previewJob=Path(saved['nativePreviewJob']);fieldRead=ROOT/'阶段2/示意/门窗内部编号_20261102/sdk-read-v149-saved-v9-floor2.json'
 else:assert sha(source)==df['SHA256']
 preview=read(previewJob/'stage2-preview-native.json');roots={e['handle']:e for e in preview['originalBefore']};fields=pf.fields_in_job(read(fieldRead)['job']);area=next(f for f in areas['floors'] if f['floor']==n)['areas'];ap=next(p for p in generation.rglob('analysis.json') if ('_一层_' in str(p))==(n==1));analysis=read(ap);hosts={h:c['wall'] for c in analysis['localWallChains'] for h in c['openings']};roles={w['handle']:w['role'] for w in analysis['wallRecognition']}
 gpfile=next(p for p in generation.rglob('door-graphic-probe.json') if ('_一层_' in str(p))==(n==1));graphic={x['opening']:x for x in read(gpfile)};rows=[];counts=collections.Counter()
 for h,e in roots.items():
  if e['type']!='TDbOpening':continue
  f=fields[h]['fields'];old=f['门窗编号'];a,b=e['grip']['points'][1:3];vertical=abs(a[0]-b[0])<1e-6;center=[(a[0]+b[0])/2,(a[1]+b[1])/2];adj=[]
  for sign in [-1,1]:
   point=[center[0]+(sign*250 if vertical else 0),center[1]+(0 if vertical else sign*250)]
   adj += [dict(id=z['id'],name=z['nameText'],shared=[t['text'] for t in z.get('sharedOtherNames',[])]) for z in area if inside(point,z['netRegionVertices'])]
  # The unnamed 4.31m² auxiliary rooms visibly contain sanitary fixtures in
  # the frozen plan. This is a drawing-sample room-function determination.
  bathroom=any(z['id'] in ['F1-R04','F2-R02'] for z in adj)
  special=bathroom or any(z['name'] and any(t in z['name'] for t in ['卧室','厨房','卫生间','公卫']) for z in adj)
  exterior=roles[hosts[h]]=='exterior';kind=f['门窗类型'];ink=graphic.get(h,{}).get('entities',[]);arcs=sum(x['type']=='AcDbArc' for x in ink);lines=[x for x in ink if x['type']=='AcDbLine' and x['depth']==2]
  # Three graphic samples contain two overlapping parallel leaves and opposing
  # motion-arrow strokes (14 public line leaves), rather than just no swing arc.
  sliding=bool(ink and arcs==0 and len(lines)==14)
  if kind=='墙洞':assert not exterior;prefix='DK';reason='公开类型墙洞，内部隔墙'
  elif kind=='窗':assert exterior;prefix='LC';reason='公开类型窗，外墙宿主'
  elif kind=='门':
   if special:prefix='M';reason='卧室/厨房/卫生间入口优先，含外墙入口'
   elif not exterior:prefix='M';reason='室内墙宿主门，含室内推拉门'
   elif sliding:prefix='LM';reason='外墙宿主，双扇重叠平行门扇及相反方向箭头判图为推拉门'
   else:assert arcs>0;prefix='FMD';reason='其余外墙平开门，按本次外立面防盗门编号类别试排'
  else:raise AssertionError((h,kind))
  if old:
   match=re.fullmatch(r'[A-Z]+(\d+)',old);assert match;number=prefix+match[1];counts[prefix]+=1
  else:number=None;assert float(f['宽度'])==1780
  # User kept the two DK labels on the original side, explicitly pending layout.
  label=e['grip']['points'][-1] if old else None;target=label
  if h=='7F39':target=[label[0]+1100,label[1],label[2]]
  if h=='81C1':target=[label[0]+1700,label[1]+100,label[2]]
  rows.append(dict(opening=h,oldNumber=old,newNumber=number,prefix=prefix,reason=reason,observedKind=kind,width=float(f['宽度']),height=float(f['高度']),host=hosts[h],hostRole=roles[hosts[h]],adjacentRooms=adj,specialRoomPriority=special,slidingGraphic=sliding,graphicArcCount=arcs,graphicLeafLineCount=len(lines),labelGripBefore=label,labelTarget=target,changed=bool(number and number!=old)))
 plan['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),displayedSource=df['DWG'],sourcePreview=str(previewJob/'stage2-preview-native.json'),sourceAudit=str(previewJob/'product-axis-document-audit.json'),sourcePublicRead=str(fieldRead),expectedOriginalModelCount=q['modelRoots'],expectedModelCount=q['modelRoots']+1,noteText='待调整：DK0918编号位置保持原侧，待人工排布。',noteStyleSource='863B' if n==1 else '8630',notePosition=[39258.84567846499+(46422 if n==2 else 0),17900,0],wallAnalysis=str(ap),wallAnalysisSHA256=sha(ap),graphicEvidence=str(gpfile),graphicEvidenceSHA256=sha(gpfile),numberedCounts=dict(counts),openings=rows))
counts=collections.Counter()
for f in plan['floors']:counts.update(f['numberedCounts'])
assert dict(counts)=={'LC':19,'FMD':4,'M':13,'LM':2,'DK':2},dict(counts)
plan['totals']=dict(counts);plan['knownNumberedOpenings']=40;plan['pending1780Openings']=2;plan['changedNumbers']=sum(o['changed'] for f in plan['floors'] for o in f['openings']);(HERE/'number-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(counts=dict(counts),changed=plan['changedNumbers'],pending=2),ensure_ascii=False))
