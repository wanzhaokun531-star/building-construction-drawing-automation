"""Release gate for the frozen scheme: exterior measurement faces and full axes.

Checks saved public geometry, not only producer counts or metadata.
Explicit labels/lengths are regressions for this SHA-bound drawing.
"""
from pathlib import Path
import hashlib,json,math,subprocess,sys
S=Path(__file__).resolve().parent
FROZEN='5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def require(ok,message):
 if not ok:raise RuntimeError(message)
def near(x,y):return abs(x-y)<1e-6

def verify_scene(analysis,a,f,scene,widths):
 require(analysis['sourceSHA256']==FROZEN,'wrong frozen drawing')
 poly=analysis['wallOuterBodyContours'][0]
 xmin=min(p[0] for p in poly);xmax=max(p[0] for p in poly);ymin=min(p[1] for p in poly);ymax=max(p[1] for p in poly)
 logical={(x['vertical'],x['confirmedLabel']):x for x in analysis['axisCandidates'] if x['confirmed']}
 def face(station,vertical,negative):
  hits=[];i=0 if vertical else 1;j=1-i
  for p,q in zip(poly,poly[1:]+poly[:1]):
   if near(p[i],q[i]):
    if near(station,p[i]):hits.extend([p[j],q[j]])
   elif min(p[i],q[i])-1e-6<=station<=max(p[i],q[i])+1e-6:
    t=max(0,min(1,(station-p[i])/(q[i]-p[i])));hits.append(p[j]+t*(q[j]-p[j]))
  return (min(hits) if negative else max(hits)) if hits else None
 decisions=[]
 for x in a['axes']:
  owner=logical[(x['vertical'],x['label'])]
  require(near(x['station'],owner['coordinate']),'rendered axis differs from logical wall axis')
  half=max(widths[h] for h in owner['walls'])
  for negative,key,reachKey in [(True,'showNegative','negativeWallReach'),(False,'showPositive','positiveWallReach')]:
   boundary=face(x['station'],x['vertical'],negative);gap=abs(x[reachKey]-boundary) if boundary is not None else None
   supported=gap is not None and gap<=half+1e-6
   side=('bottom' if negative else 'top') if x['vertical'] else ('left' if negative else 'right')
   require(not x[key] or supported,f'unsupported outward axis label: {side} {x["label"]}')
   decisions.append(dict(label=x['label'],side=side,shown=x[key],outerFace=boundary,wallReach=x[reachKey],wallFaceGap=gap,supported=supported))
 expected={'top':['1','3','5','7','8','9'],'bottom':['1','2','4','6','8','9'],'left':['A','D','E','F','G','J','L'],'right':['B','F','G','K','L']}
 for side,wanted in expected.items():require([x['label'] for x in decisions if x['side']==side and x['shown']]==wanted,'fixture side labels differ: '+side)
 require(len(logical)==20 and len(a['axes'])==18 and sum(x['shown'] for x in decisions)==24,'logical/rendered axis counts differ')
 for axis in a['axes']:
  require(axis['nativeNestedInClipBlock'],'axis lost clipping container')
  i=1 if axis['vertical'] else 0;low=min(z[i] for z in poly);high=max(z[i] for z in poly)
  for end,c in enumerate(axis['circles']):
   outward=(1 if end else -1)*(c['centre'][i]-(high if end else low))
   require(near(c['radius'],400) and near(outward,4600) and near(outward-c['radius']-3600,600),'axis circle spacing changed')
  if axis['showNegative'] and axis['showPositive']:require(axis['clipInverted'] and near(axis['clipMin'][i],low-1000) and near(axis['clipMax'][i],high+1000),'two-ended short axis gap changed')
  elif axis['showNegative']:require(not axis['clipInverted'] and near(axis['clipMax'][i],low-1000),'negative short axis gap changed')
  else:require(not axis['clipInverted'] and near(axis['clipMin'][i],high+1000),'positive short axis gap changed')

 # Independent fixture nodes: physical exterior faces are not hidden-axis nodes.
 nodeLabels={'right':[None,'B','F','G','K',None],'bottom':[None,'2','4','6','8',None],'top':[None,'3','5','7','8',None],'left':[None,'D','E','F','G','J',None]}
 lengths={'right':[2100,5600,3600,5000,2100],'bottom':[1200,4200,3600,5000,3700],'top':[4000,2200,4000,3800,3700],'left':[4100,1800,1800,3600,4200,2900]}
 saved={r['handle']:r for r in scene['dimensions']};chains={}
 for side,labels in nodeLabels.items():
  vertical=side in ('left','right');low=ymin if vertical else xmin;high=ymax if vertical else xmax
  ds=[d for d in a['dimensions'] if d['side']==side and d['row']=='axis']
  require(ds and near(ds[0]['axisFrom'],low) and near(ds[-1]['axisTo'],high),'second row does not reach exterior faces: '+side)
  nodes=[low]+[logical[(not vertical,label)]['coordinate'] for label in labels[1:-1]]+[high]
  require(len(ds)==len(nodes)-1,'second row omitted a face or retained axis: '+side)
  for i,d in enumerate(ds):
   require(near(d['axisFrom'],nodes[i]) and near(d['axisTo'],nodes[i+1]),'second row missing exterior/axis segment: '+side)
   require(d.get('fromRole')==('exterior-face' if i==0 else 'axis') and d.get('toRole')==('exterior-face' if i==len(ds)-1 else 'axis'),'dimension endpoint roles are ambiguous')
   require(near(d['expected'],lengths[side][i]),'frozen second-row length changed: '+side)
  chains[side]=[d['expected'] for d in ds]
  require(near(sum(chains[side]),high-low),'second row is not continuous wall-to-wall')
 require(a['dimensionSampling']=='exterior-faces-and-retained-axes','producer still uses axis-only dimension contract')
 require(len(a['dimensions'])==25 and a['nativeAxisIntervalDimensions']==21 and a['nativeTotalDimensions']==4,'dimension rows/counts differ')
 for d in a['dimensions']:
  r=saved[d['handle']];require(r['geometry'][0]['type']=='TDbDimension2','saved dimension lost TCH type')
  g=r['geometry'][0]['grip']['points'];side=d['side'];vertical=side in ('left','right');index=0 if vertical else 1
  wall={'bottom':ymin,'top':ymax,'left':xmin,'right':xmax}[side];direction=-1 if side in ('bottom','left') else 1;offset=2800 if d['row']=='axis' else 3600
  require(near(g[0][index],wall+direction*offset),'dimension baseline changed')
  endpoints=([wall,d['axisFrom'],0],[wall,d['axisTo'],0]) if vertical else ([d['axisFrom'],wall,0],[d['axisTo'],wall,0])
  require(all(math.dist(g[i+2],pt)<1e-6 for i,pt in enumerate(endpoints)),'saved measurement point does not match wall face or axis')
  require(near(math.dist(g[2],g[3]),d['expected']) and near(r['measurement'],d['expected']),'saved dimension measured value changed')
  clip=r['gapClip'];require(clip and clip['identityTransform'],'dimension lost actual clipping parent')
  require(near(clip['max' if direction<0 else 'min'][index],wall+direction*1000),'dimension witness no longer starts at 1000')
  if d['row']!='axis':
   require(d['row']=='total-body-envelope' and near(d['axisFrom'],ymin if vertical else xmin) and near(d['axisTo'],ymax if vertical else xmax),'total-body sample endpoints changed')
 require(len(saved)==25,'saved dimension diagnostic incomplete')


 presentation=a.get('axisPresentation',{});require(presentation.get('spanMode')=='associated-wall-ends','wall-following span contract missing: old body-wide grid');require(presentation.get('enabled'),'axis presentation missing: unsorted text-crossing baseline')
 require(presentation['clearanceModelMm']==80 and presentation['axisGroups']==20 and presentation['belowArchitectureVerifiedOnReopen'],'presentation contract differs')
 full=a['fullAxisLines'];require(len(full)==a['logicalFullAxes']==20,'logical axes lost during display segmentation')
 roots={r['handle']:r for r in scene['originalBefore']};allPieces=[];seen=set();obstacleRecords=[]
 for r in roots.values():
  if r['type'] in ('TDbText','TDbSymbElevation'):
   require(r['extentStatus']==0,'annotation extents unavailable');obstacleRecords.append(r)
 producerBoxes={x['handle']:x for x in presentation['obstacles']}
 for box in presentation['obstacles']:
  actual=roots.get(box['handle']);require(actual is not None and actual['type'] in ('TDbText','TDbSymbElevation'),'unapproved axis mask source')
  require(all(near(box['min'][i],actual['min'][i]) and near(box['max'][i],actual['max'][i]) for i in range(2)),'mask does not match actual saved annotation')
 def intervals(vertical,station,lo,hi):
  cuts=[]
  for box in obstacleRecords:
   across=0 if vertical else 1;along=1-across
   if box['min'][across]-80<=station<=box['max'][across]+80:
    x=max(lo,box['min'][along]-80);y=min(hi,box['max'][along]+80)
    if y>x:
     require(box['handle'] in producerBoxes,'producer skipped an annotation intersecting an axis');cuts.append((x,y))
  cuts.sort();cursor=lo;result=[]
  for x,y in cuts:
   if x>cursor+1e-6:result.append((cursor,x))
   cursor=max(cursor,y)
  if hi>cursor+1e-6:result.append((cursor,hi))
  return result
 endStates={(x['vertical'],x['label']):x for x in a['axes']};groups=set();cleared=0;style=None;wallRoles={x['handle']:x['role'] for x in analysis['wallRecognition']};pendingWalls=[]
 for x in full:
  key=(x['vertical'],x['label']);require(key in logical and key not in seen,'duplicate/nonlogical axis');seen.add(key);owner=logical[key];station=owner['coordinate'];require(near(x['station'],station),'axis moved off wall centre')
  ends=endStates.get(key,{});j=1 if x['vertical'] else 0;wallEnds=[]
  for h in owner['walls']:
   # Axis candidate membership also includes role-unknown walls. The frozen
   # drawing's established support rule excludes those until user confirms.
   if wallRoles.get(h) not in ('exterior','major-boundary'):
    require(wallRoles.get(h)=='unknown' and any(z['code']=='WALL_ROLE_REQUIRED' and z['handle']==h for z in analysis['issues']),'unexplained unsupported wall in logical axis');pendingWalls.append(h);continue
   w=roots.get(h);require(w and w['type']=='TDbWall' and not w['proxy'],'associated actual wall missing');c=w['curve'];require(c['startStatus']==0 and c['endStatus']==0,'associated saved wall curve unavailable');wallEnds.extend([c['start'][j],c['end'][j]])
  require(wallEnds and near(min(wallEnds),owner['negativeWallReach']) and near(max(wallEnds),owner['positiveWallReach']),'actual wall reaches disagree with analysis')
  lo=((ymin if x['vertical'] else xmin)-1000) if ends.get('showNegative') else min(wallEnds);hi=((ymax if x['vertical'] else xmax)+1000) if ends.get('showPositive') else max(wallEnds)
  point=lambda v:[station,v,0] if x['vertical'] else [v,station,0]
  require(math.dist(x['start'],point(lo))<1e-6 and math.dist(x['end'],point(hi))<1e-6,'axis crosses beyond associated wall span or misses external join')
  require(x['group'] not in ('0','') and x['group'] not in groups,'logical native group missing or shared');groups.add(x['group'])
  wanted=intervals(x['vertical'],station,lo,hi);require(len(x['pieces'])==len(wanted),'axis still crosses text or lost an unblocked interval')
  for piece,(start,end) in zip(x['pieces'],wanted):
   r=roots.get(piece['handle']);require(r and r['type']=='AcDbLine' and r['layer']=='A-DOTE' and not r['proxy'],'actual axis piece type/layer differs');curve=r['curve']
   require(math.dist(curve['start'],point(start))<1e-6 and math.dist(curve['end'],point(end))<1e-6 and near(curve['length'],end-start),'actual axis text-clear interval differs')
   require(math.dist(piece['start'],curve['start'])<1e-6 and math.dist(piece['end'],curve['end'])<1e-6,'axis piece metadata differs from saved curve');allPieces.append(piece['handle'])
  cleared+=hi-lo-sum(end-start for start,end in wanted)
  require(x['referenceHandle']=='17E4' and x['linetype']=='DOTE' and x['entityColour']==256 and x['layerColour']==1,'reference style evidence differs')
  thisStyle={k:x[k] for k in ['linetype','layer','entityColour','layerColour','entityLineWeight','layerLineWeight','linetypeScale','referenceGlobalScale','targetGlobalScale','patternLength','dashes']};require(style is None or style==thisStyle,'axis styles inconsistent');style=thisStyle
 require(seen==set(logical),'C/H or another logical axis lost');require(len(set(allPieces))==len(allPieces)==presentation['axisPieceCount']==a['nativeFullAxisLines'],'duplicate or missing axis display piece')
 require({r['handle'] for r in roots.values() if r['type']=='AcDbLine' and r['layer']=='A-DOTE'}==set(allPieces),'unlisted/duplicate actual axis strokes')
 require(f['outputModelCount']==len(roots)==260+len(allPieces),'model count does not include actual visible pieces');require(cleared>0,'no actual annotation clearance applied')
 require(scene['originalBefore']==scene['originalAfter'] and scene['globalsBefore']==scene['globalsAfter'] and scene['layoutBefore']==scene['layoutAfter'],'preview changed model/environment')
 return dict(passed=True,spanMode='associated-wall-ends',wallSupportScope='confirmed exterior/major-boundary walls; unknown roles deferred',pendingAssociatedWalls=sorted(set(pendingWalls)),logicalAxes=20,renderedNativeAxes=18,visibleEnds=24,nativeAxisStrokePieces=len(allPieces),logicalNativeGroups=len(groups),axisDimensions=21,totalSampleDimensions=4,savedMeasurementEndpoints=50,axisClipChecks=18,nativeCircles=36,witnessStart=1000,dimensionPositions=[2000,2800,3600],modelRoots=f['outputModelCount'],decisions=decisions,chains=chains,referenceAxisLineStyle=style,textClearanceModelMm=80,totalAxisDisplayGapLength=cleared,obstacleCount=len(producerBoxes),bottom3Hidden=True,top3Retained=True,allLogicalAxesRetained=True,constructionDrawingComplete=False)

