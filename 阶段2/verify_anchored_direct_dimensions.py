"""Saved native roots, actual opening anchoring and non-block third-row gate."""
from pathlib import Path
import sys,json,math,subprocess
import verify_annotation_simplification as prior
from verify_unified_annotations import S,FROZEN,read,sha,require,near
legacy_opening_rows=prior.verify_opening_rows

def verify_opening_rows(analysis,a,scene,widths):
 require(a.get('anchoredDirect') and a.get('directThirdEditChecks')==53,'anchored non-block generation/edit proof missing')
 legacy_opening_rows(analysis,a,scene,widths)
 roots={r['handle']:r for r in scene['originalBefore']};saved={r['handle']:r for r in scene['dimensions']};details={r['handle']:r for r in a['dimensionDetails']};numbers={x['handle']:x for x in analysis['openingNumbers']};walls={};rooms=[]
 for line in Path(read(S/'inputs/frozen-scheme-v2.json')['cache']['path']).read_text(encoding='utf-8-sig').splitlines():
  row=line.split()
  if row and row[0]=='W':walls[row[1]]=list(map(float,row[2:8]))
  if row and row[0]=='R' and bytes.fromhex(row[-1]).decode('utf-8')=='庭院':rooms.append(((float(row[2])+float(row[4]))/2,(float(row[3])+float(row[5]))/2))
 def centre(w):
  dx,dy=w[2]-w[0],w[3]-w[1];length=math.hypot(dx,dy);off=(w[4]-w[5])/2;return w[0]-dy/length*off,w[1]+dx/length*off,dx/length,dy/length,length
 poly=analysis['wallOuterBodyContours'][0]
 def default_side(w,pa,pb,kind,root):
  vertical=near(pa[0],pb[0]);mid=[(pa[0]+pb[0])/2,(pa[1]+pb[1])/2];across=0 if vertical else 1
  if kind=='D':return 1 if root['max'][across]-mid[across]>=mid[across]-root['min'][across] else -1
  x,y,ux,uy,length=centre(w);station=y+uy*length/2 if vertical else x+ux*length/2;plane=x if vertical else y;i=1 if vertical else 0;j=1-i;hits=[]
  for p,q in zip(poly,poly[1:]+poly[:1]):
   if near(p[i],q[i]):
    if near(station,p[i]):hits.extend((p[j],q[j]))
   elif min(p[i],q[i])-1e-6<=station<=max(p[i],q[i])+1e-6:
    t=max(0,min(1,(station-p[i])/(q[i]-p[i])));hits.append(p[j]+t*(q[j]-p[j]))
  if hits:
   half=(w[4]+w[5])/2;neg=abs(plane-min(hits))<=half+1e-6;pos=abs(plane-max(hits))<=half+1e-6
   if neg and not pos:return -1
   if pos and not neg:return 1
  require(rooms,'courtyard side source absent');r=min(rooms,key=lambda p:math.dist(p,mid));return -1 if r[across]-mid[across]<0 else 1
 for c in a['captions']:
  h=c['opening'];host=[x for x in analysis['localWallChains'] if h in x['openings']];require(len(host)==1,'caption host absent');w=walls[host[0]['wall']]
  if numbers[h]['observedKind']=='G':
   x,y,ux,uy,length=centre(w);group=next(g for g in prior.GROUP_CAPTURE['groups'] if g['handle']==h);on=[p for p in sorted(set(tuple(p[:2]) for p in group['points'])) if abs((p[0]-x)*uy-(p[1]-y)*ux)<1e-6 and -1e-6<=(p[0]-x)*ux+(p[1]-y)*uy<=length+1e-6];pairs=[(p,q) for i,p in enumerate(on) for q in on[i+1:] if near(math.dist(p,q),numbers[h]['width'])];require(len(pairs)==1,'caption group endpoints ambiguous');pa,pb=pairs[0]
  else:pa,pb=[p[:2] for p in roots[h]['grip']['points'][1:3]]
  r=roots[c['handle']];points=r['stretch']['points'];require(len(points)==1,'actual caption alignment unavailable');position=points[0][:2];require(math.dist(position,c['position'])<1e-6,'caption metadata position differs from saved alignment');vertical=near(pa[0],pb[0]);cross=0 if vertical else 1;along=1-cross;side=default_side(w,pa,pb,numbers[h]['observedKind'],roots[h]);gap=side*(position[cross]-pa[cross]);require(300-1e-6<=gap<=600+1e-6 and min(pa[along],pb[along])-1e-6<=position[along]<=max(pa[along],pb[along])+1e-6,'caption is remote, wrong-side or outside its own opening: '+h)
 third=[d for d in a['dimensions'] if d['row'] in ('opening-wall-chain','opening-end-face')];require(len(third)==53,'third row count changed')
 for d in third:
  r=roots.get(d['handle']);require(r and r['type']=='TDbDimension2' and not r['proxy'] and saved[d['handle']]['gapClip'] is None and not d['nativeNestedInGapBlock'],'third row not a directly selectable native model root');require(0<=saved[d['handle']]['dimexo']*saved[d['handle']]['dimscale']<=1000,'native witness starts outside the mask')
 for side in ('bottom','top','left','right'):
  spans=sorted((d['axisFrom'],d['axisTo']) for d in third if d['side']==side)
  require(all(spans[i][0]>=spans[i-1][1]-1e-6 for i in range(1,len(spans))),'third row overlapping/duplicated segments');require(len({round(details[d['handle']]['baseline'],6) for d in third if d['side']==side})==1,'third row split into different baselines')
 xmin=min(p[0] for p in poly);xmax=max(p[0] for p in poly);ymin=min(p[1] for p in poly);ymax=max(p[1] for p in poly);expected=[(xmin,ymin-1000,xmax,ymin),(xmin,ymax,xmax,ymax+1000),(xmin-1000,ymin,xmin,ymax),(xmax,ymin,xmax+1000,ymax)];masks=a.get('witnessMasks',[]);require(len(masks)==4 and len({m['handle'] for m in masks})==4,'native mask strips missing')
 for m,b in zip(masks,expected):
  r=roots.get(m['handle']);require(r and r['type']=='AcDbWipeout' and not r['proxy'] and r['extentStatus']==0 and r['layer']=='A-PUB_DIM','mask is not a standard native model object');require(all(near(x,y) for x,y in zip(r['min'][:2]+r['max'][:2],b)) and all(near(x,y) for x,y in zip(m['min']+m['max'],b)),'saved native mask differs from 0–1000 strip')
 require({r['handle']for r in roots.values()if r['type']=='AcDbWipeout'}=={m['handle']for m in masks},'unlisted native mask')

