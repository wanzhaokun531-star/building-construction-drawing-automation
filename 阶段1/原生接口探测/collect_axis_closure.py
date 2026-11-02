"""Archive exact task-owned staging files after host exit; bind stage1 deliveries."""
from pathlib import Path
import configparser
import ctypes
from datetime import datetime
import hashlib
import json
import shutil

PROJECT=Path(__file__).resolve().parent
STAGE=PROJECT.parent
TRUSTED=Path('C:/Users/PC/AppData/Roaming/Tangent/TArch20V8/sys').resolve()
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest().upper()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def fresh(path,data):
 if path.exists():raise RuntimeError('Existing evidence must be preserved: '+str(path))
 path.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def process_state(pid):
 kernel=ctypes.WinDLL('kernel32',use_last_error=True)
 kernel.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];kernel.OpenProcess.restype=ctypes.c_void_p
 kernel.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_uint32];kernel.CloseHandle.argtypes=[ctypes.c_void_p]
 handle=kernel.OpenProcess(0x00100000,False,pid)
 if not handle:return False if ctypes.get_last_error()==87 else None
 try:return kernel.WaitForSingleObject(handle,0)==258
 finally:kernel.CloseHandle(handle)

result=dict(contract='stage1.axis-input-staging-delivery-closure.v1',recorded=datetime.now().astimezone().isoformat(),inputs=[],staging=[],deliveries=[],phase2Started=False)
for path,expected in [
 (STAGE.parent/'阶段0/输入快照/方案平面图.dwg','5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'),
 (STAGE.parent/'阶段0/输入快照/施工图平面参考图.dwg','8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'),
 (Path('D:/cad插件/方案平面图.dwg'),'92E01E8DD5BD4BA2B65CA48C6A3E8F6C38E3CB5F7EF3C3C36E8B692980B8BDD6'),
 (Path('D:/cad插件/施工图平面参考图.dwg'),'8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27')]:
 actual=sha(path);result['inputs'].append(dict(path=str(path),sha256=actual,expectedPriorSHA256=expected,unchangedSincePrior=actual==expected,bytes=path.stat().st_size))
 if path.is_relative_to(STAGE.parent/'阶段0') and actual!=expected:raise RuntimeError('Frozen source changed')

source=STAGE/'runs/quiet-singletons-20261029-111721-958785/four-side-singletons-generated.dwg'
pdf=STAGE/'runs/quiet-preview-20261029-111750-826844/axis-preview.pdf'
png=pdf.parent/'root-review/full-plan.png'
semantic=read(STAGE/'runs/quiet-explode-20261029-111748-994266/verify-44-numbers.json')
visual_path=pdf.parent/'visual-review-window-orchestration/visual-review.json'
visual=read(visual_path)
assert semantic['result']=='PASS' and semantic['numberingSemantics']=='PASS_ACTUAL_PUBLIC_TEXT_44_CIRCLES'
assert visual['result']=='VISUAL_LABEL_SEMANTICS_CONFIRMED_WITH_C_D_OVERLAP_PENDING'
assert visual['sourceHashesUnchangedDuringFinalBindingCheck'] is True
delivery=STAGE/'交付';delivery.mkdir(exist_ok=True)
for origin,name in [(source,'方案图副本_四侧轴网能力验证_20261029.dwg'),(pdf,'四侧轴网能力验证_20261029.pdf'),(png,'四侧轴网能力验证_20261029.png')]:
 target=delivery/name
 if target.exists():raise RuntimeError('Delivery path already exists; no overwrite: '+str(target))
 shutil.copyfile(origin,target)
 assert sha(origin)==sha(target)
 result['deliveries'].append(dict(source=str(origin),path=str(target),sha256=sha(target),bytes=target.stat().st_size,classification='stage1 capability fixture; not issued construction drawing'))
result['semanticEvidence']=dict(path=str(STAGE/'runs/quiet-explode-20261029-111748-994266/verify-44-numbers.json'),sha256=sha(STAGE/'runs/quiet-explode-20261029-111748-994266/verify-44-numbers.json'),assertions=semantic['assertions'])
result['visualEvidence']=dict(path=str(visual_path),sha256=sha(visual_path),overlapPending=True)

for session in sorted((STAGE/'runs').glob('isolated-axis-20261029-*')):
 config_path=session/'worker-config.json'
 if not config_path.is_file():continue
 config=read(config_path);nonce=config['nonce'];module=Path(config['stagedARX']).resolve();ini=Path(config['stagedINI']).resolve()
 entry=dict(session=str(session),module=str(module),ini=str(ini),nonce=nonce,deleted=[],status='PENDING')
 try:
  expected_name='S1Quiet_'+config['binarySHA256'][:12]+'_'+nonce[:8]+'.arx'
  assert module.parent==TRUSTED and ini.parent==TRUSTED and module.name==expected_name and ini==module.with_suffix('.ini')
  assert Path(config['session']).resolve()==session.resolve()
  launch_path=session/'isolation-host-receipt.json'
  if not launch_path.exists():entry['status']='SKIPPED_NO_PROCESS_RECEIPT';result['staging'].append(entry);continue
  launch=read(launch_path);assert launch['nonce']==nonce and launch['origin']=='CreateProcessW'
  state=process_state(launch['processId']);entry['processId']=launch['processId'];entry['processAliveOrUnknown']=state
  if state is not False:entry['status']='SKIPPED_ACTIVE_OR_UNCERTAIN_PID';result['staging'].append(entry);continue
  archive=session/'staging-final-archive';archive.mkdir(exist_ok=True)
  if module.exists():
   assert sha(module)==config['binarySHA256'];target=archive/module.name
   if not target.exists():shutil.copyfile(module,target)
   assert sha(target)==sha(module);entry['binarySHA256']=sha(module)
  if ini.exists():
   parser=configparser.ConfigParser();parser.read(ini,encoding='utf-16')
   assert parser['worker']['nonce']==nonce and Path(parser['worker']['session']).resolve()==session.resolve()
   target=archive/ini.name
   if not target.exists():shutil.copyfile(ini,target)
   assert sha(target)==sha(ini);entry['iniSHA256']=sha(ini)
  for file in (module,ini):
   if file.exists():file.unlink();entry['deleted'].append(str(file))
  entry['status']='EXACT_OWNED_STAGING_ARCHIVED_AND_REMOVED'
 except (OSError,ValueError,KeyError,AssertionError) as error:entry['status']='PRESERVED_ON_GUARD_OR_IO_FAILURE';entry['error']=str(error)
 result['staging'].append(entry)

output=PROJECT/'四侧轴网_输入与模块终核_20261029.json'
fresh(output,result)
print(json.dumps(dict(evidence=str(output),frozenInputsPass=all(x['unchangedSincePrior'] for x in result['inputs'][:2]),deliveries=len(result['deliveries']),stagingStatuses=[x['status'] for x in result['staging']]),ensure_ascii=False))
