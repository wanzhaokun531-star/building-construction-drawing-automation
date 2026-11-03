"""Verify unsaved entities still block the real entry on an owned CAD copy."""
from pathlib import Path
import sys,json,hashlib,shutil,importlib.util,datetime
R=Path(__file__).resolve().parents[2]
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
v=sys.argv[1];b=read(R/f'阶段2/evidence/native-host-binding-{v}.json');s=Path(b['session']);ready=read(s/'quiet-worker-ready.json')
j=R/'阶段1/runs'/('stage2-document-negative-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));j.mkdir();shutil.copyfile(b['input'],j/'control.dwg')
ini=Path(b['sidecar']);before=ini.read_text('utf-16');replacement={'receiptDirectory':str(j),'testBehavior':'document-state-entity','testInput':str(j/'control.dwg'),'testOutput':str(j/'must-not-exist.dwg')}
ini.write_text('\n'.join(k+'='+replacement[k] if '=' in line and (k:=line.split('=',1)[0]) in replacement else line for line in before.splitlines())+'\n','utf-16')
spec=importlib.util.spec_from_file_location('q',R/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
report=dict(version=v,job=str(j),steps=[],passed=False)
try:
 report['steps'].append(q.dispatch(s,ready,'S1OPENPROBECONTROL',j,'native-open.done',60))
 (j/'phase2-product-async.only').write_text('BCPPRODUCTASYNC1\n','ascii')
 report['steps'].append(q.dispatch(s,ready,'S1PHASE2EXECUTE',j,'product-result.json',60))
 result=read(j/'product-result.json');probe=read(j/'document-state-probe.json')
 assert probe['after']&1 and not result['completed'] and result['error'].startswith('E_DOCUMENT_MODIFIED'),result
 assert not (j/'must-not-exist.dwg').exists() and sha(j/'control.dwg')==b['inputSHA256'] and sha(b['input'])==b['inputSHA256']
 report.update(result=result,probe=probe,passed=True)
finally:
 ini.write_text(before,'utf-16');assert sha(ini)==b['sidecarSHA256']
 (R/f'插件/evidence/document-negative-{v}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8')
 print(json.dumps(report,ensure_ascii=False))
