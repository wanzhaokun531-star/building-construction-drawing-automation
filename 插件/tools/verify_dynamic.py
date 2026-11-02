from pathlib import Path
import json,math,hashlib,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
ROOT=Path(__file__).resolve().parents[2]
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def need(v,m):
 if not v:raise ValueError(m)
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def same_points(a,b):return len(a)==len(b) and all(near(x,y) for x,y in zip(a,b))
def verify_caption_collisions(a,w,c,p,analysis,graphic_probe):
 # Recompute contacts from the separately opened DWG, not c.targets.after.
 from verify_native import box,overlaps,points,stroke_hits_expanded_box,verify_stroke_witness,verify_furniture_witness,furniture_hit
 need(analysis is not None and graphic_probe is not None,'missing actual interior-door witnesses')
 idx={e['handle']:e for e in p['originalBefore']};targets={x['handle'] for x in a['captions'] if x['text'].startswith('M')};rows={x['handle']:x for x in c['targets']}
 need(set(rows)==targets and len(rows)==len(c['targets']),'door-caption relationship coverage differs')
 lower=c['interiorLowerPlacement'];roles={x['handle']:x['role'] for x in analysis['wallRecognition']};host={op:chain['wall'] for chain in analysis['localWallChains'] for op in chain['openings']}
 expected={x['opening']:host[x['opening']] for x in a['captions'] if x['handle'] in targets and roles[host[x['opening']]]!='exterior'};lowerRows={x['handle']:x for x in lower['targets']}
 need(lower['count']==len(lowerRows) and expected=={x['opening']:x['wall'] for x in lowerRows.values()},'interior-door source scope differs')
 probes={x['opening']:x['entities'] for x in graphic_probe};need(set(probes)=={x['opening'] for x in rows.values()},'source door primitive coverage differs')
 for meta in lowerRows.values():
  verify_stroke_witness(meta['strokes'],probes[meta['opening']],allow_diagonal=True);need(meta['swingArcDetected']==any(x['arc'] for x in meta['strokes']),'door swing classification differs')
 furniture=verify_furniture_witness(idx,lower);dimBoxes=[];saved={x['handle']:x for x in w['saved']};glyphs={x['handle']:x for x in c['dimensionAudit']};need(set(glyphs)==set(saved),'dimension glyph coverage differs')
 for h,row in glyphs.items():
  n=saved[h]['segments'];gp=idx[h]['grip']['points'];need(row['segments']==len(row['texts'])==n and points(row['baseline'],gp[:2]),'dimension glyph/baseline actual witness differs');lo,hi=gp[:2];dimBoxes.append([min(lo[0],hi[0])-10,min(lo[1],hi[1])-10,max(lo[0],hi[0])+10,max(lo[1],hi[1])+10])
  for j,text in enumerate(row['texts']):
   pos=text['position'][0];b=text['fontBounds'];need(near(pos,gp[n+3+j]) and abs(text['value']-math.dist(gp[2+j],gp[3+j]))<1e-6 and text['height']>0 and b[0]<b[2] and b[1]<b[3] and b[0]<=pos[0]<=b[2] and b[1]<=pos[1]<=b[3],'dimension glyph differs from actual chain');dimBoxes.append(b)
 openings=[e for e in idx.values() if e['type'] in ('TDbOpening','TDbGroupOpening')];fixed=[e for e in idx.values() if e['type'] in ('TDbText','TDbSymbElevation','TDbDrawingName','AcDbText','AcDbMText') and e['handle'] not in targets];walls=[e for e in idx.values() if e['type']=='TDbWall']
 for h,row in rows.items():
  e=idx[h];pos=e['stretch']['points'][0];aa,bb=row['anchorA'][0],row['anchorB'][0];vertical=abs(aa[0]-bb[0])<1e-6;along=int(vertical);normal=1-along;anchors=idx[row['opening']]['grip']['points'][1:3];need(points([aa,bb],anchors) or points([bb,aa],anchors),'caption bound to wrong actual opening');need(row['styleUnchanged'],'door caption style changed')
  if h in lowerRows:
   meta=lowerRows[h];need(not meta.get('keptForReview',False),'unapproved dynamic caption position exception');own=idx[row['opening']];ob=box(own);negative=aa[normal]-ob[normal];positive=ob[normal+2]-aa[normal];side=(1 if positive>negative else -1) if meta['swingArcDetected'] else row['side'];need(meta['side']==side and side*(pos[normal]-aa[normal])>0,'door caption switched actual swing side')
   low,high=sorted([aa[along],bb[along]]);width,height=e['max'][0]-e['min'][0],e['max'][1]-e['min'][1]
   if vertical:need(low-height/2-90-1e-6<=pos[1]<=low+(high-low)*.75+1e-6 and 180+width/2-1e-6<=side*(pos[0]-aa[0])<=180+width/2+1500+1e-6,'caption outside lower opening region')
   else:
    base=aa[1]+180+height/2 if side>0 else ob[1]-height/2-90;need(low-width/2-180-1e-6<=pos[0]<=high+width/2+180+1e-6 and -1e-6<=side*(pos[1]-base)<=900+1e-6 and (side<0 or pos[1]<=aa[1]+(high-low)*.65+1e-6),'caption outside lower opening region')
   need(not any((overlaps(box(e),[min(x['a'][0],x['b'][0]),min(x['a'][1],x['b'][1]),max(x['a'][0],x['b'][0]),max(x['a'][1],x['b'][1])],80.01) if not x['arc'] and abs(x['a'][0]-x['b'][0])>1e-6 and abs(x['a'][1]-x['b'][1])>1e-6 else stroke_hits_expanded_box(x,box(e))) for x in meta['strokes']),'caption overlaps actual door strokes');need(not any(overlaps(box(e),box(o),80) for o in openings if o['handle']!=row['opening']),'caption overlaps another opening')
  else:
   need(row['side']*(pos[normal]-aa[normal])>0 and min(aa[along],bb[along])-1e-6<=pos[along]<=max(aa[along],bb[along])+1e-6,'exterior caption side/projection differs');need(not any(overlaps(box(e),box(o),80) for o in openings),'caption overlaps door/window')
  need(not any(overlaps(box(e),box(o),40) for o in walls),'caption overlaps actual wall');need(not furniture_hit(box(e),furniture),'caption overlaps actual furniture');need(not any(overlaps(box(e),box(o),60) for o in fixed) and not any(overlaps(box(e),box(idx[k]),60) for k in targets if k!=h),'caption overlaps actual fixed text');need(not any(overlaps(box(e),b,60) for b in dimBoxes),'caption overlaps dimension glyph/baseline')
