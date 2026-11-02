"""Independent WCS readback of the opt-in perimeter spacing sample."""
from pathlib import Path
import json, math, subprocess

S = Path(__file__).resolve().parent
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def close(a,b): return abs(a-b)<1e-6
n=read(S/'evidence/native-run-v18.json');v=read(S/'evidence/native-preview-run-v18.json')
a=read(Path(n['output'])/'native-annotations.json')
analysis=read(Path(n['output'])/'analysis.json')
scene=read(Path(v['job'])/'stage2-preview-native.json')
objects={x['handle']:x for x in scene['originalBefore']}
points=analysis['wallOuterBodyContours'][0]
xmin=min(x[0] for x in points);xmax=max(x[0] for x in points)
ymin=min(x[1] for x in points);ymax=max(x[1] for x in points)
assert a['spacingTrial'] and len(a['axes'])==20 and len(a['dimensions'])==28
ends=[];model_axis_ends=0
for axis in a['axes']:
 for i,circle in enumerate(axis['circles']):
  vertical=axis['vertical'];sign=1 if i else -1
  wall=(ymax if i else ymin) if vertical else (xmax if i else xmin)
  across=circle['centre'][1 if vertical else 0]
  outward=sign*(across-wall);radius=circle['radius']
  assert close(radius,400) and close(outward,3600)
  assert close(outward-radius-2600,600) and close(outward+radius,4000)
  visible=axis['showPositive'] if i else axis['showNegative']
  ends.append(dict(label=axis['label'],side=('top' if i else 'bottom') if vertical else ('right' if i else 'left'),visible=visible,centreOffset=outward,totalLineToNearCircle=outward-radius-2600,outerCircleOffset=outward+radius))
 if not axis['nativeNestedInClipBlock']:
  obj=objects[axis['handle']];assert obj['type']=='TDbAxisLabelSet'
  grip=obj['grip']['points']
  # The circle centres and wall anchors are read anew from the saved DWG.
  for i in range(2):
   assert sum(math.dist(p,axis['circles'][i]['centre'])<1e-6 for p in grip)==1
   wall=(ymin if i==0 else ymax) if axis['vertical'] else (xmin if i==0 else xmax)
   anchor=[axis['station'],wall,0] if axis['vertical'] else [wall,axis['station'],0]
   assert sum(math.dist(p,anchor)<1e-6 for p in grip)==1
   model_axis_ends+=1
assert sum(x['visible'] for x in ends)==28 and model_axis_ends==16
chains={s:[] for s in ['bottom','top','left','right']};totals={}
for dim in a['dimensions']:
 obj=objects[dim['handle']];assert obj['type']=='TDbDimension2'
 gp=obj['grip']['points'];side=dim['side'];vertical=side in ('left','right')
 offset=1800 if dim['row']=='axis' else 2600
 wall=dict(bottom=ymin,top=ymax,left=xmin,right=xmax)[side]
 baseline=wall+(-offset if side in ('bottom','left') else offset)
 assert close(gp[0][0 if vertical else 1],baseline)
 expected=([wall,dim['axisFrom'],0],[wall,dim['axisTo'],0]) if vertical else ([dim['axisFrom'],wall,0],[dim['axisTo'],wall,0])
 assert all(math.dist(gp[i+2],p)<1e-6 for i,p in enumerate(expected))
 assert close(math.dist(gp[2],gp[3]),dim['expected'])
 if dim['row']=='axis':chains[side].append(dim['expected'])
 else:
  assert dim['row']=='total-body-envelope'
  assert close(dim['axisFrom'],ymin if vertical else xmin) and close(dim['axisTo'],ymax if vertical else xmax)
  totals[side]=dim['expected']
assert [len(chains[s]) for s in ['top','bottom','left','right']]==[5,6,7,6]
assert all(close(x,y) for x,y in zip(chains['left'],[4000,1800,1800,3600,3000,1200,2800]))
assert len(totals)==4
guides=[([xmin,ymin-1000,0],[xmax,ymin-1000,0]),([xmin,ymax+1000,0],[xmax,ymax+1000,0]),([xmin-1000,ymin,0],[xmin-1000,ymax,0]),([xmax+1000,ymin,0],[xmax+1000,ymax,0])]
for start,end in guides:
 matches=[o for o in objects.values() if o['type']=='AcDbLine' and o['curve'] and math.dist(o['curve']['start'],start)<1e-6 and math.dist(o['curve']['end'],end)<1e-6]
 assert len(matches)==1,(start,end)
result=dict(contract='bcp.perimeter-spacing-trial-verification.v1',nearCircleGap=600,circleRadius=400,circleCentreOffset=3600,outerCircleOffset=4000,dimensionOffsets=[1000,1800,2600],nativeAxisCirclesVerified=40,visibleEnds=28,savedModelRootAxisEndsIndependentlyRead=16,savedDimensionEndpointsVerified=56,axisDimensions=24,bodyEnvelopeTotals=totals,innerGuides=4,threeRowsComplete=False,wholeBuildingTotalSemanticAcceptance=False,ends=ends,chains=chains)
out=S/'evidence/perimeter-spacing-trial-v18.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/'evidence/native-preview-v18-page')],check=True)
print(json.dumps({k:val for k,val in result.items() if k not in ('ends','chains')},ensure_ascii=False))
