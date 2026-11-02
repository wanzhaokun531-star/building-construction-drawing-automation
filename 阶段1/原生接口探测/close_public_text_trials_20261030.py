"""Only archive and unlink exact modules of this request's exited own hosts."""
from pathlib import Path
import hashlib,json,shutil,datetime
from collect_symbol_closure import owned_process_state
P=Path(__file__).resolve().parent;STAGE=P.parent
SESSIONS=['isolated-autocad-20261030-103816','isolated-autocad-20261030-104626','isolated-autocad-20261030-105256']
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
results=[]
for name in SESSIONS:
 session=(STAGE/'runs'/name).resolve();cfg=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json');final=read(session/'isolation-final.json');identity=owned_process_state(launch)
 assert identity['ownProcessAlive'] is False
 assert cfg['nonce']==launch['nonce']==final['nonce'] and Path(cfg['session']).resolve()==session
 assert final['workerExited'] is True and final['stillRunningOwnedWorker'] is False
 directory=Path(cfg['stagingDirectory']).resolve();assert directory.parent==Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve() and directory.name=='CodexStage1Probe_'+cfg['nonce']
 files=[Path(cfg['stagedARX']).resolve(),Path(cfg['stagedINI']).resolve()];assert all(x.parent==directory for x in files)
 archive=session/'staging-final-archive';archive.mkdir(exist_ok=True)
 records=[]
 for f in files:
  dst=archive/f.name
  if f.exists():
   digest=sha(f)
   if f.suffix.lower()=='.arx':assert digest==cfg['binarySHA256']
   else:
    text=f.read_text(encoding='utf-16');assert 'nonce='+cfg['nonce'] in text and 'session='+str(session) in text
   assert not dst.exists();shutil.copyfile(f,dst);assert sha(dst)==digest;assert sha(f)==digest;f.unlink()
  else:assert dst.is_file()
  records.append(dict(archive=str(dst),SHA256=sha(dst),stagedAbsent=not f.exists()))
 if directory.exists() and not list(directory.iterdir()):directory.rmdir()
 results.append(dict(session=name,nonce=cfg['nonce'],identity=identity,exitCode=final['workerExitCode'],forced=final['forcedTermination'],nativeQuitConfirmed=final['nativeQuitConfirmed'],ownUserShows=final['workerUserDesktopShowEvents'],ownForegroundEvents=final['workerForegroundEvents'],inputDesktopChanged=final['inputDesktopChanged'],files=records,stagingDirectoryAbsent=not directory.exists()))
prior=read(P/'纯原生阶段1_输入模块与退出终核_20261030.json');inputs=[]
for row in prior['inputs']:
 item={'id':row['id']}
 for key in ('source','snapshot'):
  f=Path(row[key]['path']);digest=sha(f);item[key]=dict(path=str(f),SHA256=digest,unchangedComparedToPrior=digest==row[key]['SHA256'])
 assert item['snapshot']['unchangedComparedToPrior'];inputs.append(item)
out=dict(recorded=datetime.datetime.now().astimezone().isoformat(),contract='stage1.public-text-failed-trials-closure.v1',sessions=results,inputs=inputs,phase1Passed=False,replacementSucceeded=False,scope='Three failed native TCH text modification trials; user clarification of native-text replacement permission pending. No failed result is rewritten as PASS.')
path=P/'天正文字替换_失败试验终核_20261030.json';assert not path.exists();path.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(sessions=len(results),allOwnHostsExited=all(x['identity']['ownProcessAlive'] is False for x in results),ownUserShows=sum(x['ownUserShows'] for x in results),ownForeground=sum(x['ownForegroundEvents'] for x in results),allStagedModulesArchivedAndRemoved=all(all(f['stagedAbsent'] for f in x['files']) for x in results),sixSnapshotsUnchanged=all(x['snapshot']['unchangedComparedToPrior'] for x in inputs)),ensure_ascii=False))
