"""Prepare only a native door-caption correction on the pinned v74 copy."""
import sys,json,shutil,datetime,importlib.util
from pathlib import Path
from chain_contract import S,read,sha
from verify_door_caption_clearance import caption_source,old_geometry
P=S.parent/'阶段1/原生接口探测';version=sys.argv[1];assert version.startswith('v') and 75<=int(version[1:])<=99
out=S/f'evidence/native-host-binding-{version}.json';assert not out.exists()
spec=importlib.util.spec_from_file_location('prepare_native_host',P/'prepare_autocad_session.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.main(P/'evidence/build-whole-chain-bridge-20261031.json',P/'evidence/autocad-support-profile-20261029.json',True)
session=Path((P/'current-autocad-session.txt').read_text(encoding='utf-8').strip());cfg=read(session/'worker-config.json');job=S.parent/'阶段1/runs'/('stage2-door-captions-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir()
source=Path(read(S/'evidence/whole-chain-delivery-v74.json')['file']);assert sha(source)=='4CAEC8BAABCAA0CCFB04594A3A01796FE68BBA5CE85ED8A389699AD9A7C8D6DF'
entities={e['handle']:e for e in old_geometry()};rows=['DOORCAP1 322 14']
for c in caption_source():
    if not c['text'].startswith('M'):continue
    a,b=entities[c['opening']]['grip']['points'][1:3];vertical=abs(a[0]-b[0])<1e-6;normal=0 if vertical else 1;centre=(a[normal]+b[normal])/2;side=1 if c['position'][normal]>centre else -1
    rows.append('CAP '+c['handle']+' '+c['opening']+' '+str(side)+' '+' '.join(format(v,'.17g') for v in a[:2]+b[:2]))
assert len(rows)==15;contract=job/'door-caption-contract.tsv';contract.write_text('\n'.join(rows)+'\n',encoding='ascii')
staging=Path(cfg['stagingDirectory']);module=staging/('BCP2_'+cfg['nonce'][:8]+'.arx');sidecar=module.with_suffix('.ini');shutil.copyfile(S/'bin/BuildingPlan2020.arx',module)
settings=dict(nonce=cfg['nonce'],receiptDirectory=job,doorCaptionClearance=1,captionInput=source,captionInputHash=sha(source),captionContract=contract,captionContractHash=sha(contract))
sidecar.write_text('[stage2]\n'+''.join(f'{k}={v}\n' for k,v in settings.items()),encoding='utf-16');hostini=Path(cfg['stagedINI']);hostini.write_text(hostini.read_text(encoding='utf-16')+f'\n[stage2]\nmodule={module}\n',encoding='utf-16')
boot=session/'boot.scr';old=boot.read_bytes();assert old.endswith(b'S1QUIETSTART\r\n');boot.write_bytes(old[:-len(b'S1QUIETSTART\r\n')]+('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\nBCP_STAGE2_ANALYZE\r\nS1QUIETSTART\r\n').encode('ascii'))
candidate=session/'phase2-candidate';candidate.mkdir();shutil.copyfile(module,candidate/module.name);sources={p.name:sha(p) for p in (S/'src').iterdir() if p.is_file()}
for name in sources:shutil.copyfile(S/'src'/name,candidate/name)
result=dict(contract='bcp.stage2.native-host-binding.v1',session=str(session),nonce=cfg['nonce'],module=str(module),moduleSHA256=sha(module),sidecar=str(sidecar),sidecarSHA256=sha(sidecar),sourceFiles=sources,job=str(job),bootSHA256=sha(boot),bridgeARXSHA256=cfg['binarySHA256'],input=str(source),inputSHA256=sha(source),captionContract=str(contract),captionContractSHA256=sha(contract),scope='SDK only door caption movement; no TCH business commands, COM/CUA; apron stopped')
out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=version,session=str(session),job=str(job)),ensure_ascii=False))
