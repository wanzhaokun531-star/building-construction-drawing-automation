"""Accept native host facts; do not confuse cached proxy graphics with TCH access."""
import json,math,sys
from pathlib import Path

def inspect(d,supported=False):
 checks=[]
 def check(ok,name):checks.append((bool(ok),name))
 check(d.get('contract')=='stage1.autocad-host-native.v1','new native host baseline contract')
 check(d.get('completed') is True and d.get('error') is None,'native bounded baseline completed')
 check(str(d.get('profile','')).startswith('CodexS1Native_') if supported else d.get('profile')=='<<未命名配置>>','actual authorized dedicated/ordinary AutoCAD profile')
 if supported:check(d.get('supportedMode') is True,'explicit supported runtime baseline mode')
 check(d.get('COMUsed') is False and d.get('ComputerUseUsed') is False and d.get('TianzhengCommandsUsed') is False,'no COM/CUA/TCH command operations')
 for stage in ('modulesBefore','modulesAfter'):
  modules=d.get(stage,[])
  check(bool(modules) and all(m.get('path') and m.get('name') for m in modules),'actual complete own-module list '+stage)
  has_tch=any('\\tangent\\' in m.get('path','').lower() or m.get('name','').lower().startswith(('tch_','tch3_','tch10_')) for m in modules)
  check(has_tch is supported,'actual component presence matches requested host mode '+stage)
 env=d.get('environment') or {}
 check(env.get('demandloadApplied')==0 and env.get('demandloadBefore')==env.get('demandloadAfter') and env.get('restored') is True,'native demand-load mode applied and restored')
 check(env.get('secureloadBefore')==env.get('secureloadAfter')==1,'security policy retained')
 sources=d.get('sources',[]);check(len(sources)==2,'two source drawings inspected')
 expected=[{'CD83':'TDbWall','CD84':'TDbOpening','CD89':'TDbOpening','CDC0':'TDbText'},{'19A0':'TDbDimension2','199F':'TDbAxisLabelSet','19F3':'TDbDrawingName','17DF':None}]
 for i,(src,count) in enumerate(zip(sources,[213,618])):
  check(src.get('readStatus')==src.get('closeInputStatus')==0 and src.get('modelCount')==count,'actual source read/count '+str(count))
  check(src.get('proxyCount')==(0 if supported else 143),'actual native/proxy entity counts '+str(count))
  samples=src.get('samples',[]);check(bool(samples),'actual source samples')
  handles=[s.get('handle') for s in samples]
  check(set(handles)==set(expected[i]),'complete expected sample handle set '+str(count))
  check(len(handles)==len(set(handles)),'no duplicate sample handles '+str(count))
  if not supported:check({s.get('handle'):s.get('originalClassName') for s in samples}==expected[i],'actual proxy origin classes, not live TCH classes '+str(count))
  for sample in samples:
   check(bool(sample.get('handle')) and bool(sample.get('rxName')) and isinstance(sample.get('proxy'),bool),'actual class identity '+str(sample.get('handle')))
   wanted=expected[i].get(sample.get('handle'))
   if supported:check(sample.get('handle') in expected[i] and sample.get('proxy') is False and sample.get('rxName')==(wanted or 'AcDbBlockReference'),'actual live native class '+str(sample.get('handle')))
   else:check(sample.get('proxy') is bool(wanted) and (not wanted or not sample.get('rxName','').startswith('TDb')),'proxy origin is not a live native TCH class '+str(sample.get('handle')))
   if sample.get('proxy'):check(bool(sample.get('originalClassName')),'proxy original class disclosed, not inferred')
 native=d.get('nativeControl') or {}
 check(native.get('rxName')=='AcDbLine' and native.get('proxy') is False,'actual standard ObjectARX line')
 check(native.get('saveStatus')==native.get('readStatus')==native.get('closeInputStatus')==0 and native.get('readerDestroyedBeforeReopen') is True,'native control saved and fresh reader opened')
 for stage in ('live','saved'):
  value=native.get(stage) or {};check(value.get('start')==[0,0,0] and value.get('end')==[0,1000,0] and abs(value.get('length',float('inf'))-1000)<1e-6,'native control '+stage+' modified geometry')
 check(d.get('sourceDrawingWritten') is False and d.get('phase2Started') is False,'source protection and stage scope')
 return checks

if __name__=='__main__':
 p=Path(sys.argv[1]);d=json.loads(p.read_text(encoding='utf-8-sig')) if p.exists() else {}
 checks=inspect(d,supported='--supported' in sys.argv[2:])
 for ok,name in checks:
  if not ok:print('FAIL:',name)
 failed=sum(not ok for ok,_ in checks)
 print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_NATIVE_HOST_FACTS_ONLY"}')
 raise SystemExit(bool(failed))
