"""Actual native trial: baseline must move, every measured node/value stays fixed."""
from pathlib import Path
import json,sys,datetime,hashlib,shutil,importlib.util
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
v=sys.argv[1];t=read(HERE/f'stretch-trial-{v}.json');session=Path(t['session']);ready=read(session/'quiet-worker-ready.json');log=Path(t['log']).read_text(encoding='cp936');assert 'UNLOCKED_TARGET_ONLY' in log and 'SAVED_LAYERS_RESTORED' in log
prior=read(ROOT/'阶段2/示意/门号轴线与面积_20261101/area-symbol-qa-v125.json')['floors'][0];assert sha(t['input'])==t['inputSHA256']==prior['DWGSHA256']
job=ROOT/'阶段1/runs'/('stage2-preview-stretch-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir();shutil.copyfile(t['control'],job/'control.dwg');(job/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {prior["modelRoots"]} 26000 17500 57000 44000 594 420\n',encoding='ascii')
spec=importlib.util.spec_from_file_location('native_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
steps=[q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90)];(job/'phase2-mask-audit.only').write_text('Read-only baseline trial\n',encoding='ascii');steps.append(q.dispatch(session,ready,'S1PHASE2EXECUTE',job,'native-mask-document-audit.json',60));audit=read(job/'product-axis-document-audit.json');assert audit['error'] is None
steps.append(q.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120));p=read(job/'stage2-preview-native.json');assert p['error'] is None and p['originalBefore']==p['originalAfter'];old=read(Path(prior['previewJob'])/'stage2-preview-native.json');before={e['handle']:e for e in old['originalBefore']};after={e['handle']:e for e in p['originalBefore']};assert before.keys()==after.keys()
for h in before:
 if h!=t['handle']:assert before[h]==after[h],('other entity changed',h)
oldAudit=read(Path(prior['previewJob'])/'product-axis-document-audit.json');a=next(e for e in oldAudit['nativeAnnotations'] if e['handle']==t['handle']);z=next(e for e in audit['nativeAnnotations'] if e['handle']==t['handle']);n=(len(a['grips'])-6)//2;assert a['type']==z['type']=='TDbDimension2';assert a['values']==z['values'] and a['grips'][2:n+3]==z['grips'][2:n+3],('measurement changed',a,z)
for i in [0,1]:assert all(abs(z['grips'][i][j]-a['grips'][i][j]-t['delta'][j])<1e-6 for j in range(3)),('baseline not moved as a unit',a,z)
out=dict(contract='bcp.native-stretch-baseline-trial.v1',version=v,completed=True,previewJob=str(job),sourceSHA256=t['inputSHA256'],outputSHA256=sha(t['control']),target=t['handle'],baselineBefore=a['grips'][:2],baselineAfter=z['grips'][:2],nodesUnchanged=True,valuesUnchanged=True,otherObjectsUnchanged=True,steps=steps,route='AutoCAD STRETCH explicit crossing, non-target layers temporarily locked and restored; no COM/CUA/C++ changes')
(HERE/f'stretch-trial-qa-{v}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({k:out[k] for k in ['completed','target','baselineBefore','baselineAfter','nodesUnchanged','valuesUnchanged','otherObjectsUnchanged']},ensure_ascii=False))
