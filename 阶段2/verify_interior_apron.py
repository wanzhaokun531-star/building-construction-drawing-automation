"""Actual direct TCH editing, user-directed chain layout and independent GEOS apron gate."""
from pathlib import Path
import sys,json,math,subprocess
import verify_anchored_direct_dimensions as base
from verify_unified_annotations import S,FROZEN,read,sha,require,near
sys.path.insert(0,str(S/'tools/geometry_packages'))
import shapely
from shapely.geometry import Polygon,LineString,Point,box
from shapely.ops import unary_union
EPS=1e-6
ACTIONS={'CD95':'OMIT','CD9E':'OMIT','CFB7':'OMIT','CDA3':'OMIT','CDA7':'OMIT','D006':'LEFT','CDB7':'LEFT','CD88':'DOWN'}
def verify_scene(analysis,a,f,scene,widths):
 annotations=a
 require(annotations.get('allDirectDimensions'),'not all generated dimensions are direct TCH roots')
 policy=annotations.get('interiorDimensionReview',{});user=read(S/'inputs/interior-dimension-decisions-v1.json');require(policy.get('enabled') and user['sourceSHA256']==FROZEN and sha(Path(user['path']))==user['SHA256']==policy.get('decisionsSHA256'),'interior decision source changed');require({x['wall']:x['action']for x in policy['decisions']}==ACTIONS and len(policy['decisions'])==8,'actual fixed input decisions differ');require(set(policy['omittedWalls'])=={k for k,v in ACTIONS.items()if v=='OMIT'},'omitted internal chain coverage wrong')
 result=base.verify_scene(analysis,a,f,scene,widths)
 roots={r['handle']:r for r in scene['originalBefore']};note=roots[annotations['apron']['note']];require(note['extentStatus']==0,'apron note bounds unavailable')
 for original in roots.values():
  if original['type']not in ('TDbText','TDbSymbElevation'):continue
  overlap=note['min'][0]<original['max'][0]+80 and note['max'][0]>original['min'][0]-80 and note['min'][1]<original['max'][1]+80 and note['max'][1]>original['min'][1]-80;require(not overlap,'apron note overlaps original room/elevation: '+original['handle'])
 saved={r['handle']:r for r in scene['dimensions']};details={r['handle']:r for r in annotations['dimensionDetails']};walls={}
 cache=Path(read(S/'inputs/frozen-scheme-v2.json')['cache']['path'])
 for line in cache.read_text(encoding='utf-8-sig').splitlines():
  row=line.split()
  if row and row[0]=='W':walls[row[1]]=list(map(float,row[2:8]))
 require(len(annotations['dimensions'])==106 and annotations['allDimensionEditChecks']==106,'native baseline/measuring grip proof incomplete');groups={}
 for d in annotations['dimensions']:
  r=roots.get(d['handle']);require(r and r['type']=='TDbDimension2' and not r['proxy'] and saved[d['handle']]['gapClip'] is None,'dimension remains blocked/exploded or nested');info=details[d['handle']];key=(d['row'],info['sourceWall']or d['side']);groups.setdefault(key,[]).append(d)
 joints=0
 for ds in groups.values():
  ds.sort(key=lambda d:d['axisFrom'])
  for da,db in zip(ds,ds[1:]):
   if math.dist(details[da['handle']]['measuredB'],details[db['handle']]['measuredA'])<EPS:joints+=1
 require(annotations['sharedJointEditChecks']==joints==68 and annotations['chainMoveChecks']==len(groups)==34,'whole chain/shared point editing proof incomplete')
 for w,action in ACTIONS.items():
  ds=[d for d in annotations['dimensions']if details[d['handle']]['sourceWall']==w]
  if action=='OMIT':require(not ds,'user omitted chain returned');continue
  require(len(ds)==3,'directed chain missing');v=walls[w];across=v[0]if action=='LEFT'else v[1];half=(v[4]+v[5])/2;baselines={round(details[d['handle']]['baseline'],6)for d in ds};require(len(baselines)==1 and next(iter(baselines))<=across-half-650+EPS,'directed chain not moved left/down as a whole');require(all(d['side']==('left'if action=='LEFT'else'bottom')for d in ds),'moved chain side metadata wrong')
 for wall in ('CD88','CDBC'):require([round(d['expected'])for d in annotations['dimensions']if details[d['handle']]['sourceWall']==wall]==[700,2400,700],'retained dining/kitchen continuous chain damaged')
 ap=annotations.get('apron');require(ap and ap['enabled'] and near(ap['width'],600) and len(ap['referenceWidths'])==3 and all(near(v,600)for v in ap['referenceWidths']),'reference-measured 600 apron missing');require(ap['referencePolyline']=='19FE' and ap['referenceWallHandles']==['17F6','17F7','17F8'],'reference source identity wrong')
 numbers={x['handle']:x for x in analysis['openingNumbers']};roles={x['handle']:x['role']for x in analysis['wallRecognition']};expected_cuts=[]
 def add_expected(source,pa,pb):
  if math.dist(pa,pb)>EPS:expected_cuts.append((source,pa,pb))
 for c in analysis['localWallChains']:
  if roles[c['wall']]!='exterior':continue
  w=walls[c['wall']];dx,dy=w[2]-w[0],w[3]-w[1];length=math.hypot(dx,dy);ux,uy=dx/length,dy/length;off=(w[4]-w[5])/2;x,y=w[0]-uy*off,w[1]+ux*off;vertical=abs(ux)<EPS;across=x if vertical else y;half=(w[4]+w[5])/2
  for h in c['openings']:
   if numbers[h]['observedKind']not in ('D','G'):continue
   if numbers[h]['observedKind']=='G':
    group=next(g for g in base.prior.GROUP_CAPTURE['groups']if g['handle']==h);on=[q for q in sorted(set(tuple(p[:2])for p in group['points']))if abs((q[0]-x)*uy-(q[1]-y)*ux)<EPS and -EPS<=(q[0]-x)*ux+(q[1]-y)*uy<=length+EPS];pairs=[(a,b)for i,a in enumerate(on)for b in on[i+1:]if near(math.dist(a,b),numbers[h]['width'])];require(len(pairs)==1,'source entry group not unique');pa,pb=pairs[0]
   else:pa,pb=(q[:2]for q in roots[h]['grip']['points'][1:3])
   lo,hi=sorted((pa[1]if vertical else pa[0],pb[1]if vertical else pb[0]));found=0;body=analysis['wallOuterBodyContours'][0]
   for a,b in zip(body,body[1:]+body[:1]):
    if vertical!=near(a[0],b[0]):continue
    face=a[0]if vertical else a[1]
    if not near(abs(face-across),half):continue
    low,high=sorted((a[1]if vertical else a[0],b[1]if vertical else b[0]));start,end=max(low,lo),min(high,hi)
    if end-start>EPS:add_expected(h,[face,start]if vertical else[start,face],[face,end]if vertical else[end,face]);found+=1
   require(found==1,'source external entry does not uniquely project')
 step=[roots[h]for h in ('CEDF','CEE0','CEE1')];pxmin,pxmax=step[0]['min'][0],step[0]['max'][0];require(all(near(v['min'][0],pxmin)and near(v['max'][0],pxmax)and near(v['min'][1],v['max'][1])for v in step),'source platform outlines unaligned');pymin=min(v['min'][1]for v in step);pymax=roots['CD93']['min'][1];require(all(near(a,b)for a,b in zip(ap['porchBounds'],[pxmin,pymin,pxmax,pymax])),'platform bounds not actual source geometry');body=analysis['wallOuterBodyContours'][0]
 for a,b in zip(body,body[1:]+body[:1]):
  vertical=near(a[0],b[0]);across=a[0]if vertical else a[1]
  if not ((pxmin-EPS<=across<=pxmax+EPS)if vertical else(pymin-EPS<=across<=pymax+EPS)):continue
  lo,hi=sorted((a[1]if vertical else a[0],b[1]if vertical else b[0]));start,end=max(lo,pymin if vertical else pxmin),min(hi,pymax if vertical else pxmax)
  if end-start>EPS:add_expected('porch-step-envelope',[across,start]if vertical else[start,across],[across,end]if vertical else[end,across])
 require(len(ap['cuts'])==len(expected_cuts)==7,'actual source cut set incomplete')
 unmatched=expected_cuts.copy()
 for c in ap['cuts']:
  pa,pb=c['points'];hits=[i for i,(source,a,b)in enumerate(unmatched)if source==c['source']and((math.dist(pa,a)<EPS and math.dist(pb,b)<EPS)or(math.dist(pa,b)<EPS and math.dist(pb,a)<EPS))];require(len(hits)==1,'cut shifted/widened or not actual source entry/platform');unmatched.pop(hits[0])
 require(not unmatched,'source cut omitted')
 inner=Polygon(ap['body']);require(inner.is_valid and not inner.is_empty,'invalid native inner contour');source=Polygon(analysis['wallOuterBodyContours'][0]);require(inner.hausdorff_distance(source)<=EPS,'apron not tied to actual body');outer=inner.buffer(ap['width'],join_style=2);require(Polygon(ap['offsetDiagnosticOnly']).hausdorff_distance(outer)<=EPS,'apron outer offset is not600');band=outer.difference(inner);cut_boxes=[]
 for c in ap['cuts']:
  pa,pb=c['points'];hits=[]
  body=ap['body']
  for x,y in zip(body,body[1:]+body[:1]):
   edge=LineString([x,y])
   if edge.distance(Point(pa))<=EPS and edge.distance(Point(pb))<=EPS:hits.append((x,y))
  require(len(hits)==1,'cut not uniquely on actual wall');x,y=hits[0];length=math.dist(x,y);nx,ny=(y[1]-x[1])/length,-(y[0]-x[0])/length;cut=Polygon([pa,pb,[pb[0]+nx*ap['width'],pb[1]+ny*ap['width']],[pa[0]+nx*ap['width'],pa[1]+ny*ap['width']]]);cut_boxes.append(cut);band=band.difference(cut)
 expected=[]
 for ring in [band.boundary] if band.boundary.geom_type=='LineString' else band.boundary.geoms:
  coords=list(ring.coords)
  for x,y in zip(coords,coords[1:]):
   edge=LineString([x,y])
   if edge.length>EPS and edge.difference(inner.boundary).length>EPS:expected.append(edge)
 expected_joints=[];body=ap['body']
 for i,point in enumerate(body):
  prev,nxt=body[i-1],body[(i+1)%len(body)];u=[point[0]-prev[0],point[1]-prev[1]];v=[nxt[0]-point[0],nxt[1]-point[1]]
  if u[0]*v[1]-u[1]*v[0]<=EPS:continue
  if any(LineString([pa,pb]).distance(Point(point))<=EPS for _,pa,pb in expected_cuts):continue
  lu,lv=math.hypot(*u),math.hypot(*v);other=[point[0]+ap['width']*(u[1]/lu+v[1]/lv),point[1]-ap['width']*(u[0]/lu+v[0]/lv)];expected_joints.append((point,other))
 actual=[];listed=set()
 for path in ap['paths']:
  h=path['handle'];require(h not in listed,'duplicate apron root');listed.add(h);r=roots.get(h);require(r and r['type']=='AcDbPolyline' and not r['proxy'] and r['layer']=='A-GROUND' and r['stretch']['status']==0,'apron is not saved native standard polyline');points=[p[:2]for p in r['stretch']['points']];require(len(points)==len(path['points']) and all(math.dist(p,q)<=EPS for p,q in zip(points,path['points'])),'saved apron points differ');line=LineString(points+[points[0]]if path['closed']else points);curve=r['curve'];require(curve and all(curve[k]==0 for k in ('startStatus','endStatus','endParamStatus','distanceStatus')) and near(curve['endParam'],len(points)if path['closed']else len(points)-1) and math.dist(curve['start'][:2],points[0])<=EPS and math.dist(curve['end'][:2],points[0]if path['closed']else points[-1])<=EPS and near(curve['length'],line.length),'actual apron curve closure/shape differs from straight polyline metadata')
  if path['kind']=='outline':actual.append(line)
  else:
   matches=[i for i,(p,q)in enumerate(expected_joints)if (math.dist(points[0],p)<EPS and math.dist(points[1],q)<EPS)or(math.dist(points[0],q)<EPS and math.dist(points[1],p)<EPS)];require(len(matches)==1,'corner seam missing/replaced/moved off actual vertex');expected_joints.pop(matches[0]);require(path['kind']=='corner-joint' and len(points)==2 and abs(abs(points[1][0]-points[0][0])-ap['width'])<=EPS and abs(abs(points[1][1]-points[0][1])-ap['width'])<=EPS,'apron convex seam not actual600 miter')
  for cut in cut_boxes:require(line.intersection(cut.buffer(-EPS)).length<=EPS,'apron still crosses entry interruption')
 require(not expected_joints,'actual convex corner seam omitted');expected=unary_union(expected);actual=unary_union(actual);require(actual.hausdorff_distance(expected)<=EPS and abs(actual.length-expected.length)<=EPS*len(ap['paths']),'actual apron outline differs from independent GEOS band minus real cuts')
 require(annotations['apronRootCount']==len(ap['paths'])+1==14 and ap['note']in roots and roots[ap['note']]['type']=='AcDbMText' and roots[ap['note']]['layer']=='A-PUB_TEXT','apron roots/note missing');require(set(ap['pending'])=={'slope-and-discharge','wall-side-level-region-mapping','central-courtyard-grey-boundary-and-covered-walkway'} and not ap['drainGenerated'],'missing data boundary misrepresented');require(shapely.__version__=='2.1.2','QA geometry dependency changed')
 result.update(allDirectNativeDimensions=106,omittedInternalChains=5,directedInternalChains=3,savedNativeMeasuringGripEdits=106,sharedJointEdits=68,chainMoveChecks=34,apronWidth=ap['width'],apronRoots=14,apronOpenPaths=len(actual.geoms)if hasattr(actual,'geoms')else 1,independentApronGeometryEngine='GEOS/Shapely2.1.2',apronPending=ap['pending']);return result

