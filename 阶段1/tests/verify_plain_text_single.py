"""Audit only the independently completed single-text portion; keep batch FAIL."""
import json,sys
from pathlib import Path
from verify_plain_text_replacement import presentation,visible_typography
FIELDS={'text','position','alignmentPoint','height','rotation','normal','widthFactor','oblique','thickness','horizontalMode','verticalMode','mirroredX','mirroredY','styleHandle'}
def inspect(r):
 out=[]
 def c(ok,name):out.append((bool(ok),name))
 c(r.get('contract') in ('stage1.native-plain-text-replacement.v1','stage1.native-plain-text-replacement.v2'),'recognized native raw report')
 c(all(r.get(k) is False for k in ('COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','sourceDrawingWritten','phase2Started')),'native SDK route and protected input scope')
 c(r.get('commandTrace')=={'lost':False,'commands':[]},'complete native command trace without business commands')
 v=r.get('conversion') or {};base=r.get('baseline') or {};glyph=v.get('sourceVisibleGlyph') or {};other=r.get('protectedBefore',[])
 c(v.get('sourceType')=='TDbText' and v.get('handle')=='CDC0' and v.get('handOverStatus')==36 and v.get('handleRetained') is True,'actual TCH to native replacement preserves selected identity')
 c(v.get('expectedNative')==v.get('actualNative')==base and base.get('type')=='AcDbText','native replacement properties match actual cloned glyph')
 c(FIELDS.issubset(base) and FIELDS.issubset(glyph) and base.get('height',0)>0 and visible_typography(glyph)==visible_typography(base),'source visible text typography and style preserved')
 c(len(other)==212 and other==r.get('protectedAfter'),'other212 public entity states protected')
 states=r.get('states',[]);c(base.get('text')=='庭院' and len(states)==4 and [s.get('expected') for s in states]==['庭院','C09235','待确认','庭院'],'source, Latin code, Chinese and restored states')
 for i,s in enumerate(states):
  c(s.get('live')==s.get('saved') and s.get('saved',{}).get('text')==s.get('expected') and s.get('saved',{}).get('type')=='AcDbText',f'state{i} actual native literal fresh readback')
  c(presentation(s.get('saved'))==presentation(base),f'state{i} source text presentation preserved')
  c(s.get('saveStatus')==0 and s.get('savedModelCount')==213 and s.get('savedProtected')==other,f'state{i} saved model and212 protected originals')
 c(r.get('singleReplacementPassed') is True,'single replacement completed independently of later batch failure')
 return out
if __name__=='__main__':
 r=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));rows=inspect(r);bad=[n for ok,n in rows if not ok]
 for name in bad:print('FAIL:',name)
 print(f'bounded_single_assertions={len(rows)} passed={len(rows)-len(bad)} failed={len(bad)} original_overall_error_preserved={r.get("error")!r}');raise SystemExit(bool(bad))
