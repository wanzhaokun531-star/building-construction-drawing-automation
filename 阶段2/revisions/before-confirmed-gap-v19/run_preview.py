"""Hash-bind a saved intermediate, then open/plot it through native SDK commands."""
from dimension_display_policy import displayed_number
from pathlib import Path
import json,hashlib,sys,shutil,datetime,importlib.util,re
S=Path(__file__).resolve().parent;P=S.parent/'阶段1/原生接口探测'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
suffix=sys.argv[1] if len(sys.argv)>1 else '-v8';b=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');sourceSuffix=sys.argv[2] if len(sys.argv)>2 else suffix;native=read(S/f'evidence/native-run{sourceSuffix}.json');source=Path(native['output'])/'方案施工图_阶段2中间图.dwg';assert sha(source)==native['DWGSHA256'];assert ready['nonce']==b['nonce']
spec=importlib.util.spec_from_file_location('native_queue',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
job=S.parent/'阶段1/runs'/('stage2-preview-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir();shutil.copyfile(source,job/'control.dwg');a=read(Path(native['output'])/'native-annotations.json');(job/'annotation-index.tsv').write_text(f'S2DIMPREVIEW2 {len(a["dimensions"])} {read(Path(native["output"])/"native-foundation.json")["outputModelCount"]}\n'+''.join(f'{d["handle"]} {d["expected"]:.17g}\n' for d in a['dimensions']),encoding='ascii');assert sha(job/'control.dwg')==native['DWGSHA256']
receipt=dict(sourceNativeRun=str(S/f'evidence/native-run{sourceSuffix}.json'),sourceNativeRunSHA256=sha(S/f'evidence/native-run{sourceSuffix}.json'),contract='bcp.stage2.preview-run.v1',session=str(session),nonce=b['nonce'],processId=ready['processId'],job=str(job),source=str(source),sourceSHA256=sha(source),controlSHA256=sha(job/'control.dwg'),indexSHA256=sha(job/'annotation-index.tsv'),steps=[],error=None)
try:
 receipt['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',60));assert read(job/'native-open.json')['openStatus']==0
 receipt['steps'].append(m.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',90));r=read(job/'stage2-preview-native.json');assert r['error'] is None,r['error'];assert r['initialModelCount']==r['finalModelCount']==read(Path(native['output'])/'native-foundation.json')['outputModelCount'] and r['originalBefore']==r['originalAfter'] and r['globalsBefore']==r['globalsAfter'] and r['layoutBefore']==r['layoutAfter']
 assert len(r['dimensions'])==len(a['dimensions'])
 for d in r['dimensions']:
  assert abs(d['expected']-d['measurement'])<1e-6
  assert d['override'] in ['', '<>'],d
  assert abs(displayed_number(d['formatted'])-d['expected'])<1e-6,d
 page=r['page'];assert page['error'] is None and page['pipelineComplete'] is True and page['engineDestroyed'] is True and page['fileObserved'] is True and page['processPlotStateAfter']==0
 receipt['PDF']=page['outputPDF'];receipt['PDFSHA256']=sha(Path(receipt['PDF']));receipt['nativeReportSHA256']=sha(job/'stage2-preview-native.json');receipt['formattedDimensionsVerified']=len(a['dimensions'])
except BaseException as e:receipt['error']=repr(e);raise
finally:
 out=S/f'evidence/native-preview-run{suffix}.json';assert not out.exists();out.write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),error=receipt['error'],PDF=receipt.get('PDF')),ensure_ascii=False))
