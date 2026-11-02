"""Close only this round's exited hosts; preserve every failed capability result."""
from pathlib import Path
import configparser,datetime,hashlib,json,re,shutil,subprocess,sys,winreg
from collect_symbol_closure import owned_process_state
from run_quiet_job import MODES
P=Path(__file__).resolve().parent;S=P.parent;ROOT=S.parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def bound(p):return dict(path=str(p.resolve()),SHA256=sha(p))
def save(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
def main():
 stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S');sessions=sorted((S/'runs').glob('isolated-autocad-20261030-*'));assert len(sessions)==3
 result=dict(contract='stage1.pure-native-closure.v1',recorded=datetime.datetime.now().astimezone().isoformat(),phase1FullAcceptance=False,phase2Started=False,COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,inputs=[],sessions=[],cases=[],failedDispatches=[])
 prior=read(P/'AutoCAD宿主迁移_输入与模块终核_20261029.json')
 for item in prior['sixGroupInputAudit']:
  source=Path(item['source']['path']);snapshot=Path(item['snapshot']['path']);assert sha(snapshot)==item['snapshot']['SHA256']
  result['inputs'].append(dict(id=item['id'],source=bound(source),sourceComparedToPrior=sha(source)==item['source']['SHA256'],snapshot=bound(snapshot),snapshotUnchanged=True))
 for session in sessions:
  cfg=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json');final=read(session/'isolation-final.json');state=owned_process_state(launch)
  assert cfg['nonce']==launch['nonce']==final['nonce'] and final['workerExited'] and state['ownProcessAlive'] is False
  assert sha(session/'candidate/NativeAccessProbe.arx')==cfg['binarySHA256']
  for n,h in cfg['sourceFiles'].items():assert sha(session/'candidate'/n)==h
  host=subprocess.run([sys.executable,str(S/'tests/verify_isolated_host.py'),str(session/'isolation-final.json')],capture_output=True,text=True,encoding='utf-8');log=session/('host-closure-'+stamp+'.txt');log.write_text(host.stdout+host.stderr,encoding='utf-8')
  if final['workerExitCode']==0:assert host.returncode in (0,2) and final['nativeQuitConfirmed'] and not final['forcedTermination']
  else:assert final['workerExitCode']==70 and final['forcedTermination'] and session.name.endswith('091926')
  result['sessions'].append(dict(path=str(session),config=bound(session/'worker-config.json'),launch=bound(session/'isolation-host-receipt.json'),final=bound(session/'isolation-final.json'),processIdentity=state,normalQuit=bound(session/'normal-quit-verification.json') if final['workerExitCode']==0 else None,exitCode=final['workerExitCode'],forced=final['forcedTermination'],ownShow=final['workerUserDesktopShowEvents'],ownForeground=final['workerForegroundEvents'],relatedShow=final['relatedCadShowEvents'],relatedForeground=final['relatedCadForegroundEvents'],windowValidator=dict(exitCode=host.returncode,log=bound(log)),binarySHA256=cfg['binarySHA256'],sourceFiles=cfg['sourceFiles']))
 sessionPaths={x['path'] for x in result['sessions']}
 for rp in sorted((S/'runs').glob('quiet-*-20261030-*/quiet-job-receipt.json')):
  receipt=read(rp)
  if receipt.get('session') not in sessionPaths:continue
  cfg=read(Path(receipt['session'])/'worker-config.json');assert receipt['nonce']==cfg['nonce'] and receipt['binarySHA256']==cfg['binarySHA256']
  run=rp.parent;controller=run/'run_quiet_job_at_run.py';assert sha(controller)==receipt['controllerSHA256']
  case=dict(mode=receipt['mode'],run=str(run),receipt=bound(rp),session=receipt['session'],binarySHA256=receipt['binarySHA256'],sourceFiles=receipt['sourceFiles'],controller=bound(controller),atRunExitCode=receipt['verificationExitCode'],atRunError=receipt['error'])
  _,report,validator,_=MODES[receipt['mode']]
  if report and (run/report).exists():
   case['nativeReport']=bound(run/report)
   if validator:
    command=[sys.executable,str(S/'tests'/validator),str(run/report)];r=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=90);log=run/('verify-pure-closure-'+stamp+'.txt');log.write_text(r.stdout+r.stderr,encoding='utf-8');m=re.search(r'assertions=(\d+) passed=(\d+) failed=(\d+) result=(\S+)',r.stdout);assert m,(run,r.stdout,r.stderr)
    case['validation']=dict(log=bound(log),validator=bound(S/'tests'/validator),exitCode=r.returncode,assertions=int(m[1]),passed=int(m[2]),failed=int(m[3]),result=m[4])
   result['cases'].append(case)
  else:result['failedDispatches'].append(case)
 # Verify all process identities first, then remove only nonce-bound staging files.
 plugins=Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve()
 for item in result['sessions']:
  session=Path(item['path']);cfg=read(session/'worker-config.json');folder=Path(cfg['stagingDirectory']).resolve();module=Path(cfg['stagedARX']).resolve();ini=Path(cfg['stagedINI']).resolve()
  assert folder.parent==plugins and folder.name=='CodexStage1Probe_'+cfg['nonce'] and module.parent==ini.parent==folder and module.name=='S1Acad_'+cfg['nonce'][:8]+'.arx' and ini==module.with_suffix('.ini')
  archive=session/'staging-final-archive';archive.mkdir(exist_ok=True)
  if module.exists():
   assert sha(module)==cfg['binarySHA256'];settings=configparser.ConfigParser();settings.read(ini,encoding='utf-16');assert settings['worker']['nonce']==cfg['nonce'] and settings['worker']['session']==str(session)
   for file in (module,ini):
    dest=archive/file.name
    if dest.exists():assert sha(dest)==sha(file)
    else:shutil.copyfile(file,dest)
    assert sha(dest)==sha(file);file.unlink()
  assert not module.exists() and not ini.exists() and sha(archive/module.name)==cfg['binarySHA256']
  if folder.exists():assert not list(folder.iterdir());folder.rmdir()
  item['staging']=dict(directoryAbsent=not folder.exists(),archive=[bound(archive/module.name),bound(archive/ini.name)])
 latest=read(P/'evidence/pure-native-v3-build-20261030.json');assert sha(P/'bin/NativeAccessProbe.arx')==latest['binarySHA256']
 for n,h in latest['sourceFiles'].items():assert sha(P/'src'/n)==h
 result['latestBuild']=bound(P/'evidence/pure-native-v3-build-20261030.json');result['analysis']=bound(P/'纯原生全量读取与字体分项_20261030.json');result['offline']=bound(P/'evidence/pure-native-final-offline-20261030.txt');result['nativeBoundaryTests']=bound(P/'evidence/pure-boundary-green-20261030.txt')
 base=r'Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804\Profiles';profile=read(P/'evidence/autocad-support-profile-20261029.json')['profile']
 with winreg.OpenKey(winreg.HKEY_CURRENT_USER,base+'\\'+profile+r'\Variables') as key:secure=winreg.QueryValueEx(key,'SECURELOAD')[0]
 assert str(secure)=='1';result['profile']=dict(name=profile,SECURELOAD=secure,scope='Dedicated profile retained; no global security policy changed')
 archive=P/'revisions'/('pure-native-acceptance-'+stamp);archive.mkdir()
 for f in [P/'run_quiet_job.py',P/'close_pure_native_20261030.py',S/'tests/verify_pure_native_continuation.py',S/'tests/test_pure_native_continuation.py',S/'tests/pure_boundary_smoke.cpp']:
  shutil.copyfile(f,archive/f.name)
 result['acceptanceArchive']=[bound(f) for f in archive.iterdir()];save(P/'纯原生阶段1_输入模块与退出终核_20261030.json',result)
 print(json.dumps(dict(sessions=len(sessions),normalQuit=2,explicitMissingFontStop=1,cases=len(result['cases']),stagingClean=True,phase1FullAcceptance=False),ensure_ascii=False))
if __name__=='__main__':main()
