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

 full=a.get('fullAxisLines',[]);require(a.get('fullInteriorAxes') and a.get('nativeFullAxisLines')==len(full)==20,'not all logical axes have full strokes')
 roots={r['handle']:r for r in scene['originalBefore']}
 require(len(roots)==len(scene['originalBefore'])==f['outputModelCount']==280,'root model count/duplicate handles differ')
 require({r['handle'] for r in roots.values() if r['type']=='AcDbLine' and r['layer']=='A-DOTE'}=={x['handle'] for x in full},'unlisted or missing actual A-DOTE axis strokes')
 endStates={(x['vertical'],x['label']):x for x in a['axes']}
 seen=set();style=None
 for x in full:
  key=(x['vertical'],x['label']);require(key in logical and key not in seen,'duplicate or nonlogical full axis');seen.add(key)
  axis=logical[key];station=axis['coordinate'];require(near(x['station'],station),'full axis moved off wall centre')
  ends=endStates.get(key,{});lo=(ymin if x['vertical'] else xmin)-(1000 if ends.get('showNegative') else 0);hi=(ymax if x['vertical'] else xmax)+(1000 if ends.get('showPositive') else 0)
  start=[station,lo,0] if x['vertical'] else [lo,station,0];end=[station,hi,0] if x['vertical'] else [hi,station,0]
  r=roots[x['handle']];curve=r.get('curve',{})
  require(r['type']=='AcDbLine' and r['layer']=='A-DOTE' and not r['proxy'],'full axis primitive type/layer differs')
  require(math.dist(curve.get('start',[]),start)<1e-6 and math.dist(curve.get('end',[]),end)<1e-6 and near(curve['length'],hi-lo),'actual saved full axis endpoints differ')
  require(math.dist(x['start'],start)<1e-6 and math.dist(x['end'],end)<1e-6,'full axis metadata does not match saved geometry')
  require(x['referenceHandle']=='17E4' and x['nativeType']=='AcDbLine' and x['linetype']=='DOTE' and x['layer']=='A-DOTE' and x['entityColour']==256 and x['layerColour']==1,'reference full axis style evidence differs')
  require(x['patternLength']>0 and x['linetypeScale']>0 and x['referenceGlobalScale']>0 and x['targetGlobalScale']>0 and any(d<0 for d in x['dashes']) and any(d>0 for d in x['dashes']),'axis dash pattern/scale evidence invalid')
  thisStyle={k:x[k] for k in ['linetype','layer','entityColour','layerColour','entityLineWeight','layerLineWeight','linetypeScale','referenceGlobalScale','targetGlobalScale','patternLength','dashes']}
  require(style is None or style==thisStyle,'full axis styles inconsistent');style=thisStyle
 require(seen==set(logical),'hidden-end logical axes C/H were omitted')
 require(scene['originalBefore']==scene['originalAfter'] and scene['globalsBefore']==scene['globalsAfter'] and scene['layoutBefore']==scene['layoutAfter'],'preview changed drawing or environment')
 return dict(passed=True,logicalAxes=20,renderedNativeAxes=18,visibleEnds=24,nativeFullAxisLines=20,axisDimensions=21,totalSampleDimensions=4,savedMeasurementEndpoints=50,axisClipChecks=18,nativeCircles=36,witnessStart=1000,dimensionPositions=[2000,2800,3600],modelRoots=280,decisions=decisions,chains=chains,referenceAxisLineStyle=style,bottom3Hidden=True,top3Retained=True,allLogicalAxesThroughInterior=True,constructionDrawingComplete=False)

def main():
 suffix='-v22' if '--baseline-v22' in sys.argv else '-v23' if '--baseline-v23' in sys.argv else '-v25'
 n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json');p=Path(n['output'])
 analysis=read(p/'analysis.json');a=read(p/'native-annotations.json');f=read(p/'native-foundation.json')
 require(sha(p/'analysis.json')==n['analysisSHA256'],'analysis changed')
 require(sha(p/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']==v['sourceSHA256'],'DWG/preview mismatch')
 require(sha(Path(v['PDF']))==v['PDFSHA256'],'PDF changed')
 cache=Path(read(Path(n['binding']['inputManifest']))['cache']['path']);require(sha(cache)==n['binding']['cacheSHA256'],'cache changed')
 widths={row[1]:(float(row[6])+float(row[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines() if (row:=line.split()) and row[0]=='W'}
 scenePath=Path(v['job'])/'stage2-preview-native.json';require(sha(scenePath)==v['nativeReportSHA256'],'saved diagnostics changed')
 result=verify_scene(analysis,a,f,read(scenePath),widths)
 result.update(contract='bcp.perimeter-full-axes-release.v1',nativeRun=str(S/f'evidence/native-run{suffix}.json'),DWGSHA256=n['DWGSHA256'],PDFSHA256=v['PDFSHA256'],analysisSHA256=n['analysisSHA256'],annotationsSHA256=sha(p/'native-annotations.json'))
 if '--check-only' not in sys.argv:
  out=S/f'evidence/perimeter-axes-release{suffix}{"-final" if "--final-receipt" in sys.argv else ""}.json';require(not out.exists(),'gate receipt already exists');out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
  subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
 print(json.dumps({k:v for k,v in result.items() if k not in ('decisions','chains')},ensure_ascii=False))
if __name__=='__main__':main()
