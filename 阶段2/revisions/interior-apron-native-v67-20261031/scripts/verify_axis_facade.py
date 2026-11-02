"""Release gate: independently reject outward labels with no facade support.

Reads frozen wall widths and saved diagnostics; does not call the C++ predicate.
The explicit label cases below are regressions for this SHA-bound drawing only.
"""
from pathlib import Path
import hashlib,json,math,subprocess,sys
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def require(ok,message):
 if not ok:raise RuntimeError(message)
def near(x,y):return abs(x-y)<1e-6
suffix='-v22' if '--baseline-v22' in sys.argv else '-v23'
n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json')
p=Path(n['output']);analysis=read(p/'analysis.json');a=read(p/'native-annotations.json');f=read(p/'native-foundation.json')
require(sha(p/'analysis.json')==n['analysisSHA256'],'analysis file changed')
require(sha(p/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']==v['sourceSHA256'],'saved DWG/preview mismatch')
require(sha(Path(v['PDF']))==v['PDFSHA256'],'PDF changed')
require(analysis['sourceSHA256']=='5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E','wrong frozen drawing')
cache=Path(read(Path(n['binding']['inputManifest']))['cache']['path']);require(sha(cache)==n['binding']['cacheSHA256'],'frozen cache changed')
widths={row[1]:(float(row[6])+float(row[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines() if (row:=line.split()) and row[0]=='W'}
poly=analysis['wallOuterBodyContours'][0]
def face(station,vertical,negative):
 hits=[];across=0 if vertical else 1;along=1-across
 for x,y in zip(poly,poly[1:]+poly[:1]):
  lo=x[across];hi=y[across]
  if near(lo,hi):
   if near(station,lo):hits.extend([x[along],y[along]])
  elif min(lo,hi)-1e-6<=station<=max(lo,hi)+1e-6:
   t=max(0,min(1,(station-lo)/(hi-lo)));hits.append(x[along]+t*(y[along]-x[along]))
 return (min(hits) if negative else max(hits)) if hits else None
logical={(x['vertical'],x['confirmedLabel']):x for x in analysis['axisCandidates'] if x['confirmed']}
decisions=[];bad=[]
for x in a['axes']:
 owner=logical[(x['vertical'],x['label'])];require(near(x['station'],owner['coordinate']),'rendered axis station differs from logical wall axis');half=max(widths[h] for h in owner['walls'])
 for negative,key,reachKey in [(True,'showNegative','negativeWallReach'),(False,'showPositive','positiveWallReach')]:
  boundary=face(x['station'],x['vertical'],negative);gap=abs(x[reachKey]-boundary) if boundary is not None else None
  supported=gap is not None and gap<=half+1e-6
  side=('bottom' if negative else 'top') if x['vertical'] else ('left' if negative else 'right')
  decisions.append(dict(label=x['label'],side=side,shown=x[key],outerFace=boundary,wallReach=x[reachKey],wallHalfThickness=half,wallFaceGap=gap,supported=supported))
  if x[key] and not supported:bad.append(f'{side} {x["label"]}: wall-to-face gap={gap}')
require(not bad,'unsupported outward axis labels: '+ '; '.join(bad))
expected={'top':['1','3','5','7','8','9'],'bottom':['1','2','4','6','8','9'],'left':['A','D','E','F','G','J','L'],'right':['B','F','G','K','L']}
for side,wanted in expected.items():require([x['label'] for x in decisions if x['side']==side and x['shown']]==wanted,'fixture side labels differ: '+side)
require(len(logical)==20 and len(a['axes'])==18 and sum(x['shown'] for x in decisions)==24,'logical/rendered axis counts differ')
require(f['outputModelCount']==259 and len(a['dimensions'])==24,'saved model or dimension count differs')
for axis in a['axes']:
 require(axis['nativeNestedInClipBlock'],'axis display lost clipping container');i=1 if axis['vertical'] else 0;low=min(z[i] for z in poly);high=max(z[i] for z in poly)
 for side,c in enumerate(axis['circles']):
  wall=high if side else low;outward=(1 if side else -1)*(c['centre'][i]-wall)
  require(near(c['radius'],400) and near(outward,4600) and near(outward-c['radius']-3600,600),'axis ring spacing changed')
 if axis['showNegative'] and axis['showPositive']:require(axis['clipInverted'] and near(axis['clipMin'][i],low-1000) and near(axis['clipMax'][i],high+1000),'two-ended axis start gap changed')
 elif axis['showNegative']:require(not axis['clipInverted'] and near(axis['clipMax'][i],low-1000),'negative axis start gap changed')
 else:require(not axis['clipInverted'] and near(axis['clipMin'][i],high+1000),'positive axis start gap changed')

scenePath=Path(v['job'])/'stage2-preview-native.json';require(sha(scenePath)==v['nativeReportSHA256'],'saved diagnostic changed');scene=read(scenePath)
records={x['handle']:x for x in scene['dimensions']};chains={};expectedPairs={}
for side in expected:
 positions=[x['station'] for x in a['axes'] if x['label'] in expected[side] and x['vertical']==(side in ('bottom','top'))]
 expectedPairs[side]=list(zip(positions,positions[1:]));chains[side]=[]
for d in a['dimensions']:
 r=records[d['handle']];g=r['geometry'][0]['grip']['points'];side=d['side'];vertical=side in ('left','right');axisIndex=0 if vertical else 1
 wall=min(z[1] for z in poly) if side=='bottom' else max(z[1] for z in poly) if side=='top' else min(z[0] for z in poly) if side=='left' else max(z[0] for z in poly)
 direction=-1 if side in ('bottom','left') else 1;offset=2800 if d['row']=='axis' else 3600
 require(near(g[0][axisIndex],wall+direction*offset),'dimension baseline changed')
 endpoints=([wall,d['axisFrom'],0],[wall,d['axisTo'],0]) if vertical else ([d['axisFrom'],wall,0],[d['axisTo'],wall,0])
 require(all(math.dist(g[i+2],pt)<1e-6 for i,pt in enumerate(endpoints)),'actual saved dimension points changed')
 require(near(math.dist(g[2],g[3]),d['expected']) and near(r['measurement'],d['expected']),'dimension measurement changed')
 clip=r['gapClip'];require(clip and clip['identityTransform'],'dimension lost actual clipping parent')
 cut=clip['max'][axisIndex] if direction<0 else clip['min'][axisIndex];require(near(cut,wall+direction*1000),'dimension wall-side gap changed')
 if d['row']=='axis':
  chains[side].append(d['expected']);pair=(d['axisFrom'],d['axisTo']);require(any(near(pair[0],x) and near(pair[1],y) for x,y in expectedPairs[side]),'dimension still references a hidden axis')
for side,pairs in expectedPairs.items():require(len(chains[side])==len(pairs),'dimension side chain incomplete')
require(all(near(x,y) for x,y in zip(chains['bottom'],[1100,4200,3600,5000,3600])),'bottom 3 regression: 4200 not recomputed')
result=dict(contract='bcp.axis-facade-release-gate.v1',passed=True,nativeRun=str(S/f'evidence/native-run{suffix}.json'),DWGSHA256=n['DWGSHA256'],PDFSHA256=v['PDFSHA256'],analysisSHA256=n['analysisSHA256'],annotationsSHA256=sha(p/'native-annotations.json'),logicalAxes=20,renderedNativeAxes=18,visibleEnds=24,axisDimensions=20,totalSampleDimensions=4,savedMeasurementEndpoints=48,axisClipChecks=18,nativeCircles=36,witnessStart=1000,dimensionPositions=[2000,2800,3600],modelRoots=259,decisions=decisions,chains=chains,bottom3Hidden=True,top3Retained=True,constructionDrawingComplete=False)
if '--check-only' not in sys.argv:
 out=S/f'evidence/axis-facade-release{suffix}{"-final" if "--final-receipt" in sys.argv else ""}.json';require(not out.exists(),'gate receipt already exists');out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
print(json.dumps({k:val for k,val in result.items() if k not in ('decisions','chains')},ensure_ascii=False))
