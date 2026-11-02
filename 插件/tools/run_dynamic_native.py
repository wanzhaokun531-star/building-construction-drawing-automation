from prepare_native import ROOT,S,P,read,sha
from pathlib import Path
import time,sys,json,importlib.util
v=sys.argv[1];b=read(S/f'evidence/native-host-binding-{v}.json');session=Path(b['session']);job=Path(b['job']);deadline=time.monotonic()+150
while not (session/'quiet-worker-ready.json').exists():
 if time.monotonic()>deadline:raise TimeoutError('worker not ready')
 time.sleep(.25)
ready=read(session/'quiet-worker-ready.json');assert ready['timerStarted'] and ready['error'] is None and ready['nonce']==b['nonce']
spec=importlib.util.spec_from_file_location('owned_queue',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
r=dict(contract='bcp.input-probe-native.v1',version=v,binding=b,steps=[],error=None)
try:
 r['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90));(job/'phase2-product-async.only').write_text('BCPPRODUCTASYNC1\n',encoding='ascii')
 r['steps'].append(m.dispatch(session,ready,'S1PHASE2EXECUTE',job,'product-result.json',300));result=read(job/'product-result.json');r['result']=result;assert result['completed'] and result['error'] is None;assert result['sourceSHA256']==b['inputSHA256']==sha(b['input']) and (all(Path(x['path']).is_file() and sha(x['path'])==x['SHA256'] for x in result['outputs']) if 'outputs' in result else Path(b['output']).exists());r['result']=result
except BaseException as e:r['error']=repr(e);raise
finally:
 out=ROOT/f'插件/evidence/dynamic-generation-{v}.json';assert not out.exists();out.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(error=r['error'],result=r.get('result',{})),ensure_ascii=False)[:1600])
