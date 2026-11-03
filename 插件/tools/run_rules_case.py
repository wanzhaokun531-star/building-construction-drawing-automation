"""Run another original scheme copy through the same owned candidate module."""
from pathlib import Path
import sys,json,hashlib,shutil,importlib.util,datetime
R=Path(__file__).resolve().parents[2]
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
v,tag,source=sys.argv[1:4];source=Path(source).resolve()
b=read(R/f'阶段2/evidence/native-host-binding-{v}.json');s=Path(b['session']);ready=read(s/'quiet-worker-ready.json')
j=R/'阶段1/runs'/('stage2-rules-'+tag+'-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));j.mkdir();shutil.copyfile(source,j/'control.dwg');original=sha(source)
ini=Path(b['sidecar']);before=ini.read_text('utf-16');replacements={'receiptDirectory':str(j),'testBehavior':'generate','testInput':str(j/'control.dwg'),'testOutput':str(j/'插件生成施工图.dwg')}
ini.write_text('\n'.join(k+'='+replacements[k] if '=' in line and (k:=line.split('=',1)[0]) in replacements else line for line in before.splitlines())+'\n','utf-16')
spec=importlib.util.spec_from_file_location('q',R/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
report=dict(version=v,tag=tag,job=str(j),source=str(source),sourceSHA256=original,moduleSHA256=b['moduleSHA256'],steps=[],error=None)
try:
 report['steps'].append(q.dispatch(s,ready,'S1OPENPROBECONTROL',j,'native-open.done',60));(j/'phase2-product-async.only').write_text('BCPPRODUCTASYNC1\n','ascii')
 report['steps'].append(q.dispatch(s,ready,'S1PHASE2EXECUTE',j,'product-result.json',240));result=read(j/'product-result.json');report['result']=result
 assert result['completed'] and result.get('error') is None,result
 for output in result.get('outputs',[dict(path=result.get('output'),SHA256=result.get('outputSHA256'))]):assert sha(output['path'])==output['SHA256']
 assert sha(source)==original
except BaseException as e:report['error']=repr(e);raise
finally:
 ini.write_text(before,'utf-16');assert sha(ini)==b['sidecarSHA256'];(R/f'插件/evidence/native-rules-{v}-{tag}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8');print(json.dumps(dict(job=str(j),error=report['error']),ensure_ascii=False))
