"""Build editable annotation prototypes and source associations for the approved project."""
from pathlib import Path
import json,hashlib,importlib.util,shutil,math
ROOT=Path(__file__).resolve().parents[2];ARCHIVE=ROOT.parent/'项目清理归档/建筑施工图自动化_20261102-181100';OUT=ROOT/'插件/resources/approved-layout';OUT.mkdir(parents=True,exist_ok=True)
def resolved(p):
 p=Path(p);return p if p.exists() else ARCHIVE/p.relative_to(ROOT)
def read(p):return json.loads(resolved(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def point(p):return ' '.join(format(float(x),'.17g') for x in p)
def quoted(s):return json.dumps(str(s),ensure_ascii=False)
q=read(ROOT/'阶段2/示意/卫生间合并面积与房名避让_20261102/qa-v154.json');bath=read(ROOT/'阶段2/示意/卫生间合并面积与房名避让_20261102/manifest-v154.json');area=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-placement-final.json');sourcePreview=read(Path(read(ROOT/'阶段2/示意/原生轴号与可拉伸轴线_20261102/capture-v165-input-original.json')['job'])/'stage2-preview-native.json');original={e['handle']:e for e in sourcePreview['originalBefore']};spec=importlib.util.spec_from_file_location('public_fields',ROOT/'阶段2/示意/门窗内部编号_20261102/public_fields.py');pf=importlib.util.module_from_spec(spec);spec.loader.exec_module(pf);summary=[]
for n in [1,2]:
 f=q['floors'][n-1];latest=resolved(f['control']) if n==1 else ROOT/'阶段2/示意/原生轴号与可拉伸轴线_20261102/方案施工图_二层_原生轴号与可拉伸轴线确认样张_v12.dwg';native=read(Path(f['previewJob'])/'stage2-preview-native.json') if n==1 else read(ROOT/'docs/归档前关键证据_v164/最终样张原生几何.json');roots={e['handle']:e for e in native['originalBefore']};shared=set(original)&set(roots)
 replaceTypes={'TDbDimension2','TDbText','TDbDrawingName','AcDbText','AcDbMText'};keep={h for h in shared if roots[h]['type'] not in replaceTypes};clone=set(roots)-keep
 # Source architecture is retained; only annotation roots and confirmed original note/title copies are imported.
 assert len([h for h in keep if roots[h]['type']=='TDbWall'])==34
 donor=OUT/f'floor-{n}.dwg';shutil.copyfile(latest,donor);rows=[f'BCPAPPROVED1 {n} {sha(donor)}',f'KEEP {len(keep)}']+['K '+h+' '+roots[h]['type'] for h in sorted(keep)]+[f'CLONE {len(clone)}']+['C '+h+' '+roots[h]['type'] for h in sorted(clone)]
 sdk=read(ROOT/f'阶段2/示意/门窗内部编号_20261102/sdk-read-v154-floor{n}.json');fields=pf.fields_in_job(resolved(sdk['job']));numbers=[]
 for h in sorted(keep):
  e=roots[h]
  if e['type']!='TDbOpening':continue
  label=fields[h]['fields']['门窗编号'];g=e['grip']['points'];core=original[h]['grip']['points'];assert len(g)==len(core)+(1 if label else 0)
  assert all(math.dist(a,b)<1e-5 for a,b in zip(g,core));numbers.append(dict(handle=h,number=label,core=core,target=g[-1] if label else [0,0,0]))
 rows.append(f'NUMBERS {len(numbers)}')
 for x in numbers:rows.append('N '+x['handle']+' '+quoted(x['number'])+' '+str(len(x['core']))+' '+point(x['target'])+' '+' '.join(point(p) for p in x['core']))
 groups=f['groups'];rows.append('GROUPS 10')
 for name,members in groups.items():
  if name.startswith('BCP_AREA_'):rows.append('G '+name+' '+str(len(members))+' '+' '.join(h for h,t in members))
 areaOriginal=read(ROOT/f'阶段2/示意/门号轴线与面积_20261101/room-areas-floor-{n}.json');rectangles={}
 for line in resolved(areaOriginal['cache']).read_text('utf-8').splitlines():
  words=line.split()
  if not words or words[0]!='W':continue
  x0,y0,x1,y1,left,right=map(float,words[2:8]);length=math.hypot(x1-x0,y1-y0);nx=-(y1-y0)/length;ny=(x1-x0)/length;corners=[(x+nx*d,y+ny*d) for x,y in [(x0,y0),(x1,y1)] for d in [left,-right]];rectangles[words[1]]=[min(x for x,y in corners),min(y for x,y in corners),max(x for x,y in corners),max(y for x,y in corners)]
 def refs(poly):
  out=[]
  for vertex in poly:
   for axis,value in enumerate(vertex[:2]):
    candidates=[(abs(value-v),h,i) for h,box in rectangles.items() for i,v in enumerate(box) if i%2==axis];d,h,i=min(candidates);assert d<1e-4,(n,vertex,axis,d);out.append(h+' '+str(i))
  return out
 areas=[]
 for record in area['floors'][n-1]['areas']:
  if record['id'].endswith('R09'):continue
  poly=bath['floors'][n-1]['boundary'] if record['id'].endswith('R10') else record['netRegionVertices'];body=record['members'][0];areas.append((body,1,poly))
 total=area['floors'][n-1]['totalAreaMembersUnchanged'][0]
 for poly in areaOriginal['exteriorContours']:areas.append((total,1,poly))
 rows.append(f'AREAS {len(areas)}')
 for h,k,poly in areas:rows.append('A '+h+' '+str(k)+' '+str(len(poly))+' '+' '.join(refs(poly)))
 # Porch geometry is a separately confirmed platform, not staircase projection.
 rows.append(f'PORCH {total} '+('4600 1380' if n==1 else '0 0'))
 if n==1:
  audit=read(Path(f['previewJob'])/'product-axis-document-audit.json');axes=[]
  for x in audit['axes']:
   vertical='_V_' in x['name'];centres=[c['centre'][0] for c in x['circles']];visible=[]
   for i,c in enumerate(centres):
    inside=all(x['min'][j]-1e-5<=c[j]<=x['max'][j]+1e-5 for j in [0,1]);visible.append(not inside if x['inverted'] else inside)
   assert any(visible);side=0 if all(visible) else -1 if visible[0] else 1
   lo=x['min'][1 if vertical else 0] if x['inverted'] else x['max'][1 if vertical else 0] if side<0 else x['min'][1 if vertical else 0];hi=x['max'][1 if vertical else 0]
   axes.append((x,vertical,centres,side,lo,hi))
  rows.append(f'AXES {len(axes)}')
  for x,v,c,side,lo,hi in axes:rows.append('X '+x['container']+' '+str(int(v))+' '+str(side)+' '+point(c[0])+' '+point(c[1])+' '+point([lo,hi]))
 else:rows.append('AXES 0')
 rows.append('END');contract=OUT/f'floor-{n}.tsv';contract.write_text('\n'.join(rows)+'\n',encoding='utf-8');summary.append(dict(floor=n,template=str(donor.relative_to(ROOT)),templateSHA256=sha(donor),contractSHA256=sha(contract),keep=len(keep),clone=len(clone),sourceWalls=34,numbered=sum(bool(x['number']) for x in numbers),allOpenings=len(numbers),areas=9,areaPolygons=len(areas),axisWrappers=0 if n==2 else len(axes),expectedSourceSampleRoots=len(roots)))
(OUT/'manifest.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(summary,ensure_ascii=True))
