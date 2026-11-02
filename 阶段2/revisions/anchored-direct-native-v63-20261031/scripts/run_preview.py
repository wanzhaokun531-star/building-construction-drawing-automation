"""Hash-bind a saved intermediate, then open/plot it through native SDK commands."""
from dimension_display_policy import displayed_number
from pathlib import Path
import json,hashlib,sys,shutil,datetime,importlib.util,re
S=Path(__file__).resolve().parent;P=S.parent/'阶段1/原生接口探测'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
suffix=sys.argv[1] if len(sys.argv)>1 else '-v8';b=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');sourceSuffix=sys.argv[2] if len(sys.argv)>2 else suffix;native=read(S/f'evidence/native-run{sourceSuffix}.json');source=Path(native['output'])/'方案施工图_阶段2中间图.dwg';assert sha(source)==native['DWGSHA256'];assert ready['nonce']==b['nonce']
spec=importlib.util.spec_from_file_location('native_queue',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
job=S.parent/'阶段1/runs'/('stage2-preview-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir();shutil.copyfile(source,job/'control.dwg');a=read(Path(native['output'])/'native-annotations.json');header=f'S2DIMPREVIEW2 {len(a["dimensions"])} {read(Path(native["output"])/"native-foundation.json")["outputModelCount"]}\n'
if suffix in ['-v19','-v20','-v21','-v22','-v23','-v24','-v25','-v26','-v27','-v28','-v29','-v30','-v31','-v32','-v33','-v34','-v35','-v36','-v37','-v38','-v39','-v40','-v41','-v42','-v43','-v44','-v45','-v46','-v47','-v48','-v49','-v50','-v51','-v52','-v53','-v54','-v55','-v56','-v57','-v58','-v59','-v60','-v61','-v62','-v63','-v64','-v65']:header=f'S2DIMPREVIEW3 {len(a["dimensions"])} {read(Path(native["output"])/"native-foundation.json")["outputModelCount"]} 9400 11350 38000 40400 594 420\n'
(job/'annotation-index.tsv').write_text(header+''.join(f'{d["handle"]} {d["expected"]:.17g}\n' for d in a['dimensions']),encoding='ascii');assert sha(job/'control.dwg')==native['DWGSHA256']
receipt=dict(sourceNativeRun=str(S/f'evidence/native-run{sourceSuffix}.json'),sourceNativeRunSHA256=sha(S/f'evidence/native-run{sourceSuffix}.json'),contract='bcp.stage2.preview-run.v1',session=str(session),nonce=b['nonce'],processId=ready['processId'],job=str(job),source=str(source),sourceSHA256=sha(source),controlSHA256=sha(job/'control.dwg'),indexSHA256=sha(job/'annotation-index.tsv'),steps=[],error=None)
try:
 receipt['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',60));assert read(job/'native-open.json')['openStatus']==0
 if a.get('anchoredDirect'):
  audit=job/'native-mask-document-audit.json';(job/'phase2-mask-audit.only').write_text('owned-mask-audit-only\n',encoding='ascii');receipt['steps'].append(m.dispatch(session,ready,'S1PHASE2EXECUTE',job,str(audit),60));q=read(audit);assert q['masks']==4 and q['frame70']==q['WIPEOUTFRAME']==q['publicVisibleFrames']==0 and q['readOnly'] and q['error'] is None;receipt['maskDocumentAudit']=str(audit);receipt['maskDocumentAuditSHA256']=sha(audit)
 receipt['steps'].append(m.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',90));r=read(job/'stage2-preview-native.json');assert r['error'] is None,r['error'];assert r['initialModelCount']==r['finalModelCount']==read(Path(native['output'])/'native-foundation.json')['outputModelCount'] and r['originalBefore']==r['originalAfter'] and r['globalsBefore']==r['globalsAfter'] and r['layoutBefore']==r['layoutAfter']
 assert len(r['dimensions'])==len(a['dimensions'])
 for d in r['dimensions']:
  assert abs(d['expected']-d['measurement'])<1e-6
  assert d['override'] in ['', '<>'],d
  precision=next((x['precision'] for x in a.get('dimensionDetails',[]) if x['handle']==d['handle']),None)
  assert abs(displayed_number(d['formatted'])-d['expected'])<((.5*10**(-precision)+1e-6) if precision is not None else 1e-6),d
 page=r['page'];assert page['error'] is None and page['pipelineComplete'] is True and page['engineDestroyed'] is True and page['fileObserved'] is True and page['processPlotStateAfter']==0
 receipt['PDF']=page['outputPDF'];receipt['PDFSHA256']=sha(Path(receipt['PDF']));receipt['nativeReportSHA256']=sha(job/'stage2-preview-native.json');receipt['formattedDimensionsVerified']=len(a['dimensions'])
except BaseException as e:receipt['error']=repr(e);raise
finally:
 out=S/f'evidence/native-preview-run{suffix}.json';assert not out.exists();out.write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),error=receipt['error'],PDF=receipt.get('PDF')),ensure_ascii=False))
