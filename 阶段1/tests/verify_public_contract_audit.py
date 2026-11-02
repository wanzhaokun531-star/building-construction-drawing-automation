"""Read-only discovery acceptance; raw DXF presence is never a setter proof."""
import json,sys
from pathlib import Path
def inspect(r):
 rows=[]
 def c(ok,name):rows.append((bool(ok),name))
 c(r.get('contract')=='stage1.public-native-contract-audit.v1' and r.get('error') is None,'native discovery completed')
 c(r.get('commandTrace')=={'lost':False,'commands':[]} and all(r.get(k) is False for k in ('COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten')),'read-only SDK route')
 sources=r.get('sources',[]);c(len(sources)==2,'two frozen sources')
 for source in sources:
  c(source.get('modelCount') in (213,618) and len(source.get('originalBefore',[]))==source.get('modelCount') and source.get('originalBefore')==source.get('originalAfter'),'model public state unchanged')
  d=source.get('databaseInventory',{});c(d.get('complete') is True and d.get('openFailures')==0 and d.get('openedCount',0)>source.get('modelCount',0),'whole native handle inventory with no unread object omitted')
  samples=source.get('samples',[]);c(bool(samples) and len({x.get('type') for x in samples})==len(samples),'one actual sample per native TCH class')
  for x in samples:c(x.get('publicDxf',{}).get('available') is True and x.get('publicDxf',{}).get('truncated') is False and x.get('readError') is None,'actual public field bag '+x.get('handle','?'))
 return rows
if __name__=='__main__':
 r=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(r);bad=[n for ok,n in checks if not ok]
 for name in bad:print('FAIL:',name)
 print(f'assertions={len(checks)} passed={len(checks)-len(bad)} failed={len(bad)} DISCOVERY_ONLY_NOT_WRITE_CAPABILITY');raise SystemExit(bool(bad))
