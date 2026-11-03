"""Exercise the real BCP_BUILD command in the existing isolated host controller."""
from pathlib import Path
import sys,json,time,importlib.util,hashlib
R=Path(__file__).resolve().parents[2];v=sys.argv[1]
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
b=read(R/f'阶段2/evidence/native-host-binding-{v}.json');s=Path(b['session']);j=Path(b['job'])
deadline=time.monotonic()+120
while not (s/'quiet-worker-ready.json').exists():
 if time.monotonic()>deadline:raise TimeoutError('owned host not ready')
 time.sleep(.25)
ready=read(s/'quiet-worker-ready.json');assert ready['nonce']==b['nonce'] and ready['inputDesktop']!=ready['desktop']
spec=importlib.util.spec_from_file_location('queue',R/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
report=dict(version=v,steps=[],error=None)
try:
 report['steps'].append(q.dispatch(s,ready,'S1OPENPROBECONTROL',j,'native-open.done',60))
 (j/'phase2-product-async.only').write_text('BCPPRODUCTASYNC1\n','ascii')
 report['steps'].append(q.dispatch(s,ready,'S1PHASE2EXECUTE',j,'product-result.json',240))
 result=read(j/'product-result.json');report['result']=result;assert result['completed'] and result.get('error') is None,result
 outputs=result.get('outputs',[dict(path=result.get('output'),SHA256=result.get('outputSHA256'))])
 for output in outputs:assert hashlib.sha256(Path(output['path']).read_bytes()).hexdigest().upper()==output['SHA256']
 assert hashlib.sha256(Path(b['input']).read_bytes()).hexdigest().upper()==b['inputSHA256']
except BaseException as e:report['error']=repr(e);raise
finally:
 (R/f'插件/evidence/native-rules-{v}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8');print(json.dumps(report,ensure_ascii=False))
