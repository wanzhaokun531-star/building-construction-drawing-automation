"""Render saved product drawings for internal QA; PDFs are never user deliverables."""
from pathlib import Path
import json,sys,shutil,importlib.util,subprocess
R=Path(__file__).resolve().parents[2];v=sys.argv[1];read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
b=read(R/f'阶段2/evidence/native-host-binding-{v}.json');s=Path(b['session']);j=(Path(sys.argv[2]) if len(sys.argv)>2 else Path(b['job'])).resolve();ready=read(s/'quiet-worker-ready.json');result=read(j/'product-result.json')
spec=importlib.util.spec_from_file_location('q',R/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
if result['contract']=='bcp.two-plan-result.v1':
 cases=[]
 for i,o in enumerate(result['outputs'],1):
  one=read(j/f'floor-{i}/product-result.json');cases.append((i,Path(o['path']),one['finalRoots'],[26000 if i==1 else 72422,17000,57000 if i==1 else 103422,43500]))
elif result['finalRoots']>8000:cases=[(n,Path(result['output']),result['finalRoots'],[dx-6500,-10500,dx+19500,19000]) for n,dx in enumerate([0,42887.596250418486,86035.314872116665],1)]
else:cases=[(1,Path(result['output']),result['finalRoots'],[9400,11350,38000,40400])]
for n,source,count,box in cases:
 run=j/f'preview-final-{n}';run.mkdir(exist_ok=True);assert not (run/'stage2-preview-native.json').exists();shutil.copyfile(source,run/'control.dwg');(run/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {count} '+ ' '.join(map(str,box))+' 594 420\n','ascii')
 q.dispatch(s,ready,'S1OPENPROBECONTROL',run,'native-open.done',60);q.dispatch(s,ready,'S1PHASE2PREVIEW',run,'stage2-preview-native.json',90)
 subprocess.run(['pdftoppm','-scale-to','2800','-png','-singlefile',str(run/'stage2-A2-1-100.pdf'),str(run/'preview')],check=True,capture_output=True);print(str(run/'preview.png'),flush=True)
