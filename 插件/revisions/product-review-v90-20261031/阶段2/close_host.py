"""Attest exited private worker and archive only four exact staged module files."""
from pathlib import Path
import json,hashlib,sys,datetime,winreg,time
S=Path(__file__).resolve().parent;P=S.parent/'阶段1/原生接口探测';sys.path.insert(0,str(P))
from collect_symbol_closure import owned_process_state
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def main():
 suffix=sys.argv[1] if len(sys.argv)>1 else '';assert suffix in ['', '-v2','-v3','-v4','-v5','-v6','-v7','-v8','-v9','-v10','-v11','-v12','-v13','-v14','-v15','-v16','-v17','-v18','-v19','-v20','-v21','-v22','-v23','-v24','-v25','-v26','-v27','-v28','-v29','-v30','-v31','-v32','-v33','-v34','-v35','-v36','-v37','-v38','-v39','-v40','-v41','-v42','-v43','-v44','-v45','-v46','-v47','-v48','-v49','-v50','-v51','-v52','-v53','-v54','-v55','-v56','-v57','-v58','-v59','-v60','-v61','-v62','-v63','-v64','-v65','-v66','-v67','-v68','-v69','-v70']+[f'-v{i}' for i in range(71,100)];binding=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(binding['session']);cfg=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json');finalPath=session/'isolation-host-final.json'
 deadline=time.monotonic()+3
 while not finalPath.exists() and not (session/'isolation-final.json').exists() and time.monotonic()<deadline:time.sleep(.1)
 if not finalPath.exists():finalPath=session/'isolation-final.json'
 final=read(finalPath);assert cfg['nonce']==binding['nonce']==launch['nonce']==final['nonce'] and str(session)==cfg['session']==launch['session']==final['session'];state=owned_process_state(launch);assert state['ownProcessAlive'] is False
 staging=Path(cfg['stagingDirectory']).resolve();assert staging.parent==Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve() and staging.name=='CodexStage1Probe_'+cfg['nonce'];archive=session/'staging-final-archive';archive.mkdir(exist_ok=True);items=[]
 expected={cfg['stagedARX']:cfg['binarySHA256'],binding['module']:binding['moduleSHA256'],binding['sidecar']:binding['sidecarSHA256']}
 for text in [cfg['stagedARX'],cfg['stagedINI'],binding['module'],binding['sidecar']]:
  file=Path(text);assert file.parent.resolve()==staging;h=sha(file)
  if text in expected:assert h==expected[text]
  else:assert cfg['nonce'] in file.read_text(encoding='utf-16') and str(session) in file.read_text(encoding='utf-16')
  target=archive/file.name;assert not target.exists();target.write_bytes(file.read_bytes());assert sha(target)==h;file.unlink();items.append(dict(path=str(file),archive=str(target),SHA256=h,removed=not file.exists()))
 assert not list(staging.iterdir());staging.rmdir();prior=P/'evidence/native-continuation-all-six-closure-20261030.json';inputs=[]
 for row in read(prior)['inputs']:
  snap=Path(row['snapshot']['path']);source=Path(row['source']['path']);inputs.append(dict(id=row['id'],snapshot=dict(path=str(snap),SHA256=sha(snap)),snapshotUnchanged=sha(snap)==row['snapshot']['SHA256'],source=dict(path=str(source),SHA256=sha(source)),sourceEqualsPriorObservation=sha(source)==row['source']['SHA256']))
 shared=[]
 for name in ['CP_ARX','CP_NET']:
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804\Applications'+'\\'+name) as key:value,kind=winreg.QueryValueEx(key,'LOADCTRLS')
  assert value==2 and kind==winreg.REG_DWORD;shared.append(dict(name=name,LOADCTRLS=value,registryType=kind,action='read only'))
 attempted=(S/f'evidence/host{suffix}-exit-observation.log').exists()
 result=dict(contract='bcp.stage2.host-closure.v1',recorded=datetime.datetime.now().astimezone().isoformat(),session=str(session),process=state,finalPath=str(finalPath),final=final,files=items,stagingRemoved=not staging.exists(),inputs=inputs,sharedRegistrations=shared,phase2Started=True,stage2Complete=False,normalQuitPassed=False,exitAttempted=attempted,exitObservationSeconds=60 if attempted else None,scope='Exact own stop retained; absent exit attempt is not a normal-QUIT test; previous failures remain')
 out=S/f'evidence/host-closure{suffix}.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(allSnapshotsUnchanged=all(x['snapshotUnchanged'] for x in inputs),filesCleaned=len(items),stage2Complete=False),ensure_ascii=False))
if __name__=='__main__':main()
