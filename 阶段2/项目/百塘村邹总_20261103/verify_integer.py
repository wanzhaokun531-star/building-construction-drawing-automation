from pathlib import Path
import json,math,sys,hashlib,collections
T=Path(__file__).parent;R=T.parents[2]
read=lambda p:json.loads(p.read_text('utf-8-sig'))
b=read(R/f'阶段2/evidence/native-host-binding-{sys.argv[1]}.json');j=Path(b['job']);report=read(j/'tidy.done');plan=read(T/'integer-plan.json')
old={e['handle']:e for e in read(R/'阶段1/runs/stage2-product-20261103-112612/preview-final-1/stage2-preview-native.json')['originalBefore']}
new={e['handle']:e for e in read(j/'preview-final-1/stage2-preview-native.json')['originalBefore']}
assert set(old)==set(new);assert all(e['type']!='AcDbWipeout' for e in new.values())
near=lambda a,b:math.dist(a,b)<1e-5
actions=[s.split() for s in (T/'integer-actions.tsv').read_text().splitlines()];changed={s[1] for s in actions};walls=0;openings=0
for h,e in new.items():
 assert (e['type'],e['layer'],e['proxy'])==(old[h]['type'],old[h]['layer'],old[h]['proxy'])
 if h not in changed:assert all(near(a,b) for a,b in zip(e['grip']['points'],old[h]['grip']['points'])),h
for a in actions:
 typ,h=a[:2];p=new[h]['grip']['points'];o=old[h]['grip']['points'];v=list(map(float,a[2:]))
 if typ=='W':
  assert near(p[0],v[:3]) and near(p[1],v[3:]);normal=0 if abs(o[0][0]-o[1][0])<1e-5 else 1
  width=lambda e:e['max'][normal]-e['min'][normal]
  assert abs(width(new[h])-width(old[h]))<1e-4,(h,'wall width')
  assert abs(math.dist(p[0],p[1])-round(math.dist(p[0],p[1])))<1e-5;walls+=1
 if typ=='T':
  for x,y in zip(o,p):assert near([x[k]+v[k] for k in range(3)],y),h
  if new[h]['type'] in ['TDbOpening','TDbGroupOpening']:openings+=1
 if typ=='G':assert near(p[int(v[0])],v[1:]),h
 if typ=='S':
  c,q,sx,sy=v[:3],v[3:6],v[6],v[7]
  for x,y in zip(o,p):assert near([q[0]+sx*(x[0]-c[0]),q[1]+sy*(x[1]-c[1]),x[2]],y),h
actual={d['handle']:d for d in report['dimensionChecks']}
for d in plan['dimensions']:
 a=actual[d['handle']];assert len(a['values'])==len(d['values']);assert all(abs(x-y)<1e-5 for x,y in zip(a['values'],d['values']))
 assert all(near(x,y) for x,y in zip(a['grips'],new[d['handle']]['grip']['points']))
 assert abs(sum(a['values'])-math.dist(a['grips'][2],a['grips'][len(a['values'])+2]))<1e-5
assert not any(m['rejected'] for row in plan['maps'] for m in row)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert sha(Path(b['input']))==b['inputSHA256'];assert sha(j/'百塘村邹总_三层平面_尺寸归整_v4.dwg')==report['outputSHA256']
result=dict(walls=walls,openingGeometryPreservedExceptTranslation=openings,wholeDimensions=len(actual),integerSegments=report['integerSegments'],allDimensionChainsClosed=True,wallThicknessPreserved=True,noWipeout=True,unchangedOtherRootGrips=True,savedReadbackMatches=True,sourcePreserved=True,areas=plan['areas'])
(T/f'qa-integer-{sys.argv[1]}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print({k:v for k,v in result.items() if k!='areas'})
