"""Verify the saved style-only sample through unchanged native SDK readers."""
from pathlib import Path
import json,hashlib,datetime,importlib.util,shutil
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
m=read(HERE/'area-symbol-manifest-v125.json');session=Path(m['session']);ready=read(session/'quiet-worker-ready.json')
spec=importlib.util.spec_from_file_location('native_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
result=dict(contract='bcp.area-symbol-native-qa.v1',version='v125',pluginCodeChanged=False,typography=m['typography'],floors=[])
for f in m['floors']:
 assert sha(f['source'])==f['sourceSHA256'];rows=Path(f['readbackLog']).read_text(encoding='cp936').splitlines();groups={};name=None;count=None
 for row in rows:
  r=row.split('|')
  if r[0]=='GROUP':name=r[1];groups[name]=[]
  elif r[0]=='TEXT':groups[name].append(dict(handle=r[1],text=r[2],height=float(r[3]),position=[float(r[4]),float(r[5]),0],style=r[6]))
  elif r[0]=='COUNT':count=int(r[1])
 assert count==f['expectedModelCount'] and len(groups)==len(f['pairs'])
 for pair in f['pairs']:
  group=groups[pair['id']];assert len(group)==2;body=next(e for e in group if e['handle']==pair['bodyHandle']);sup=next(e for e in group if e['handle']!=pair['bodyHandle'])
  assert body['text']==pair['bodyText'] and sup['text']=='2'
  assert abs(body['height']-pair['height'])<1e-8 and abs(sup['height']/body['height']-.5)<1e-8 and body['style']==sup['style']
  assert abs((sup['position'][1]-body['position'][1])/body['height']-m['typography']['baselineRiseFactor'])<1e-8
  if pair['id'].endswith('TOTAL'):assert f'{pair["areaM2"]:.2f}m' in body['text']
  else:assert body['text']==f'{pair["areaM2"]:.2f}m' and not any(x in body['text'] for x in ['净','连通','面积'])
 floor=f['floor'];job=ROOT/'阶段1/runs'/('stage2-preview-area-symbol-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+f'-floor{floor}');job.mkdir();shutil.copyfile(f['control'],job/'control.dwg');x0=72422 if floor==2 else 26000
 (job/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {count} {x0} 17500 {x0+31000} 44000 594 420\n',encoding='ascii')
 steps=[q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90)];(job/'phase2-mask-audit.only').write_text('Read-only area symbol sample\n',encoding='ascii');steps.append(q.dispatch(session,ready,'S1PHASE2EXECUTE',job,'native-mask-document-audit.json',60));audit=read(job/'product-axis-document-audit.json');assert audit['error'] is None
 steps.append(q.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120));p=read(job/'stage2-preview-native.json');assert p['error'] is None and p['originalBefore']==p['originalAfter'] and p['initialModelCount']==p['finalModelCount']==count
 actual={e['handle']:e for e in p['originalBefore']};texts={e['handle']:e for e in audit['nativeAnnotations'] if e['type']=='AcDbText'};old=read(Path(f['oldPreview'])/'stage2-preview-native.json');changed={x['bodyHandle'] for x in f['pairs']}|{x['handle'] for x in f['notes']}
 for before in old['originalBefore']:
  assert before['handle'] in actual
  if before['handle'] not in changed:assert before==actual[before['handle']],('unexpected object change',before['handle'])
 for pair in f['pairs']:
  gs=groups[pair['id']]
  for e in gs:
   a=texts[e['handle']];geometry=actual[e['handle']]
   # A left/baseline AcDbText has no meaningful alignmentPoint; use its actual insertion/stretch point.
   assert a['text']==e['text'] and geometry['type']=='AcDbText' and len(geometry['stretch']['points'])==1
   assert all(abs(x-y)<1e-7 for x,y in zip(geometry['stretch']['points'][0],e['position']))
   # The Chinese big-font prefix of TOTAL has ink taller than the nominal ASCII text height.
   if pair['id'].endswith('TOTAL') and e['handle']==pair['bodyHandle']:
    original=next(x for x in old['originalBefore'] if x['handle']==e['handle']);expectedHeight=original['max'][1]-original['min'][1]
   else:expectedHeight=e['height']
   assert abs(geometry['max'][1]-geometry['min'][1]-expectedHeight)<1e-7
  if pair['mode']=='center':
   bounds=[actual[e['handle']] for e in gs];cx=(min(e['min'][0] for e in bounds)+max(e['max'][0] for e in bounds))/2;cy=(min(e['min'][1] for e in bounds)+max(e['max'][1] for e in bounds))/2
   assert abs(cx-pair['anchor'][0])<1e-6 and abs(cy-pair['anchor'][1])<1e-6,('center differs',pair['id'],cx,cy,pair['anchor'])
 for note in f['notes']:assert texts[note['handle']]['text']==note['text']
 assert all('m2' not in x['text'] for x in texts.values())
 target=HERE/('方案施工图_'+('一层' if floor==1 else '二层')+'_面积符号确认样张_v4.dwg');assert not target.exists();shutil.copyfile(f['control'],target)
 summary=dict(floor=floor,rooms=len(f['pairs'])-1,groupedLabels=len(groups),modelRoots=count,DWG=str(target),DWGSHA256=sha(target),previewJob=str(job),PDF=p['page']['outputPDF'],reopenedGroups=groups,steps=steps,sourcePreserved=sha(f['source'])==f['sourceSHA256']);result['floors'].append(summary);print(json.dumps({k:summary[k] for k in ['floor','rooms','modelRoots','DWG']},ensure_ascii=False),flush=True)
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
out=HERE/'area-symbol-qa-v125.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
