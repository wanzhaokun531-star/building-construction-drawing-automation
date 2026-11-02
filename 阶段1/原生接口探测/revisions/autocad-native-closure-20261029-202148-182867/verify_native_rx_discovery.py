"""Accept read-only discovery, never infer a business setter from metadata."""
import json,sys
from pathlib import Path
def inspect(d):
 checks=[]
 def c(ok,n):checks.append((bool(ok),n))
 c(d.get('contract')=='stage1.native-rx-discovery.v1','native RX discovery contract')
 c(d.get('completed') is True and d.get('error') is None,'actual native discovery completed')
 c(d.get('COMUsed') is False and d.get('ComputerUseUsed') is False and d.get('TianzhengCommandsUsed') is False,'native API only')
 c(d.get('setValueCalls')==0 and d.get('sourceDrawingWritten') is False and d.get('phase2Started') is False,'read-only stage1 discovery')
 control=d.get('nativeControl') or {};c(control.get('rxName')=='AcDbLine' and control.get('memberCount',0)>0,'native positive control exposes metadata')
 sources=d.get('sources',[]);c([x.get('modelCount') for x in sources]==[213,618],'both frozen drawings inspected')
 found=set()
 for source in sources:
  c(source.get('originalBefore')==source.get('originalAfter') and len(source.get('originalBefore',[]))==source.get('modelCount'),'original public states unchanged')
  names=[x.get('metadata',{}).get('rxName') for x in source.get('samples',[])];found.update(names)
  c(bool(names) and len(names)==len(set(names)) and all(n and n.startswith('TDb') for n in names),'one observed real TCH entity per class')
  for sample in source.get('samples',[]):
   m=sample['metadata'];c(bool(sample.get('handle')) and m.get('memberLimitHit') is False and m.get('memberCount')==len(m.get('members',[])),'bounded actual metadata '+m['rxName'])
 c({'TDbWall','TDbOpening','TDbText','TDbDimension2','TDbAxisLabelSet','TDbDrawingName'}.issubset(found),'all six independently known native families sampled')
 return checks
if __name__=='__main__':
 d=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(d)
 for ok,n in checks:
  if not ok:print('FAIL:',n)
 failed=sum(not ok for ok,_ in checks);print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_READ_ONLY_DISCOVERY"}');raise SystemExit(bool(failed))
