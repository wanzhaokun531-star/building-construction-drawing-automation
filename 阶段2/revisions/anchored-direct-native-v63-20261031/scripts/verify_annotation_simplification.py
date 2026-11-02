"""Fixed-input saved-geometry gate for bathroom/blank-wall omissions and continuous chains."""
import verify_unified_annotations as base
from verify_unified_annotations import S,FROZEN,read,sha,require,near
from pathlib import Path
import math,json,sys,subprocess
OMITTED_WALLS={'CD9D','CDBE','CF60','CF64','CF66','CF68'}
OMITTED_DOORS={'CDBF','CF65','CF69'}
GROUP_CAPTURE=None

def verify_opening_rows(analysis,a,scene,widths):
 policy=a.get('annotationSimplification',{});require(policy.get('enabled') and set(policy['omittedPartitionWalls'])==OMITTED_WALLS and set(policy['omittedPartitionDoors'])==OMITTED_DOORS,'new bathroom omission policy missing or inaccurate')
 roots={x['handle']:x for x in scene['originalBefore']};saved={x['handle']:x for x in scene['dimensions']};details={x['handle']:x for x in a['dimensionDetails']};roles={x['handle']:x['role'] for x in analysis['wallRecognition']};chains={x['wall']:x for x in analysis['localWallChains']};numbers={x['handle']:x for x in analysis['openingNumbers']}
 for h in OMITTED_WALLS:require(roles[h]=='interior-partition' and near(widths[h],50),'suppressed wall is not a known 100 partition')
 for h in OMITTED_DOORS:require(numbers[h]['observedKind']=='D' and len([c for c in chains.values() if h in c['openings'] and c['wall'] in OMITTED_WALLS])==1,'suppressed door is not hosted by a bathroom partition')
 require(len(details)==len(saved)==len(a['dimensions']),'detailed dimensions incomplete')
 for d in a['dimensions']:
  actual=saved[d['handle']];info=details[d['handle']];g=actual['geometry'][0]['grip']['points'];i=0 if info['vertical'] else 1
  require(actual['geometry'][0]['type']=='TDbDimension2','native dimension type lost')
  require(all(math.dist(g[k+2][:2],info[key])<1e-6 for k,key in enumerate(('measuredA','measuredB'))),'saved endpoints differ')
  require(near(math.dist(g[2],g[3]),d['expected']) and near(actual['measurement'],d['expected']) and near(g[0][i],info['baseline']),'saved measurement/baseline differs')
  require(actual['geometry'][0]['layer']==('A-AXIS' if d['row'] in ('axis','total-body-envelope') else 'A-PUB_DIM'),'dimension layer wrong')
  require(info['sourceWall'] not in OMITTED_WALLS,'bathroom partition dimensions returned')
 poly=analysis['wallOuterBodyContours'][0];xmin=min(x[0] for x in poly);xmax=max(x[0] for x in poly);ymin=min(x[1] for x in poly);ymax=max(x[1] for x in poly)
 cache=read(S/'inputs/frozen-scheme-v2.json');walls={}
 for line in Path(cache['cache']['path']).read_text(encoding='utf-8-sig').splitlines():
  row=line.split()
  if row and row[0]=='W':walls[row[1]]=list(map(float,row[2:8]))
 def centre(w):
  dx,dy=w[2]-w[0],w[3]-w[1];l=math.hypot(dx,dy);off=(w[4]-w[5])/2
  return w[0]-dy/l*off,w[1]+dx/l*off,dx/l,dy/l,l
 def facade_at(w,point):
  x,y,ux,uy,l=centre(w);vertical=abs(ux)<1e-6;station=point[1] if vertical else point[0];across=x if vertical else y;i=1 if vertical else 0;j=1-i;hits=[]
  for p,q in zip(poly,poly[1:]+poly[:1]):
   if near(p[i],q[i]):
    if near(station,p[i]):hits.extend([p[j],q[j]])
   elif min(p[i],q[i])-1e-6<=station<=max(p[i],q[i])+1e-6:
    t=max(0,min(1,(station-p[i])/(q[i]-p[i])));hits.append(p[j]+t*(q[j]-p[j]))
  if not hits:return None
  half=(w[4]+w[5])/2;neg=abs(across-min(hits))<=half+1e-6;pos=abs(across-max(hits))<=half+1e-6
  return ('left' if vertical else 'bottom') if neg and not pos else ('right' if vertical else 'top') if pos and not neg else None
 capture=GROUP_CAPTURE or read(Path(read(S/'evidence/native-run-v41.json')['output'])/'endpoint-capture.json');require(capture['sourceSHA256']==FROZEN,'group evidence source differs');groups={x['handle']:x for x in capture['groups']}
 def endpoints(h,w):
  if numbers[h]['observedKind']!='G':
   gp=roots[h]['grip']['points'];pa,pb=gp[1][:2],gp[2][:2];require(near(math.dist(pa,pb),numbers[h]['width']),'source width wrong');return pa,pb
  group=groups[h];require(group['status']==0 and group['uniqueWidthAndHostResolved'],'group public points unavailable');x,y,ux,uy,l=centre(w);on=[p for p in sorted(set(tuple(p[:2]) for p in group['points'])) if abs((p[0]-x)*uy-(p[1]-y)*ux)<1e-6 and -1e-6<=(p[0]-x)*ux+(p[1]-y)*uy<=l+1e-6];pairs=[(p,q) for i,p in enumerate(on) for q in on[i+1:] if near(math.dist(p,q),numbers[h]['width'])];require(len(pairs)==1,'group endpoints not unique');return pairs[0]
 def facade_side(h,c):
  if roles[h]!='exterior' or not c['openings']:return None
  sides=[]
  for o in c['openings']:
   pa,pb=endpoints(o,walls[h]);sides.extend(facade_at(walls[h],p) for p in (pa,pb,[(pa[0]+pb[0])/2,(pa[1]+pb[1])/2]))
  return sides[0] if sides and sides[0] is not None and len(set(sides))==1 else None
 def trim(w,at_end):
  x,y,ux,uy,l=centre(w);point=(x+(l if at_end else 0)*ux,y+(l if at_end else 0)*uy);amount=0
  for other in walls.values():
   if other is w:continue
   px,py,vx,vy,length=centre(other)
   if abs(ux*vx+uy*vy)>1e-6:continue
   across=abs((point[0]-px)*vy-(point[1]-py)*vx);along=(point[0]-px)*vx+(point[1]-py)*vy
   if across<1e-6 and -1e-6<=along<=length+1e-6:amount=max(amount,(other[4]+other[5])/2)
  return amount
 source_sides={h:facade_side(h,c) for h,c in chains.items()};accounted=set()
 for h,c in chains.items():
  ds=[d for d in a['dimensions'] if details[d['handle']]['sourceWall']==h]
  if not c['openings'] or h in OMITTED_WALLS:require(not ds,'blank wall or bathroom partition dimension returned: '+h);continue
  w=walls[h];x,y,ux,uy,l=centre(w);vertical=abs(ux)<1e-6;side=source_sides[h];stops=c['stations'].copy();plane={'left':xmin,'right':xmax,'bottom':ymin,'top':ymax}.get(side)
  if not side:stops[0]=trim(w,False);stops[-1]=l-trim(w,True)
  require(len(ds)==len(stops)-1,'eligible complete wall chain missing: '+h)
  baselines={round(saved[d['handle']]['geometry'][0]['grip']['points'][0][0 if vertical else 1],6) for d in ds};require(len(baselines)==1,'local chain baseline split: '+h)
  for d,lo,hi in zip(ds,stops,stops[1:]):
   pa,pb=[x+ux*lo,y+uy*lo],[x+ux*hi,y+uy*hi]
   if side:pa[0 if vertical else 1]=pb[0 if vertical else 1]=plane
   if pa[1 if vertical else 0]>pb[1 if vertical else 0]:pa,pb=pb,pa
   gp=saved[d['handle']]['geometry'][0]['grip']['points'];require(math.dist(gp[2][:2],pa)<1e-6 and math.dist(gp[3][:2],pb)<1e-6,'complete chain physical points wrong: '+h)
   require(d['row']==('opening-wall-chain' if side else 'internal-opening-chain'),'opening projected to wrong facade: '+h)
   if side:
    require(d['side']==side and near(details[d['handle']]['baseline'],plane+(-2000 if side in ('left','bottom') else 2000)),'third row side/distance wrong')
   accounted.add(d['handle'])
  # Every retained door/window's exact public width is represented on its own host.
  for opening in c['openings']:
   pa,pb=endpoints(opening,w);j=1 if vertical else 0;lo,hi=sorted((pa[j],pb[j]));require(any(near(min(details[d['handle']]['measuredA'][j],details[d['handle']]['measuredB'][j]),lo) and near(max(details[d['handle']]['measuredA'][j],details[d['handle']]['measuredB'][j]),hi) and near(d['expected'],numbers[opening]['width']) for d in ds),'retained host opening width missing: '+opening)
 terminals=[d for d in a['dimensions'] if d['row']=='opening-end-face'];require(len(terminals)==8,'third row terminal legs missing')
 for side in ('bottom','top','left','right'):
  vertical=side in ('left','right');negative=side in ('left','bottom');stations=sorted(x['station'] for x in a['axes'] if x['vertical']!=vertical and x['showNegative' if negative else 'showPositive']);lo,hi=(ymin,ymax) if vertical else (xmin,xmax);plane={'left':xmin,'right':xmax,'bottom':ymin,'top':ymax}[side];ds=[d for d in terminals if d['side']==side];require(len(ds)==2,'third row end legs not paired')
  for d,(start,end),rolesWanted in zip(ds,((lo,stations[0]),(stations[-1],hi)),(('exterior-face','axis'),('axis','exterior-face'))):
   require(near(d['axisFrom'],start) and near(d['axisTo'],end) and (d['fromRole'],d['toRole'])==rolesWanted,'third row terminal face/axis wrong');
   info=details[d['handle']];gp=saved[d['handle']]['geometry'][0]['grip']['points'];pa,pb=([plane,start,0],[plane,end,0]) if vertical else ([start,plane,0],[end,plane,0]);require(info['vertical']==vertical and math.dist(gp[2],pa)<1e-6 and math.dist(gp[3],pb)<1e-6 and math.dist(info['measuredA'],pa[:2])<1e-6 and math.dist(info['measuredB'],pb[:2])<1e-6,'third row terminal physical face/axis endpoints wrong');require(near(details[d['handle']]['baseline'],plane+(-2000 if negative else 2000)),'third row terminal baseline wrong');require(not details[d['handle']]['sourceWall'],'terminal leg pretending to be a wall chain');accounted.add(d['handle'])
 for d in a['dimensions']:
  if d['row'] not in ('axis','total-body-envelope'):
   require(d['handle'] in accounted,'unaccounted/spurious opening dimension')
   if d['row']!='internal-opening-chain':
    info=details[d['handle']];side=d['side'];i=0 if info['vertical'] else 1;plane={'left':xmin,'right':xmax,'bottom':ymin,'top':ymax}[side];direction=-1 if side in ('left','bottom') else 1;clip=saved[d['handle']]['gapClip']
    if a.get('anchoredDirect'):
     r=roots.get(d['handle']);require(clip is None and r and r['type']=='TDbDimension2' and not r['proxy'],'third row still wrapped in a block')
    else:require(clip and clip['identityTransform'] and near(clip['max' if direction<0 else 'min'][i],plane+direction*1000),'third row witness start wrong')
 captions=a['captions'];require(len(captions)==27 and {x['opening'] for x in captions}==set(numbers)-OMITTED_DOORS,'door omission/caption coverage wrong')
 user=read(S/'inputs/user-opening-prefixes-v1.json');require(user['sourceSHA256']==FROZEN and sha(Path(user['path']))==user['SHA256'],'C prefix input changed');require(set(user['openings'])=={h for h,v in numbers.items() if v['observedKind'] in ('W','G')},'C prefix assignment scope wrong')
 for x in captions:
  n=numbers[x['opening']];require(not x['pending'] and x['text']==n['number'] and x['text'].startswith('C' if n['observedKind'] in ('W','G') else 'M'),'C/M caption number wrong');r=roots[x['handle']];require(r['type']=='AcDbText' and r['layer']=='A-WINDOW-TEXT','caption native type/layer wrong')
 require(sum(x['text'].startswith('C') for x in captions)==13,'13 C numbers missing')
 # Saved actual text boxes and dimension grips are checked independently.
 require(a['layoutReview']['unresolvedConflicts']==0,'unresolved text conflicts must not be released')
 blocked=[(r['min'][0],r['min'][1],r['max'][0],r['max'][1]) for r in roots.values() if r['type'] in ('TDbText','TDbSymbElevation')]
 def overlap(a,b):return a[0]<b[2]+80 and a[2]>b[0]-80 and a[1]<b[3]+80 and a[3]>b[1]-80
 for c in captions:
  r=roots[c['handle']];box=(r['min'][0],r['min'][1],r['max'][0],r['max'][1]);require(not any(overlap(box,b) for b in blocked),'saved caption conflict');blocked.append(box)
 for d in a['dimensions']:
  p=saved[d['handle']]['geometry'][0]['grip']['points'][4];vertical=details[d['handle']]['vertical'];width=max(250,len(str(int(math.floor(d['expected']+.5))))*250*.65);box=(p[0]-150,p[1]-width/2,p[0]+150,p[1]+width/2) if vertical else (p[0]-width/2,p[1]-150,p[0]+width/2,p[1]+150);require(not any(overlap(box,b) for b in blocked),'saved dimension text conflict: '+d['handle']);blocked.append(box)
  clip=saved[d['handle']]['gapClip']
  if clip:require(box[0]>=clip['min'][0]-1e-6 and box[1]>=clip['min'][1]-1e-6 and box[2]<=clip['max'][0]+1e-6 and box[3]<=clip['max'][1]+1e-6,'dimension text clipped after avoidance: '+d['handle'])
 contract=read(S/'inputs/layer-standard-20261031/mapping.json');expected={x['name']:x for x in contract['layers']};require(a['layerAudit']['standardSHA256']==contract['sourceSHA256'],'layer standard differs')
 for layer in a['layerAudit']['layers']:
  e=expected[layer['name']];require(all(layer[k]==e[k] for k in ('colour','lineWeight','linetype')),'saved standard layer attribute wrong')
 for r in roots.values():require(r['layer'] in expected,'saved root layer not standard')

