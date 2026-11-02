"""Close this bounded native-symbol probe; never start or attach to CAD.

Only exact, nonce/hash-bound staging files for already-exited own hosts may
be archived and removed. Historical failures are indexed, never rewritten.
"""
import argparse
import configparser
import ctypes
from ctypes import wintypes
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from collect_elevation_closure import process_state

PROJECT=Path(__file__).resolve().parent
STAGE=PROJECT.parent
ROOT=STAGE.parent
TRUSTED=Path('C:/Users/PC/AppData/Roaming/Tangent/TArch20V8/sys').resolve()
SESSION=STAGE/'runs/isolated-axis-20261029-154336'
CASES=[
 ('slope','quiet-slope-20261029-154346-195615','quiet-symbolpreview-20261029-154454-860589',15),
 ('section','quiet-section-20261029-154435-453099','quiet-symbolpreview-20261029-154500-028151',15),
 ('pointer-index','quiet-pointerindex-20261029-154402-285592','quiet-symbolpreview-20261029-154456-582067',23),
 ('section-index','quiet-sectionindex-20261029-154431-408405','quiet-symbolpreview-20261029-154458-319779',27),
]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def fresh(p,data):
 with p.open('x',encoding='utf-8') as out:json.dump(data,out,ensure_ascii=False,indent=2)
def bound(p):return dict(path=str(p),SHA256=sha(p))

def owned_process_state(launch):
 """Read-only process identity; a recycled numeric PID is not the old worker."""
 k=ctypes.WinDLL('kernel32',use_last_error=True)
 k.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];k.OpenProcess.restype=wintypes.HANDLE
 k.CloseHandle.argtypes=[wintypes.HANDLE];k.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
 k.GetProcessTimes.argtypes=[wintypes.HANDLE,*([ctypes.POINTER(wintypes.FILETIME)]*4)]
 k.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
 handle=k.OpenProcess(0x00101000,False,launch['processId'])
 if not handle:
  error=ctypes.get_last_error();return dict(ownProcessAlive=False if error==87 else None,openError=error)
 try:
  wait=k.WaitForSingleObject(handle,0)
  if wait==0:return dict(ownProcessAlive=False,currentProcessSignaled=True)
  if wait!=258:return dict(ownProcessAlive=None,waitResult=wait)
  times=[wintypes.FILETIME() for _ in range(4)];image=ctypes.create_unicode_buffer(32768);size=wintypes.DWORD(len(image))
  if not k.GetProcessTimes(handle,*[ctypes.byref(t) for t in times]) or not k.QueryFullProcessImageNameW(handle,0,image,ctypes.byref(size)):return dict(ownProcessAlive=None,identityError=ctypes.get_last_error())
  created=(times[0].dwHighDateTime<<32)|times[0].dwLowDateTime
  same=created==int(launch['creationFileTime']) and Path(image.value).resolve()==Path(launch['executable']).resolve()
  return dict(ownProcessAlive=k.WaitForSingleObject(handle,0)==258 if same else False,sameProcessIdentity=same,currentCreationFileTime=str(created),currentImage=image.value,pidReused=not same)
 finally:k.CloseHandle(handle)

def validate(script,path,stamp,allow_review=False,extra=()):
 result=subprocess.run([sys.executable,str(STAGE/'tests'/script),str(path),*[str(x) for x in extra]],capture_output=True,text=True,encoding='utf-8')
 log=path.parent/(script.removesuffix('.py')+'-closure-'+stamp+'.txt')
 with log.open('x',encoding='utf-8') as out:out.write(result.stdout+result.stderr)
 if result.returncode not in ((0,2) if allow_review else (0,)):raise RuntimeError(f'{script} failed: {log}')
 counts=re.search(r'assertions=(\d+) passed=(\d+) failed=(\d+) result=(\S+)',result.stdout)
 if not counts:raise RuntimeError('No actual assertion summary: '+str(log))
 return dict(**bound(log),assertions=int(counts[1]),passed=int(counts[2]),failed=int(counts[3]),result=counts[4])

