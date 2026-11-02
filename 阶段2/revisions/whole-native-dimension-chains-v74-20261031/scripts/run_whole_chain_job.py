import sys,time,importlib.util,json
from pathlib import Path
from chain_contract import S,read,sha
P=S.parent/'阶段1/原生接口探测'
version=sys.argv[1];binding=read(S/f'evidence/native-host-binding-{version}.json');session=Path(binding['session']);job=Path(binding['job'])
spec=importlib.util.spec_from_file_location('native_dispatch',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
deadline=time.monotonic()+180
while not (session/'quiet-worker-ready.json').exists():
    if time.monotonic()>deadline:raise TimeoutError('owned native worker readiness timeout; no window/process action')
    time.sleep(.25)
ready=read(session/'quiet-worker-ready.json');assert ready['nonce']==binding['nonce'] and ready['inputDesktop']!=ready['desktop']
assert sha(binding['module'])==binding['moduleSHA256'] and sha(binding['sidecar'])==binding['sidecarSHA256']
result=dict(contract='bcp.whole-chain-dispatch.v1',session=str(session),nonce=ready['nonce'],processId=ready['processId'],job=str(job),binding=binding,steps=[],error=None)
try:
    result['steps'].append(m.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90))
    assert read(job/'native-open.json')['error'] is None
    result['steps'].append(m.dispatch(session,ready,'S1PHASE2EXECUTE',job,'whole-dimension-chains.json',240))
    report=read(job/'whole-dimension-chains.json');assert report['error'] is None and report['savedReopened']
    result['reportSHA256']=sha(job/'whole-dimension-chains.json');result['DWG']=str(job/'方案施工图_整体连续天正尺寸.dwg');result['DWGSHA256']=sha(result['DWG'])
except BaseException as e:result['error']=repr(e);raise
finally:
    path=S/f'evidence/native-whole-chain-run-{version}.json';assert not path.exists();path.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),error=result['error'],DWG=result.get('DWG')),ensure_ascii=False))
