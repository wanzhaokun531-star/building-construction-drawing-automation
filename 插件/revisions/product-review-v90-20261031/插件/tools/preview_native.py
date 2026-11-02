"""Internal QA only. No PDF is included in the user delivery."""
from prepare_native import ROOT,S,P,read,sha
from pathlib import Path
import sys,json,datetime,shutil,importlib.util
version=sys.argv[1];b=read(S/f'evidence/native-host-binding-{version}.json');session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');run=read(ROOT/f'插件/evidence/native-product-{version}.json');result=run['result'];source=Path(result['output']);assert run['error'] is None and sha(source)==result['outputSHA256']
job=ROOT/'阶段1/runs'/('stage2-preview-product-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir();shutil.copyfile(source,job/'control.dwg');(job/'annotation-index.tsv').write_text('S2DIMPREVIEW3 0 308 9400 11350 38000 40400 594 420\n',encoding='ascii')
spec=importlib.util.spec_from_file_location('native_queue',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
r=dict(contract='bcp.product-opened-preview.v1',session=str(session),nonce=b['nonce'],processId=ready['processId'],job=str(job),source=str(source),sourceSHA256=sha(source),steps=[],error=None)
try:
 r['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90));(job/'phase2-mask-audit.only').write_text('owned mask audit\n',encoding='ascii')
 r['steps'].append(m.dispatch(session,ready,'S1PHASE2EXECUTE',job,'native-mask-document-audit.json',60));axes=read(job/'product-axis-document-audit.json');assert axes['error'] is None and axes['carriers']==18 and axes['logicalGroups']==20 and axes['axisLines']==22;audit=read(job/'native-mask-document-audit.json');assert audit['error'] is None and audit['masks']==4 and audit['frame70']==audit['WIPEOUTFRAME']==audit['publicVisibleFrames']==0
 r['steps'].append(m.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120));p=read(job/'stage2-preview-native.json');assert p['error'] is None and p['originalBefore']==p['originalAfter'] and p['initialModelCount']==p['finalModelCount']==308
 assert p['globalsBefore']==p['globalsAfter'] and p['layoutBefore']==p['layoutAfter'];r.update(PDF=p['page']['outputPDF'],PDFSHA256=sha(p['page']['outputPDF']),nativeReportSHA256=sha(job/'stage2-preview-native.json'),controlSHA256=sha(job/'control.dwg'))
 assert r['controlSHA256']==r['sourceSHA256']
except BaseException as e:r['error']=repr(e);raise
finally:
 out=ROOT/f'插件/evidence/preview-product-{version}.json';assert not out.exists();out.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),error=r['error'],PDF=r.get('PDF')),ensure_ascii=False))
