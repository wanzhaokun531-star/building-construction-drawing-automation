"""Prepare an owned session for only the user-authorized TMergeDim operation."""
import sys, json, shutil, datetime, importlib.util
from pathlib import Path
from chain_contract import S,read,sha,write_contract
P=S.parent/'阶段1/原生接口探测'
version=sys.argv[1];assert version.startswith('v') and 70<=int(version[1:])<=99
binding_path=S/f'evidence/native-host-binding-{version}.json';assert not binding_path.exists()
build=P/'evidence/build-whole-chain-bridge-20261031.json'
spec=importlib.util.spec_from_file_location('host_prepare',P/'prepare_autocad_session.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.main(build,P/'evidence/autocad-support-profile-20261029.json',True)
session=Path((P/'current-autocad-session.txt').read_text(encoding='utf-8').strip());cfg=read(session/'worker-config.json')
job=S.parent/'阶段1/runs'/('stage2-whole-chain-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir()
source=Path(read(S/'evidence/native-run-v67.json')['output'])/'方案施工图_阶段2中间图.dwg'
assert sha(source)=='F31F245A0303E627EE2B120A6897B12CC954E54B352F330900548ABB385E7723'
shutil.copyfile(source,job/'control.dwg');contract=job/'whole-chain-contract.tsv';chains=write_contract(contract)
(job/'chain-contract.json').write_text(json.dumps(chains,ensure_ascii=False,indent=2),encoding='utf-8')
(job/'phase2-chain-merge.only').write_text('User authorized fixed TMergeDim only.\n',encoding='ascii')
staging=Path(cfg['stagingDirectory']);module=staging/('BCP2_'+cfg['nonce'][:8]+'.arx');sidecar=module.with_suffix('.ini');binary=S/'bin/BuildingPlan2020.arx';shutil.copyfile(binary,module)
component=Path('C:/Tangent/TArchT20V8/SYS23x64/tch_annotate.arx')
settings=dict(nonce=cfg['nonce'],receiptDirectory=job,wholeDimensionChains=1,allowTMergeDim=1,chainInput=source,chainInputHash=sha(source),chainContract=contract,chainContractHash=sha(contract),chainComponent=component,chainComponentHash=sha(component))
sidecar.write_text('[stage2]\n'+''.join(f'{k}={v}\n' for k,v in settings.items()),encoding='utf-16')
host_ini=Path(cfg['stagedINI']);host_ini.write_text(host_ini.read_text(encoding='utf-16')+f'\n[stage2]\nmodule={module}\n',encoding='utf-16')
boot=session/'boot.scr';old=boot.read_bytes();assert old.endswith(b'S1QUIETSTART\r\n');boot.write_bytes(old[:-len(b'S1QUIETSTART\r\n')]+('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\nS1QUIETSTART\r\n').encode('ascii'))
candidate=session/'phase2-candidate';candidate.mkdir();shutil.copyfile(module,candidate/module.name)
sources={p.name:sha(p) for p in (S/'src').iterdir() if p.is_file()}
for name in sources:shutil.copyfile(S/'src'/name,candidate/name)
result=dict(contract='bcp.stage2.native-host-binding.v1',session=str(session),nonce=cfg['nonce'],module=str(module),moduleSHA256=sha(module),sidecar=str(sidecar),sidecarSHA256=sha(sidecar),sourceFiles=sources,job=str(job),bootSHA256=sha(boot),bridgeARXSHA256=cfg['binarySHA256'],chainContract=str(contract),chainContractSHA256=sha(contract),inputSHA256=sha(source),component=str(component),componentSHA256=sha(component),scope='User-authorized fixed TMergeDim only; ObjectARX; no COM/CUA; apron work stopped')
binding_path.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(version=version,session=str(session),job=str(job),moduleSHA256=sha(module)),ensure_ascii=False))
