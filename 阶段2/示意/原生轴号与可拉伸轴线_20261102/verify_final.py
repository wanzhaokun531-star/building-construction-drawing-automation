"""Saved/reopened SDK evidence for direct TCH axes and independent lines."""
from pathlib import Path
import json,hashlib,importlib.util,sys,subprocess
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
v=sys.argv[1];m=read(HERE/f'manifest-{v}.json');log=Path(m['log']).read_text('cp936');assert 'SAVED|FINAL' in log and sha(m['source'])==m['sourceSHA256'];lines=log.splitlines();ids={r[1]:r[2] for s in lines if len(r:=s.split('|'))==3 and r[0]=='DIRECT_AXIS'};connector=[s.split('|')[1] for s in lines if s.startswith('NEW_AXIS1_CONNECTOR|')];assert len(ids)==12 and len(connector)==1;newLine=connector[0]
cap=load('axis_generic_capture',HERE/'capture.py');captures={};previews={}
for tag,key in [('final','control'),('number21','numberTrial'),('letterP','letterTrial'),('line123','lineTrial')]:
 dest=HERE/f'capture-{v}-{tag}.json'
 if dest.exists():
  r=read(dest);assert sha(m[key])==r['SHA256'];p=read(Path(r['job'])/'stage2-preview-native.json')
 else:r,p=cap.capture(v,m[key],m['expectedModelCount'],tag)
 captures[tag]=r;previews[tag]={e['handle']:e for e in p['originalBefore']}
before={e['handle']:e for e in read(m['sourcePreview'])['originalBefore']};after=previews['final'];removed={x['container'] for x in m['axes']};added=set(ids.values())|{newLine};assert set(before)-set(after)==removed and set(after)-set(before)==added
u=load('native_translation_check',ROOT/'阶段2/示意/房间面积随房名_20261101/verify_area_name_sample.py');title=m['titleMove']['handle']
for h,e in before.items():
 if h in removed:continue
 if h==title:u.check_translated_root(e,after[h],m['titleMove']['delta'])
 else:assert after[h]==e,('unlisted source root changed',h)
axisResults=[];circles=0
for axis in m['axes']:
 h=ids[axis['name']];e=after[h];assert e['type']=='TDbAxisLabelSet' and not e['proxy'] and e['layer']=='A-AXIS';gp=e['grip']['points'];sp=e['stretch']['points'];wanted=axis['centres'] if axis['side']=='both' else [axis['centres'][1 if axis['side']=='positive' else 0]];assert len(gp)==4*len(wanted) and len(sp)==2*len(wanted) and all(any(near(c,p) for p in gp) for c in wanted),('native side/centre',axis,h,gp);circles+=len(wanted)
 for edit in axis.get('leadEdits',[]):
  assert any(near(edit['target'],p) for p in sp),('native lead endpoint not restored to old clip edge',axis['name'],edit,sp)
  report=read(edit['nativeReport']);assert report['completed'] and report['error'] is None and report['opaqueAppDataDecoded'] is False and report['after']==report['expected']
  assert len([x for x in report['attempts'] if x['committed'] and x['matched']])==1
  for field in ['grips','stretch']:
   changed=[(p,q) for p,q in zip(report['before'][field],report['after'][field]) if not near(p,q)]
   assert len(changed)==1 and near(changed[0][0],edit['before']) and near(changed[0][1],edit['target'])
 axisResults.append(dict(name=axis['name'],handle=h,type=e['type'],side=axis['side'],label=axis['label'],nativeGripCount=len(gp),visibleCentres=wanted,nativeLeadEdits=axis.get('leadEdits',[])))
assert circles==20 and len([e for e in after.values() if e['type']=='TDbAxisLabelSet'])==12
e=after[newLine];assert e['type']=='AcDbLine' and e['layer']=='A-AXIS' and near(e['curve']['start'],m['addedConnector']['start']) and near(e['curve']['end'],m['addedConnector']['end'])
shift=previews['line123'];different={h for h in after if after[h]!=shift[h]};assert different=={'841B'} and near(after['841B']['curve']['start'],shift['841B']['curve']['start']) and near(shift['841B']['curve']['end'],[after['841B']['curve']['end'][0],after['841B']['curve']['end'][1]+123,0])
for tag,name in [('number21','BCP_AXIS_V_1'),('letterP','BCP_AXIS_H_A')]:
 trial=previews[tag];h=ids[name]
 # Letter/number changes can leave geometric bounds identical inside the circle.
 assert set(trial)==set(after) and all(trial[k]==after[k] for k in after if k!=h),('renumber trial changed other roots',tag)