def main():
 suffix='-v31' if '--baseline-v31' in sys.argv else '-v22' if '--baseline-v22' in sys.argv else '-v25' if '--baseline-v25' in sys.argv else '-v23' if '--baseline-v23' in sys.argv else '-v32'
 n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json');p=Path(n['output'])
 analysis=read(p/'analysis.json');a=read(p/'native-annotations.json');f=read(p/'native-foundation.json')
 require(sha(p/'analysis.json')==n['analysisSHA256'],'analysis changed')
 require(sha(p/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']==v['sourceSHA256'],'DWG/preview mismatch')
 require(sha(Path(v['PDF']))==v['PDFSHA256'],'PDF changed')
 cache=Path(read(Path(n['binding']['inputManifest']))['cache']['path']);require(sha(cache)==n['binding']['cacheSHA256'],'cache changed')
 widths={row[1]:(float(row[6])+float(row[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines() if (row:=line.split()) and row[0]=='W'}
 require(all(row[-1]=='U' for line in cache.read_text(encoding='utf-8-sig').splitlines() if (row:=line.split()) and row[0]=='W'),'wall-role fixture changed; explicit roles need reviewed support contract')
 scenePath=Path(v['job'])/'stage2-preview-native.json';require(sha(scenePath)==v['nativeReportSHA256'],'saved diagnostics changed')
 result=verify_scene(analysis,a,f,read(scenePath),widths)
 result.update(contract='bcp.wall-following-axes-release.v1',nativeRun=str(S/f'evidence/native-run{suffix}.json'),DWGSHA256=n['DWGSHA256'],PDFSHA256=v['PDFSHA256'],analysisSHA256=n['analysisSHA256'],annotationsSHA256=sha(p/'native-annotations.json'))
 if '--check-only' not in sys.argv:
  out=S/f'evidence/wall-following-axes-release{suffix}{"-final" if "--final-receipt" in sys.argv else ""}.json';require(not out.exists(),'gate receipt already exists');out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
  subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
 print(json.dumps({k:v for k,v in result.items() if k not in ('decisions','chains')},ensure_ascii=False))
if __name__=='__main__':main()
