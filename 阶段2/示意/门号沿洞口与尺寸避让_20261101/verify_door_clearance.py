"""Independent saved-DWG checks for the one-off door/dimension review sample."""
from pathlib import Path
import json,hashlib,datetime,importlib.util,shutil,math,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def hit(a,b,g=60):return a[0]<b[2]+g-1e-6 and a[2]>b[0]-g+1e-6 and a[1]<b[3]+g-1e-6 and a[3]>b[1]-g+1e-6
version=sys.argv[1] if len(sys.argv)>1 else 'v128'
m=read(HERE/f'door-clearance-manifest-{version}.json');session=Path(m['session']);ready=read(session/'quiet-worker-ready.json');spec=importlib.util.spec_from_file_location('native_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
out=dict(contract='bcp.door-clearance-native-qa.v1',version=version,pluginCodeChanged=False,floors=[])
for f in m['floors']:
 assert sha(f['source'])==f['sourceSHA256'];assert f'SAVED|F{f["floor"]}' in Path(f['log']).read_text(encoding='cp936')
 rows=Path(f['reopenedLog']).read_text(encoding='cp936').splitlines();groups={};current=None
 for line in rows:
  r=line.split('|')
  if r[0]=='GROUP':current=r[1];groups[current]=[]
  elif r[0]=='MEMBER':groups[current].append(dict(handle=r[1],type=r[2]))
  elif r[0]=='COUNT':assert int(r[1])==f['expectedModelCount']
  elif r[0]=='FRAME':assert r[1]=='0'
 assert set(groups)=={x['handle'] for x in f['doors']}
 for h,items in groups.items():assert len(items)==2 and {x['type'] for x in items}=={'TEXT','INSERT'} and any(x['handle']==h and x['type']=='TEXT' for x in items)
 floor=f['floor'];job=ROOT/'阶段1/runs'/('stage2-preview-door-clearance-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+f'-floor{floor}');job.mkdir();shutil.copyfile(f['control'],job/'control.dwg');x0=72422 if floor==2 else 26000
 (job/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {f["expectedModelCount"]} {x0} 17500 {x0+31000} 44000 594 420\n',encoding='ascii')
 steps=[q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90)];(job/'phase2-mask-audit.only').write_text('Native opened DWG door/dimension sample\n',encoding='ascii');steps.append(q.dispatch(session,ready,'S1PHASE2EXECUTE',job,'native-mask-document-audit.json',60));maskAudit=read(job/'native-mask-document-audit.json');assert maskAudit['error'] is None and maskAudit['masks']==4;audit=read(job/'product-axis-document-audit.json');assert audit['error'] is None
 steps.append(q.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120));p=read(job/'stage2-preview-native.json');assert p['error'] is None and p['originalBefore']==p['originalAfter'] and p['initialModelCount']==p['finalModelCount']==f['expectedModelCount']
 old=read(f['sourcePreview']);before={x['handle']:x for x in old['originalBefore']};after={x['handle']:x for x in p['originalBefore']};actual={x['handle']:x for x in audit['nativeAnnotations']};moves={x['handle']:x['delta'] for x in f['doors']};moves.update({x['handle']:x['delta'] for x in f['annotationMoves']});changedDims={x['handle'] for x in f['dimensionOps']}
 for h,e in before.items():
  assert h in after and after[h]['type']==e['type'],('lost/changed type',h)
  if h in moves:
   delta=moves[h];a=after[h];assert a['layer']==e['layer'] and a['proxy']==e['proxy'];assert near(a['min'],[x+y for x,y in zip(e['min'],delta)]) and near(a['max'],[x+y for x,y in zip(e['max'],delta)]),('caption bounds',h)
   for key in ['grip','stretch']:
    assert len(a[key]['points'])==len(e[key]['points']) and all(near(z,[x+y for x,y in zip(v,delta)]) for z,v in zip(a[key]['points'],e[key]['points'])),('caption points',h)
  elif h not in changedDims:assert e==after[h],('unexpected original object change',h)
 oldAudit=read(Path(f['sourcePreview']).parent/'product-axis-document-audit.json');oldDims={x['handle']:x for x in oldAudit['nativeAnnotations'] if x['type']=='TDbDimension2' and x['visible']};witness={x['handle']:x for x in read(f['captionWitness'])['dimensionAudit']};fontBoxes=[];baselines=[]
 for h,d in oldDims.items():
  now=actual[h];n=(len(d['grips'])-6)//2;assert now['type']=='TDbDimension2' and now['visible'] and now['layer']==d['layer'];assert near(now['values'],d['values']) and all(near(x,y) for x,y in zip(now['grips'][2:n+3],d['grips'][2:n+3])),('measurement changed',h)
  assert len(now['grips'])==len(f['expectedGrips'][h]) and all(near(x,y) for x,y in zip(now['grips'],f['expectedGrips'][h])),('grip outcome differs',h,now['grips'],f['expectedGrips'][h])
  a,b=now['grips'][:2];baselines.append((h,[min(a[0],b[0])-1,min(a[1],b[1])-1,max(a[0],b[0])+1,max(a[1],b[1])+1]))
  for j,text in enumerate(witness[h]['texts']):
   k=n+3+j;delta=[x-y for x,y in zip(now['grips'][k],d['grips'][k])];box=text['fontBounds'];fontBoxes.append((h,j,[box[0]+delta[0],box[1]+delta[1],box[2]+delta[0],box[3]+delta[1]]))
 contacts=[]
 for c in f['doors']:
  e=actual[c['handle']];assert e['text']==c['text'] and abs(e['rotation']-c['rotation'])<1e-9 and near(e['position'][0],c['desiredPosition']);bbox=[*after[c['handle']]['min'][:2],*after[c['handle']]['max'][:2]];assert near(bbox,c['desiredBounds'])
  for h,j,box in fontBoxes:
   if hit(bbox,box):contacts.append((c['opening'],'dimension-text',h,j))
  for h,box in baselines:
   if hit(bbox,box):contacts.append((c['opening'],'dimension-baseline',h))
 assert not contacts,('door/dimension contacts',contacts)
 new=set(after)-set(before);refs={next(x['handle'] for x in groups[c['handle']] if x['type']=='INSERT') for c in f['doors']};assert new==refs
 for mask in f['masks']:
  h=next(x['handle'] for x in groups[mask['caption']] if x['type']=='INSERT');e=after[h];assert e['type']=='AcDbBlockReference' and not e['proxy'] and e['layer']=='A-WINDOW-TEXT';assert near(e['min'],mask['min']) and near(e['max'],mask['max']),('clearance background bounds',h,e['min'],e['max'],mask)
 summary=dict(floor=floor,source=f['source'],sourceSHA256=f['sourceSHA256'],control=f['control'],SHA256=sha(f['control']),roomsAndAreasUnchanged=True,doors=len(f['doors']),movedDoors=sum(any(abs(x)>1e-6 for x in d['delta']) for d in f['doors']),dimensions=len(oldDims),textMoves=sum(x['kind']=='text' for x in f['dimensionOps']),baselineMoves=sum(x['kind']=='baseline' for x in f['dimensionOps']),nodesAndValuesUnchanged=True,doorDimensionContacts=contacts,numberBackgrounds=len(refs),modelRoots=len(after),previewJob=str(job),PDF=p['page']['outputPDF'],steps=steps)
 out['floors'].append(summary);print(json.dumps({k:summary[k] for k in ['floor','doors','textMoves','baselineMoves','modelRoots','nodesAndValuesUnchanged']},ensure_ascii=False),flush=True)
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
if '--no-publish' not in sys.argv[2:]:
 for f in out['floors']:
  target=HERE/('方案施工图_'+('一层' if f['floor']==1 else '二层')+'_门编号与尺寸避让确认样张_v5.dwg');assert not target.exists();shutil.copyfile(f['control'],target);f['DWG']=str(target);f['DWGSHA256']=sha(target)
(HERE/f'door-clearance-qa-{version}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
