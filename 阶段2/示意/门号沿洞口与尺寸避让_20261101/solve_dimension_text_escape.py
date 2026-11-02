"""Offline text-grip candidate proposal for six existing dimension chains only."""
from pathlib import Path
import json,math,hashlib
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def box(e):return [*e['min'][:2],*e['max'][:2]]
def shift(b,v):return [b[0]+v[0],b[1]+v[1],b[2]+v[0],b[3]+v[1]]
def hit(a,b,g=0):return a[0]<b[2]+g-1e-6 and a[2]>b[0]-g+1e-6 and a[1]<b[3]+g-1e-6 and a[3]>b[1]-g+1e-6
def segbox(a,b):return [min(a[0],b[0]),min(a[1],b[1]),max(a[0],b[0]),max(a[1],b[1])]
source=HERE/'door-caption-complete-and-dimension-plan.json';d=read(source)
otherTextPlanPath=HERE/'three-other-chains-text-grips.json';otherTextPlan=read(otherTextPlanPath) if otherTextPlanPath.exists() else {'plans':[]}
# Key=(chain handle, zero-based segment). Points are WCS/model millimetres.
preferred={1:{('846C',1):[39481.70152820693,31535.213801995444,0],('848C',2):[46431.70152820693,31215.213801956677,0]},2:{('8475',2):[88603.70152820226,35015.21380195621,0],('8478',2):[89903.70152818436,31215.213801956095,0],('847B',2):[92853.70152820693,31215.213801956677,0],('8481',0):[83700,32285.213801723497,0],('8481',1):[85903.70152820693,32995.21380149078,0]}}
result={'contract':'bcp.dimension-text-grip-escape-proposal.v1','source':str(source),'sourceSHA256':sha(source),'otherTextPlan':str(otherTextPlanPath) if otherTextPlanPath.exists() else None,'otherTextPlanSHA256':sha(otherTextPlanPath) if otherTextPlanPath.exists() else None,'scope':'Offline candidate data only, no CAD action or C++ change; parent must recompute contacts after overall merge and actual native shifts','clearancesModelMm':{'textAndBaseline':60,'wall':40,'opening':80},'clearanceStatus':'Previously used sample clearance choices, not newly user-specified construction values','floors':[]}
for f in d['floors']:
 n=f['floor'];p=read(f['actualPreview']);idx={e['handle']:e for e in p['originalBefore']};w=read(f['captionWitness']);digits={(r['handle'],i):{'handle':r['handle'],'segment':i,'point':t['position'][0],'bounds':t['fontBounds'],'value':t['value'],'n':r['segments']} for r in w['dimensionAudit'] for i,t in enumerate(r['texts'])};chains={r['handle']:r for r in w['dimensionAudit']};targets=preferred[n];changed=set(targets)
 # Selected overall-chain shifts are included conservatively with both old and translated glyph boxes,
 # because manually positioned dimension text can stay fixed when its baseline moves.
 baselineMoves={}
 otherTextMoves={(r['handle'],r['segment']):r for r in otherTextPlan['plans'] if r['floor']==n}
 if n==2:baselineMoves['8475']=[0,700,0]
 obstacles=[]
 doors={r['handle']:r for r in f['candidates']};annotationMoves={r['handle']:r for r in f['annotationMoves']}
 for e in idx.values():
  if e['min'] is None:continue
  h=e['handle'];typ=e['type']
  if typ=='TDbWall':obstacles.append(('wall',h,box(e),40))
  elif typ in ['TDbOpening','TDbGroupOpening']:obstacles.append(('opening-and-swing-conservative-bounds',h,box(e),80))
  elif typ in ['TDbText','TDbSymbElevation','TDbDrawingName','AcDbText','AcDbMText'] and h not in doors and e['layer']!='BCP-OLD-AREA':obstacles.append(('fixed-text-area-or-name',h,annotationMoves.get(h,{}).get('newBounds',box(e)),60))
 for h,r in doors.items():obstacles.append(('new-door-caption',h,r['desiredBounds'],60))
 for h,c in chains.items():
  v=baselineMoves.get(h,[0,0,0]);a,b=c['baseline'];obstacles.append(('dimension-baseline',h,shift(segbox(a,b),v),60))
 for key,t in digits.items():
  if key in changed:continue
  h,i=key;v=baselineMoves.get(h,[0,0,0])
  if key in otherTextMoves:
   obstacles.append(('other-dimension-text-new-three-chain-plan',f'{h}:{i}',otherTextMoves[key]['fontBoundsAfter'],60));continue
  if h=='8475' and n==2:obstacles.append(('other-dimension-text',f'{h}:{i}',shift(t['bounds'],v),60))
  else:
   obstacles.append(('other-dimension-text',f'{h}:{i}',t['bounds'],60))
   if v!=[0,0,0]:obstacles.append(('other-dimension-text-may-follow-baseline',f'{h}:{i}',shift(t['bounds'],v),60))
 furniture=[e for e in idx.values() if e['min'] is not None and e['type']=='AcDbBlockReference' and e['layer']=='A-立面看线']
 candidates=[]
 for key,preferredPoint in targets.items():
  h,i=key;t=digits[key];chain=chains[h];gp=idx[h]['grip']['points'];v=baselineMoves.get(h,[0,0,0]);before=list(t['point']);afterBase=[before[j]+v[j] for j in range(3)] if h=='8475' and n==2 else before[:];baseBounds=shift(t['bounds'],v) if h=='8475' and n==2 else t['bounds']
  trial=[]
  for dx in [0,-100,100,-200,200,-400,400,-600,600,-800,800,-1000,1000,-1400,1400]:
   for dy in [0,-100,100,-200,200,-400,400,-600,600,-800,800,-1000,1000]:
    pt=[preferredPoint[0]+dx,preferredPoint[1]+dy,0];delta=[pt[j]-afterBase[j] for j in range(3)];b=shift(baseBounds,delta);contacts=[{'kind':kind,'handle':oh} for kind,oh,ob,g in obstacles if hit(b,ob,g)]
    if contacts:continue
    advisory=[e['handle'] for e in furniture if hit(b,box(e),0)]
    trial.append({'target':pt,'deltaFromAfterPrerequisite':delta,'boundsAfter':b,'hardContacts':contacts,'furnitureRootBoundsAdvisory':advisory,'displacementFromPreferred':math.hypot(dx,dy),'cost':math.hypot(dx,dy)+len(advisory)*200})
  trial.sort(key=lambda x:(x['cost'],math.dist(afterBase,x['target'])))
  startWindow={'min':[afterBase[0]-30,afterBase[1]-30,0],'max':[afterBase[0]+30,afterBase[1]+30,0]};win2=[afterBase[0]-30,afterBase[1]-30,afterBase[0]+30,afterBase[1]+30];textIndex=i+chain['segments']+3
  own=[{'index':j,'point':g} for j,g in enumerate(gp) if j!=textIndex and win2[0]<=g[0]<=win2[2] and win2[1]<=g[1]<=win2[3]];other=[]
  for oh,oe in idx.items():
   if oh==h or oe['type']!='TDbDimension2':continue
   for j,g in enumerate(oe['stretch']['points']):
    if win2[0]<=g[0]<=win2[2] and win2[1]<=g[1]<=win2[3]:other.append({'handle':oh,'index':j,'point':g,'mayBeHiddenSourceDimension':oh not in chains})
  candidates.append({'handle':h,'segment':i,'value':t['value'],'segments':chain['segments'],'textIndex':textIndex,'originalGrip':before,'gripAfterPrerequisiteBaseline':afterBase,'prerequisiteBaselineDelta':v if h=='8475' and n==2 else [0,0,0],'prerequisiteComment':'For 8475 the baseline first moves +700Y; default-text follow must be checked from actual native grips before the text selection window is used' if h=='8475' and n==2 else 'No baseline change needed for this text proposal','windowAfterPrerequisite':startWindow,'ownOtherGripInside':own,'otherChainGripsInside':other,'nativeSelectionGuard':'Only the target text grip must be inside; actual ssget and all unlocked entity identities still require live guard. Lock unrelated layers, not other same-layer dimensions. No prebuilt individual selection for STRETCH.','preferredPoint':preferredPoint,'candidateCount':len(trial),'candidates':trial[:12]})
 # Jointly select candidates that also avoid each other's final glyph boxes.
 selected={};order=sorted(range(len(candidates)),key=lambda k:candidates[k]['candidateCount'])
 def choose(k):
  if k==len(order):return True
  ci=order[k]
  for cand in candidates[ci]['candidates']:
   if any(hit(cand['boundsAfter'],other['boundsAfter'],60) for other in selected.values()):continue
   selected[ci]=cand
   if choose(k+1):return True
   del selected[ci]
  return False
 solved=choose(0)
 for i,c in enumerate(candidates):
  c['selected']=selected.get(i)
  if n==2 and c['handle']=='8481' and c['segment']==0:
   c['leaderReview']={'status':'actual-TCH-leader-render-required','illustrativeBaselineAttachment':[chains['8481']['baseline'][0][0],c['originalGrip'][1],0],'nearestDoorCaption':'84AA','illustrativeStraightLeaderDistanceBelowCaptionMm':55.00000023271423,'note':'An illustrative straight leader would not cross the caption box but has only about 55 mm gap; this is not the actual TCH leader shape or a passed leader-clearance check.'}
 result['floors'].append({'floor':n,'actualPreview':f['actualPreview'],'actualPreviewSHA256':sha(f['actualPreview']),'captionWitness':f['captionWitness'],'captionWitnessSHA256':sha(f['captionWitness']),'baselineAssumptions':[{'handle':h,'delta':v} for h,v in baselineMoves.items()],'baseline8475MustMoveBeforeText':n==2,'jointTextSolutionFound':solved,'textMoves':candidates,'limitations':['The baseline +700Y for 8475 is a proposal selected to clear the preferred 84AA caption, not a native result.','Manual/default text follow after a baseline shift must be read back; do not assume all text follows.','Furniture root bounds are advisory, not leaf collision proof.','New automatic leaders after text grip moves are not predicted; actual rendered inspection is required.','Crossing windows are selection aids, not drawing tolerances; calculate them from fresh actual native grips.']})
out=HERE/'dimension-text-escape-candidates.json';out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
for f in result['floors']:
 print('floor',f['floor'],'joint',f['jointTextSolutionFound'])
 for c in f['textMoves']:print(c['handle'],c['segment'],c['candidateCount'],c['selected']['target'] if c['selected'] else None,c['selected']['furnitureRootBoundsAdvisory'] if c['selected'] else None)
