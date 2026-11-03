from pathlib import Path
import json, hashlib, shutil, sys
R=Path(__file__).resolve().parents[3]
def read(p): return json.loads(p.read_text('utf-8-sig'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
records=[]
for arg in sys.argv[1:]:
 b=read(R/f'阶段2/evidence/native-host-binding-{arg}.json');s=Path(b['session']);c=read(s/'worker-config.json');f=read(s/'isolation-final.json');q=read(s/'normal-quit-verification.json')
 assert f['nonce']==q['nonce']==b['nonce']==c['nonce'] and f['workerExitCode']==0 and q['completed'] and not f['forcedTermination']
 stage=Path(c['stagingDirectory']).resolve();assert stage.parent==Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve() and stage.name=='CodexStage1Probe_'+b['nonce']
 a=s/'staging-final-archive';a.mkdir(exist_ok=True);files=[]
 for name in [c['stagedARX'],c['stagedINI'],b['module'],b['sidecar']]:
  p=Path(name).resolve();assert p.parent==stage;target=a/p.name
  if p.exists():
   digest=sha(p)
   if target.exists():assert sha(target)==digest
   else:shutil.copyfile(p,target)
   assert sha(target)==digest;p.unlink()
  else:assert target.is_file();digest=sha(target)
  files.append(dict(path=str(p),archive=str(target),SHA256=digest))
 if stage.exists():
  assert not list(stage.iterdir());stage.rmdir()
 records.append(dict(version=arg,nonce=b['nonce'],processId=f['processId'],normalQUIT=True,files=files,ownShowEvents=f['workerUserDesktopShowEvents'],ownForegroundEvents=f['workerForegroundEvents']))
out=Path(__file__).parent/('closed-hosts-'+sys.argv[1]+'-'+sys.argv[-1]+'.json');out.write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8');print(str(out))
