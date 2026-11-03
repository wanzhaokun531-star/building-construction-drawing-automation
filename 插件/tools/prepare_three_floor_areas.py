"""Extract editable wall/column-linked area polygons, never cached area totals."""
from pathlib import Path
import json,sys,math
R=Path(__file__).resolve().parents[2];T=R/'阶段2/项目/百塘村邹总_20261103';OUT=R/'插件/resources/three-floor'
sys.path.insert(0,str(T/'_deps'))
from shapely.geometry import Polygon,box
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
b=read(R/'阶段2/evidence/native-host-binding-v191.json')
scene=read(Path(b['job'])/'preview-final-1/stage2-preview-native.json')['originalBefore']
roots={e['handle']:e for e in scene};data=read(T/'integer-plan.json');calcs=read(T/'area-calculation.json');areas=read(T/'areas-computed.json')
def mapper(n,a,value):
 m=data['maps'][n][a];best=min(range(len(m['old'])),key=lambda k:abs(m['old'][k]-value))
 if abs(m['old'][best]-value)<.01:return m['new'][best]
 # Interpolation mirrors the already accepted shared-station correction map.
 import numpy as np
 return float(np.interp(value,m['old'],m['new']))
records=[]
for n,f in enumerate(areas):
 mp=lambda p:(mapper(n,0,p[0]),mapper(n,1,p[1]))
 poly=lambda points:Polygon([mp(p) for p in points])
 walls={}
 for line in (T/f'floor-{n+1}.tsv').read_text().splitlines():
  s=line.split()
  if s and s[0]=='W':walls[s[1]]=list(map(float,s[2:6]))
 public=walls[['97','BE','E5'][n]];x0=min(public[0],public[2])+120;x1=max(public[0],public[2])-120;cut=public[1]
 dry=next(i for i,z in enumerate(f['regions']) if any(x['name']=='公卫' for x in z['labels']) and z['area']<3.6)
 top=Polygon(f['regions'][dry]['outline']).bounds[3]
 bottom=min(w[1] for w in walls.values() if abs(w[1]-w[3])<1e-5 and cut-2500<w[1]<cut-500 and abs(w[2]-w[0])>600)+120
 wet=poly(list(box(x0,bottom,x1,cut-60).exterior.coords)[:-1])
 door=roots[['E29','1357','18E1'][n]]['grip']['points'];doorwidth=math.dist(door[1],door[2])
 xx0,yy0=mp((x0,bottom));xx1,yy1=mp((x1,top));yc=mapper(n,1,cut)
 bath=box(xx0,yy0,xx1,yy1).difference(box(xx0,yc-60,xx1-doorwidth,yc+60))
 offset=[0,42887.596250418486,86035.314872116665][n]
 shaft=[w for w in walls.values() if abs(w[0]-w[2])<1e-5 and offset+1000<w[0]<offset+4000 and w[1]>8000 and w[3]>8000 and 2000<abs(w[3]-w[1])<3000]
 sx0=min(w[0] for w in shaft)+120;sx1=max(w[0] for w in shaft)-120;sy0=min(min(w[1],w[3]) for w in shaft)+120;sy1=max(max(w[1],w[3]) for w in shaft)-120
 shaftpoly=poly(list(box(sx0,sy0,sx1,sy1).exterior.coords)[:-1])
 for i,z in enumerate(f['regions']):
  geom=Polygon([mp(p) for p in z['outline']],[[mp(p) for p in hole] for hole in z.get('holes',[])])
  if i==dry:geom=bath
  if any(x['name']=='楼梯' for x in z['labels']):geom=geom.difference(shaftpoly).difference(wet)
  records.append((n,False,[(1,geom)]))
 body=poly(read(T/f'analysis-{n+1}.json')['wallOuterBodyContours'][0]);bounds=next(a['platformBounds'] for a in calcs if a['floor']==n+1 and 'total' in a)
 platform=poly(list(box(*bounds).exterior.coords)[:-1]).difference(body)
 records.append((n,True,[(1,body),(.5,platform)]))
# Match the accepted numeric source associations; polygon evaluation is independently checked.
actions=[s.split() for s in (T/'integer-actions.tsv').read_text().splitlines() if s.startswith('A ')]
assert len(actions)==len(records)==23
remaining=list(records);rows=['BCPAREAPOLY1'];summary=[]
refs={h:e for h,e in roots.items() if e['type']=='TDbWall' or h in data['columnMembers']}
def vertex(p):
 out=[]
 for a,v in enumerate(p[:2]):
  diff,h,edge=min((abs(v-e[k][a]),h,a+(0 if k=='min' else 3)) for h,e in refs.items() for k in ['min','max'])
  value=refs[h]['min' if edge<3 else 'max'][a]
  out += [h,str(edge),format(v-value,'.17g')]
 return ' '.join(out)
for _,h,value,isTotal in actions:
 candidates=[(i,v) for i,v in enumerate(remaining) if v[1]==bool(int(isTotal)) and abs(sum(k*g.area for k,g in v[2])/1e6-float(value))<1e-6]
 # Same room area on different floors is disambiguated by its existing text position.
 x=roots[h]['min'][0];n=min(range(3),key=lambda k:abs(x-[0,42887.596250418486,86035.314872116665][k]-6500))
 candidates=[v for v in candidates if v[1][0]==n];assert len(candidates)==1,(h,value,candidates)
 i,(_,total,parts)=candidates[0];remaining.pop(i)
 for coefficient,geometry in parts:
  polygons=list(geometry.geoms) if geometry.geom_type=='MultiPolygon' else [geometry]
  for polygon in polygons:
   for sign,ring in [(1,polygon.exterior)]+[(-1,hole) for hole in polygon.interiors]:
    points=list(ring.coords)[:-1];rows.append('POLY '+h+' '+str(int(total))+' '+str(coefficient*sign)+' '+str(len(points))+' '+' '.join(vertex(p) for p in points))
 summary.append(dict(text=h,area=float(value),total=total,floor=n+1))
rows.append('END');(OUT/'areas.tsv').write_text('\n'.join(rows)+'\n','utf-8');(OUT/'areas-extraction.json').write_text(json.dumps(summary,indent=2),'utf-8')
print('wall/column-linked areas',len(summary),'polygon rows',len(rows)-2)