def verify_model(plan,a,w,c,p,audit,analysis=None,graphic_probe=None):
 need('nativeAnnotations' in audit,'actual annotation witnesses absent')
 need(p['error'] is None and p['originalBefore']==p['originalAfter'],'preview altered drawing or failed')
 idx={e['handle']:e for e in p['originalBefore']};need(len(idx)==p['finalModelCount'],'duplicate/missing model roots')
 actual={e['handle']:e for e in audit['nativeAnnotations']};need(len(actual)==len(audit['nativeAnnotations']),'duplicate actual annotation witness')
 rows=[r.split() for r in plan.splitlines()[1:]]
 for row in rows:
  tag,h=row[:2];need(h in idx,'missing retained architecture '+h);e=idx[h];need(not e['proxy'],'proxy architecture')
  if tag=='W':need(e['type']=='TDbWall' and near(e['curve']['start'][:2],list(map(float,row[2:4]))) and near(e['curve']['end'][:2],list(map(float,row[4:6]))),'wall geometry changed '+h)
  elif tag=='O':need(e['type'] in ('TDbOpening','TDbGroupOpening') and near(e['grip']['points'][1][:2],list(map(float,row[2:4]))) and near(e['grip']['points'][2][:2],list(map(float,row[4:6]))),'opening geometry changed '+h)
  elif tag=='R':need(e['type']=='TDbText' and near(e['min'][:2],list(map(float,row[2:4]))) and near(e['max'][:2],list(map(float,row[4:6]))),'source text bounds changed '+h)
 need(w['savedReopened'] and w['protectedAfterIdentical'] and c['savedReopened'] and c['allProtectedUnchanged'] and c['wholeDimensionsUnchanged'],'native preservation checks failed')
 need(w['chains']==len(w['saved'])==len(w['live']),'chain count differs')
 need(sum(x['segments'] for x in w['saved'])==w['sourceSegments'],'dimension segment coverage differs')
 for chain,live in zip(w['saved'],w['live']):
  h=chain['handle'];n=chain['segments'];need(h in idx and h in actual,'whole chain absent from actual DWG')
  need(idx[h]['type']==actual[h]['type']=='TDbDimension2' and actual[h]['visible'],'whole chain is hidden or wrong type')
  gp=actual[h]['grips'];need(same_points(gp,idx[h]['grip']['points']) and same_points(gp,chain['grips']),'actual whole-chain grips differ')
  need(len(gp)==2*n+6 and len(actual[h]['values'])==n and near(actual[h]['values'],chain['values']),'whole-chain shape/value differs')
  nodes=gp[2:n+3];distances=[math.dist(a,b) for a,b in zip(nodes,nodes[1:])];need(near(distances,actual[h]['values']),'dimension value differs from actual measuring nodes')
  need(live['exercisePerformed'] and live['singleGripWholeBaseline'] and live['sharedNodeChecks']==n-1,'live chain editing not exercised')
  need(chain['singleNativeEntity'] and chain['ownerModel'] and chain['originalStyleAndTextVerified'],'whole-chain object/style lost')
 changed={x['handle']:x for x in c['targets']};need(len(changed)==c['targetCount'],'door caption coverage differs')
 need(len(a['captions'])==len({x['opening'] for x in a['captions']}),'duplicate opening captions')
 for caption in a['captions']:
  h=caption['handle'];need(h in actual and h in idx and actual[h]['type']=='AcDbText','caption absent or wrong type')
  observed=actual[h];need(observed['text']==caption['text'] and abs(observed['rotation']-caption['rotation'])<1e-6,'actual caption content or rotation differs')
  wanted=changed[h]['after'][0] if h in changed else [*caption['position'],0]
  need(near(observed['position'][0],wanted) and near(idx[h]['stretch']['points'][0],wanted),'caption position differs')
 for caption in changed.values():
  e=idx[caption['handle']];need(near([*e['min'][:2],*e['max'][:2]],caption['textBounds']),'actual door text bounds differ')
 expectedAxes={(x['vertical'],x['label']):x for x in a['axes']};need(len(expectedAxes)==len(audit['axes']),'axis carriers missing')
 for axis in audit['axes']:
  name=axis['name'].split('_');key=(name[-2]=='V',name[-1]);need(key in expectedAxes,'unexpected numbered axis')
  expected=expectedAxes[key];need(len(axis['circles'])==2,'native axis circle pair lost')
  for i,circle in enumerate(axis['circles']):
   point=circle['centre'][0];need(abs(circle['radius']-400)<1e-6 and near(point,expected['circles'][i]['centre']),'actual axis circle geometry differs')
   inside=axis['min'][0]<=point[0]<=axis['max'][0] and axis['min'][1]<=point[1]<=axis['max'][1]
   visible=not inside if axis['inverted'] else inside;need(visible==expected['showNegative' if i==0 else 'showPositive'],'actual axis endpoint visibility differs')
 groups={x['name']:x for x in audit['groups']};need(len(groups)==a['logicalFullAxes'],'logical axis groups missing')
 walls=[list(map(float,r[2:8])) for r in rows if r[0]=='W']
 for line in a['fullAxisLines']:
  name='BCP_GRID_'+('V_' if line['vertical'] else 'H_')+line['label'];need(name in groups,'logical axis group missing '+name)
  expectedPieces={x['handle']:[x['start'],x['end']] for x in line['pieces']};need({x['handle'] for x in groups[name]['lines']}==set(expectedPieces),'logical axis pieces missing')
  coordinate=line['station'];supported=False
  for x,y,X,Y,left,right in walls:
   length=math.hypot(X-x,Y-y);cx=x-(Y-y)/length*(left-right)/2;cy=y+(X-x)/length*(left-right)/2
   if (abs(X-x)<1e-6)==line['vertical'] and abs((cx if line['vertical'] else cy)-coordinate)<1e-6:supported=True
  need(supported,'logical axis has no real source-wall centre')
  for segment in groups[name]['lines']:
   need(same_points(segment['curve'],expectedPieces[segment['handle']]),'logical axis visible interval differs')
   need(segment['linetype']=='DOTE','axis linetype differs')
   for point in segment['curve']:need(abs(point[0 if line['vertical'] else 1]-line['station'])<1e-6,'logical axis station changed')
 verify_caption_collisions(a,w,c,p,analysis,graphic_probe)
 return dict(modelRoots=len(idx),dimensionChains=w['chains'],dimensionSegments=w['sourceSegments'],captions=len(a['captions']),axisCarriers=len(audit['axes']),logicalAxes=len(groups))

