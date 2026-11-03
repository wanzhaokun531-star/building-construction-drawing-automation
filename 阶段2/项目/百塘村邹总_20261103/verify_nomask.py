from pathlib import Path
import json,math,sys,hashlib
R=Path(__file__).resolve().parents[3];T=Path(__file__).parent
def read(p):return json.loads(p.read_text('utf-8-sig'))
b=read(R/f'阶段2/evidence/native-host-binding-{sys.argv[1]}.json');j=Path(b['job']);report=read(j/'tidy.done')
previous=read(R/'阶段1/runs/stage2-product-20261103-110641/preview-final-1/stage2-preview-native.json')['originalBefore']
actual=read(j/'preview-final-1/stage2-preview-native.json')['originalBefore'];by_handle={e['handle']:e for e in actual}
assert not any(e['type']=='AcDbWipeout' for e in actual)
assert len(previous)-len(actual)==report['removedWipeouts']==12
changes=[]
for e in previous:
 if e['type']=='AcDbWipeout':assert e['handle'] not in by_handle;continue
 f=by_handle[e['handle']];assert e['type']==f['type'] and e['layer']==f['layer']
 old=e['grip']['points'];new=f['grip']['points'];assert len(old)==len(new)
 limit=len(old)-3 if e['type']=='TDbDimension2' else len(old)
 for k in range(limit):
  n=(len(old)-6)//2
  if e['type']=='TDbDimension2' and 2<=k<=n+2:
   vertical=abs(old[0][0]-old[1][0])<1e-5;along=1 if vertical else 0
   assert abs(old[k][along]-new[k][along])<1e-5,e['handle']
  else:assert math.dist(old[k],new[k])<1e-5,e['handle']
 if any(math.dist(x,y)>1e-5 for x,y in zip(old,new)):changes.append(e['handle'])
for d in report['dimensionChecks']:
 f=by_handle[d['handle']];assert all(math.dist(x,y)<1e-5 for x,y in zip(d['grips'],f['grip']['points']))
assert len(changes)==report['adjustedExteriorChains']
assert report['protectedGeometryIdentical'] and report['savedReopened']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert sha(Path(b['input']))==b['inputSHA256']
assert sha(j/'百塘村邹总_三层平面_取消区域覆盖_v2.dwg')==report['outputSHA256']
result=dict(wipeoutCount=0,removed=12,rootCount=len(actual),adjustedExteriorChains=len(changes),allOtherRootGripsUnchanged=True,dimensionAlongWallStationsTextAndBaselineUnchanged=True,savedPublicGripReadbackMatches=True,previousDeliveryPreserved=True)
(T/f'qa-nomask-{sys.argv[1]}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(result)
