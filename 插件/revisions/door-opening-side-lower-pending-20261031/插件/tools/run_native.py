from prepare_native import ROOT,S,P,read,sha
from pathlib import Path
import sys,time,json,importlib.util
version=sys.argv[1];b=read(S/f'evidence/native-host-binding-{version}.json');session=Path(b['session']);job=Path(b['job']);deadline=time.monotonic()+150
while not (session/'quiet-worker-ready.json').exists():
 if time.monotonic()>deadline:raise TimeoutError('owned product worker not ready')
 time.sleep(.25)
ready=read(session/'quiet-worker-ready.json');assert ready['nonce']==b['nonce'] and ready['inputDesktop']!=ready['desktop']
assert ready.get('timerStarted') and not ready.get('error'), 'isolated worker rejected desktop: '+str(ready.get('error'))
spec=importlib.util.spec_from_file_location('owned_queue',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
r=dict(contract='bcp.product-native-integration.v1',session=str(session),job=str(job),binding=b,steps=[],error=None)
try:
 r['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90))
 (job/'phase2-product-recovery.only').write_text('owned native recovery failure test\n',encoding='ascii')
 r['steps'].append(m.dispatch(session,ready,'S1PHASE2EXECUTE',job,'product-recovery-test.json',60));recovery=read(job/'product-recovery-test.json');r['recovery']=recovery;assert recovery['realDiagnosticWriteFailure'] and recovery['jobCleared'] and not recovery['retainedJobLock'],recovery
 (job/'phase2-product-recovery.only').unlink()
 (job/'phase2-product-async.only').write_text('BCPPRODUCTASYNC1\n',encoding='ascii')
 r['steps'].append(m.dispatch(session,ready,'S1PHASE2EXECUTE',job,'product-result.json',240))
 result=read(job/'product-result.json');assert result.get('completed') and result['error'] is None,result
 assert result['sourceMemoryObserved'] and result['sourceUnchanged'] and result['intermediateDocumentClosed'] and result['finalDocumentOpened'] and result['apronGenerated'] is False
 assert result['dimensionChains']==24 and result['dimensionSegments']==106 and result['finalRoots']==308 and result['openingCaptions']==27
 assert result['output']==b['output'] and sha(result['output'])==result['outputSHA256'] and sha(job/'control.dwg')==b['controlInputSHA256']==result['sourceSHA256']
 assert sha(b['module'])==b['moduleSHA256'] and sha(b['sidecar'])==b['sidecarSHA256'];r.update(result=result,resultSHA256=sha(job/'product-result.json'))
except BaseException as e:r['error']=repr(e);raise
finally:
 out=ROOT/f'插件/evidence/native-product-{version}.json';out.parent.mkdir(exist_ok=True);assert not out.exists();out.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=version,error=r['error'],output=r.get('result',{}).get('output')),ensure_ascii=False))
