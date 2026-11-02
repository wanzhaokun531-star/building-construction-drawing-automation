"""Bind native AcPl symbol PDF to an explicit saved native business fixture."""
import hashlib,json,sys
from pathlib import Path
from pypdf import PdfReader

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))

if __name__=='__main__':
 report=Path(sys.argv[1]);source_report=Path(sys.argv[2]);d=read(report);s=read(source_report);checks=[]
 def check(ok,name):checks.append((bool(ok),name))
 check(d.get('contract')=='stage1.symbol-preview-native.v1' and d.get('error') is None and d.get('fixtureVerified') is True,'native symbol preview contract')
 check(s.get('completed') is True and s.get('contract') in ('stage1.symbol-business-native.v1','stage1.symbol-business-native.v2','stage1.symbol-business-native.v3','stage1.symbol-business-native.v4'),'explicit successful native business source')
 source=Path((s.get('snapshot') or {}).get('outputDWG',''));control=Path(d.get('controlDWG',''))
 check(source.is_file() and control.is_file() and digest(source)==digest(control),'exact saved source DWG bytes')
 check(len(d.get('originalBefore',[]))==215 and d.get('originalBefore')==d.get('originalAfter'),'all215 actual public states unchanged')
 check(d.get('globalsBefore')==d.get('globalsAfter') and bool(d.get('globalsBefore')),'plot globals restored')
 check(d.get('layoutBefore')==d.get('layoutAfter') and bool(d.get('layoutBefore')),'saved layout unchanged')
 check(d.get('dwgSaved') is False,'preview did not save DWG')
 page=d.get('page') or {};check(page.get('pipelineComplete') is True and page.get('engineDestroyed') is True and page.get('cleanupThrew') is False and page.get('error') is None,'actual native graphics pipeline completed')
 engine=page.get('engineStatuses') or {};check(set(engine)=={'create','beginPlot','beginDocument','beginPage','beginGraphics','endGraphics','endPage','endDocument','endPlot'},'all nine native graphics stages actually reported')
 for name,status in engine.items():check(type(status) is int and status==0,'native engine '+name)
 settings=page.get('validatedSettings') or {};check(settings.get('scale')==[1,100] and settings.get('useStandardScale') is False and settings.get('paperUnits')==1,'actual1:100 millimetre plotting without fit')
 check(settings.get('paperMM')==[420,297] and page.get('validateStatus')==0 and page.get('isValidated') is True,'actual A3 validated paper')
 pdf=Path(d.get('outputPDF',''));check(pdf.is_file() and page.get('fileObserved') is True and page.get('fileBytes')==pdf.stat().st_size,'actual native PDF file')
 reader=PdfReader(str(pdf));check(len(reader.pages)==1,'one actual PDF page')
 box=reader.pages[0].mediabox;mm=[float(box.width)*25.4/72,float(box.height)*25.4/72];check(abs(mm[0]-420)<1 and abs(mm[1]-297)<1,'PDF physical dimensions independently measured')
 for ok,name in checks:
  if not ok:print('FAIL:',name)
 failed=sum(not ok for ok,_ in checks);print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_NATIVE_SYMBOL_PREVIEW"}')
 raise SystemExit(bool(failed))
