"""Use actual public strokes and actual native point returns for direction checks."""
import json,math,sys
from pathlib import Path

FIXTURES={
 'slope': [[[50000,20000,0],[52000,20000,0]],[[52000,24000,0],[50000,24000,0]]],
 'section': [[[48000,26000,0],[52000,26000,0],[50000,27000,0]],[[48000,31000,0],[52000,31000,0],[50000,30000,0]]],
 'pointer-index': [[[48000,36000,0],[49500,36500,0],[51000,36500,0]],[[51000,40000,0],[49500,40500,0],[48000,40500,0]]],
 'section-index': [[[48000,45000,0],[49500,45500,0],[51000,45500,0],[48500,46000,0]],[[51000,49000,0],[49500,49500,0],[48000,49500,0],[50500,48000,0]]],
}

def near(a,b):return isinstance(a,list) and isinstance(b,list) and len(a)==len(b)==3 and math.dist(a,b)<1e-6
def vertices(node):
 if node.get('kind')=='line':return [node['start'],node['end']]
 return node.get('vertices',[])

def inspect(d):
 checks=[]
 def check(ok,name):checks.append((bool(ok),name))
 kind=d.get('kind');check(kind in FIXTURES,'known independent geometric fixture')
 symbols=d.get('symbols',[]);check(len(symbols)==2,'two actual opposite-direction samples')
 creates=[o for o in d.get('operations',[]) if o.get('mode')=='create']
 check(len(creates)==2,'two actual public creation commands')
 for i,s in enumerate(symbols[:2]):
  wanted=FIXTURES.get(kind,[[],[]])[i]
  if i<len(creates):
   actual=[e.get('pointOut') for e in creates[i].get('inputTrace',[]) if e.get('event')=='endGetPoint' and e.get('returnStatus')==5100]
   check(len(actual)==len(wanted) and all(near(a,b) for a,b in zip(actual,wanted)),f'symbol{i} actual WCS point returns equal independent fixture')
   env=[e for e in creates[i].get('events',[]) if e.get('event')=='point-environment-restored']
   if env:
    rows=env[0].get('variables',[])
    check(len(env)==1 and env[0].get('restored') is True and {r.get('name'):r.get('applied') for r in rows}=={'SNAPMODE':0,'OSMODE':0,'ORTHOMODE':0,'OSNAPCOORD':1} and all(r.get('restored') is True and r.get('before')==r.get('after') for r in rows),f'symbol{i} actual native point environment applied then restored')
  for stage in ('liveReadback','savedReadback'):
   nodes=(s.get(stage) or {}).get('nodes',[]);polygons=[vertices(n) for n in nodes if n.get('kind') in ('polyline','solid')];strokes=[]
   for n in nodes:
    pts=vertices(n);strokes.extend(zip(pts,pts[1:]))
   p=wanted;prefix=f'symbol{i}/{stage}'
   if kind=='section':
    sign=1 if p[2][1]>p[0][1] else -1
    for end in p[:2]:
     tick=False
     for a,b in strokes:
      if near(a,end):other=b
      elif near(b,end):other=a
      else:continue
      if abs(other[0]-end[0])<1e-6 and (other[1]-end[1])*sign>1e-6:tick=True
     check(tick,prefix+' actual end/view tick matches selected direction at '+str(end[:2]))
   elif kind=='slope':
    tip,tail=p;ux=tail[0]-tip[0];uy=tail[1]-tip[1]
    head=False
    for pts in polygons:
     if len(pts)<3 or not any(near(q,tip) for q in pts):continue
     crosses=[ux*(q[1]-tip[1])-uy*(q[0]-tip[0]) for q in pts]
     forward=[ux*(q[0]-tip[0])+uy*(q[1]-tip[1]) for q in pts]
     if max(crosses)-min(crosses)>1e-6 and min(forward)>=-1e-6:head=True
    # A native TArrow can draw its filled triangular head as a tapered-width
    # polyline segment. Vertex noncollinearity alone would miss real geometry.
    for n in nodes:
     if n.get('kind')!='polyline':continue
     pts=n.get('vertices',[]);widths=n.get('segmentWidths',[])
     for j,(a,b) in enumerate(zip(pts,pts[1:])):
      if j>=len(widths) or not near(a,tip):continue
      w=widths[j];dot=ux*(b[0]-tip[0])+uy*(b[1]-tip[1]);cross=ux*(b[1]-tip[1])-uy*(b[0]-tip[0])
      if abs(w.get('start',-1))<1e-6 and w.get('end',0)>1e-6 and abs(w.get('bulge',1))<1e-6 and dot>0 and abs(cross)<1e-6:head=True
    check(head,prefix+' actual polygon or tapered-width head at requested tip points away from tail')
    grips=s.get('geometry',{}).get('grip',{}).get('points',[])
    check(any(near(q,tail) for q in grips) or any(near(q,tail) for edge in strokes for q in edge),prefix+' tail anchor present in actual geometry')
   else:
    allpoints=[q for edge in strokes for q in edge]
    check(any(near(q,p[0]) for q in allpoints),prefix+' actual leader starts at indexed location')
    check(any(near(q,p[1]) for q in allpoints),prefix+' actual elbow matches selected location')
    circles=[n for n in nodes if n.get('kind')=='circle' and n.get('radius',0)>0]
    check(bool(circles),prefix+' actual index circle')
    check(any(math.dist(c['center'],p[2])<=2*c['radius']+1e-6 for c in circles),prefix+' index circle bound to selected label position')
    if kind=='section-index':
     dx=p[1][0]-p[0][0];dy=p[1][1]-p[0][1];view=dx*(p[3][1]-p[0][1])-dy*(p[3][0]-p[0][0]);ticks=[]
     for n in nodes:
      pts=vertices(n)
      if n.get('kind')!='polyline' or len(pts)!=2 or n.get('closed'):continue
      a,b=pts;parallel=dx*(b[1]-a[1])-dy*(b[0]-a[0]);side=dx*(a[1]-p[0][1])-dy*(a[0]-p[0][0])
      if math.dist(a,b)>1e-6 and abs(parallel)<1e-4 and side*view<0:ticks.append(n)
     check(bool(ticks),prefix+' actual cutting stroke offset follows requested view side')
 return checks

if __name__=='__main__':
 d=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(d)
 for ok,name in checks:
  if not ok:print('FAIL:',name)
 failed=sum(not ok for ok,_ in checks)
 print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_ACTUAL_PUBLIC_GEOMETRY_AND_WCS_INPUTS"}')
 raise SystemExit(bool(failed))
