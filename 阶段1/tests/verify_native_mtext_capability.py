import json,sys,math
from pathlib import Path
def inspect(r):
 rows=[]
 def c(ok,n):rows.append((bool(ok),n))
 c(r.get('contract')=='stage1.native-mtext-capability.v1' and r.get('passed') is True and r.get('error') is None,'native MText completed')
 c(r.get('commandTrace')==dict(lost=False,commands=[]) and all(r.get(k) is False for k in ['COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten','phase2Started']),'native SDK only route')
 original=r.get('originalBefore',[]);c(len(original)==213 and original==r.get('originalAfter'),'original213 retained')
 states=r.get('states',[]);c(len(states)==3 and states[0].get('expectedContents')==states[-1].get('expectedContents') and states[0].get('expectedContents')!=states[1].get('expectedContents'),'edit and restore states')
 source=r.get('sourceText',{})
 for s in states:
  live=s.get('live',{});saved=s.get('saved',{});c(s.get('passed') is True and live==saved and saved.get('type')=='AcDbMText','actual native MText saved and reopened')
  c(s.get('savedModelCount')==214 and s.get('savedProtected')==original,'only new MText added')
  c(live.get('contents')==s.get('expectedContents') and live.get('plainText','').replace('\r\n','\n').replace('\r','\n')==s.get('expectedContents','').replace('\\P','\n'),'Chinese paragraph contents actual readback')
  c(live.get('rotation') is not None and math.isclose(live['rotation'],s.get('expectedRotation',float('nan')),abs_tol=1e-10),'rotation actual readback')
  c(live.get('styleHandle')==source.get('styleHandle') and live.get('height')==source.get('height') and live.get('width')==5000,'actual source font style and glyph height')
 return rows
if __name__=='__main__':
 checks=inspect(json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig')));bad=[n for ok,n in checks if not ok]
 for n in bad:print('FAIL:',n)
 print(f'assertions={len(checks)} passed={len(checks)-len(bad)} failed={len(bad)}');raise SystemExit(bool(bad))
