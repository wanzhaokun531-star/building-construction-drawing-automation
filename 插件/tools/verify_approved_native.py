"""Saved-output verification against approved annotation geometry and source architecture."""
from pathlib import Path
import json,hashlib,sys,importlib.util,shutil,datetime
ROOT=Path(__file__).resolve().parents[2];ARCHIVE=ROOT.parent/'项目清理归档/建筑施工图自动化_20261102-181100'
def resolve(p):
 p=Path(p);return p if p.exists() else ARCHIVE/p.relative_to(ROOT)
def read(p):return json.loads(resolve(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def module(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
v=sys.argv[1];b=read(ROOT/f'阶段2/evidence/native-host-binding-{v}.json');job=Path(b['job']);session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');q=module('approved_preview_queue',ROOT/'阶段1/原生接口探测/run_quiet_job.py');result=read(job/'product-result.json');assert result['completed'];base=read(ROOT/'阶段2/示意/卫生间合并面积与房名避让_20261102/qa-v154.json');report={'version':v,'floors':[],'passed':False}
def same(a,b):
 if isinstance(a,(int,float)) and isinstance(b,(int,float)):return abs(a-b)<1e-5
 if type(a)!=type(b):return False
 if isinstance(a,list):return len(a)==len(b) and all(same(x,y) for x,y in zip(a,b))
 if isinstance(a,dict):return a.keys()==b.keys() and all(same(a[k],b[k]) for k in a)
 return a==b
for n,row in enumerate(result['outputs'],1):
 one=read(job/f'floor-{n}/product-result.json');records=Path(one['records']);mapping={r['template']:r['output'] for r in read(records/'approved-final-object-map.json')};source=Path(row['path']);tag='approved-floor'+str(n);run=ROOT/'阶段1/runs'/('approved-native-preview-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'-'+tag);run.mkdir();shutil.copyfile(source,run/'control.dwg');xmin=26000 if n==1 else 72422;(run/'annotation-index.tsv').write_text(f'S2DIMPREVIEW3 0 {one["finalRoots"]} {xmin} 17500 {xmin+31000} 44000 594 420\n',encoding='ascii');steps=[q.dispatch(session,ready,'S1OPENPROBECONTROL',run,'native-open.done',60),q.dispatch(session,ready,'S1PHASE2PREVIEW',run,'stage2-preview-native.json',90)];preview=read(run/'stage2-preview-native.json');assert preview['error'] is None and preview['originalBefore']==preview['originalAfter'];assert preview['page']['pipelineComplete'] and preview['page']['fileObserved'];actual={e['handle']:e for e in preview['originalBefore']}
 donor=read(Path(base['floors'][0]['previewJob'])/'stage2-preview-native.json') if n==1 else read(ROOT/'docs/归档前关键证据_v164/最终样张原生几何.json');expected={e['handle']:e for e in donor['originalBefore']};assert set(mapping)==set(expected)
 wrappers=[];differ=[];checked=0
 for h,e in expected.items():
  got=actual[mapping[h]]
  if n==1 and e['type']=='AcDbBlockReference' and h in [s.split()[1] for s in (ROOT/'插件/resources/approved-layout/floor-1.tsv').read_text('utf-8').splitlines() if s.startswith('X ')]:
   assert got['type']=='TDbAxisLabelSet' and not got['proxy'] and got['layer']=='A-AXIS';wrappers.append(mapping[h]);continue
  if e['type']=='TDbDrawingName':assert got['type']==e['type'];continue
  wanted={k:v for k,v in e.items() if k!='handle'};seen={k:v for k,v in got.items() if k!='handle'}
  if not same(wanted,seen):differ.append({'source':h,'output':mapping[h],'type':e['type'],'expected':wanted,'actual':seen})
  checked+=1
 (run/'geometry-differences.json').write_text(json.dumps(differ,ensure_ascii=False,indent=2),encoding='utf-8');assert not differ,('approved geometry differs',n,len(differ),str(run/'geometry-differences.json'))
 axes=[e for e in actual.values() if e['type']=='TDbAxisLabelSet'];assert len(axes)==12;assert not any(e['proxy'] for e in actual.values());assert len(actual)==one['finalRoots'];assert len([e for e in actual.values() if e['type']=='TDbWall'])==34;assert sha(source)==row['SHA256'];report['floors'].append({'floor':n,'DWG':str(source),'SHA256':sha(source),'previewJob':str(run),'PDF':preview['page']['outputPDF'],'geometryCompared':checked,'directNativeAxes':12,'convertedWrapperAxes':wrappers,'modelRoots':len(actual),'nativeTCHDimensions':sum(e['type']=='TDbDimension2' for e in actual.values()),'openingCount':sum(e['type']=='TDbOpening' for e in actual.values()),'records':str(records),'steps':steps})
report['passed']=True;(ROOT/f'插件/evidence/approved-saved-qa-{v}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'passed':True,'roots':[f['modelRoots'] for f in report['floors']],'nativeAxes':[12,12]},ensure_ascii=True))
