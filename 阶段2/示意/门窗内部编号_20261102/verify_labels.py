"""Verify saved native labels, internal fields, original geometry and groups."""
from pathlib import Path
import json,hashlib,importlib.util,sys
from public_fields import read,fields_in_job,validate_number_targets
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');assignment=read(HERE/'manifest-v145.json');reads={n:read(HERE/f'sdk-read-{v}-floor{n}.json') for n in [1,2]};fields=validate_number_targets(assignment,reads)
area=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-qa-v136.json');session=Path(m['session']);ready=read(session/'quiet-worker-ready.json');q=load('q_native_labels',ROOT/'阶段1/原生接口探测/run_quiet_job.py');u=load('u_native_labels',ROOT/'阶段2/示意/房间面积随房名_20261101/verify_area_name_sample.py')
out=dict(contract='bcp.native-opening-number-final-qa.v1',version=v,pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],geometryPassed=False,actualStoredNumbersMatched=0,floors=[])
for f in m['floors']:
 n=f['floor'];assert f'SAVED|F{n}' in Path(f['log']).read_text(encoding='cp936');assert sha(f['source'])==f['sourceSHA256'] and sha(f['originalSource'])==f['originalSourceSHA256'];controlHash=sha(f['control']);job,p,a,steps=u.native_preview(f,session,ready,q);assert sha(f['control'])==controlHash
 before={x['handle']:x for x in read(f['originalSourcePreview'])['originalBefore']};after={x['handle']:x for x in p['originalBefore']};oldAnn={x['handle']:x for x in read(f['originalSourceAudit'])['nativeAnnotations']};nowAnn={x['handle']:x for x in a['nativeAnnotations']};assigned={x['handle']:x for x in read(f['sourcePreview'])['originalBefore']};removed={x['caption'] for x in f['ops']};changed={x['opening'] for x in f['ops']}
 assert set(before)-set(after)==removed and not set(after)-set(before)
 for h,e in before.items():
  if h not in removed|changed:assert after[h]==e,('unlisted root changed',n,h)
 for h,e in oldAnn.items():
  if h not in removed:assert nowAnn[h]==e,('unlisted dimension or ordinary text changed',n,h)
 for op in f['ops']:
  h=op['opening'];e=after[h];old=before[h];added=assigned[h];assert e['type']==old['type']=='TDbOpening' and not e['proxy'] and e['layer']==old['layer']
  gp=e['grip']['points'];original=old['grip']['points'];assert len(gp)==len(original)+1 and all(near(x,y) for x,y in zip(gp[:-1],original)) and near(gp[-1],op['target']),('opening core/number grip',n,h)
  sp=e['stretch']['points'];assert len(sp)==len(old['stretch']['points'])+1 and all(near(x,y) for x,y in zip(sp[:-1],old['stretch']['points'])) and near(sp[-1],op['target']),('opening stretch core/label point',h)
  assert e['curve']==old['curve'] and h not in nowAnn and op['caption'] not in after
 groups={};current=None
 for line in Path(f['groupsLog']).read_text(encoding='cp936').splitlines():
  x=line.split('|')
  if x[0]=='GROUP':current=x[1];assert current not in groups;groups[current]=[]
  elif x[0]=='MEMBER':assert current is not None;groups[current].append((x[1],x[2]))
 for op in f['ops']:
  if op['group']:
   members=groups[op['group']];assert len(members)==2 and (op['opening'],'TCH_OPENING') in members and any(t=='INSERT' and after[h]['type']=='AcDbBlockReference' for h,t in members),('door group not rebound',op,members)
 oldAreas=next(x for x in area['floors'] if x['floor']==n)
 for name,g in oldAreas['groups'].items():assert set(groups[g['name']])=={tuple(x) for x in g['members']}
 actualFields=next(x for x in fields if x['floor']==n);originalFields=fields_in_job(read(HERE/f'sdk-read-v145-floor{n}.json')['job'])
 assert set(actualFields['allFields'])==set(originalFields)
 for h,r in actualFields['allFields'].items():assert r['fields']==originalFields[h]['fields'],('public property changed during label placement',n,h)
 nativeDims=[x for x in a['nativeAnnotations'] if x['type']=='TDbDimension2' and x['visible']];assert len(nativeDims)==(25 if n==1 else 27)
 summary=dict(floor=n,control=f['control'],SHA256=controlHash,previewJob=str(job),PDF=p['page']['outputPDF'],steps=steps,actualStoredNumbersMatched=len(f['targets']),nativeOpeningLabelsAtSampleTargets=True,duplicateStandaloneTextRemoved=len(removed),rebuiltDoorLabelGroups=sum(bool(x['group']) for x in f['ops']),roomAreaGroupsPreserved=10,allRoomAndTotalAreaTextAndAllOtherNativeAnnotationsUnchanged=True,allOriginalOpeningGeometryGripsAndStretchPointsPreserved=True,allPublicOpeningPropertiesPreservedFromAssignment=True,wholeTCHDimensionChains=len(nativeDims),modelRoots=len(after),numberReadback=actualFields['targets'],allCurrentOpeningNumbers=actualFields['allCurrentOpeningNumbers'],groups=groups)
 out['floors'].append(summary);out['actualStoredNumbersMatched']+=len(f['targets']);print(json.dumps({k:summary[k] for k in ['floor','actualStoredNumbersMatched','modelRoots','wholeTCHDimensionChains']},ensure_ascii=False),flush=True)
assert out['actualStoredNumbersMatched']==40 and all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
out['geometryPassed']=True;out['fontScope']='Native embedded TCH numbering uses the opening native style (_TCH_WINDOW). Scratch EXPLODE evidence shows 3.5 paper-mm TDbText rather than former 300 model-mm standalone Text. Not asserted identical glyph metrics.'
out['scope']='DWG samples only; not a BCP_BUILD implementation or complete construction drawing acceptance.'
(HERE/f'qa-{v}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print('NATIVE_LABEL_QA_PASSED',flush=True)
