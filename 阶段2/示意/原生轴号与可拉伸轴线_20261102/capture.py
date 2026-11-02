"""Read-only generic native preview; supports direct TCH axes without old containers."""
from pathlib import Path
import datetime,hashlib,importlib.util,json,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def capture(version,source,count,tag):
 b=read(ROOT/f'阶段2/evidence/native-host-binding-{version}.json');session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');spec=importlib.util.spec_from_file_location('q_direct_axes',ROOT/'阶段1/原生接口探测/run_quiet_job.py');q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
 source=Path(source);before=sha(source);job=ROOT/'阶段1/runs'/('stage2-preview-direct-axes-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'-'+tag);job.mkdir();shutil.copyfile(source,job/'control.dwg');(job/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {count} 72422 17500 103422 44000 594 420\n',encoding='ascii')
 steps=[q.dispatch(session,ready,'S1OPENPROBECONTROL',job,'native-open.done',90),q.dispatch(session,ready,'S1PHASE2PREVIEW',job,'stage2-preview-native.json',120)];p=read(job/'stage2-preview-native.json');assert p['error'] is None and p['originalBefore']==p['originalAfter'] and p['initialModelCount']==p['finalModelCount']==count and p['globalsBefore']==p['globalsAfter'] and p['layoutBefore']==p['layoutAfter'];assert p['page']['pipelineComplete'] and p['page']['fileObserved'] and p['page']['error'] is None;assert sha(source)==sha(job/'control.dwg')==before
 r=dict(version=version,tag=tag,source=str(source),SHA256=before,job=str(job),PDF=p['page']['outputPDF'],steps=steps,readOnly=True,rootCount=count);(HERE/f'capture-{version}-{tag}.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(tag=tag,job=str(job),counts={t:sum(e['type']==t for e in p['originalBefore']) for t in ['TDbAxisLabelSet','AcDbBlockReference','AcDbLine','TDbDimension2']}),ensure_ascii=True),flush=True);return r,p
if __name__=='__main__':capture(sys.argv[1],sys.argv[2],int(sys.argv[3]),sys.argv[4])
