"""Independent WCS readback of the opt-in perimeter spacing sample."""
from pathlib import Path
import json, math, subprocess, sys

S = Path(__file__).resolve().parent
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def close(a,b): return abs(a-b)<1e-6
suffix='-v18' if '--baseline-v18' in sys.argv else ('-v22' if '-v22' in sys.argv else ('-v21' if '-v21' in sys.argv else ('-v20' if '-v20' in sys.argv else '-v19')))
n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json')
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
  assert close(radius,400) and close(outward,4600)
  assert close(outward-radius-3600,600) and close(outward+radius,5000)
  visible=axis['showPositive'] if i else axis['showNegative']
  ends.append(dict(label=axis['label'],side=('top' if i else 'bottom') if vertical else ('right' if i else 'left'),visible=visible,centreOffset=outward,totalLineToNearCircle=outward-radius-3600,outerCircleOffset=outward+radius))
 if not axis['nativeNestedInClipBlock']:
  obj=objects[axis['handle']];assert obj['type']=='TDbAxisLabelSet'
  grip=obj['grip']['points']
  # The circle centres and wall anchors are read anew from the saved DWG.
  for i in range(2):
   assert sum(math.dist(p,axis['circles'][i]['centre'])<1e-6 for p in grip)==1
   wall=(ymin if i==0 else ymax) if axis['vertical'] else (xmin if i==0 else xmax)
   anchorWall=wall+((-1000 if i==0 else 1000) if suffix=='-v20' else 0)
   anchor=[axis['station'],anchorWall,0] if axis['vertical'] else [anchorWall,axis['station'],0]
   assert sum(math.dist(p,anchor)<1e-6 for p in grip)==1
   model_axis_ends+=1
assert sum(x['visible'] for x in ends)==28 and model_axis_ends==(0 if suffix in ['-v21','-v22'] else 16)
if suffix in ['-v21','-v22']:
 for axis in a['axes']:
  assert axis['nativeNestedInClipBlock']
  index=1 if axis['vertical'] else 0;low=ymin if axis['vertical'] else xmin;high=ymax if axis['vertical'] else xmax
  if axis['showNegative'] and axis['showPositive']:assert axis['clipInverted'] and close(axis['clipMin'][index],low-1000) and close(axis['clipMax'][index],high+1000)
  elif axis['showNegative']:assert not axis['clipInverted'] and close(axis['clipMax'][index],low-1000)
  else:assert not axis['clipInverted'] and close(axis['clipMin'][index],high+1000)
chains={s:[] for s in ['bottom','top','left','right']};totals={}
for dim in a['dimensions']:
 record=next(x for x in scene['dimensions'] if x['handle']==dim['handle']);obj=record['geometry'][0];assert obj['type']=='TDbDimension2'
 assert dim['nativeNestedInGapBlock'] and record['gapClip'] and record['gapClip']['identityTransform']
 assert objects[record['gapClip']['container']]['type']=='AcDbBlockReference'
 assert not record['fixedExtensionLength'] and record['dimexo']*record['dimscale']<1000
 gp=obj['grip']['points'];side=dim['side'];vertical=side in ('left','right')
 offset=2800 if dim['row']=='axis' else 3600
 wall=dict(bottom=ymin,top=ymax,left=xmin,right=xmax)[side]
 baseline=wall+(-offset if side in ('bottom','left') else offset)
 clip=record['gapClip'];boundary=clip['max'][0 if vertical else 1] if side in ('bottom','left') else clip['min'][0 if vertical else 1]
 assert close(boundary,wall+(-1000 if side in ('bottom','left') else 1000))
 assert all(close(x,y) for x,y in zip(clip['min'],dim['gapClipMin'])) and all(close(x,y) for x,y in zip(clip['max'],dim['gapClipMax']))
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
guides=[([xmin,ymin-2000,0],[xmax,ymin-2000,0]),([xmin,ymax+2000,0],[xmax,ymax+2000,0]),([xmin-2000,ymin,0],[xmin-2000,ymax,0]),([xmax+2000,ymin,0],[xmax+2000,ymax,0])]
for start,end in guides:
 matches=[o for o in objects.values() if o['type']=='AcDbLine' and o['curve'] and math.dist(o['curve']['start'],start)<1e-6 and math.dist(o['curve']['end'],end)<1e-6]
 assert len(matches)==1,(start,end)
result=dict(contract='bcp.perimeter-spacing-trial-verification.v1',nearCircleGap=600,circleRadius=400,circleCentreOffset=4600,outerCircleOffset=5000,dimensionOffsets=[2000,2800,3600],witnessStart=1000,actualSavedGapFilters=28,nativeAxisCirclesVerified=40,visibleEnds=28,savedModelRootAxisEndsIndependentlyRead=model_axis_ends,savedDimensionEndpointsVerified=56,axisDimensions=24,bodyEnvelopeTotals=totals,innerGuides=4,threeRowsComplete=False,wholeBuildingTotalSemanticAcceptance=False,ends=ends,chains=chains)
out=S/f'evidence/confirmed-gap-layout{suffix}.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
print(json.dumps({k:val for k,val in result.items() if k not in ('ends','chains')},ensure_ascii=False))
