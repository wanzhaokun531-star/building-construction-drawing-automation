"""Project-only native DWG edit contract; never overrides dimension text."""
from pathlib import Path
import json, math, bisect, sys
import numpy as np
T=Path(__file__).parent; R=T.parents[2]
sys.path.insert(0,str(T/'_deps'))
from shapely.geometry import Polygon,box

scene=json.loads((R/'阶段1/runs/stage2-product-20261103-112612/preview-final-1/stage2-preview-native.json').read_text('utf-8'))['originalBefore']
by={e['handle']:e for e in scene}; offsets=[0,42887.596250418486,86035.314872116665]
def floor(x):return min(range(3),key=lambda k:abs(x-offsets[k]-6500))
def gp(e):return e['grip']['points']
walls=[[] for _ in offsets]; openings=[[] for _ in offsets]
for n in range(3):
 for line in (T/f'floor-{n+1}.tsv').read_text('utf-8').splitlines():
  s=line.split()
  if s[0]=='W':walls[n].append((s[1],list(map(float,s[2:8]))))
  if s[0]=='O':
   h=s[1];a=gp(by[h]); pts=(a[0],a[6]) if by[h]['type']=='TDbGroupOpening' else (a[1],a[2])
   openings[n].append((h,pts,float(s[6]),float(s[7])))

class AxisMap:
 def __init__(self,n,axis):
  self.n=n;self.axis=axis;self.values=[];self.fixed=[];self.soft=[];self.rejected=[]
 def add(self,v):self.values.append(float(v))
 def edge(self,a,b,d,label,hard=False):
  self.add(a);self.add(b);(self.fixed if hard else self.soft).append((a,b,d,label))
 def solve(self):
  self.x=[]
  for v in sorted(self.values):
   if not self.x or v-self.x[-1]>1e-4:self.x.append(v)
  def idx(v):return min(range(len(self.x)),key=lambda k:abs(self.x[k]-v))
  self.idx=idx;size=len(self.x);parent=list(range(size));delta=[0]*size
  def find(i):
   if parent[i]!=i:
    p=parent[i];parent[i]=find(p);delta[i]+=delta[p]
   return parent[i]
  def link(a,b,d):
   i,j=idx(a),idx(b);u,v=find(i),find(j)
   if u==v:return abs(delta[j]-delta[i]-d)<1e-6
   parent[v]=u;delta[v]=d+delta[i]-delta[j];return True
  for a,b,d,label in self.fixed:assert link(a,b,d),('fixed constraint',label,a,b,d)
  # Prefer the smallest corrections; geometry closure takes priority over incompatible snaps.
  for a,b,d,label in sorted(self.soft,key=lambda e:abs((e[1]-e[0])-e[2])):
   if not link(a,b,d):self.rejected.append(dict(label=label,old=b-a,wanted=d))
  for a,b in zip(self.x,self.x[1:]):
   if b-a<1:link(a,b,0)
  groups={}
  for i in range(size):groups.setdefault(find(i),[]).append(i)
  origin=min(min(w[1][self.axis],w[1][self.axis+2]) for w in walls[self.n]) if self.axis==0 else 0.0
  self.y=[0.0]*size
  for members in groups.values():
   shifts=[self.x[i]-origin-delta[i] for i in members]
   k=round(float(np.mean(shifts)))
   zero=[i for i in members if abs(self.x[i]-origin)<1e-5]
   if zero:k=-delta[zero[0]]
   for i in members:self.y[i]=origin+k+delta[i]
  assert all(b>=a-2 for a,b in zip(self.y,self.y[1:])),('nonmonotone',self.n,self.axis,[(self.x[i],self.x[i+1],a,b) for i,(a,b) in enumerate(zip(self.y,self.y[1:])) if b<a-1e-6])
  assert max(abs(a-b) for a,b in zip(self.x,self.y))<=10.01,('movement limit',self.n,self.axis)
 def __call__(self,v):
  k=bisect.bisect_left(self.x,v)
  if k==0:return v+self.y[0]-self.x[0]
  if k==len(self.x):return v+self.y[-1]-self.x[-1]
  if abs(v-self.x[k])<.005:return self.y[k]
  if abs(v-self.x[k-1])<.005:return self.y[k-1]
  f=(v-self.x[k-1])/(self.x[k]-self.x[k-1]);return self.y[k-1]+f*(self.y[k]-self.y[k-1])

maps=[[AxisMap(n,a) for a in range(2)] for n in range(3)]
def snapedge(m,a,b,label):
 d=b-a;target=round(d/100)*100
 if abs(d)>=50 and abs(d-target)<=10:m.edge(a,b,target,label)
 else:m.add(a);m.add(b)
