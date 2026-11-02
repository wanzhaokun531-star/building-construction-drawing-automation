"""Stage a uniquely named native module and immutable boot binding; never attach to CAD."""
import hashlib
import json
import argparse
from pathlib import Path
import shutil
import uuid
from datetime import datetime

project=Path(__file__).resolve().parent
stage=project.parent
parser=argparse.ArgumentParser()
parser.add_argument('--reuse-session',type=Path)
args=parser.parse_args()
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
source_root=project/'src'
binary=project/'bin/NativeAccessProbe.arx'
reuse=None
if args.reuse_session:
 prior=args.reuse_session.resolve()
 assert prior.is_relative_to((stage/'runs').resolve())
 prior_config=prior/'worker-config.json'
 bound=json.loads(prior_config.read_text(encoding='utf-8-sig'))
 source_root=prior/'candidate'
 binary=source_root/'NativeAccessProbe.arx'
 assert digest(binary)==bound['binarySHA256']
 for name,wanted in bound['sourceFiles'].items():
  assert Path(name).name==name and digest(source_root/name)==wanted
 reuse=dict(session=str(prior),configSHA256=digest(prior_config))
scheme=stage.parent/'阶段0/输入快照/方案平面图.dwg'
reference=stage.parent/'阶段0/输入快照/施工图平面参考图.dwg'
assert digest(scheme)=='5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'
assert digest(reference)=='8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'
nonce=uuid.uuid4().hex
session=stage/'runs'/('isolated-axis-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
session.mkdir()
(session/'queue').mkdir()
sha=digest(binary)
trusted=Path('C:/Users/PC/AppData/Roaming/Tangent/TArch20V8/sys')
assert trusted.is_dir()
module=trusted/('S1Quiet_'+sha[:12]+'_'+nonce[:8]+'.arx')
ini=module.with_suffix('.ini')
assert not module.exists() and not ini.exists()
shutil.copyfile(binary,module)
assert digest(module)==sha
ini.write_text(f'[probe]\nroot={stage}\nscheme={scheme}\nreference={reference}\noutput={session}\n[worker]\nnonce={nonce}\nsession={session}\n',encoding='utf-16')
(session/'boot.scr').write_bytes(('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\nS1QUIETSTART\r\n').encode('ascii'))
sources={p.name:digest(p) for p in sorted(source_root.iterdir()) if p.suffix in ('.inc','.cpp','.vcxproj')}
config=dict(nonce=nonce,session=str(session),stagedARX=str(module),stagedINI=str(ini),binarySHA256=sha,sourceFiles=sources,controllerSHA256=digest(project/'run_quiet_job.py'),launcherSHA256=digest(project/'启动隔离桌面宿主.ps1'),helperSHA256=digest(project/'IsolatedCadHostNative.cs'),task='stage1 native/Tianzheng capability tests in isolated desktop; no phase2')
if reuse:config['sourceReuseEvidence']=reuse
(session/'worker-config.json').write_text(json.dumps(config,ensure_ascii=False,indent=2),encoding='utf-8')
archive=session/'candidate';archive.mkdir()
shutil.copyfile(binary,archive/binary.name)
for p in source_root.iterdir():
 if p.suffix in ('.inc','.cpp','.vcxproj'):shutil.copyfile(p,archive/p.name)
for name in ('run_quiet_job.py','启动隔离桌面宿主.ps1','IsolatedCadHostNative.cs'):shutil.copyfile(project/name,archive/name)
(project/'current-quiet-session.txt').write_text(str(session),encoding='utf-8')
print(json.dumps(dict(session=str(session),nonce=nonce,binarySHA256=sha),ensure_ascii=False))
