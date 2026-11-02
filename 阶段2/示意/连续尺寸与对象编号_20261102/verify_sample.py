"""Read saved samples with the existing ObjectARX probe. No plugin changes."""
from pathlib import Path
import json,hashlib,importlib.util,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def hit(a,b,g=0):return a[0]<b[2]+g-1e-6 and a[2]>b[0]-g+1e-6 and a[1]<b[3]+g-1e-6 and a[3]>b[1]-g+1e-6
v=sys.argv[1];mp=HERE/f'manifest-{v}.json';m=read(mp);session=Path(m['session']);ready=read(session/'quiet-worker-ready.json');q=load('queue_continuous',ROOT/'阶段1/原生接口探测/run_quiet_job.py');util=load('preview_continuous',ROOT/'阶段2/示意/房间面积随房名_20261101/verify_area_name_sample.py')
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
doorPlan=read(ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/door-clearance-manifest-v134.json');witnesses={f['floor']:read(f['captionWitness']) for f in doorPlan['floors']}
oldcontract=read(HERE/'manifest-v137.json');out=dict(contract='bcp.continuous-chain-native-qa.v1',version=v,manifest=str(mp),manifestSHA256=sha(mp),pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],geometryPassed=False,internalOpeningNumberWritten=False,internalOpeningNumberStatus='Unavailable in exposed native properties; no TCH numbering command authorized',floors=[])
for f in m['floors']:
 n=f['floor'];assert sha(f['source'])==f['sourceSHA256'] and sha(f['originalSource'])==f['originalSourceSHA256']
 if n==1:assert 'SAVED|F1' in Path(f['log']).read_text(encoding='cp936')
 controlHash=sha(f['control']);job,p,a,steps=util.native_preview(f,session,ready,q);assert sha(f['control'])==controlHash
 original=read(f['originalSourcePreview']);oldAudit=read(f['originalSourceAudit']);before={e['handle']:e for e in original['originalBefore']};after={e['handle']:e for e in p['originalBefore']};oldAnn={e['handle']:e for e in oldAudit['nativeAnnotations']};nowAnn={e['handle']:e for e in a['nativeAnnotations']}
 removed={'8498','8485'} if n==1 else set();changed={'847E'} if n==1 else set()
 assert set(before)-set(after)==removed and not set(after)-set(before)
 for h,e in before.items():
  if h not in removed|changed:assert after[h]==e,('unlisted root changed',n,h)
 for h,e in oldAnn.items():
  if h not in removed|changed:assert nowAnn[h]==e,('unlisted annotation changed',n,h)
 summary=dict(floor=n,source=f['originalSource'],sourceSHA256=f['originalSourceSHA256'],control=f['control'],SHA256=controlHash,previewJob=str(job),PDF=p['page']['outputPDF'],steps=steps,modelRoots=len(after),wholeTCHDimensionChains=sum(e['type']=='TDbDimension2' and e['visible'] for e in a['nativeAnnotations']),unlistedRootGeometryAndNativeAnnotationsUnchanged=True)
 if n==1:
  e=nowAnn['847E'];chain=next(x for x in oldcontract['floors'] if x['floor']==1);stations=chain['stations'];nodes=e['grips'][2:13];values=e['values'];assert len(values)==10 and len(e['grips'])==26
  assert e['type']=='TDbDimension2' and e['visible'] and e['layer']==oldAnn['847E']['layer'] and not after['847E']['proxy']
  assert all(near(x,y) for x,y in zip(e['grips'],f['expectedGrips']))
  assert all(near(p,[36231.70152820693,y,0]) for p,y in zip(nodes,stations))
  assert near(values,[b-a for a,b in zip(stations,stations[1:])]) and near(e['grips'][:1][0][:1],oldAnn['847E']['grips'][0][:1])
  assert abs(e['grips'][0][0]-e['grips'][1][0])<1e-6
  assert abs(sum(values)-(stations[-1]-stations[0]))<1e-6
  originalFonts={d['handle']:d for d in witnesses[1]['dimensionAudit']};gap=next(d for d in witnesses[2]['dimensionAudit'] if d['handle']=='848A')['texts'][0]
  references=[originalFonts['847E']['texts'][0],originalFonts['847E']['texts'][1],originalFonts['847E']['texts'][2],gap,originalFonts['8498']['texts'][0],originalFonts['8498']['texts'][1],originalFonts['8498']['texts'][2],gap,originalFonts['8485']['texts'][0],originalFonts['8485']['texts'][1]]
  boxes=[]
  for j,t in enumerate(references):
   point=e['grips'][13+j];b=t['fontBounds'];center=t['position'][0];relative=[b[0]-center[0],b[1]-center[1],b[2]-center[0],b[3]-center[1]]
   if j in [3,7]:relative=[-relative[3],relative[0],-relative[1],relative[2]]
   box=[relative[i]+point[i%2] for i in range(4)];boxes.append(box)
   assert abs(point[1]-(stations[j]+stations[j+1])/2)<1e-6,('numeric text left its measured interval',j)
   for h,r in after.items():
    if h=='847E':continue
    if h in nowAnn and nowAnn[h].get('visible') is False:continue
    if r['type'] in ['AcDbText','AcDbMText','TDbText','TDbWall','TDbOpening','TDbGroupOpening']:
     rb=[*r['min'][:2],*r['max'][:2]];assert not hit(box,rb,20),('new dimension glyph near protected text/wall/opening',j,h,box,rb)
  for i,b in enumerate(boxes):
   for j,z in enumerate(boxes[:i]):assert not hit(b,z,20),('dimension digits overlap',i,j)
  summary.update(joinedSources=['847E','8498','8485'],resultHandle='847E',singleDirectNativeTCHChain=True,segments=10,sharedNodes=9,measurementNodes=nodes,values=values,baseline=e['grips'][:2],glyphs=boxes,allNumericTextsRemainAtOwnIntervalMidpoint=True,newGlyphContactsWithTextWallsOpeningsAndEachOther=0,fontEvidence='Transferred actual source font extents of unchanged styles; 240 rotated from same-height native dimension witness',newEditingExercisePerformed=False)
 else:summary['drawingGeometryFullyUnchanged']=True
 out['floors'].append(summary);print(json.dumps({k:summary[k] for k in ['floor','modelRoots','wholeTCHDimensionChains']},ensure_ascii=False),flush=True)
out['geometryPassed']=True
out['note']='Dimensions only; internal door/window number remains pending native write capability or explicit TCH-number-command authorization. Native output is a DWG sample, not updated BCP_BUILD.'
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
(HERE/f'qa-{v}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print('QA_PASSED',flush=True)
