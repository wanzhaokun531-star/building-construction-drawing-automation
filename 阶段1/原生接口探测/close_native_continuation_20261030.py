"""Archive exact exited-worker modules only. Never stop a process or edit shared registrations."""
import argparse,hashlib,json,shutil,datetime,winreg
from pathlib import Path
from collect_symbol_closure import owned_process_state
P=Path(__file__).resolve().parent;S=P.parent
ALLOWED={'121837','123535','125019','130005','131436','132330'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def main(names,output):
 assert output.resolve().parent==(P/'evidence').resolve() and not output.exists();rows=[]
 for name in names:
  assert name.startswith('isolated-autocad-20261030-') and name.rsplit('-',1)[1] in ALLOWED
  session=(S/'runs'/name).resolve();cfg=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json');finalPath=session/'isolation-host-final.json'
  if not finalPath.exists():finalPath=session/'isolation-final.json'
  final=read(finalPath)
  assert str(session)==cfg['session']==launch['session']==final['session'] and cfg['nonce']==launch['nonce']==final['nonce']
  process=owned_process_state(launch);assert process['ownProcessAlive'] is False
  staging=Path(cfg['stagingDirectory']).resolve();assert staging.parent==Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve() and staging.name=='CodexStage1Probe_'+cfg['nonce']
  module=Path(cfg['stagedARX']);ini=Path(cfg['stagedINI']);assert module.parent.resolve()==staging and ini.parent.resolve()==staging and ini==module.with_suffix('.ini')
  archive=session/'staging-final-archive';archive.mkdir(exist_ok=True)
  records=[]
  for f in [module,ini]:
   dest=archive/f.name
   if f.exists():
    h=sha(f)
    if f==module:assert h==cfg['binarySHA256']
    else:
     body=f.read_text(encoding='utf-16');assert cfg['nonce'] in body and str(session) in body
    if dest.exists():assert sha(dest)==h
    else:shutil.copyfile(f,dest);assert sha(dest)==h
    f.unlink()
   else:assert dest.is_file()
   records.append(dict(path=str(f),archive=str(dest),SHA256=sha(dest),removed=not f.exists()))
  if staging.exists() and not list(staging.iterdir()):staging.rmdir()
  window=(final.get('monitorStartedBeforeWorker') is True and final.get('monitorError') is None and final.get('droppedEvents')==0 and all(final.get(k)==0 for k in ['workerForegroundEvents','workerUserDesktopShowEvents','workerForegroundSamples']) and all(final.get(k) is False for k in ['inputDesktopChanged','switchDesktopCalled','userCadTouched']))
  rows.append(dict(session=str(session),process=process,finalPath=str(finalPath),final=final,windowIsolationPassed=window,normalQuitPassed=final.get('workerExitCode')==0 and final.get('nativeQuitConfirmed') is True and final.get('forcedTermination') is False,files=records,stagingDirectoryRemoved=not staging.exists()))
 inputs=[]
 baseline=P/'文字替换_输入模块与退出终核_20261030.json'
 for row in read(baseline)['inputs']:
  snapshot=Path(row['snapshot']['path']);source=Path(row['source']['path']);inputs.append(dict(id=row['id'],snapshot=dict(path=str(snapshot),SHA256=sha(snapshot)),snapshotUnchanged=sha(snapshot)==row['snapshot']['SHA256'],source=dict(path=str(source),SHA256=sha(source)),sourceEqualsPriorObservation=sha(source)==row['source']['SHA256']))
 shared=[]
 for name in ['CP_ARX','CP_NET']:
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804\Applications'+'\\'+name) as key:value,kind=winreg.QueryValueEx(key,'LOADCTRLS')
  assert value==2 and kind==winreg.REG_DWORD;shared.append(dict(name=name,LOADCTRLS=value,registryType=kind,action='read only; shared registration changes prohibited'))
 result=dict(contract='stage1.native-continuation-closure.v1',recorded=datetime.datetime.now().astimezone().isoformat(),sessions=rows,inputComparisonBaseline=str(baseline),inputs=inputs,sharedRegistrations=shared,phase1FullAcceptance=False,phase2Started=False)
 with output.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
 print(json.dumps(dict(output=str(output),sessions=len(rows),allSnapshotsUnchanged=all(x['snapshotUnchanged'] for x in inputs),sharedWrites=False),ensure_ascii=False))
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('sessions',nargs='+');a.add_argument('--output',required=True,type=Path);args=a.parse_args();main(args.sessions,args.output)
