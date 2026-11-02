import json,sys
from pathlib import Path
from verify_public_text_patch import visible,typography
def inspect(r):
 checks=[]
 def c(ok,name):checks.append((bool(ok),name))
 c(r.get('contract')=='stage1.native-text-only-dxf.v1' and r.get('error') is None,'known native operation completed')
 c(r.get('commandTrace')=={'lost':False,'commands':[]} and all(r.get(k) is False for k in ('COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten')),'native SDK only route')
 original=r.get('originalBefore',[]);source=visible(r.get('sourceText'));states=r.get('states',[])
 c(len(original)==213 and original==r.get('originalAfter'),'all213 original public states retained')
 c(r.get('exportStatus')==0 and r.get('unchangedRoundtripPassed') is True and visible(r.get('unchangedRoundtrip'))==source and source is not None,'unmodified isolated TCH DXF text roundtrip')
 c(len(states)==3 and [s.get('expected') for s in states]==['C09235','待确认',source[0] if source else None],'three literal states and reverse restoration')
 anchor=typography(states[0].get('before')) if states else []
 for i,s in enumerate(states):
  c(visible(s.get('importedDonor'))==visible(s.get('live'))==visible(s.get('saved'))==[s.get('expected')],f'state{i} actual imported, native copy and fresh saved literal')
  c(s.get('copyFromStatus')==s.get('saveStatus')==0,f'state{i} native same-class copyFrom and save succeeded')
  c(s.get('liveRootType')==s.get('savedRootType')=='TDbText' and s.get('liveModelCount')==s.get('savedModelCount')==214,f'state{i} native TCH roots retained')
  c(s.get('savedOriginal')==original and visible(s.get('savedSource'))==source,f'state{i} saved original213 and room name retained')
  c(typography(s.get('live'))==typography(s.get('saved'))==anchor,f'state{i} insertion and typography retained')
 c(r.get('businessContentWritePassed') is True,'unsupported is not PASS')
 return checks
if __name__=='__main__':
 r=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(r);bad=[n for ok,n in checks if not ok]
 for n in bad:print('FAIL:',n)
 print(f'assertions={len(checks)} passed={len(checks)-len(bad)} failed={len(bad)}');raise SystemExit(bool(bad))
