"""Add a pinned project-owned ARX to the already authorized private native host."""
from pathlib import Path
import json,hashlib,shutil,datetime,sys
S=Path(__file__).resolve().parent;P=S.parent/'阶段1/原生接口探测'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def main():
 suffix=sys.argv[1] if len(sys.argv)>1 else "";assert suffix in ["","-v2","-v3","-v4","-v5","-v6","-v7","-v8","-v9","-v10","-v11","-v12","-v13","-v14","-v15","-v16","-v17"]
 session=Path((P/'current-autocad-session.txt').read_text(encoding='utf-8').strip());cfg=read(session/'worker-config.json');assert not (session/'isolation-host-receipt.json').exists()
 staging=Path(cfg['stagingDirectory']).resolve();assert staging.parent==Path('C:/Program Files/Autodesk/ApplicationPlugins').resolve() and staging.name=='CodexStage1Probe_'+cfg['nonce']
 module=staging/('BCP2_'+cfg['nonce'][:8]+'.arx');sidecar=module.with_suffix('.ini');assert not module.exists() and not sidecar.exists();binary=S/'bin/BuildingPlan2020.arx';shutil.copyfile(binary,module);assert sha(module)==sha(binary)
 inputManifest=S/'inputs'/('frozen-scheme-v2.json' if suffix in ['-v4','-v5','-v6','-v7','-v8','-v9','-v10','-v11','-v12','-v13','-v14','-v15','-v16','-v17'] else 'frozen-scheme-v1.json');inputs=read(inputManifest);job=S.parent/'阶段1/runs'/('stage2-foundation-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir()
 body='[stage2]\n'+''.join(f'{key}={value}\n' for key,value in dict(nonce=cfg['nonce'],source=inputs['source']['path'],sourceHash=inputs['source']['SHA256'],cache=inputs['cache']['path'],cacheHash=inputs['cache']['SHA256'],outputRoot=S/'runs',receiptDirectory=job).items());
 if suffix in ['-v4','-v5','-v6','-v7','-v8','-v9','-v10','-v11','-v12','-v13','-v14','-v15','-v16','-v17']:
  bank=read(S/'resources/native-axis-number-templates.json');reference=S.parent/'阶段0/输入快照/施工图平面参考图.dwg'
  body+=''.join(f'{k}={v}\n' for k,v in dict(axisBank=bank['library'],axisBankHash=bank['librarySHA256'],axisIndex=bank['index'],axisIndexHash=bank['indexSHA256'],reference=reference,referenceHash=sha(reference)).items())
 if suffix in ['-v9','-v10','-v11','-v12','-v13','-v14','-v15','-v16','-v17']:
  decisions=read(S/'inputs/user-axis-exclusions-v1.json');body+=f'axisDecisions={decisions["path"]}\naxisDecisionsHash={decisions["SHA256"]}\n'
 sidecar.write_text(body,encoding='utf-16')
 hostIni=Path(cfg['stagedINI']);text=hostIni.read_text(encoding='utf-16');assert '[stage2]' not in text;hostIni.write_text(text+f'\n[stage2]\nmodule={module}\n',encoding='utf-16')
 boot=session/'boot.scr';old=boot.read_bytes();assert old.endswith(b'S1QUIETSTART\r\n');boot.write_bytes(old[:-len(b'S1QUIETSTART\r\n')]+('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\n'+('BCP_STAGE2_ANALYZE\r\n' if suffix in ['-v6','-v8','-v9','-v10','-v11','-v12','-v13','-v14','-v15','-v16','-v17'] else '')+'S1QUIETSTART\r\n').encode('ascii'))
 candidate=session/'phase2-candidate';candidate.mkdir();shutil.copyfile(module,candidate/module.name)
 sources={f.name:sha(f) for f in (S/'src').iterdir() if f.is_file()}
 for f in (S/'src').iterdir():
  if f.is_file():shutil.copyfile(f,candidate/f.name)
 evidence=dict(contract='bcp.stage2.native-host-binding.v1',session=str(session),nonce=cfg['nonce'],module=str(module),moduleSHA256=sha(module),sidecar=str(sidecar),sidecarSHA256=sha(sidecar),sourceFiles=sources,inputManifest=str(inputManifest),sourceSHA256=inputs['source']['SHA256'],cacheSHA256=inputs['cache']['SHA256'],job=str(job),offlineAnalysis=str(S/'runs'/('analysis-v3' if suffix else 'analysis-v2')/'analysis.json'),bootSHA256=sha(boot),bridgeARXSHA256=cfg['binarySHA256'],scope='ObjectARX native callback to project-owned module;no COM/CUA/TCH commands;shared registration unchanged')
 out=S/f'evidence/native-host-binding{suffix}.json';assert not out.exists();out.write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(session=str(session),job=str(job),moduleSHA256=sha(module)),ensure_ascii=False))
if __name__=='__main__':main()
