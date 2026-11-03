"""Existing read-only ObjectARX LIST path, bound to a saved sample copy."""
from pathlib import Path
import json,hashlib,shutil,importlib.util,sys,datetime
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
version=sys.argv[1];source=Path(sys.argv[2]).resolve();tag=sys.argv[3]
bp=ROOT/f'阶段2/evidence/native-host-binding-{version}.json';b=read(bp);session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');assert source.is_relative_to(ROOT/'阶段1/runs')
job=ROOT/'阶段1/runs'/('stage2-inspect-number-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'-'+tag);job.mkdir();shutil.copyfile(source,job/'control.dwg');expected=sha(source)
ini=Path(b['sidecar']);before=ini.read_text(encoding='utf-16');lines=[]
for line in before.splitlines():
 if line.startswith('receiptDirectory='):line='receiptDirectory='+str(job)
 elif line.startswith('testBehavior='):line='testBehavior='+(sys.argv[4] if len(sys.argv)>4 else 'inspect-list')
 elif line.startswith('testInput='):line='testInput='+str(job/'control.dwg')
 elif line.startswith('testOutput='):line='testOutput='+str(job/'DO_NOT_CREATE.dwg')
 lines.append(line)
text='\n'.join(lines)+'\n';ini.write_text(text,encoding='utf-16')
spec=importlib.util.spec_from_file_location('native_number_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
steps=[]
try:
 steps.append(q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90));(job/'phase2-product-async.only').write_text('Read-only native SDK public properties\n',encoding='ascii');steps.append(q.dispatch(session,ready,'S1PHASE2EXECUTE',job,'product-result.json',90));result=read(job/'product-result.json');assert result['completed'] and result['error'] is None and not result['drawingWritten'] and result['sourceSHA256']==expected
 assert not (job/'DO_NOT_CREATE.dwg').exists() and sha(source)==sha(job/'control.dwg')==expected
 report=dict(contract='bcp.saved-opening-number-sdk-read.v1',version=version,tag=tag,source=str(source),sourceSHA256=expected,job=str(job),sidecarBeforeSHA256=hashlib.sha256(before.encode('utf-16')).hexdigest().upper(),temporarySidecarSHA256=sha(ini),steps=steps,result=result)
 (HERE/f'sdk-read-{version}-{tag}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(job=str(job),counts=result['input']['types']),ensure_ascii=False))
finally:
 ini.write_text(before,encoding='utf-16');assert sha(ini)==b['sidecarSHA256']
