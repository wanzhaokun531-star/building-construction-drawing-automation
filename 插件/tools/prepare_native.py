"""Owned isolated-desktop integration; not installed or run by end users."""
from pathlib import Path
import json,hashlib,shutil,sys,datetime,importlib.util
ROOT=Path(__file__).resolve().parents[2];S=ROOT/'阶段2';P=ROOT/'阶段1/原生接口探测';APP=ROOT/'插件'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def main(version,source_override=None,behavior="generate",module_override=None,resources_override=None) :
 assert version.startswith('v') and 80<=int(version[1:])<=299
 binding=S/f'evidence/native-host-binding-{version}.json';assert not binding.exists()
 spec=importlib.util.spec_from_file_location('prepare_owned_host',P/'prepare_autocad_session.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 m.main(P/'evidence/build-product-bridge-v5-20261101.json',P/'evidence/autocad-support-profile-20261029.json',True)
 session=Path((P/'current-autocad-session.txt').read_text(encoding='utf-8').strip());cfg=read(session/'worker-config.json')
 job=ROOT/'阶段1/runs'/('stage2-product-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));job.mkdir()
 source=Path(source_override).resolve() if source_override else ROOT/'阶段0/输入快照/方案平面图.dwg';assert source.is_file();shutil.copyfile(ROOT/'阶段0/输入快照/方案平面图.dwg' if behavior.startswith('inspect') else source,job/'control.dwg');test_input=job/'inspection-input.dwg' if behavior.startswith('inspect') else job/'control.dwg';
 if behavior.startswith('inspect'):shutil.copyfile(source,test_input)
 output=job/'插件生成施工图.dwg'
 staging=Path(cfg['stagingDirectory']);module=staging/('BCP2_'+cfg['nonce'][:8]+'.arx');sidecar=module.with_suffix('.ini');shutil.copyfile(Path(module_override).resolve() if module_override else APP/'bin/BuildingPlanPlugin2020.arx',module)
 resources=Path(resources_override).resolve() if resources_override else APP/'build/input-adaptation-resources'
 settings=f'[stage2]\nnonce={cfg["nonce"]}\nreceiptDirectory={job}\n[product]\nvalidationMode=1\ntangentDirectory=C:\\Tangent\\TArchT20V8\\SYS23x64\nresourceRoot={resources}\ntestBehavior={behavior}\ntestInput={test_input}\ntestOutput={output}\n'
 sidecar.write_text(settings,encoding='utf-16');ini=Path(cfg['stagedINI']);ini.write_text(ini.read_text(encoding='utf-16')+f'\n[stage2]\nmodule={module}\n',encoding='utf-16')
 boot=session/'boot.scr';old=boot.read_bytes();assert old.endswith(b'S1QUIETSTART\r\n');boot.write_bytes(old[:-len(b'S1QUIETSTART\r\n')]+('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\nS1QUIETSTART\r\n').encode('ascii'))
 candidate=session/'phase2-candidate';candidate.mkdir();shutil.copyfile(module,candidate/module.name);sources={f.name:sha(f) for f in (S/'src').iterdir() if f.is_file()}
 for name in sources:shutil.copyfile(S/'src'/name,candidate/name)
 product_sources={str(f.relative_to(APP)):sha(f) for f in (APP/'src').iterdir() if f.is_file()};(candidate/'product').mkdir()
 for name in product_sources:shutil.copyfile(APP/name,candidate/'product'/Path(name).name)
 r=dict(contract='bcp.stage2.native-host-binding.v1',session=str(session),nonce=cfg['nonce'],module=str(module),moduleSHA256=sha(module),sidecar=str(sidecar),sidecarSHA256=sha(sidecar),sourceFiles=sources,productSources=product_sources,job=str(job),bootSHA256=sha(boot),bridgeARXSHA256=cfg['binarySHA256'],input=str(source),inputSHA256=sha(source),controlInputSHA256=sha(job/'control.dwg'),output=str(output),resources={str(f.relative_to(resources)):sha(f) for f in resources.rglob('*') if f.is_file()},scope='BCP_BUILD same production function; native AutoCAD own copy, only authorized TMergeDim, no COM/CUA; apron paused')
 binding.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=version,session=str(session),job=str(job)),ensure_ascii=False))
if __name__=='__main__':main(sys.argv[1],sys.argv[2] if len(sys.argv)>2 else None,sys.argv[3] if len(sys.argv)>3 else 'generate',sys.argv[4] if len(sys.argv)>4 else None,sys.argv[5] if len(sys.argv)>5 else None)