prior.base.verify_opening_rows=verify_opening_rows

def verify_scene(analysis,a,f,scene,widths):
 r=prior.verify_scene(analysis,a,f,scene,widths);r.update(anchoredCaptions=27,directNativeThirdDimensions=53,nativeWitnessMasks=4,witnessDisplayStart=1000,thirdNativeEditRestoreChecks=53);return r

def main():
 suffix='-v45' if '--baseline-v45' in sys.argv else next(('-'+x[2:] for x in sys.argv if x.startswith('--v') and x[3:].isdigit()),'-v51');n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json');p=Path(n['output']);audit=Path(v['maskDocumentAudit']);q=read(audit);require(sha(audit)==v['maskDocumentAuditSHA256'] and q['masks']==4 and q['frame70']==q['WIPEOUTFRAME']==q['publicVisibleFrames']==0 and q['readOnly'] and q['error'] is None,'active document mask frame audit failed');analysis=read(p/'analysis.json');a=read(p/'native-annotations.json');f=read(p/'native-foundation.json');scene=Path(v['job'])/'stage2-preview-native.json';require(sha(p/'analysis.json')==n['analysisSHA256'] and sha(p/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']==v['sourceSHA256'] and sha(scene)==v['nativeReportSHA256'] and sha(Path(v['PDF']))==v['PDFSHA256'],'native source/preview binding changed');cache=Path(read(Path(n['binding']['inputManifest']))['cache']['path']);require(sha(cache)==n['binding']['cacheSHA256'],'cache changed');widths={r[1]:(float(r[6])+float(r[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines() if(r:=line.split()) and r[0]=='W'};prior.GROUP_CAPTURE=read(p/'endpoint-capture.json');require(sha(p/'endpoint-capture.json')==n['endpointCaptureSHA256'],'public endpoint capture changed');result=verify_scene(analysis,a,f,read(scene),widths);result.update(contract='bcp.anchored-direct-release.v1',gateScriptSHA256=sha(Path(__file__)),nativeRun=str(S/f'evidence/native-run{suffix}.json'),DWGSHA256=n['DWGSHA256'],PDFSHA256=v['PDFSHA256'],analysisSHA256=n['analysisSHA256'],annotationsSHA256=sha(p/'native-annotations.json'),gateDependencies={name:sha(S/name) for name in ('verify_unified_annotations.py','verify_annotation_simplification.py')})
 if '--check-only' not in sys.argv:
  out=S/f'evidence/anchored-direct-release{suffix}{"-final" if "--final-receipt" in sys.argv else ""}.json';require(not out.exists(),'receipt exists');out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
 print(json.dumps({k:v for k,v in result.items() if k not in ('decisions','chains','supportRangesByAxis')},ensure_ascii=False))
if __name__=='__main__':main()