def verify_run(version):
 b=read(ROOT/f'阶段2/evidence/native-host-binding-{version}.json');job=Path(b['job']);batch=read(job/'product-result.json');need(batch['completed'] and batch['error'] is None,'batch generation did not complete');need(len(batch['outputs'])==2,'two separate outputs missing');summaries=[]
 for floor in [1,2]:
  result=read(job/f'floor-{floor}/product-result.json');root=Path(result['records']);preview=read(ROOT/f'插件/evidence/preview-dynamic-{version}-floor{floor}.json');p=read(Path(preview['job'])/'stage2-preview-native.json');audit=read(Path(preview['job'])/'product-axis-document-audit.json')
  digest=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
  need(result['completed'] and digest(result['output'])==result['outputSHA256']==preview['sourceSHA256']==digest(Path(preview['job'])/'control.dwg'),'DWG/preview identity differs')
  need(preview['nonce']==b['nonce'] and preview['source']==result['output'] and preview['error'] is None,'preview host/source differs')
  summary=verify_model((root/'过程/foundation/current-plan.tsv').read_text(encoding='utf-8'),read(root/'过程/foundation/annotations.json'),read(root/'过程/join/whole-dimension-chains.json'),read(root/'过程/caption/door-caption-clearance.json'),p,audit,read(root/'过程/foundation/analysis.json'),read(root/'过程/caption/door-graphic-probe.json'))
  need(summary['modelRoots']==result['finalRoots'] and summary['dimensionChains']==result['dimensionChains'] and summary['dimensionSegments']==result['dimensionSegments'],'native result counters differ')
  summary.update(floor=floor,DWG=result['output'],SHA256=result['outputSHA256']);summaries.append(summary)
 return summaries
if __name__=='__main__':print(json.dumps(verify_run(sys.argv[1]),ensure_ascii=False))
