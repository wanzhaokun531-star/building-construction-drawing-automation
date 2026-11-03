from pathlib import Path
import json,sys,shutil,importlib.util,subprocess,hashlib
R=Path(__file__).resolve().parents[3]
b=json.loads((R/f'阶段2/evidence/native-host-binding-{sys.argv[1]}.json').read_text('utf-8'));s=Path(b['session']);j=Path(b['job']);ready=json.loads((s/'quiet-worker-ready.json').read_text('utf-8'))
spec=importlib.util.spec_from_file_location('q',R/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
source=Path(sys.argv[2]) if len(sys.argv)>2 else j/'百塘村邹总_标注深化候选.dwg'
count=json.loads((j/'tidy.done').read_text('utf-8'))['rootCount'] if (j/'tidy.done').exists() else json.loads((j/'whole-dimension-chains.json').read_text('utf-8'))['outputModelCount']
for n,dx in enumerate([0,42887.596250418486,86035.314872116665],1):
 run=j/f'preview-final-{n}';run.mkdir();shutil.copyfile(source,run/'control.dwg')
 (run/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {count} {dx-6500} -10500 {dx+19500} 19000 594 420\n',encoding='ascii')
 q.dispatch(s,ready,'S1OPENPROBECONTROL',run,'native-open.done',60)
 q.dispatch(s,ready,'S1PHASE2PREVIEW',run,'stage2-preview-native.json',90)
 subprocess.run(['pdftoppm','-scale-to','3400','-png','-singlefile',str(run/'stage2-A2-1-100.pdf'),str(run/'preview')],check=True,capture_output=True)
 print(str(run/'preview.png'),flush=True)
