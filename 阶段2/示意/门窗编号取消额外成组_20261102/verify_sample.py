"""Native saved-DWG verification of group removal only."""
from pathlib import Path
import json,hashlib,importlib.util,subprocess,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent;PREVIOUS=ROOT/'阶段2/示意/门窗内部编号_20261102'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
v=sys.argv[1];mp=HERE/f'manifest-{v}.json';m=read(mp);session=Path(m['session']);ready=read(session/'quiet-worker-ready.json');q=load('q_ungroup',ROOT/'阶段1/原生接口探测/run_quiet_job.py');u=load('u_ungroup',ROOT/'阶段2/示意/房间面积随房名_20261101/verify_area_name_sample.py');pf=load('p_ungroup',PREVIOUS/'public_fields.py');oldQA=read(PREVIOUS/'qa-v147.json')
out=dict(contract='bcp.native-number-no-extra-group-qa.v1',version=v,pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],geometryPassed=False,floors=[])
for f in m['floors']:
 n=f['floor'];log=Path(f['log']).read_text(encoding='cp936');assert f'SAVED|F{n}' in log and all('DISSOLVED|'+name in log for name in f['removedGroups']);assert sha(f['source'])==f['sourceSHA256'];groups={};current=None
 for line in Path(f['groupsLog']).read_text(encoding='cp936').splitlines():
  row=line.split('|')
  if row[0]=='GROUP':current=row[1];assert current not in groups;groups[current]=[]
  elif row[0]=='MEMBER':assert current is not None;groups[current].append((row[1],row[2]))
 assert groups and not any(name in groups for name in f['removedGroups'])
 openingHandles={t['opening'] for t in f['targets']};assert not any(h in openingHandles for members in groups.values() for h,t in members),('opening still in a native group',n)
 for name,members in f['oldGroups'].items():
  if name not in f['removedGroups']:assert set(groups[name])=={tuple(x) for x in members},('protected area group changed',name)
 subprocess.run([sys.executable,str(PREVIOUS/'read_sdk.py'),v,f['control'],f'floor{n}'],check=True)
 sdk=read(PREVIOUS/f'sdk-read-{v}-floor{n}.json');fields=pf.fields_in_job(sdk['job']);oldFields=pf.fields_in_job(read(PREVIOUS/f'sdk-read-v147-floor{n}.json')['job']);assert set(fields)==set(oldFields)
 for h,x in fields.items():assert x['fields']==oldFields[h]['fields'],('native public property changed',n,h)
 for t in f['targets']:assert fields[t['opening']]['fields']['门窗编号']==t['number']
 fileHash=sha(f['control']);job,p,a,steps=u.native_preview(f,session,ready,q);assert sha(f['control'])==fileHash;oldP=read(f['sourcePreview']);oldA=read(f['sourceAudit']);assert p['originalBefore']==oldP['originalBefore'];assert a['nativeAnnotations']==oldA['nativeAnnotations'] and a['groups']==oldA['groups'] and a['axes']==oldA['axes']
 roots={e['handle']:e for e in p['originalBefore']};assert all(roots[h]['type']=='TDbOpening' and not roots[h]['proxy'] for h in openingHandles)
 result=dict(floor=n,control=f['control'],SHA256=fileHash,previewJob=str(job),PDF=p['page']['outputPDF'],steps=steps,sdkRead=str(PREVIOUS/f'sdk-read-{v}-floor{n}.json'),actualInternalNumbersVerified=len(f['targets']),removedExtraGroups=len(f['removedGroups']),noOpeningInAnyNativeGroup=True,allOpeningsDirectModelTDbOpening=True,noNewOrDeletedModelRoots=True,allGeometryAndNativeAnnotationsIdentical=True,modelRoots=len(roots),wholeTCHDimensions=sum(e['type']=='TDbDimension2' and e['visible'] for e in a['nativeAnnotations']),reopenedGroups=groups)
 out['floors'].append(result);print(json.dumps({k:result[k] for k in ['floor','actualInternalNumbersVerified','removedExtraGroups','modelRoots']},ensure_ascii=False),flush=True)
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256'];out['geometryPassed']=True;(HERE/f'qa-{v}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print('UNGROUP_NATIVE_QA_PASSED',flush=True)