for n in range(3):
 for h,w in walls[n]:
  x,y,u,v,l,r=w;vertical=abs(x-u)<1e-5;along=1 if vertical else 0;normal=1-along
  for a in [0,1]:maps[n][a].add(w[a]);maps[n][a].add(w[a+2])
  snapedge(maps[n][along],w[along],w[along+2],'wall:'+h)
  c=w[normal];maps[n][normal].edge(c,c-l,-l,'wall-face:'+h,True);maps[n][normal].edge(c,c+r,r,'wall-face:'+h,True)
 for h,(a,b),width,height in openings[n]:
  along=0 if abs(a[1]-b[1])<1e-4 else 1
  assert abs(abs(b[along]-a[along])-width)<1e-4
  maps[n][along].edge(a[along],b[along],round(b[along]-a[along]),'opening-width:'+h,True)
  maps[n][1-along].add(a[1-along])
 for line in (T/f'columns-{n+1}.tsv').read_text().splitlines():
  b=list(map(float,line.split()))
  for a in [0,1]:
   c=(b[a]+b[a+2])/2;half=round((b[a+2]-b[a])/100)*50 if abs((b[a+2]-b[a])-round((b[a+2]-b[a])/100)*100)<=10 else round((b[a+2]-b[a])/2)
   maps[n][a].edge(c,b[a],-half,'column-half',True);maps[n][a].edge(c,b[a+2],half,'column-half',True)
for e in scene:
 p=gp(e)
 if e['type']=='TDbDimension2':
  a=1 if abs(p[0][0]-p[1][0])<1e-5 else 0;n=floor((p[0][0]+p[1][0])/2);count=(len(p)-6)//2
  for k in range(2,count+2):snapedge(maps[n][a],p[k][a],p[k+1][a],'dimension:'+e['handle'])
 if e['type']=='TDbAxisLabelSet':
  a=0 if abs(p[1][0]-p[0][0])<1 else 1;n=floor((e['min'][0]+e['max'][0])/2);maps[n][a].add(p[1][a])
for row in maps:
 for m in row:m.solve()

def point(n,p):return [maps[n][0](p[0]),maps[n][1](p[1]),p[2] if len(p)>2 else 0]
actions=[];dim_values=[]
for e in scene:
 p=gp(e)
 if not p:continue
 n=floor((e['min'][0]+e['max'][0])/2);h=e['handle'];typ=e['type']
 if typ=='TDbWall':actions.append(['W',h,*point(n,p[0]),*point(n,p[1])])
 elif typ in ['TDbOpening','TDbGroupOpening']:
  target=next(o for o in openings[n] if o[0]==h);a,b=target[1];ma,mb=point(n,a),point(n,b)
  shift=[(ma[k]+mb[k]-a[k]-b[k])/2 for k in range(3)];shift[2]=0
  assert all(abs(ma[k]-a[k]-shift[k])<1e-5 and abs(mb[k]-b[k]-shift[k])<1e-5 for k in [0,1]),h
  actions.append(['T',h,*shift])
 elif typ=='TDbDimension2':
  count=(len(p)-6)//2;a=1 if abs(p[0][0]-p[1][0])<1e-5 else 0
  vals=[]
  for k in range(2,count+3):
   q=p[k].copy();q[a]=maps[n][a](q[a]);actions.append(['G',h,k,*q])
   if k>2:vals.append(abs(q[a]-maps[n][a](p[k-1][a])))
  assert all(abs(v-round(v))<1e-5 and v>0 for v in vals),(h,vals)
  # Baseline and displaced text retain the approved normal offset. Text follows its own interval.
  for k in range(count):
   q=p[count+3+k].copy();shift=(maps[n][a](p[2+k][a])+maps[n][a](p[3+k][a])-p[2+k][a]-p[3+k][a])/2;q[a]+=shift
   actions.append(['G',h,count+3+k,*q])
  dim_values.append(dict(handle=h,values=vals))
 elif typ=='TDbAxisLabelSet':
  a=0 if abs(p[1][0]-p[0][0])<1 else 1;shift=[0,0,0];shift[a]=maps[n][a](p[1][a])-p[1][a];actions.append(['T',h,*shift])
 elif typ=='AcDbLine' and e['layer'] in ['A-DOTE','A-AXIS']:
  # Only transverse axis position changes; existing end connections and circle rows remain.
  a=0 if abs(p[0][0]-p[1][0])<1e-4 else 1
  for k in [0,1]:
   q=p[k].copy();q[a]=maps[n][a](q[a]);actions.append(['G',h,k,*q])

# Porch/balcony columns are source PDF vectors, not native TCH columns.
# Move/resize their actual outlines and fills with their independent column axes.
column_members=[]
for n in range(3):
 for line in (T/f'columns-{n+1}.tsv').read_text().splitlines():
  b=list(map(float,line.split()));c=[(b[0]+b[2])/2,(b[1]+b[3])/2,0];q=point(n,c)
  target=[maps[n][0](b[0]),maps[n][1](b[1]),maps[n][0](b[2]),maps[n][1](b[3])]
  sx=(target[2]-target[0])/(b[2]-b[0]);sy=(target[3]-target[1])/(b[3]-b[1])
  for e in scene:
   if e['type'] not in ['AcDbPolyline','AcDbHatch']:continue
   lo,hi=e['min'],e['max']
   if lo[0]>=b[0]-.02 and hi[0]<=b[2]+.02 and lo[1]>=b[1]-.02 and hi[1]<=b[3]+.02:
    actions.append(['S',e['handle'],*c,*q,sx,sy]);column_members.append(e['handle'])