base.verify_opening_rows=verify_opening_rows

def verify_scene(analysis,a,f,scene,widths):
 r=base.verify_scene(analysis,a,f,scene,widths);r.update(formalCaptions=27,pendingCaptions=0,omittedBathroomDoors=sorted(OMITTED_DOORS),omittedBathroomWalls=sorted(OMITTED_WALLS),thirdRowTerminalLegs=8,continuousLocalChains=True,CNumbers=13);return r

def main():
 global GROUP_CAPTURE
 suffix='-v43' if '--baseline-v43' in sys.argv else '-v45' if '--v45' in sys.argv else '-v44'
 n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json');p=Path(n['output']);analysis=read(p/'analysis.json');a=read(p/'native-annotations.json');f=read(p/'native-foundation.json')
 require(sha(p/'analysis.json')==n['analysisSHA256'] and sha(p/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']==v['sourceSHA256'],'native DWG/analysis binding differs');require(sha(Path(v['PDF']))==v['PDFSHA256'],'internal preview changed');cache=Path(read(Path(n['binding']['inputManifest']))['cache']['path']);require(sha(cache)==n['binding']['cacheSHA256'],'cache differs');widths={row[1]:(float(row[6])+float(row[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines() if (row:=line.split()) and row[0]=='W'};scenePath=Path(v['job'])/'stage2-preview-native.json';require(sha(scenePath)==v['nativeReportSHA256'],'saved scene differs');GROUP_CAPTURE=read(p/'endpoint-capture.json');require(sha(p/'endpoint-capture.json')==n.get('endpointCaptureSHA256'),'group public capture hash differs')
 result=verify_scene(analysis,a,f,read(scenePath),widths);result.update(contract='bcp.annotation-simplification-release.v1',nativeRun=str(S/f'evidence/native-run{suffix}.json'),DWGSHA256=n['DWGSHA256'],PDFSHA256=v['PDFSHA256'],analysisSHA256=n['analysisSHA256'],annotationsSHA256=sha(p/'native-annotations.json'),gateDependencySHA256=sha(S/'verify_unified_annotations.py'))
 if '--check-only' not in sys.argv:
  out=S/f'evidence/annotation-simplification-release{suffix}{"-final-reviewed" if "--final-receipt" in sys.argv else ""}.json';require(not out.exists(),'receipt exists');out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
 print(json.dumps({k:v for k,v in result.items() if k not in ('decisions','chains','supportRangesByAxis')},ensure_ascii=False))
if __name__=='__main__':main()
