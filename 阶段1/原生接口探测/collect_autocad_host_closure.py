"""Evidence closure only; never launch, attach, activate or terminate CAD."""
import configparser,datetime,hashlib,json,re,shutil,subprocess,sys,winreg
from pathlib import Path
from collect_symbol_closure import owned_process_state
from run_quiet_job import MODES
P=Path(__file__).resolve().parent;S=P.parent;ROOT=S.parent
STAMP=datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def bound(p):return dict(path=str(p.resolve()),SHA256=sha(p),bytes=p.stat().st_size)
def write(p,d):
 with p.open('x',encoding='utf-8') as f:json.dump(d,f,ensure_ascii=False,indent=2)
def validate(script,target,extra=()):
 r=subprocess.run([sys.executable,str(S/'tests'/script),str(target),*extra],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
 log=target.parent/(script[:-3]+'-closure-'+STAMP+'.txt');log.write_text(r.stdout+r.stderr,encoding='utf-8')
 m=re.search(r'assertions=(\d+) passed=(\d+) failed=(\d+) result=(\S+)',r.stdout)
 if not m and script=='verify_isolated_host.py' and r.returncode==1:
  final=read(target);assert final['workerExitCode']==70 and final['forcedTermination'] and not (target.parent/'native.quit.done').exists()
  return dict(**bound(log),exitCode=1,assertions=None,passed=None,failed=None,result='FAIL_INCOMPLETE_NO_NATIVE_QUIT',validator=bound(S/'tests'/script))
 assert m,(script,r.stdout,r.stderr)
 return dict(**bound(log),exitCode=r.returncode,assertions=int(m[1]),passed=int(m[2]),failed=int(m[3]),result=m[4],validator=bound(S/'tests'/script))
def regvalue(path,name):
 try:
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,path) as k:return winreg.QueryValueEx(k,name)[0]
 except FileNotFoundError:return None
