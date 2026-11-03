"""Run BCP_BUILD on the confirmed two-floor source in an owned host."""
from pathlib import Path
import sys,json,importlib.util,time,hashlib
ROOT=Path(__file__).resolve().parents[2];v=sys.argv[1];b=json.loads((ROOT/f'阶段2/evidence/native-host-binding-{v}.json').read_text('utf-8'));session=Path(b['session']);job=Path(b['job']);spec=importlib.util.spec_from_file_location('native_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
deadline=time.monotonic()+120
while not (session/'quiet-worker-ready.json').exists():
 if time.monotonic()>deadline:raise TimeoutError('owned CAD did not become ready')
 time.sleep(.25)
ready=json.loads((session/'quiet-worker-ready.json').read_text('utf-8-sig'));report=dict(version=v,job=str(job),steps=[],error=None)
try:
 report['steps'].append(q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',60));(job/'phase2-product-async.only').write_text('BCPPRODUCTASYNC1\n',encoding='ascii');report['steps'].append(q.dispatch(session,ready,'S1PHASE2EXECUTE',job,'product-result.json',180));result=json.loads((job/'product-result.json').read_text('utf-8'));assert result['completed'] and result['error'] is None,result;assert result['contract']=='bcp.two-plan-result.v1' and len(result['outputs'])==2
 for n,row in enumerate(result['outputs'],1):
  assert hashlib.sha256(Path(row['path']).read_bytes()).hexdigest().upper()==row['SHA256'];one=json.loads((job/f'floor-{n}/product-result.json').read_text('utf-8'));assert one['completed'] and one['sourceUnchanged'] and one['finalDocumentOpened'] and one['version']=='0.1.3';proof=json.loads((Path(one['records'])/'approved-generation.json').read_text('utf-8'));assert proof['annotationsCreatedFromNativePrototypes'] and not proof['sourceWallsReplaced'] and not proof['runtimePython'];assert proof['nativeOpeningNumbers']==(21 if n==1 else 19) and len(proof['areas'])==10
 report['result']=result
except BaseException as e:report['error']=repr(e);raise
finally:(ROOT/f'插件/evidence/native-approved-{v}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'version':v,'error':report['error'],'job':str(job)},ensure_ascii=True))
