"""Independent candidate-publication and persisted native-edit acceptance."""
import json,math,sys
from pathlib import Path
def close(a,b):return isinstance(a,list) and isinstance(b,list) and len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def inspect(d):
 checks=[]
 def c(ok,name):checks.append((bool(ok),name))
 kind=d.get('contract');c(kind in ('stage1.native-candidate-discard.v1','stage1.native-reopen-dimension.v1','stage1.native-reopen-dimension.v2'),'known bounded native lifecycle contract')
 c(d.get('completed') is True and d.get('error') is None,'native probe completed')
 c(d.get('COMUsed') is False and d.get('ComputerUseUsed') is False and d.get('TianzhengCommandsUsed') is False,'pure native operations')
 c(d.get('sourceCount')==213 and len(d.get('originalBefore',[]))==213,'whole original scheme protected')
 c(d.get('phase2Started') is False,'stage1 only')
 if kind=='stage1.native-candidate-discard.v1':
  c(d.get('originalBefore')==d.get('originalFresh'),'fresh source unchanged after all cases')
  cases=d.get('cases',[]);c([x.get('kind') for x in cases]==['validation-failure','publication-collision','success'],'all three real publication cases')
  for x in cases:
   p=x.get('kind','?');c(x.get('error') is None,p+' no unexpected error')
   c(x.get('candidateCount')==214 and x.get('candidateSaveStatus')==0,p+' real private candidate persisted')
   c(x.get('candidateDestroyed') is True and x.get('candidateExistsAfter') is False,p+' private database destroyed and temporary file absent')
   if p=='validation-failure':c(x.get('injectedFailureCaught') is True and x.get('publicationExists') is False and x.get('published') is False,p+' no output published')
   elif p=='publication-collision':
    c(x.get('moveAttempted') is True and x.get('published') is False and x.get('moveError') in (80,183),p+' no replacement on collision')
    c(x.get('collisionSentinelUnchanged') is True and x.get('collisionSentinelRemoved') is True,p+' owned preexisting target protected and cleaned')
   else:
    c(x.get('published') is True and x.get('publicationExists') is True and x.get('publishedCount')==214,p+' validated publication exists')
    c(x.get('publishedOriginal')==d.get('originalBefore'),p+' published originals retained')
 elif kind in ('stage1.native-reopen-dimension.v1','stage1.native-reopen-dimension.v2'):
  c(d.get('sourceModelCount')==214 and d.get('originalBefore')==d.get('originalAfter')==d.get('originalSavedForward')==d.get('originalSavedRestored'),'all213 originals in live/saved stages retained')
  states=d.get('states',{});c(set(states)=={'before','forward','savedForward','restored','savedRestored'},'all five actual dimension states')
  base=(states.get('before') or {}).get('geometry',{});bg=base.get('grip',{}).get('points',[])
  for name,s in states.items():
   geo=s.get('geometry',{});g=geo.get('grip',{}).get('points',[]);read=s.get('readback',{})
   c(geo.get('type')=='TDbDimension2' and geo.get('proxy') is False,name+' actual native dimension')
   c(len(g)>=4 and len(bg)>=4,name+' actual public grips')
   if len(g)>=4 and len(bg)>=4:
    c(close(g[2],bg[2]) and close(g[3],bg[3]),name+' actual measurement points preserved')
    target=list(bg[0]);target[1]-=123 if name in ('forward','savedForward') else 0
    c(close(g[0],target),name+' actual line grip matches requested move/restore')
   if kind.endswith('.v1'):
    c(read.get('completed') is True and read.get('ownershipSafe') is True,name+' actual safe public readback')
    c('3600' in [t.get('plainText') for t in read.get('texts',[])],name+' actual visible3600 text')
   else:
    r=s.get('dimensionRepresentation') or {};items=r.get('representations',[])
    c(r.get('contract')=='stage1.native-dimension-representation.v1' and r.get('completed') is True and r.get('error') is None,name+' actual native dimension representation')
    c(r.get('ownershipSafe') is True and r.get('returnedCount')==r.get('deletedCount')==1,name+' exactly one returned transient released')
    c(r.get('rootBefore')==r.get('rootAfter') and r.get('rootExplodeStatus')==0,name+' original TCH root unchanged by public readback')
    c(r.get('renderedTextRead') is False and r.get('representationAppendedToDatabase') is False,name+' formatter evidence explicit; no rendered-text or object-replacement claim')
    c(len(items)==1,name+' one actual dimension representation')
    for item in items:
     value=item.get('measurement');c(item.get('type')=='AcDbAlignedDimension' and item.get('measurementStatus')==0 and isinstance(value,(int,float)) and abs(value-3600)<1e-6,name+' actual native measurement3600')
     c(item.get('formatStatus')==0 and item.get('plainFormattedMeasurement')=='3600',name+' actual native formatted3600')
     c(item.get('objectIdNull') is True and item.get('ownerIdNull') is True and item.get('databaseNull') is True,name+' public representation remains transient')
  c(d.get('forwardSaveStatus')==d.get('restoredSaveStatus')==0,'both edited states saved')
 return checks
if __name__=='__main__':
 p=Path(sys.argv[1]);d=json.loads(p.read_text(encoding='utf-8-sig')) if p.is_file() else {};checks=inspect(d)
 if d.get('contract')=='stage1.native-candidate-discard.v1':
  for case in d.get('cases',[]):
   checks.append((not Path(case.get('candidatePath','')).exists(),'physical private candidate absent '+str(case.get('kind'))))
   checks.append((Path(case.get('publicationPath','')).is_file() is (case.get('kind')=='success'),'physical publication presence '+str(case.get('kind'))))
 for ok,name in checks:
  if not ok:print('FAIL:',name)
 failed=sum(not ok for ok,_ in checks);print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_BOUNDED_NATIVE_LIFECYCLE"}');raise SystemExit(bool(failed))