# Exact public outline polygons retained from the established area calculation.
areas=json.loads((T/'areas-computed.json').read_text('utf-8'));calcs=json.loads((T/'area-calculation.json').read_text('utf-8'))
records=[];labels=[]
for n,f in enumerate(areas):
 mx,my=maps[n];poly=lambda pts:Polygon([(mx(x),my(y)) for x,y in pts])
 original_regions=[Polygon(v['outline'],v.get('holes',[])) for v in f['regions']];new_regions=[Polygon([(mx(x),my(y)) for x,y in v['outline']], [[(mx(x),my(y)) for x,y in hole] for hole in v.get('holes',[])]) for v in f['regions']]
 cache={h:w for h,w in walls[n]};public=cache[['97','BE','E5'][n]];x0=min(public[0],public[2])+120;x1=max(public[0],public[2])-120;cut=public[1]
 dry_index=next(i for i,z in enumerate(f['regions']) if any(x['name']=='公卫' for x in z['labels']) and z['area']<3.6)
 top=original_regions[dry_index].bounds[3]
 bottom=min(w[1] for h,w in walls[n] if abs(w[1]-w[3])<1e-5 and cut-2500<w[1]<cut-500 and abs(w[2]-w[0])>600)+120
 wet_old=box(x0,bottom,x1,cut-60);wet_new=box(mx(x0),my(bottom),mx(x1),my(cut-60))
 door=next(o for o in openings[n] if o[0]==['E29','1357','18E1'][n]);door_width=door[2]
 bath_old=(x1-x0)*(top-bottom)-120*((x1-x0)-door_width)
 bath_new=(mx(x1)-mx(x0))*(my(top)-my(bottom))-120*((mx(x1)-mx(x0))-door_width)
 # Elevator shaft clear rectangle, bounded by its three actual native walls.
 shaftwalls=[w for h,w in walls[n] if abs(w[0]-w[2])<1e-5 and offsets[n]+1000<w[0]<offsets[n]+4000 and w[1]>8000 and w[3]>8000 and 2000<abs(w[3]-w[1])<3000]
 sx0=min(w[0] for w in shaftwalls)+120;sx1=max(w[0] for w in shaftwalls)-120;sy0=min(min(w[1],w[3]) for w in shaftwalls)+120;sy1=max(max(w[1],w[3]) for w in shaftwalls)-120
 shaft_old=box(sx0,sy0,sx1,sy1);shaft_new=box(mx(sx0),my(sy0),mx(sx1),my(sy1))
 for i,z in enumerate(f['regions']):
  is_living=any(x['name']=='楼梯' for x in z['labels']);old=original_regions[i].area;new=new_regions[i].area
  if i==dry_index:old=bath_old;new=bath_new
  if is_living:old-=shaft_old.area+wet_old.area;new-=shaft_new.area+wet_new.area
  labels.append((n,old/1e6,new/1e6));records.append(dict(floor=n+1,old=old/1e6,new=new/1e6,kind='room'))
 # Building outline derived from the established actual native wall-face envelope (not porch bbox).
 analysis=json.loads((T/f'analysis-{n+1}.json').read_text('utf-8'));outline=analysis['wallOuterBodyContours'][0];body_old=Polygon(outline);body_new=poly(outline)
 oldtotal=next(a for a in calcs if a['floor']==n+1 and 'total' in a);b=oldtotal['platformBounds'];platform_old=box(*b).difference(body_old);platform_new=box(mx(b[0]),my(b[1]),mx(b[2]),my(b[3])).difference(body_new)
 before=(body_old.area+platform_old.area/2)/1e6;after=(body_new.area+platform_new.area/2)/1e6
 assert abs(before-oldtotal['total'])<.005,(n,before,oldtotal['total'])
 labels.append((n,before,after));records.append(dict(floor=n+1,old=before,new=after,kind='total'))
# Area text bodies were appended in area-labels.tsv order, as direct Text paired with superscript2.
body=[e for e in scene if e['type']=='AcDbText' and int(e['handle'],16)>=int('3FBB',16)][:46:2]
rows=[list(map(float,s.split())) for s in (T/'area-labels.tsv').read_text().splitlines()]
assert len(body)==len(rows)==23
for e,row in zip(body,rows):
 n=abs(int(row[0]))-1;old=row[3];candidates=[a for a in labels if a[0]==n and abs(a[1]-old)<.02]
 assert len(candidates)==1,(e['handle'],old,candidates,labels)
 actions.append(['A',e['handle'],candidates[0][2],int(row[0]<0)])

(T/'integer-actions.tsv').write_text('\n'.join(' '.join(format(x,'.17g') if isinstance(x,float) else str(x) for x in row) for row in actions)+'\n',encoding='ascii')
report=dict(maps=[[dict(old=m.x,new=m.y,rejected=m.rejected) for m in row] for row in maps],dimensions=dim_values,areas=records,columnMembers=column_members,actionCount=len(actions))
(T/'integer-plan.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('Actions',len(actions),'dimensions',len(dim_values),'areas',len(records),'rejected',sum(len(m.rejected) for row in maps for m in row))
