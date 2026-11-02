"""Actual saved-DWG SDK fields, root geometry, whole dimensions and groups."""
from pathlib import Path
import json,hashlib,importlib.util,subprocess,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent;PF=ROOT/'阶段2/示意/门窗内部编号_20261102'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');session=Path(m['session']);ready=read(session/'quiet-worker-ready.json');q=load('q_category',ROOT/'阶段1/原生接口探测/run_quiet_job.py');u=load('u_category',ROOT/'阶段2/示意/房间面积随房名_20261101/verify_area_name_sample.py');pf=load('p_category',PF/'public_fields.py');oldQA=read(ROOT/'阶段2/示意/门窗编号取消额外成组_20261102/qa-v148.json')
out=dict(contract='bcp.category-number-saved-qa.v1',version=v,pluginCodeChanged=False,passed=False,floors=[])
for f in m['floors']:
 n=f['floor'];log=Path(f['log']).read_text(encoding='cp936');assert f'SAVED|F{n}' in log and all('NUMBER_RETURNED|'+o['opening']+'|'+o['newNumber'] in log for o in f['openings'] if o['changed']);assert sha(f['source'])==f['sourceSHA256'];fileHash=sha(f['control']);groups={};current=None
 for line in Path(f['groupsLog']).read_text(encoding='cp936').splitlines():
  row=line.split('|')
  if row[0]=='GROUP':current=row[1];assert current not in groups;groups[current]=[]
  elif row[0]=='MEMBER':assert current is not None;groups[current].append((row[1],row[2]))
 oldG=next(x for x in oldQA['floors'] if x['floor']==n)['reopenedGroups'];assert {g:set(x) for g,x in groups.items()}=={g:{tuple(z) for z in x} for g,x in oldG.items()}
 handles={o['opening'] for o in f['openings']};assert not any(h in handles for members in groups.values() for h,t in members)
 sdkPath=PF/f'sdk-read-{v}-floor{n}.json'
 if not sdkPath.exists():subprocess.run([sys.executable,str(PF/'read_sdk.py'),v,f['control'],f'floor{n}'],check=True)
 sdk=read(sdkPath);assert sdk['sourceSHA256']==fileHash;fields=pf.fields_in_job(sdk['job']);beforeFields=pf.fields_in_job(read(f['sourcePublicRead'])['job']);assert set(fields)==set(beforeFields)
 ops={o['opening']:o for o in f['openings']};numbered=0
 for h,r in fields.items():
  expected=dict(beforeFields[h]['fields'])
  if h in ops:
   o=ops[h];expected['门窗编号']=o['newNumber'] or '';assert r['fields']['门窗编号']==expected['门窗编号'];numbered+=bool(o['newNumber'])
  assert r['fields']==expected,('public field changed',n,h,r['fields'],expected)
 checkpoint=HERE/f'preview-{v}-floor{n}.json'
 if checkpoint.exists():
  cached=read(checkpoint);assert cached['controlSHA256']==fileHash;job=Path(cached['job']);p=read(job/'stage2-preview-native.json');a=read(job/'product-axis-document-audit.json');steps=cached['steps']
 else:
  job,p,a,steps=u.native_preview(f,session,ready,q);checkpoint.write_text(json.dumps(dict(job=str(job),controlSHA256=fileHash,steps=steps),ensure_ascii=False,indent=2),encoding='utf-8')
 assert sha(f['control'])==fileHash
 before={e['handle']:e for e in read(f['sourcePreview'])['originalBefore']};after={e['handle']:e for e in p['originalBefore']};notes=[x.split('|')[1] for x in log.splitlines() if x.startswith('NOTE|')];assert len(notes)==(1 if f.get('noteText') else 0);assert set(after)-set(before)==set(notes) and not set(before)-set(after) and len(after)==f['expectedModelCount']
 for h,e in before.items():
  now=after[h];o=ops.get(h)
  if not o or not o['changed']:assert now==e,('unlisted root changed',n,h)
  else:
   assert e['type']==now['type']=='TDbOpening' and not now['proxy']
   for k in e:
    if k in ['min','max']:continue # Number string bounds deliberately change.
    if k in ['grip','stretch']:
     assert set(e[k])==set(now[k])
     for sk in e[k]:
      if sk=='points':
       assert len(now[k][sk])==len(e[k][sk]) and all(near(x,y) for x,y in zip(now[k][sk][:-1],e[k][sk][:-1])),('physical opening points changed',n,h,k)
       assert near(now[k][sk][-1],o['labelTarget']),('number label point',n,h,k,now[k][sk][-1],o['labelTarget'])
      else:assert now[k][sk]==e[k][sk]
    else:assert now[k]==e[k],('other opening field changed',n,h,k)
 oldA=read(f['sourceAudit']);assert [e for e in a['nativeAnnotations'] if e['handle'] not in notes]==oldA['nativeAnnotations'] and a['groups']==oldA['groups'] and a['axes']==oldA['axes'],('protected annotations changed',n)
 for h in notes:
  e=next(e for e in a['nativeAnnotations'] if e['handle']==h);assert e['type']=='AcDbText' and e['text']==f['noteText'] and after[h]['type']=='AcDbText'
 dims=sum(x['type']=='TDbDimension2' and x['visible'] for x in a['nativeAnnotations']);assert dims==(25 if n==1 else 27)
 result=dict(floor=n,control=f['control'],SHA256=fileHash,previewJob=str(job),PDF=p['page']['outputPDF'],sdkRead=str(sdkPath),steps=steps,modelRoots=len(after),numbered=numbered,changed=sum(o['changed'] for o in f['openings']),wholeTCHDimensions=dims,allNumbersActualPublicFieldsVerified=True,allOpeningPhysicalPropertiesAndCoreGripsPreserved=True,allOtherGeometryAreasNamesAxesAndDimensionsPreserved=True,noOpeningInAnyCADGroup=True,allNumberedOpeningsDirectTDbOpening=True,groups=groups,pending=[o['opening'] for o in f['openings'] if not o['newNumber']])
 out['floors'].append(result);print(json.dumps({k:result[k] for k in ['floor','numbered','changed','wholeTCHDimensions','modelRoots']},ensure_ascii=False),flush=True)
assert sum(f['numbered'] for f in out['floors'])==40 and sum(f['changed'] for f in out['floors'])==27
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']
out['passed']=True;(HERE/f'qa-{v}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print('CATEGORY_NUMBER_NATIVE_QA_PASSED',flush=True)
