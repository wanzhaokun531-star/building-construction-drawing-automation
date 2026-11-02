import sys,time,json
from pathlib import Path
from chain_contract import S,read,sha
version=sys.argv[1];b=read(S/f'evidence/native-host-binding-{version}.json');session=Path(b['session']);job=Path(b['job']);deadline=time.monotonic()+180
while not (session/'quiet-worker-ready.json').exists() or not (job/'door-caption-clearance.json').exists():
    if (job/'stage2-analysis.error').exists():raise RuntimeError((job/'stage2-analysis.error').read_text(encoding='utf-8'))
    if time.monotonic()>deadline:raise TimeoutError('owned caption worker readiness/result timeout; no UI action')
    time.sleep(.25)
ready=read(session/'quiet-worker-ready.json');r=read(job/'door-caption-clearance.json');assert ready['nonce']==b['nonce'] and ready['inputDesktop']!=ready['desktop'] and r['error'] is None
assert sha(b['module'])==b['moduleSHA256'] and sha(b['sidecar'])==b['sidecarSHA256'] and sha(b['input'])==b['inputSHA256']==r['inputSHA256']
source=job/'方案施工图_门编号避让.dwg';assert sha(source)==r['outputSHA256']
result=dict(contract='bcp.native-door-caption-run.v1',session=str(session),nonce=b['nonce'],processId=ready['processId'],binding=b,job=str(job),DWG=str(source),DWGSHA256=sha(source),reportSHA256=sha(job/'door-caption-clearance.json'),error=None)
out=S/f'evidence/native-caption-run-{version}.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),moved=r['moved'],DWGSHA256=sha(source)),ensure_ascii=False))
