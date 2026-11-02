"""Internal visual QA only. User delivery remains DWG."""
import sys,json,shutil,datetime,importlib.util
from pathlib import Path
from chain_contract import S,read,sha
P=S.parent/'阶段1/原生接口探测';version=sys.argv[1];b=read(S/f'evidence/native-host-binding-{version}.json');session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');source=Path(b['job'])/'方案施工图_整体连续天正尺寸.dwg';assert source.exists() and ready['nonce']==b['nonce']
job=S.parent/'阶段1/runs'/('stage2-preview-whole-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir();shutil.copyfile(source,job/'control.dwg');(job/'annotation-index.tsv').write_text('S2DIMPREVIEW3 0 322 9400 11350 38000 40400 594 420\n',encoding='ascii')
spec=importlib.util.spec_from_file_location('native_queue',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
r=dict(contract='bcp.whole-chain-preview.v1',session=str(session),nonce=b['nonce'],processId=ready['processId'],job=str(job),source=str(source),sourceSHA256=sha(source),steps=[],error=None)
try:
    r['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90))
    r['steps'].append(m.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120))
    p=read(job/'stage2-preview-native.json');assert p['error'] is None,p['error'];assert p['originalBefore']==p['originalAfter'] and p['initialModelCount']==p['finalModelCount']==322
    assert p['globalsBefore']==p['globalsAfter'] and p['layoutBefore']==p['layoutAfter']
    r['PDF']=p['page']['outputPDF'];r['PDFSHA256']=sha(r['PDF']);r['nativeReportSHA256']=sha(job/'stage2-preview-native.json')
except BaseException as e:r['error']=repr(e);raise
finally:
    out=S/f'evidence/whole-chain-preview-{version}.json';assert not out.exists();out.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),error=r['error'],PDF=r.get('PDF')),ensure_ascii=False))
