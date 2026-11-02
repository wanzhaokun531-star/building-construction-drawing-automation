"""Require independent displayed values, real TCH types and preserved source roots."""
import json,sys
from pathlib import Path
def inspect(r):
 rows=[]
 def c(ok,n):rows.append((bool(ok),n))
 def text(v):return [x.get('plainText') for x in v.get('texts',[])]
 c(r.get('contract')=='stage1.native-public-symbol-patch.v1' and r.get('error') is None and r.get('passed') is True,'native symbol roundtrip completed')
 c(r.get('commandTrace')==dict(lost=False,commands=[]) and all(r.get(k) is False for k in ['COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten','phase2Started']),'SDK only isolated route')
 original=r.get('originalBefore',[]);c(len(original)==213 and original==r.get('originalAfter'),'original213 public states retained')
 c(r.get('type') in ['TDbSymbElevation','TDbSymbSection'] and bool(r.get('target')),'real TCH symbol case selected')
 elevation=r.get('type')=='TDbSymbElevation';old='-0.030' if elevation else '1';next_value='0.123' if elevation else '12';n=1 if elevation else 2
 c(text(r.get('sourceVisible',{}))==[old]*n,'source visible literals independently match public field')
 control=r.get('standardPositiveControl') or {};c(control.get('entModStatus')==5100 and control.get('entUpdStatus')==5100,'ordinary native SDK positive control')
 states=r.get('states',[]);c(len(states)==2 and [s.get('expected') for s in states]==[next_value,old],'forward and restored values both covered')
 for s in states:
  c(s.get('api',{}).get('entModStatus')==5100 and s.get('api',{}).get('entUpdStatus')==5100,'actual SDK status')
  c(s.get('changed') is True and s.get('reopened') is True and s.get('saveStatus')==0,'actual changed state saved and reopened')
  c(s.get('liveRootType')==r.get('type')==s.get('savedRootType'),'TCH type retained')
  c(s.get('liveModelCount')==214==s.get('savedModelCount') and s.get('savedProtected')==original,'one symbol added and original213 protected')
  c(text(s.get('live',{}))==[s.get('expected')]*n==text(s.get('saved',{})),'independent displayed value live and saved')
  bag=s.get('savedPublicBag',{});c(bag.get('available') is True and [x.get('value') for x in bag.get('stringGroups',[]) if x.get('code')==1]==[s.get('expected')],'saved public group matches displayed value')
 return rows
if __name__=='__main__':
 checks=inspect(json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig')));bad=[n for ok,n in checks if not ok]
 for n in bad:print('FAIL:',n)
 print(f'assertions={len(checks)} passed={len(checks)-len(bad)} failed={len(bad)}');raise SystemExit(bool(bad))
