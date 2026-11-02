"""Verify actual authorized native component loading, never a GUI command alias."""
import json,sys
from pathlib import Path

def inspect(d):
 checks=[]
 def c(ok,name):checks.append((bool(ok),name))
 c(d.get('contract')=='stage1.autocad-support-load-native.v1','native component contract')
 c(str(d.get('profile','')).startswith('CodexS1Native_'),'dedicated authorized profile')
 c(d.get('completed') is True and d.get('error') is None,'actual native load complete')
 c(d.get('trustedStatus')==5100 and d.get('secureload')==1,'AutoCAD trusted-file validation and retained security')
 wanted='c:\\tangent\\tarcht20v8\\sys23x64\\tch_kernal.arx'
 c(str(d.get('component','')).lower()==str(d.get('trustedPath','')).lower()==wanted,'exact installed component path')
 c(d.get('loadModuleResult') is True,'actual ObjectARX loadModule result')
 c(d.get('wallClassRegistered') is True and d.get('dimensionClassRegistered') is True,'actual native wall/dimension classes registered')
 c(any(str(m.get('path','')).lower()==wanted for m in d.get('modulesAfter',[])),'actual loaded component mapping')
 c(d.get('menuBefore')==d.get('menuAfter') and bool(d.get('menuBefore')),'standard host main menu preserved')
 c(d.get('COMUsed') is False and d.get('ComputerUseUsed') is False and d.get('TianzhengCommandsUsed') is False,'no COM/CUA/TCH command operations')
 return checks

if __name__=='__main__':
 d=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'));checks=inspect(d)
 for ok,name in checks:
  if not ok:print('FAIL:',name)
 failed=sum(not ok for ok,_ in checks);print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_NATIVE_COMPONENT_LOAD"}');raise SystemExit(bool(failed))
