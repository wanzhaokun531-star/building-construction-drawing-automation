"""Accept actual public text, native root, original protection and every saved state."""
import json,sys
from pathlib import Path
def visible(value):
 if not isinstance(value,dict) or value.get('completed') is not True or value.get('ownershipSafe') is not True or value.get('error') is not None:return None
 return [x.get('plainText') for x in value.get('texts',[])]
def typography(value):
 return [{k:x.get(k) for k in ('position','alignmentPoint','height','rotation')} for x in (value or {}).get('texts',[])]
def inspect(r):
 checks=[]
 def c(ok,name):checks.append((bool(ok),name))
 c(r.get('error') is None,'native operation completed')
 c(r.get('contract')=='stage1.native-public-text-patch.v1','known contract')
 c(all(r.get(k) is False for k in ('COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten')),'native route and frozen source protection')
 trace=r.get('commandTrace') or {};c(trace.get('lost') is False and trace.get('commands')==[],'complete trace contains no nested business commands')
 original=r.get('originalBefore',[]);c(len(original)==213 and original==r.get('originalAfter'),'original213 public geometry retained')
 c((r.get('standardControl') or {}).get('entModStatus')==5100,'native entity-data standard positive control returned RTNORM')
 states=r.get('states',[]);source=visible(r.get('sourceText'));c(source is not None and len(source)==1,'independent source text')
 expected=['C09235','待确认',source[0] if source else None]
 c(len(states)==3 and [s.get('expected') for s in states]==expected,'three distinct literal states including reverse restore')
 anchor=typography(states[0].get('before')) if states else []
 for i,s in enumerate(states):
  c(s.get('api',{}).get('entModStatus')==5100,f'state{i} native EntMod RTNORM')
  c(visible(s.get('live'))==[s.get('expected')] and visible(s.get('saved'))==[s.get('expected')],f'state{i} actual literal live and fresh saved reader')
  c(s.get('liveRootType')==s.get('savedRootType')=='TDbText' and s.get('liveModelCount')==s.get('savedModelCount')==214,f'state{i} actual TCH roots and model counts')
  c(s.get('saveStatus')==0 and s.get('savedOriginal')==original and visible(s.get('savedSource'))==source,f'state{i} saved source originals protected')
  c(typography(s.get('live'))==typography(s.get('saved'))==anchor,f'state{i} insertion and typography retained')
  before=s.get('api',{}).get('beforePublicBag',{});after=s.get('api',{}).get('afterPublicBag',{});saved=s.get('savedPublicBag',{})
  def other(b):return [(x.get('code'),x.get('value')) for x in b.get('stringGroups',[]) if x.get('code')!=1]
  def text(b):return [x.get('value') for x in b.get('stringGroups',[]) if x.get('code')==1]
  c(all(b.get('available') is True and b.get('truncated') is False for b in (before,after,saved)) and other(before)==other(after)==other(saved) and text(after)==text(saved)==[s.get('expected')],f'state{i} public group1 matches independently read text; other strings unchanged')
 c(r.get('businessContentWritePassed') is True,'probe did not report unsupported as success')
 return checks
if __name__=='__main__':
 r=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(r);bad=[name for ok,name in checks if not ok]
 for name in bad:print('FAIL:',name)
 print(f'assertions={len(checks)} passed={len(checks)-len(bad)} failed={len(bad)}');raise SystemExit(bool(bad))
