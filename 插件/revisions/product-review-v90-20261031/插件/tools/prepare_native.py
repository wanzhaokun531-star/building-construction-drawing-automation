"""Owned isolated-desktop integration; not installed or run by end users."""
from pathlib import Path
import json,hashlib,shutil,sys,datetime,importlib.util
ROOT=Path(__file__).resolve().parents[2];S=ROOT/'阶段2';P=ROOT/'阶段1/原生接口探测';APP=ROOT/'插件'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def main(version):
 assert version.startswith('v') and 80<=int(version[1:])<=99
 binding=S/f'evidence/native-host-binding-{version}.json';assert not binding.exists()
 spec=importlib.util.spec_from_file_location('prepare_owned_host',P/'prepare_autocad_session.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 m.main(P/'evidence/build-product-bridge-v3-20261031.json',P/'evidence/autocad-support-profile-20261029.json',True)
 session=Path((P/'current-autocad-session.txt').read_text(encoding='utf-8').strip());cfg=read(session/'worker-config.json')
 job=ROOT/'阶段1/runs'/('stage2-product-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir()
 source=ROOT/'阶段0/输入快照/方案平面图.dwg';shutil.copyfile(source,job/'control.dwg');output=job/'插件生成施工图.dwg'
 staging=Path(cfg['stagingDirectory']);module=staging/('BCP2_'+cfg['nonce'][:8]+'.arx');sidecar=module.with_suffix('.ini');shutil.copyfile(APP/'bin/BuildingPlanPlugin2020.arx',module)
 resources=APP/'build/BuildingPlan2020.bundle/Contents/Win64/resources'
 settings=f'[stage2]\nnonce={cfg["nonce"]}\nreceiptDirectory={job}\n[product]\nvalidationMode=1\ntangentDirectory=C:\\Tangent\\TArchT20V8\\SYS23x64\nresourceRoot={resources}\ntestInput={job/"control.dwg"}\ntestOutput={output}\n'
 sidecar.write_text(settings,encoding='utf-16');ini=Path(cfg['stagedINI']);ini.write_text(ini.read_text(encoding='utf-16')+f'\n[stage2]\nmodule={module}\n',encoding='utf-16')
 boot=session/'boot.scr';old=boot.read_bytes();assert old.endswith(b'S1QUIETSTART\r\n');boot.write_bytes(old[:-len(b'S1QUIETSTART\r\n')]+('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\nS1QUIETSTART\r\n').encode('ascii'))
 candidate=session/'phase2-candidate';candidate.mkdir();shutil.copyfile(module,candidate/module.name);sources={f.name:sha(f) for f in (S/'src').iterdir() if f.is_file()}
 for name in sources:shutil.copyfile(S/'src'/name,candidate/name)
 product_sources={str(f.relative_to(APP)):sha(f) for f in (APP/'src').iterdir() if f.is_file()};(candidate/'product').mkdir()
 for name in product_sources:shutil.copyfile(APP/name,candidate/'product'/Path(name).name)
 r=dict(contract='bcp.stage2.native-host-binding.v1',session=str(session),nonce=cfg['nonce'],module=str(module),moduleSHA256=sha(module),sidecar=str(sidecar),sidecarSHA256=sha(sidecar),sourceFiles=sources,productSources=product_sources,job=str(job),bootSHA256=sha(boot),bridgeARXSHA256=cfg['binarySHA256'],input=str(source),inputSHA256=sha(source),controlInputSHA256=sha(job/'control.dwg'),output=str(output),resources={str(f.relative_to(resources)):sha(f) for f in resources.iterdir() if f.is_file()},scope='BCP_BUILD same production function; native AutoCAD own copy, only authorized TMergeDim, no COM/CUA; apron paused')
 binding.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=version,session=str(session),job=str(job)),ensure_ascii=False))
if __name__=='__main__':main(sys.argv[1])