def main():
 suffix=next(('-'+x[2:]for x in sys.argv if x.startswith('--v')and x[3:].isdigit()),'-v65');n=read(S/f'evidence/native-run{suffix}.json');v=read(S/f'evidence/native-preview-run{suffix}.json');p=Path(n['output']);scene=Path(v['job'])/'stage2-preview-native.json';require(sha(p/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']==v['sourceSHA256'] and sha(scene)==v['nativeReportSHA256'] and sha(Path(v['PDF']))==v['PDFSHA256'],'actual source binding differs');audit=Path(v['maskDocumentAudit']);q=read(audit);require(sha(audit)==v['maskDocumentAuditSHA256'] and q['masks']==4 and q['WIPEOUTFRAME']==q['frame70']==q['publicVisibleFrames']==0 and q['readOnly'],'actual document border audit failed');require(sha(p/'analysis.json')==n['analysisSHA256'] and sha(p/'endpoint-capture.json')==n['endpointCaptureSHA256'],'analysis/endpoints changed');base.prior.GROUP_CAPTURE=read(p/'endpoint-capture.json');cache=Path(read(Path(n['binding']['inputManifest']))['cache']['path']);require(sha(cache)==n['binding']['cacheSHA256'],'cache changed');widths={r[1]:(float(r[6])+float(r[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines()if(r:=line.split())and r[0]=='W'};result=verify_scene(read(p/'analysis.json'),read(p/'native-annotations.json'),read(p/'native-foundation.json'),read(scene),widths);result.update(contract='bcp.interior-apron-release.v1',DWGSHA256=n['DWGSHA256'],PDFSHA256=v['PDFSHA256'],gateScriptSHA256=sha(Path(__file__)),inputHashes={name:sha(S/name)for name in ('inputs/interior-dimension-decisions-v1.json','inputs/interior-dimension-decisions-v1.tsv','inputs/layer-standard-20261031/mapping.json')},geometryLibrarySHA256=sha(Path(shapely.lib.__file__)),geometryLibraryPath=str(Path(shapely.lib.__file__)),gateDependencies={name:sha(S/name)for name in ('verify_anchored_direct_dimensions.py','verify_annotation_simplification.py','verify_unified_annotations.py')})
 if '--check-only'not in sys.argv:
  out=S/f'evidence/interior-apron-release{suffix}-final.json';require(not out.exists(),'receipt exists');out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');subprocess.run(['pdftoppm','-scale-to','2400','-png','-singlefile',v['PDF'],str(S/f'evidence/native-preview{suffix}-page')],check=True)
 print(json.dumps({k:v for k,v in result.items()if k not in ('decisions','chains','supportRangesByAxis')},ensure_ascii=False))
if __name__=='__main__':main()