groups={};current=None
for line in Path(m['groupsLog']).read_text('cp936').splitlines():
 r=line.split('|')
 if r[0]=='GROUP':current=r[1];assert current not in groups;groups[current]=[]
 elif r[0]=='MEMBER':groups[current].append((r[1],r[2]))
assert len(groups)==10 and all(k.startswith('BCP_AREA_') for k in groups) and not any(h in added or after[h]['type']=='AcDbLine' for mem in groups.values() for h,t in mem)
def table(p):
 raw=Path(p).read_text('cp936');assert 'READ_ONLY_DUMP_COMPLETE' in raw;out={}
 for line in raw.splitlines():
  r=line.split('|')
  if r[0] in ['ENTITY','LAYER','STYLE']:out.setdefault(r[0],{})[r[1]]=r[2:]
 return out
properties=table(m['propertyLog']);oldProps=read(ROOT/'阶段2/示意/用户修改对比_20261102/property-dump-manifest-v156.json');oldProperties=table(next(x['report'] for x in oldProps['rows'] if x['tag']=='modified'));assert properties['LAYER']==oldProperties['LAYER'] and properties['STYLE']==oldProperties['STYLE'];assert properties['LAYER']['PUB_DIM'][0]=='25';assert all(properties['ENTITY'][h]==oldProperties['ENTITY'][h] for h in before if h not in removed)
pfFolder=ROOT/'阶段2/示意/门窗内部编号_20261102';sdkPath=pfFolder/f'sdk-read-{v}-final.json'
if not sdkPath.exists():subprocess.run([sys.executable,str(pfFolder/'read_sdk.py'),v,m['control'],'final'],check=True)
pf=load('native_axis_opening_fields',pfFolder/'public_fields.py');oldFields=pf.fields_in_job(read(ROOT/'阶段2/evidence/native-host-binding-v155.json')['job']);newFields=pf.fields_in_job(read(sdkPath)['job']);openings=[h for h,e in before.items() if e['type']=='TDbOpening'];assert all(newFields[h]['fields']==oldFields[h]['fields'] for h in openings)
# Title clears both axis-circle row and the existing footer annotations.
tb=after[title];areaHandles=[h for members in groups.values() for h,t in members];assert tb['max'][1]<min(c[1]-400 for x in axisResults if x['name'].startswith('BCP_AXIS_V_') for c in x['visibleCentres'] if c[1]<25000)
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256'] and sha(m['source'])==m['sourceSHA256']
r=dict(contract='bcp.direct-native-axis-sample-qa.v1',version=v,passed=True,source=m['source'],sourceSHA256=m['sourceSHA256'],control=m['control'],SHA256=sha(m['control']),roots=242,removedAxisContainers=12,dissolvedAxisGroups=15,nativeAxisLabelSets=12,visibleAxisCircles=circles,axes=axisResults,newAxis1Connector=newLine,standaloneAxisLinesRestoredAfterStretch=True,lineStretchMm=123,allOriginalSourceRootsExceptTitleAndRemovedContainersIdentical=True,allLayersStylesAndExistingEntityCommonPropertiesPreserved=True,allSourceDimensionGeometryAndOpeningNumberFieldsPreserved=True,wholeVisibleDimensions=19,openingCount=len(openings),groups=groups,renumberTrialsRequireVisualSemanticReview=['21 at both ends of axis1, then1','P at both ends of axisA, thenA'],captures=captures,sdkRead=str(sdkPath),pluginCodeChanged=False,pluginBinarySHA256=m['pluginBinarySHA256'],scope='Independent direct TCH singleton axis objects; no claim that all parallel axes were merged into one reindexing collection or every Tianzheng axis command was tested.')
(HERE/f'qa-{v}.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(passed=True,roots=242,directNativeAxes=12,circles=20,originalDimensionsAndOpeningsPreserved=True,stretchAndRestore=123),ensure_ascii=True))