def main():
 sessions=sorted((S/'runs').glob('isolated-autocad-20261029-*'));configs={str(p):read(p/'worker-config.json') for p in sessions}
 assert len(sessions)==9,'explicit nine-session closure expected'
 result=dict(contract='stage1.autocad-native-host-closure.v1',recorded=datetime.datetime.now().astimezone().isoformat(),phase1FullAcceptance=False,phase2Started=False,COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,inputs=[],sessions=[],cases=[],negativeProtection=[],preflightFailures=[])
 frozen=[(ROOT/'阶段0/输入快照/方案平面图.dwg','5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'),(ROOT/'阶段0/输入快照/施工图平面参考图.dwg','8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27')]
 for path,expected in frozen:
  item=bound(path);item.update(expectedSHA256=expected,unchanged=item['SHA256']==expected);assert item['unchanged'];result['inputs'].append(item)
 for path in (Path('D:/cad插件/方案平面图.dwg'),Path('D:/cad插件/施工图平面参考图.dwg')):
  item=bound(path);item.update(lastWrite=datetime.datetime.fromtimestamp(path.stat().st_mtime).astimezone().isoformat(),testInput=False,scope='Read-only working-original observation; no architectural-change inference or restore');result['inputs'].append(item)
 prior=read(P/'全需求验证_只读调查后终核_20261029.json');result['sixGroupInputAudit']=[]
 for old in prior['Inputs']:
  source=bound(Path(old['Source']));snapshot=bound(Path(old['Snapshot']));assert snapshot['SHA256']==old['SnapshotSHA256']
  if old['Id'] not in ('S01','S02'):assert source['SHA256']==old['SourceSHA256']
  result['sixGroupInputAudit'].append(dict(id=old['Id'],source=source,snapshot=snapshot,snapshotUnchanged=True,sourceMatchesPriorObservation=source['SHA256']==old['SourceSHA256'],sourceIsCurrentTestInput=False))
 # Verify every host exited before any staging mutation. Numeric PID reuse is not ownership.
 for session in sessions:
  cfg=configs[str(session)];launch=read(session/'isolation-host-receipt.json');final=read(session/'isolation-final.json');state=owned_process_state(launch)
  assert cfg['nonce']==launch['nonce']==final['nonce'] and str(session)==cfg['session']==launch['session']==final['session']
  assert final['workerExited'] is True and state['ownProcessAlive'] is False
  assert sha(session/'candidate/NativeAccessProbe.arx')==cfg['binarySHA256']
  for name,digest in cfg['sourceFiles'].items():assert sha(session/'candidate'/name)==digest
  host=validate('verify_isolated_host.py',session/'isolation-final.json')
  if final['workerExitCode']==0:assert host['exitCode'] in (0,2)
  item=dict(session=str(session),config=bound(session/'worker-config.json'),launch=bound(session/'isolation-host-receipt.json'),final=bound(session/'isolation-final.json'),processIdentity=state,windowAcceptance=host,workerExitCode=final['workerExitCode'],forcedTermination=final['forcedTermination'],ownShow=final['workerUserDesktopShowEvents'],ownForeground=final['workerForegroundEvents'],relatedShow=final['relatedCadShowEvents'],relatedForeground=final['relatedCadForegroundEvents'],sampleCount=final['sampleCount'],binarySHA256=cfg['binarySHA256'],sourceFiles=cfg['sourceFiles'])
  if final['workerExitCode']==0:
   q=read(session/'normal-quit-verification.json');assert q['completed'] and not q['forcedTermination'] and q['nonce']==cfg['nonce'];item['normalQuit']=dict(**bound(session/'normal-quit-verification.json'),seconds=q['queueToSupervisorExitSeconds'])
  else:assert final['workerExitCode']==70 and final['forcedTermination'] is True;item['failureRetained']='signature-security prompt, not accepted; only exact own host stopped'
  if (session/'startup-tip-recovery.json').exists():item['knownStartupTipRecovery']=bound(session/'startup-tip-recovery.json')
  result['sessions'].append(item)
 for receipt_path in sorted((S/'runs').glob('quiet-*/quiet-job-receipt.json')):
  receipt=read(receipt_path);session=receipt.get('session')
  if session not in configs:continue
  cfg=configs[session];run=receipt_path.parent;assert receipt['nonce']==cfg['nonce'] and receipt['binarySHA256']==cfg['binarySHA256'] and receipt['sourceFiles']==cfg['sourceFiles']
  item=dict(mode=receipt['mode'],run=str(run),receipt=bound(receipt_path),session=session,binarySHA256=cfg['binarySHA256'],sourceFiles=cfg['sourceFiles'],atRunVerificationExitCode=receipt['verificationExitCode'],atRunError=receipt.get('error'),controllerSHA256=receipt['controllerSHA256'])
  controller=run/'run_quiet_job_at_run.py'
  if not controller.exists():controller=Path(session)/'candidate/run_quiet_job.py'
  if controller.exists() and sha(controller)==receipt['controllerSHA256']:item['controllerAtRun']=bound(controller)
  else:item['controllerArchiveGap']='Recorded at-run controller hash retained; no matching source archive found. Bootstrap controller is not substituted.'
  mode=receipt['mode'];spec=MODES.get(mode)
  if spec and spec[1] and (run/spec[1]).exists():
   item['nativeReport']=bound(run/spec[1])
   if spec[2]:
    validation=validate(spec[2],run/spec[1],('--supported',) if mode=='AutoCadSupportedHost' else ());item['currentValidation']=validation
    expected=4 if mode=='FamilyNative' else 2 if mode=='SafetyNative' else 10 if mode=='ReopenDimensionNative' and read(run/spec[1])['contract']=='stage1.native-reopen-dimension.v1' else 0
    assert validation['failed']==expected,(run,validation)
   item['dwgFiles']=[bound(f) for f in sorted(run.glob('*.dwg'))]
   result['cases'].append(item)
  else:result['preflightFailures'].append(item)
 for f in sorted((S/'runs').glob('quiet-preexistingcandidate-*/preexisting-protection.json')):
  d=read(f)
  if d['session'] in configs:
   assert d['binarySHA256']==configs[d['session']]['binarySHA256'] and sha(f.parent/'native-candidate-discard.json')==d['nativeReportSHA256']
   assert sha(f.parent/'ProbePreexistingCandidate.py')==d['driverSHA256']
   if d['preserved']:assert Path(d['marker']).is_file() and sha(Path(d['marker']))==d['sentinelSHA256']
   result['negativeProtection'].append(dict(**bound(f),completed=d['completed'],preserved=d['preserved'],binarySHA256=d['binarySHA256'],driverAtRun=bound(f.parent/'ProbePreexistingCandidate.py')))
 assert result['negativeProtection'][-1]['completed'] is True
 auth=read(P/'evidence/autocad-support-profile-20261029.json');base=r'Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804\Profiles';task=base+'\\'+auth['profile']
 current=dict(profile=auth['profile'],SECURELOAD=regvalue(task+r'\Variables','SECURELOAD'),TRUSTEDPATHS=regvalue(task+r'\Variables','TRUSTEDPATHS'),ACAD=regvalue(task+r'\General','ACAD'),defaultProfile=regvalue(base,''),sourceSECURELOAD=regvalue(base+'\\'+auth['sourceProfile']+r'\Variables','SECURELOAD'),sourceTRUSTEDPATHS=regvalue(base+'\\'+auth['sourceProfile']+r'\Variables','TRUSTEDPATHS'))
 assert str(current['SECURELOAD'])=='1' and auth['componentPath'].rsplit('\\',1)[0].lower() in current['TRUSTEDPATHS'].lower()
 assert current['defaultProfile']==auth['defaultProfileBefore'] and sha(Path(auth['componentPath']))==auth['componentSHA256']
 result['profile']=dict(authorization=bound(P/'evidence/autocad-support-profile-20261029.json'),correction=bound(P/'evidence/autocad-support-profile-correction-20261029.json'),current=current,scope='Direct configuration writes only to the dedicated test profile. Vendor/application cache changes by other processes are not globally judged equal. Dedicated profile retained for authorized reuse.')
 # Exact owned Program Files files only; no recursive deletion, vendor install edits or registry cleanup.
 plugins=Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve()
 for item in result['sessions']:
  session=Path(item['session']);cfg=configs[str(session)];folder=Path(cfg['stagingDirectory']).resolve();module=Path(cfg['stagedARX']).resolve();ini=Path(cfg['stagedINI']).resolve()
  assert folder.parent==plugins and folder.name=='CodexStage1Probe_'+cfg['nonce'] and module.parent==ini.parent==folder and module.name=='S1Acad_'+cfg['nonce'][:8]+'.arx' and ini==module.with_suffix('.ini')
  archive=session/'staging-final-archive';archive.mkdir(exist_ok=True);assert archive.resolve().is_relative_to(session.resolve())
  if module.exists():
   assert sha(module)==cfg['binarySHA256'];settings=configparser.ConfigParser();settings.read(ini,encoding='utf-16');assert settings['worker']['nonce']==cfg['nonce'] and settings['worker']['session']==str(session)
   for source in (module,ini):
    target=archive/source.name
    if target.exists():assert sha(target)==sha(source)
    else:shutil.copyfile(source,target)
    assert sha(target)==sha(source);source.unlink()
  assert not module.exists() and not ini.exists() and sha(archive/module.name)==cfg['binarySHA256']
  if folder.exists():assert not list(folder.iterdir());folder.rmdir()
  item['stagingCleanup']=dict(status='exact_two_files_archived_and_removed',archive=[bound(archive/module.name),bound(archive/ini.name)],directoryAbsent=not folder.exists())
 latest=read(P/'evidence/autocad-rx-discovery-build-20261029.json');assert sha(P/'bin/NativeAccessProbe.arx')==latest['binarySHA256']
 for n,h in latest['sourceFiles'].items():assert sha(P/'src'/n)==h
 result['latestCandidate']=dict(build=bound(P/'evidence/autocad-rx-discovery-build-20261029.json'),binarySHA256=latest['binarySHA256'],sourceFiles=latest['sourceFiles'])
 archive=P/'revisions'/('autocad-native-closure-'+STAMP);archive.mkdir()
 files=[P/'collect_autocad_host_closure.py',P/'prepare_autocad_session.py',P/'run_quiet_job.py',P/'创建专用AutoCAD测试配置.ps1',P/'diagnostics/ProbePreexistingCandidate.py',P/'diagnostics/CloseOwnedStartupTip.ps1',*(S/'tests').glob('verify_*.py')]
 for f in files:shutil.copyfile(f,archive/f.name)
 result['acceptanceArchive']=[bound(f) for f in sorted(archive.iterdir())]
 result['offlineRegression']=bound(P/'evidence/autocad-host-reviewed-with-rx-offline-regression-20261029.txt')
 result['completed']=True;out=P/'AutoCAD宿主迁移_输入与模块终核_20261029.json';write(out,result)
 print(json.dumps(dict(path=str(out),cases=len(result['cases']),sessions=len(sessions),normalQuit=sum(x['workerExitCode']==0 for x in result['sessions']),forcedSecurityStops=sum(x['workerExitCode']==70 for x in result['sessions']),original= result['inputs'][2]['SHA256'],stagingClean=True),ensure_ascii=False))
if __name__=='__main__':main()