def main(cleanup):
 stamp=datetime.now().strftime('%Y%m%d-%H%M%S-%f')
 final=read(SESSION/'isolation-final.json');quit_receipt=read(SESSION/'normal-quit-verification.json');config=read(SESSION/'worker-config.json')
 assert final['nonce']==quit_receipt['nonce']==config['nonce'] and final['workerExited'] and final['workerExitCode']==0 and not final['forcedTermination'] and quit_receipt['completed']
 assert owned_process_state(read(SESSION/'isolation-host-receipt.json'))['ownProcessAlive'] is False
 assert sha(SESSION/'candidate/NativeAccessProbe.arx')==config['binarySHA256']
 for name,digest in config['sourceFiles'].items():assert sha(SESSION/'candidate'/name)==digest
 result=dict(contract='stage1.symbol-and-quit-closure.v1',recorded=datetime.now().astimezone().isoformat(),phase2Started=False,phase1FullAcceptance=False,
             COMUsed=False,ComputerUseUsed=False,binarySHA256=config['binarySHA256'],session=str(SESSION),inputs=[],cases=[],staging=[],
             normalQuit=bound(SESSION/'normal-quit-verification.json'),hostFinal=bound(SESSION/'isolation-final.json'),sourceBinding=bound(SESSION/'worker-config.json'))
 inputs=[(ROOT/'阶段0/输入快照/方案平面图.dwg','5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'),
         (ROOT/'阶段0/输入快照/施工图平面参考图.dwg','8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'),
         (Path('D:/cad插件/方案平面图.dwg'),'92E01E8DD5BD4BA2B65CA48C6A3E8F6C38E3CB5F7EF3C3C36E8B692980B8BDD6'),
         (Path('D:/cad插件/施工图平面参考图.dwg'),'8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27')]
 for p,expected in inputs:
  actual=sha(p);is_working_original=p==Path('D:/cad插件/方案平面图.dwg')
  if not is_working_original:assert actual==expected
  result['inputs'].append(dict(path=str(p),SHA256=actual,previouslyRecordedSHA256=expected,unchanged=actual==expected,bytes=p.stat().st_size,lastWrite=datetime.fromtimestamp(p.stat().st_mtime).astimezone().isoformat(),role='unmodified frozen test input' if p in [inputs[0][0],inputs[1][0]] else 'user working original; observed only, not test input',scope='File bytes only; no new architectural-change inference or overwrite'))
 for kind,name,preview,expected_geometry in CASES:
  run=STAGE/'runs'/name;report=run/'symbol-business-native.json';p=STAGE/'runs'/preview
  receipt=read(run/'quiet-job-receipt.json');assert receipt['session']==str(SESSION) and receipt['nonce']==config['nonce'] and receipt['binarySHA256']==config['binarySHA256'] and receipt['processId']==final['processId'] and receipt['verificationExitCode']==0 and receipt['COMUsed'] is False
  assert receipt['controlSourceSHA256']==receipt['controlCopySHA256']==inputs[0][1]
  preview_receipt=read(p/'quiet-job-receipt.json');assert preview_receipt['session']==str(SESSION) and preview_receipt['binarySHA256']==config['binarySHA256'] and preview_receipt['controlSourceSHA256']==sha(run/'symbol-generated.dwg')
  entry=dict(kind=kind,run=str(run),report=bound(report),drawing=bound(run/'symbol-generated.dwg'),
             business=validate('verify_symbol_business_native.py',report,stamp),geometry=validate('verify_symbol_geometry_native.py',report,stamp),
             preview=validate('verify_symbol_preview_native.py',p/'symbol-preview-native.json',stamp,extra=(report,)),pdf=bound(p/'symbol-preview.pdf'),render=bound(p/'symbol-preview.png'))
  assert entry['geometry']['assertions']==expected_geometry and entry['business']['failed']==entry['preview']['failed']==0
  result['cases'].append(entry)
 regression=subprocess.run([sys.executable,str(STAGE/'tests/test_symbol_acceptance_regressions.py')],capture_output=True,text=True,encoding='utf-8')
 regression_log=PROJECT/('symbol-acceptance-regressions-'+stamp+'.txt');regression_log.write_text(regression.stdout+regression.stderr,encoding='utf-8');assert regression.returncode==0
 result['regressions']=bound(regression_log)
 host=validate('verify_isolated_host.py',SESSION/'isolation-final.json',stamp,allow_review=True);result['hostValidation']=host
 for session in sorted((STAGE/'runs').glob('isolated-axis-20261029-*')):
  if session.name<'isolated-axis-20261029-134013':continue
  item=dict(session=str(session),deleted=[],status='PRESERVED')
  try:
   cfg=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json');nonce=cfg['nonce']
   assert launch['nonce']==nonce and launch['origin']=='CreateProcessW' and Path(cfg['session']).resolve()==session.resolve()
   module=Path(cfg['stagedARX']).resolve();ini=Path(cfg['stagedINI']).resolve()
   assert module.parent==TRUSTED and ini==module.with_suffix('.ini') and module.name=='S1Quiet_'+cfg['binarySHA256'][:12]+'_'+nonce[:8]+'.arx'
   identity=owned_process_state(launch);state=identity['ownProcessAlive'];item.update(processId=launch['processId'],processAliveOrUnknown=state,currentProcessIdentity=identity)
   if state is not False:raise RuntimeError('Active or uncertain process; no cleanup')
   assert read(session/'isolation-final.json')['workerExited'] is True
   if cleanup:
    archive=session/'staging-final-archive';archive.mkdir(exist_ok=True)
    for path in (module,ini):
     if not path.exists():continue
     if path==module:assert sha(path)==cfg['binarySHA256']
     else:
      parser=configparser.ConfigParser();parser.read(path,encoding='utf-16')
      assert parser['worker']['nonce']==nonce and Path(parser['worker']['session']).resolve()==session.resolve()
     target=archive/path.name
     if not target.exists():shutil.copyfile(path,target)
     assert sha(target)==sha(path);path.unlink();item['deleted'].append(str(path))
    item['status']='EXACT_OWNED_STAGING_ARCHIVED_AND_REMOVED'
   else:item['status']='EXITED_IDENTITY_BOUND'
  except (OSError,ValueError,KeyError,AssertionError,RuntimeError) as error:item['error']=str(error)
  result['staging'].append(item)
 # Freeze the exact acceptance programs as well as the already archived C++.
 archive=PROJECT/'evidence'/('symbol-closure-'+stamp);archive.mkdir()
 for name in ('verify_symbol_business_native.py','verify_symbol_geometry_native.py','verify_symbol_preview_native.py','verify_isolated_host.py','test_symbol_acceptance_regressions.py'):
  shutil.copyfile(STAGE/'tests'/name,archive/name)
 result['validatorArchive']=[bound(p) for p in sorted(archive.iterdir())]
 output=PROJECT/('坡向剖切索引与QUIT_终核_'+stamp+'.json');fresh(output,result)
 (PROJECT/'current-symbol-closure.txt').write_text(str(output),encoding='utf-8')
 print(json.dumps(dict(path=str(output),frozenInputsUnchanged=all(x['unchanged'] for x in result['inputs'][:2]),workingOriginalBytesUnchanged=result['inputs'][2]['unchanged'],business=sum(e['business']['passed'] for e in result['cases']),geometry=sum(e['geometry']['passed'] for e in result['cases']),preview=sum(e['preview']['passed'] for e in result['cases']),host=host['result'],cleanup=[e['status'] for e in result['staging']]),ensure_ascii=False))

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--cleanup',action='store_true');main(parser.parse_args().cleanup)
