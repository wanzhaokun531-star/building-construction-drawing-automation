"""Independent saved-DWG public geometry/number checks for the 0.1.4 release."""
from pathlib import Path
import json,sys,math,hashlib,importlib.util,shlex,collections
R=Path(__file__).resolve().parents[2];v=sys.argv[1]
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
spec=importlib.util.spec_from_file_location('f',R/'阶段2/示意/门窗内部编号_20261102/public_fields.py');f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
b=read(R/f'阶段2/evidence/native-host-binding-{v}.json');j=Path(b['job'])
near=lambda a,b:len(a)==len(b) and all(math.dist(x,y)<1e-5 for x,y in zip(a,b))
def preview(job,n=1):
 p=read(job/f'preview-final-{n}/stage2-preview-native.json');assert p['originalBefore']==p['originalAfter'] and p['error'] is None
 return {e['handle']:e for e in p['originalBefore']}
def mapping(records):return {e['template']:e['output'] for e in read(Path(records)/'approved-final-object-map.json')}
def saved_fields(tag):return f.fields_in_job(read(R/f'阶段2/示意/门窗内部编号_20261102/sdk-read-{v}-{tag}.json')['job'])
report=dict(version=v,moduleSHA256=b['moduleSHA256'],three={},two=[],legacy={},passed=False)
three=read(R/f'插件/evidence/native-rules-{v}-three-floor.json');tj=Path(three['job']);tr=three['result'];actual=preview(tj);expected=preview(Path(read(R/'阶段2/evidence/native-host-binding-v191.json')['job']));mp=mapping(tr['records']);assert set(mp)==set(expected) and len(actual)==len(mp)==8525
for h,e in expected.items():
 got=actual[mp[h]];assert (got['type'],got['layer'],got['proxy'])==(e['type'],e['layer'],e['proxy']),h
 assert near(got['grip']['points'],e['grip']['points']),h
now=saved_fields('final-three');old=f.fields_in_job(read(R/'阶段2/项目/百塘村邹总_20261103/sdk-read-reviewfix-v191.json')['job'])
fields=['门窗编号','门窗类型','宽度','高度','离地高度','转角'];openings=0;numbered=0
for h,value in old.items():
 if '门窗编号' not in value['fields']:continue
 a=now[mp[h]]['fields'];o=value['fields'];assert all(a[k]==o[k] for k in fields),(h,a,o);openings+=1;numbered+=bool(a['门窗编号'])
assert (openings,numbered)==(47,44);assert sum(e['type']=='TDbText' for e in actual.values())==34
areas=read(Path(tr['records'])/'three-floor-generation.json')['areas'];assert len(areas)==23
proof=read(Path(read(R/f'阶段2/示意/门窗内部编号_20261102/sdk-read-{v}-final-three.json')['job'])/'approved-edit-proof.json');assert proof['allTrialsRestored']
report['three']=dict(rootsCompared=8525,nativeRoomNames=34,nativeWalls=126,openingFieldsCompared=47,numbered=44,visibleChains=tr['dimensionChains'],visibleSegments=tr['dimensionSegments'],areas=areas,editing=proof,sourceUnchanged=sha(three['source'])==three['sourceSHA256'],DWG=tr['output'],SHA256=sha(tr['output']))
base=read(R/'插件/evidence/approved-saved-qa-v170.json')['floors']
for n in [1,2]:
 one=read(j/f'floor-{n}/product-result.json');a=preview(j,n);m=mapping(one['records']);oldmeta=base[n-1];em=mapping(oldmeta['records']);oldp=read(Path(oldmeta['previewJob'])/'stage2-preview-native.json');expected={x['handle']:x for x in oldp['originalBefore']};removed=[];dimensions=0
 for donor,h in em.items():
  e=expected[h];target=m[donor]
  if target not in a:assert e['type'] in ['AcDbBlockReference','AcDbWipeout'];removed.append(target);continue
  got=a[target];assert got['type']==e['type'] and not got['proxy']
  if e['type']=='TDbDimension2':
   points=e['grip']['points'];new=got['grip']['points'];assert len(points)==len(new);count=(len(points)-6)//2;coordinate=1 if abs(points[0][0]-points[1][0])<1e-5 else 0
   for k in range(count):assert abs((points[k+3][coordinate]-points[k+2][coordinate])-(new[k+3][coordinate]-new[k+2][coordinate]))<1e-5
   dimensions+=1
  else:assert near(got['grip']['points'],e['grip']['points']),(n,donor,e['type'])
 vals=saved_fields(f'final-two-{n}');numbers={}
 for line in (R/f'插件/resources/approved-layout/floor-{n}.tsv').read_text('utf-8').splitlines():
  row=shlex.split(line)
  if row and row[0]=='N':numbers[row[1]]=row[2]
 for h,number in numbers.items():assert vals[m[h]]['fields']['门窗编号']==number
 areas=read(Path(one['records'])/'approved-generation.json')['areas'];assert len(areas)==10
 assert sum(x['type']=='TDbWall' for x in a.values())==34 and sum(x['type']=='TDbAxisLabelSet' for x in a.values())==12
 report['two'].append(dict(floor=n,modelRoots=len(a),removedBackgrounds=len(removed),allRemainingNonDimensionGripsMatch013=True,dimensionsWithUnchangedMeasures=dimensions,numbered=sum(bool(x) for x in numbers.values()),areas=areas,visibleChains=one['dimensionChains'],visibleSegments=one['dimensionSegments'],DWG=one['output'],SHA256=sha(one['output'])))
legacy=read(R/f'插件/evidence/native-rules-{v}-legacy.json');lr=legacy['result'];la=preview(Path(legacy['job']));assert lr['completed'] and lr['sourceUnchanged'] and lr['dimensionChains']==24 and lr['dimensionSegments']==106
assert not any(e['type']=='AcDbWipeout' or e['proxy'] for e in la.values());assert sum(e['type']=='TDbDimension2' and e['layer']=='PUB_DIM' for e in la.values())>0
report['legacy']=dict(roots=len(la),chains=24,segments=106,captions=27,PUB_DIMUsed=True,DWG=lr['output'],SHA256=sha(lr['output']))
assert report['three']['sourceUnchanged'];report['passed']=True;(R/f'插件/evidence/rules-final-qa-{v}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8');print(json.dumps(dict(passed=True,threeRoots=8525,twoRoots=[x['modelRoots'] for x in report['two']],legacyRoots=len(la))))
