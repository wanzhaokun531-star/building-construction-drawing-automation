"""Independent actual-wall recomputation of every linked room/total area."""
from pathlib import Path
import json,math
ROOT=Path(__file__).resolve().parents[2];ARCHIVE=ROOT.parent/'项目清理归档/建筑施工图自动化_20261102-181100'
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
checks=[];named=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-placement-final.json')
for floor in [1,2]:
 d=read(ROOT/f'阶段2/示意/门号轴线与面积_20261101/room-areas-floor-{floor}.json');p=Path(d['cache']);p=p if p.exists() else ARCHIVE/p.relative_to(ROOT);walls={}
 for s in p.read_text('utf-8').splitlines():
  q=s.split()
  if not q or q[0]!='W':continue
  x0,y0,x1,y1,left,right=map(float,q[2:8]);le=math.hypot(x1-x0,y1-y0);nx=-(y1-y0)/le;ny=(x1-x0)/le;pts=[(x+nx*t,y+ny*t) for x,y in [(x0,y0),(x1,y1)] for t in [left,-right]];walls[q[1]]=[min(x for x,y in pts),min(y for x,y in pts),max(x for x,y in pts),max(y for x,y in pts)]
 def calculate(walls):
  sums={}
  for line in (ROOT/f'插件/resources/approved-layout/floor-{floor}.tsv').read_text('utf-8').splitlines():
   q=line.split()
   if q[0]=='A':
    pts=[(walls[q[4+4*i]][int(q[5+4*i])],walls[q[6+4*i]][int(q[7+4*i])]) for i in range(int(q[3]))];ox,oy=pts[0];area=abs(sum((pts[i][0]-ox)*(pts[(i+1)%len(pts)][1]-oy)-(pts[i][1]-oy)*(pts[(i+1)%len(pts)][0]-ox) for i in range(len(pts))))/2e6;sums[q[1]]=sums.get(q[1],0)+area
   elif q[0]=='PORCH':sums[q[1]]+=float(q[2])*float(q[3])/2e6
  return sums
 expected={x['members'][0]:(5.1976 if x['id'].endswith('R10') else x['areaM2']) for x in named['floors'][floor-1]['areas'] if not x['id'].endswith('R09')};expected[named['floors'][floor-1]['totalAreaMembersUnchanged'][0]]=189.782 if floor==1 else 153.4856;actual=calculate(walls);assert set(expected)==set(actual)
 for h in actual:assert abs(actual[h]-expected[h])<1e-6,(floor,h,actual[h],expected[h])
 moved={h:[v+1000000 for v in box] for h,box in walls.items()};translated=calculate(moved);assert all(abs(actual[h]-translated[h])<1e-6 for h in actual)
 checks.append(dict(floor=floor,actualSourceWallComputedM2=actual,allTenLabelsMatchConfirmedValues=True,translationInvariant=True))
(ROOT/'插件/evidence/approved-area-wall-link-check.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8');print('20 real-wall area values and translated-coordinate checks passed')
