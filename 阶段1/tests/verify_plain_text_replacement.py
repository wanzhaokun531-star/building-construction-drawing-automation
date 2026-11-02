"""Native text replacement acceptance, after explicit user type authorization."""
import json,sys
from pathlib import Path
def presentation(row):return {k:v for k,v in (row or {}).items() if k!='text'}
def visible_typography(row):
 return {k:(row or {}).get(k) for k in ('text','position','alignmentPoint','height','rotation','normal','widthFactor','oblique','thickness','horizontalMode','verticalMode','mirroredX','mirroredY','styleHandle')}
def inspect(r):
 checks=[]
 def c(ok,name):checks.append((bool(ok),name))
 c(r.get('contract')=='stage1.native-plain-text-replacement.v2' and r.get('error') is None,'native replacement completed')
 c(r.get('commandTrace')=={'lost':False,'commands':[]} and all(r.get(k) is False for k in ('COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten','phase2Started')),'SDK only, no phase2 or protected input writes')
 conversion=r.get('conversion') or {};baseline=r.get('baseline') or {};original=r.get('protectedBefore',[])
 c(conversion.get('handOverStatus')==36 and conversion.get('handle')=='CDC0' and conversion.get('handleRetained') is True and conversion.get('actualNative')==conversion.get('expectedNative')==baseline,'native handOverTo preserves target identity and cloned typography')
 c(visible_typography(conversion.get('sourceVisibleGlyph'))==visible_typography(baseline),'native typography matches actual source visible glyph')
 c(len(original)==212 and original==r.get('protectedAfter'),'other212 public object states unchanged')
 states=r.get('states',[]);c(len(states)==4 and [x.get('expected') for x in states]==[baseline.get('text'),'C09235','待确认',baseline.get('text')],'unchanged, changed, Chinese and restored states')
 for i,s in enumerate(states):
  c(s.get('live')==s.get('saved') and s.get('saved',{}).get('text')==s.get('expected') and s.get('saved',{}).get('type')=='AcDbText',f'state{i} actual native literal survives fresh DWG reader')
  c(presentation(s.get('saved'))==presentation(baseline),f'state{i} style, position, orientation and graphical properties retained')
  c(s.get('saveStatus')==0 and s.get('savedModelCount')==213 and s.get('savedProtected')==original,f'state{i} saved other212 unchanged')
 c(r.get('singleReplacementPassed') is True,'single replacement accepted')
 b=r.get('batch') or {};rows=b.get('conversions',[])
 candidates=b.get('candidates',[]);kept=b.get('retainedTextHandles',[]);selected={x.get('handle') for x in candidates if x.get('eligible') is True};unselected={x.get('handle') for x in candidates if x.get('eligible') is False};converted={x.get('handle') for x in rows}
 c(b.get('initialTextCount')==len(candidates)==34 and len({x.get('handle') for x in candidates})==34 and b.get('convertedCount')==len(rows)==len(converted)>0 and converted==selected and set(kept)==unselected and len(kept)==len(set(kept)) and not converted.intersection(kept) and len(converted)+len(kept)==34,'all34 originals partitioned into safely converted and explicitly retained objects')
 c(b.get('expected')==b.get('saved') and len(b.get('saved',[]))==len(converted) and b.get('savedModelCount')==213 and b.get('savedVerified') is True,'every selected native content and typography survives fresh readback')
 c(len(b.get('protectedRemainingBefore',[]))==213-len(converted) and b.get('protectedRemainingBefore')==b.get('protectedRemainingAfter'),'all unselected native/TCH public object states retained')
 for row in rows:
  c(row.get('sourceType')=='TDbText' and row.get('handOverStatus')==36 and row.get('expectedNative')==row.get('actualNative') and row.get('actualNative',{}).get('type')=='AcDbText' and visible_typography(row.get('sourceVisibleGlyph'))==visible_typography(row.get('actualNative')),'batch actual native replacement '+str(row.get('handle')))
 return checks
if __name__=='__main__':
 r=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(r);bad=[n for ok,n in checks if not ok]
 for n in bad:print('FAIL:',n)
 print(f'assertions={len(checks)} passed={len(checks)-len(bad)} failed={len(bad)}');raise SystemExit(bool(bad))
