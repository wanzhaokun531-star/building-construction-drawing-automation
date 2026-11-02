"""Verify actual saved/reopened bath merge and dimension-layer whitelist."""
from pathlib import Path
import json,hashlib,importlib.util,subprocess,sys,re
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent;PF=ROOT/'阶段2/示意/门窗内部编号_20261102'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');session=Path(m['session']);ready=read(session/'quiet-worker-ready.json');q=load('q_bath',ROOT/'阶段1/原生接口探测/run_quiet_job.py');u=load('u_bath',ROOT/'阶段2/示意/房间面积随房名_20261101/verify_area_name_sample.py');pf=load('p_bath',PF/'public_fields.py');out=dict(contract='bcp.bath-merge-and-dimension-layer-qa.v1',version=v,passed=False,pluginCodeChanged=False,floors=[])
for f in m['floors']:
 n=f['floor'];log=Path(f['log']).read_text(encoding='cp936');assert f'SAVED|F{n}' in log and sha(f['source'])==f['sourceSHA256'];actualArea=[float(x.split('|')[1]) for x in log.splitlines() if x.startswith('MERGED_AREA_MM2|')];assert len(actualArea)==1 and abs(actualArea[0]-f['netAreaMm2'])<0.01;assert f'{actualArea[0]/1e6:.2f}m'==f['newText'];fileHash=sha(f['control']);groups={};current=None
 for line in Path(f['groupsLog']).read_text(encoding='cp936').splitlines():
  row=line.split('|')
  if row[0]=='GROUP':current=row[1];assert current not in groups;groups[current]=[]
  elif row[0]=='MEMBER':assert current is not None;groups[current].append((row[1],row[2]))
 expectedGroups={k:{tuple(z) for z in x} for k,x in f['oldGroups'].items() if k!=f['removedGroup']};assert {k:set(x) for k,x in groups.items()}==expectedGroups and f['removedGroup'] not in groups
 areaGroups={k:x for k,x in groups.items() if k.startswith('BCP_AREA_') and not k.endswith('_TOTAL')};assert len(areaGroups)==9 and set(areaGroups[f['retainedGroup']])=={(h,'TEXT') for h in f['retainedAreaMembers']}
 meta=Path(f['metadataLog']).read_text(encoding='cp936');metaRows={r[0]:r for line in meta.splitlines() if len(r:=line.split('|'))==5};body,sup=f['retainedAreaMembers'];assert metaRows[body][1]==f['newText'] and metaRows[sup][1]=='2';assert float(metaRows[body][3])==240 and float(metaRows[sup][3])==120 and metaRows[body][2]==metaRows[sup][2]=='_TCH_WINDOW';assert float(metaRows[body][4])==float(metaRows[sup][4])==0.65
 # Actual entget record, including lineweight and plot flag, after reopen.
 for token in ['(2 . "PUB_DIM")','(62 . 3)','(6 . "Continuous")','(370 . -3)']:assert token in meta,('layer attributes',token,meta)
 checkpoint=HERE/f'preview-{v}-floor{n}.json'
 if checkpoint.exists():
  z=read(checkpoint);assert z['controlSHA256']==fileHash;job=Path(z['job']);p=read(job/'stage2-preview-native.json');a=read(job/'product-axis-document-audit.json');steps=z['steps']
 else:
  job,p,a,steps=u.native_preview(f,session,ready,q);checkpoint.write_text(json.dumps(dict(job=str(job),controlSHA256=fileHash,steps=steps),ensure_ascii=False,indent=2),encoding='utf-8')
 before={e['handle']:e for e in read(f['sourcePreview'])['originalBefore']};after={e['handle']:e for e in p['originalBefore']};assert set(before)-set(after)==set(f['removed']) and not set(after)-set(before) and len(after)==f['expectedModelCount']
 for h,e in before.items():
  if h in f['removed']:continue
  now=after[h]
  if h in f['moves']:u.check_translated_root(e,now,f['moves'][h])
  elif h in f['dimensionHandles']:
   expected=dict(e);expected['layer']='PUB_DIM';assert now==expected,('dimension geometry change',n,h)
  else:assert now==e,('unlisted root changed',n,h)
 oldA=read(f['sourceAudit']);oldAnn={e['handle']:e for e in oldA['nativeAnnotations']};newAnn={e['handle']:e for e in a['nativeAnnotations']};assert set(oldAnn)-set(newAnn)==set(f['removed']) and not set(newAnn)-set(oldAnn)
 for h,e in oldAnn.items():
  if h in f['removed']:continue
  now=newAnn[h]
  if h in f['moves']:
   expected=dict(e)
   # This audit field is alignmentPoint, which stays zero for baseline/left
   # AcDbText. Actual insertion/stretch points were checked rigidly above.
   expected['position']=[pt if near(pt,[0,0,0]) else [x+y for x,y in zip(pt,f['moves'][h])] for pt in e['position']]
   if h==f['bodyHandle']:expected['text']=f['newText']
   assert now==expected,('changed area/name annotation differs',n,h,now,expected)
  elif h in f['dimensionHandles']:
   expected=dict(e);expected['layer']='PUB_DIM';assert now==expected
  else:assert now==e,('other native annotation changed',n,h)
 assert a['groups']==oldA['groups'] and a['axes']==oldA['axes']
 dimensions=[e for e in a['nativeAnnotations'] if e['type']=='TDbDimension2' and e['visible']];assert len(dimensions)==(25 if n==1 else 27);assert sum(e['layer']=='PUB_DIM' for e in dimensions)==len(f['dimensionHandles']);assert all(e['layer']=='A-AXIS' for e in dimensions if e['handle'] not in f['dimensionHandles'])
 # Native name/area ink geometry, not the meaningless alignmentPoint on Text.
 name=after[f['nameHandle']];ab=[min(after[h]['min'][i] for h in f['retainedAreaMembers']) for i in [0,1]]+[max(after[h]['max'][i] for h in f['retainedAreaMembers']) for i in [0,1]];assert near(ab,f['targetAreaBounds']) and near([*name['min'][:2],*name['max'][:2]],f['targetNameBounds']);assert abs(name['min'][1]-ab[3]-50)<1e-6 and abs((name['min'][0]+name['max'][0]-ab[0]-ab[2])/2)<1e-6
 sdkPath=PF/f'sdk-read-{v}-floor{n}.json'
 if not sdkPath.exists():subprocess.run([sys.executable,str(PF/'read_sdk.py'),v,f['control'],f'floor{n}'],check=True)
 sdk=read(sdkPath);assert sdk['sourceSHA256']==fileHash;fields=pf.fields_in_job(sdk['job']);oldFields=pf.fields_in_job(read(f['sourcePublicRead'])['job']);openings=[h for h,e in before.items() if e['type']=='TDbOpening'];assert all(fields[h]['fields']==oldFields[h]['fields'] for h in openings);assert not any(h in openings for members in groups.values() for h,t in members)
 result=dict(floor=n,control=f['control'],SHA256=fileHash,previewJob=str(job),PDF=p['page']['outputPDF'],steps=steps,sdkRead=str(sdkPath),mergedAreaM2=actualArea[0]/1e6,display='5.20m²',functionalRoomAreaGroups=9,removedDuplicateAreaMembers=f['removed'],retainedMembers=f['retainedAreaMembers'],movedName=f['nameHandle'],areaBelowNameGap=50,nativeOpeningsAllFieldsAndGripsUnchanged=True,noOpeningInAnyCADGroup=True,wholeTCHDimensions=len(dimensions),dimensionsToPUB_DIM=len(f['dimensionHandles']),otherDimensionsUnchanged=True,allOtherRootGeometryAndAreaTextUnchanged=True,modelRoots=len(after),groups=groups)
 out['floors'].append(result);print(json.dumps({k:result[k] for k in ['floor','mergedAreaM2','functionalRoomAreaGroups','dimensionsToPUB_DIM','modelRoots']},ensure_ascii=False),flush=True)
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256'];out['passed']=True;(HERE/f'qa-{v}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print('BATH_MERGE_AND_LAYER_NATIVE_QA_PASSED',flush=True)
