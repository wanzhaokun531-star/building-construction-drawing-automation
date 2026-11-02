"""Read back the saved one-off sample through the unchanged native audit/plot bridge."""
from pathlib import Path
import json,sys,importlib.util,shutil,datetime,hashlib
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
m=read(HERE/'drawing-preview-manifest.json');readback=read(ROOT/f'阶段2/evidence/native-host-binding-{sys.argv[1]}.json') if len(sys.argv)>1 else m
session=Path(readback['session']);ready=read(session/'quiet-worker-ready.json')
spec=importlib.util.spec_from_file_location('native_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
summaries=[]
for f in m['floors']:
 floor=f['floor'];rows=Path(f['log']).read_text(encoding='cp936').splitlines();assert rows[-1]=='SUCCESS',rows[-3:]
 tabs=[x.split() for x in rows];count=int(next(x[1] for x in tabs if x[0]=='COUNT'))
 observed={x[1]:float(x[2])/1e6 for x in tabs if x[0]=='AREA'}
 assert set(observed)=={x['region'] for x in f['areas']}
 assert all(abs(observed[x['region']]-x['expectedM2'])<1e-6 for x in f['areas'])
 total=float(next(x[1] for x in tabs if x[0]=='TOTAL'))/1e6;assert abs(total-f['totalM2'])<1e-6
 assert sha(f['source'])==f['sourceSHA256'] and sha(f['control'])!=f['sourceSHA256']
 job=ROOT/'阶段1/runs'/('stage2-preview-demo-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+f'-floor{floor}');job.mkdir();shutil.copyfile(f['control'],job/'control.dwg')
 x0=72422 if floor==2 else 26000
 (job/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {count} {x0} 17500 {x0+31000} 44000 594 420\n',encoding='ascii')
 steps=[q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90)]
 (job/'phase2-mask-audit.only').write_text('Read-only sample audit\n',encoding='ascii')
 steps.append(q.dispatch(session,ready,'S1PHASE2EXECUTE',job,'native-mask-document-audit.json',60))
 audit=read(job/'product-axis-document-audit.json');assert audit['error'] is None
 steps.append(q.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120))
 p=read(job/'stage2-preview-native.json');assert p['error'] is None and p['originalBefore']==p['originalAfter'] and p['initialModelCount']==p['finalModelCount']==count
 current={x['handle']:x for x in p['originalBefore']};actual={x['handle']:x for x in audit['nativeAnnotations']}
 prior=read(ROOT/('插件/evidence/preview-dynamic-v118-floor'+str(floor)+'.json'));old=read(Path(prior['job'])/'stage2-preview-native.json');previous={x['handle']:x for x in old['originalBefore']}
 moves={x['handle']:x for x in f['moves']}
 for h,e in previous.items():
  assert h in current and current[h]['type']==e['type']
  if e['type'] in ('TDbWall','TDbOpening','TDbGroupOpening') or e['layer'] in ('A-DOTE','A-AXIS'):
   assert current[h]==e,('architecture/axes/dimensions changed',h)
 for h,move in moves.items():
  assert all(abs(a-b)<1e-6 for a,b in zip(actual[h]['position'][0],move['to'])),h
 for row in tabs:
  if row[0]=='AREA':
   handle=row[3];expected=next(x for x in f['areas'] if x['region']==row[1]);text=actual[handle]['text'];prefix='连通净' if expected['text'].startswith('连通净') else '净';unit=m.get('unitDisplay','㎡');assert text.startswith(prefix) and text.endswith(unit) and abs(float(text[len(prefix):-len(unit)])-round(expected['expectedM2'],2))<1e-8;assert all(abs(a-b)<1e-6 for a,b in zip(actual[handle]['position'][0][:2],expected['position'][:2]))
  elif row[0]=='LINE':
   line=next(x for x in f['lines'] if x['id']==row[1]);e=current[row[2]];assert e['type']=='AcDbLine' and e['layer']=='A-AXIS'
   assert all(abs(a-b)<1e-6 for key in ['start','end'] for a,b in zip(e['curve'][key],line[key]))
 target=HERE/('方案施工图_'+('一层' if floor==1 else '二层')+'_门号轴线面积确认样张'+m.get('outputSuffix','')+'.dwg');assert not target.exists();shutil.copyfile(f['control'],target)
 summary=dict(floor=floor,modelRoots=count,netAreasM2=observed,totalM2=total,doorMoves=len(moves),newContinuousLeaders=len(f['lines']),previewJob=str(job),PDF=p['page']['outputPDF'],DWG=str(target),DWGSHA256=sha(target),sourcePreserved=sha(f['source'])==f['sourceSHA256'],steps=steps)
 summaries.append(summary);print(json.dumps({k:summary[k] for k in ['floor','modelRoots','totalM2','DWG']},ensure_ascii=False),flush=True)
out=HERE/'native-drawing-preview-qa.json';assert not out.exists();out.write_text(json.dumps(dict(contract='bcp.one-off-preview-qa.v1',pluginCodeChanged=False,scope='Native AREA numerical readback and saved annotation/geometry verification, visual checks separate; not plugin feature implementation',floors=summaries),ensure_ascii=False,indent=2),encoding='utf-8')
