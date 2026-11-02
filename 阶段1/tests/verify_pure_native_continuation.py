"""Independent observation/acceptance predicates; discovery is not capability PASS."""
from pathlib import PureWindowsPath
from pathlib import Path
import json,sys
def exact_font_loaded(row):
 bits=row.get('loadBits');files=row.get('resolvedFiles',[])
 def stem(value):return PureWindowsPath(value).stem.casefold() if value else ''
 primary=stem(row.get('declaredPrimary') or row.get('loadedPrimary'))
 big=stem(row.get('declaredBig'))
 face=row.get('before',{}).get('font',{}).get('typeface','')
 if face and (row.get('gsAfterTypeface')!=face or row.get('gsAfterFontStatus')!=0 or PureWindowsPath(row.get('declaredPrimary','')).suffix.casefold() not in ('.ttf','.ttc','.otf')):return False
 identity_same=False
 before=row.get('beforeResolvedFiles',[])
 if len(before)==len(files) and files:
  required={'volume','indexHigh','indexLow','bytes','writeHigh','writeLow'}
  identity_same=all(set(a.get('fileIdentity') or {})==required and a.get('fileIdentity')==b.get('fileIdentity') and a.get('fileExists') is True and b.get('fileExists') is True for a,b in zip(before,files))
 return (isinstance(bits,int) and bits&1==1 and bits&6==0 and (bits&0x18==0 or identity_same) and row.get('conversionStatus')==0
         and bool(primary) and primary==stem(row.get('loadedPrimary')) and big==stem(row.get('loadedBig'))
         and len(files)==(2 if big else 1) and all(x.get('status')==0 and x.get('fileExists') is True and x.get('path') for x in files))

def complete_native_catalog(report):
 before=report.get('originalBefore',[]);after=report.get('originalAfter',[]);samples=report.get('samples',[])
 expected={x.get('handle'):x.get('type') for x in before if str(x.get('type','')).startswith('TDb')}
 handles=[x.get('handle') for x in samples]
 return (report.get('error') is None and len(before)==report.get('expectedCount')==report.get('initialModelCount')==report.get('finalModelCount')
         and before==after and bool(expected) and set(handles)==set(expected) and len(handles)==len(set(handles))
         and all(x.get('returned') is True and x.get('error') is None and x.get('actualType')==expected.get(x.get('handle')) for x in samples))

def inspect(report,report_path=None):
 checks=[]
 def c(ok,name):checks.append((bool(ok),name))
 c(report.get('error') is None,'native invocation returned without an unhandled error')
 c(report.get('COMUsed') is False and report.get('ComputerUseUsed') is False and report.get('TianzhengBusinessCommandsUsed') is False,'public native SDK route')
 trace=report.get('commandTrace') or {};c(trace.get('lost') is False,'actual command trace complete')
 c(all(x.upper().lstrip('_.') in ('LOGFILEON','LOGFILEOFF') for x in trace.get('commands',[])),'no observed TCH business commands in this native probe')
 kind=report.get('contract')
 if kind=='stage1.native-semantic-catalog.v1':
  c(complete_native_catalog(report),'every original TCH entity listed exactly once with matching type and protected originals')
  c(report.get('drawingSaved') is False,'read-only catalog')
  c(report.get('logging',{}).get('restored') is True,'logging environment restored')
  if report_path is not None:
   log=Path((report.get('logFile') or {}).get('path',''))
   inside=log.resolve().is_relative_to(Path(report_path).parent.resolve())
   c(inside and log.is_file(),'raw catalog log belongs to this exact run')
   if inside and log.is_file():
    cp=(report.get('logFile') or {}).get('ansiCodePage',936);text=log.read_text(encoding='utf-8-sig' if cp==65001 else 'cp'+str(cp),errors='strict')
    for sample in report.get('samples',[]):
     capture=sample.get('capture',{});begin=capture.get('beginMarker');end=capture.get('endMarker')
     c(bool(begin and end) and text.count(begin)==text.count(end)==1 and text.index(begin)<text.index(end),'unique actual raw block '+sample.get('handle','?'))
 elif kind=='stage1.native-actual-fonts.v1':
  sources=report.get('sources',[]);c(len(sources)==2,'both frozen style sets')
  c(report.get('sourceDrawingWritten') is False,'source DWGs not written')
  for source in sources:
   for style in source.get('styles',[]):
    before=dict(style.get('before') or {});after=dict(style.get('after') or {});before.pop('fontFiles',None);after.pop('fontFiles',None)
    c(before==after,'declared style unchanged '+style.get('name','?'))
    c(exact_font_loaded(style),'exact font file loading without substitution '+style.get('name','?'))
 elif kind=='stage1.native-text-finder.v1':
  c(len(report.get('originalBefore',[]))==213 and report.get('originalBefore')==report.get('originalAfter'),'finder protects whole original scheme')
  c(report.get('standardControlFound') is True and report.get('standardControlReplaced') is True,'public SDK finder positive control works')
  c(report.get('targetFound') is True and report.get('targetReplaced') is True,'actual selected TCH root found and replaced')
  c(report.get('businessContentWritePassed') is True and report.get('reverseSucceeded') is True and report.get('saveStatus')==report.get('restoreSaveStatus')==0 and report.get('savedRootType')=='TDbText' and report.get('savedModelCount')==214 and report.get('originalBefore')==report.get('originalSaved'),'native business content and root/originals roundtrip')
 elif kind=='stage1.native-generic-text-copy.v1':
  c(report.get('sourceCount')==len(report.get('originalBefore',[]))==213 and report.get('originalBefore')==report.get('originalAfter'),'original213 public states protected')
  c(report.get('rootType')=='TDbText','actual TCH root retained')
  c(report.get('standardControlStatus')==0,'standard native copyFrom positive control')
  c(report.get('copyFromStatus')!=-9999,'TCH generic API was actually invoked')
  c(report.get('businessContentWritePassed') is True and report.get('copyFromStatus')==0 and report.get('saveStatus')==0 and report.get('savedRootType')=='TDbText' and report.get('savedModelCount')==214 and report.get('originalSaved')==report.get('originalBefore'),'required C09235 business content and TCH root/originals saved/reopened; unsupported is not PASS')
 else:c(False,'known native continuation contract')
 return checks

if __name__=='__main__':
 report=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(report,Path(sys.argv[1]))
 for ok,name in checks:
  if not ok:print('FAIL:',name)
 failed=sum(not ok for ok,_ in checks)
 print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_BOUNDED_NATIVE_CONTRACT"}')
 raise SystemExit(bool(failed))
