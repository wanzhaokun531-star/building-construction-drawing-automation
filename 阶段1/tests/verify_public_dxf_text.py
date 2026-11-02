import json,sys
from pathlib import Path
from verify_public_text_patch import visible
def inspect(r):
 return [(r.get('error') is None,'native DXF completed'),(r.get('exportStatus')==r.get('importStatus')==0,'native SDK export and import successful'),(len(r.get('originalBefore',[]))==213 and r.get('originalBefore')==r.get('originalAfter')==r.get('importedOriginals') and r.get('importedCount')==213,'all original public states roundtrip unchanged'),(r.get('importedTextRootType')=='TDbText' and visible(r.get('sourceText'))==visible(r.get('importedText')) and visible(r.get('sourceText')) is not None,'actual TCH text and literal preserved'),(r.get('commandTrace')=={'lost':False,'commands':[]} and all(r.get(k) is False for k in ('COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten')),'native only route')]
if __name__=='__main__':
 r=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));c=inspect(r);bad=[name for ok,name in c if not ok]
 for x in bad:print('FAIL:',x)
 print(f'assertions={len(c)} passed={len(c)-len(bad)} failed={len(bad)}');raise SystemExit(bool(bad))
