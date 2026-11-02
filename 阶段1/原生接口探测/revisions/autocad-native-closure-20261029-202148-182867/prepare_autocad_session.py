"""Bind the existing native probe to a standard AutoCAD profile.

No Tianzheng launcher, profile edit, autoload registration, COM, or UI driver.
The temporary Program Files directory has no bundle manifest and is not an
installed/autoloaded plugin. Load the unique module explicitly in boot.scr.
"""
import argparse,hashlib,json,shutil,uuid
from pathlib import Path
from datetime import datetime

P=Path(__file__).resolve().parent;S=P.parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def main(build_receipt,profile_config):
 nonce=uuid.uuid4().hex
 session=S/'runs'/('isolated-autocad-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
 template=Path('C:/Users/PC/AppData/Local/Autodesk/AutoCAD 2020/R23.1/chs/Template/acadiso.dwt')
 binary=P/'bin/NativeAccessProbe.arx';profile='<<未命名配置>>';support=None
 if profile_config:
  support=json.loads(profile_config.read_text(encoding='utf-8-sig'));assert support['contract']=='stage1.dedicated-autocad-profile.v1' and support['authorization']=='User: 允许，仅修改专用测试配置' and support['SECURELOAD']=='1' and support['sourceProfileSHA256Before']==support['sourceProfileSHA256After']
  profile=support['profile'];assert profile.startswith('CodexS1Native_')
  assert sha(Path(support['componentPath']))==support['componentSHA256']
 scheme=S.parent/'阶段0/输入快照/方案平面图.dwg';reference=S.parent/'阶段0/输入快照/施工图平面参考图.dwg'
 assert sha(scheme)=='5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'
 assert sha(reference)=='8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'
 assert template.is_file()
 prior=S/'runs/isolated-axis-20261029-154336';cfg=json.loads((prior/'worker-config.json').read_text(encoding='utf-8-sig'))
 if build_receipt:
  build_receipt=build_receipt.resolve();assert build_receipt.is_relative_to((P/'evidence').resolve())
  build=json.loads(build_receipt.read_text(encoding='utf-8-sig'));assert build['exitCode']==0 and sha(binary)==build['binarySHA256']
  sources=build['sourceFiles']
 else:
  assert sha(binary)=='987D633C3CBE660E3AEDF7B2D8DD2B11083F55D409BED182CC67BBD4E2384C1E';sources=cfg['sourceFiles']
 for name,digest in sources.items():assert sha(P/'src'/name)==digest
 session.mkdir();(session/'queue').mkdir()
 trusted=Path('C:/Program Files/Autodesk/ApplicationPlugins')/('CodexStage1Probe_'+nonce)
 trusted.mkdir();module=trusted/('S1Acad_'+nonce[:8]+'.arx');ini=module.with_suffix('.ini')
 shutil.copyfile(binary,module)
 extra=f'expectedProfile={profile}\nsupportComponent={support["componentPath"]}\n' if support else ''
 ini.write_text(f'[probe]\nroot={S}\nscheme={scheme}\nreference={reference}\noutput={session}\n[worker]\nnonce={nonce}\nsession={session}\n'+extra,encoding='utf-16')
 (session/'boot.scr').write_bytes(('_.ARX\r\n_L\r\n"'+module.as_posix()+'"\r\nS1QUIETSTART\r\n').encode('ascii'))
 candidate=session/'candidate';candidate.mkdir();shutil.copyfile(binary,candidate/binary.name)
 for name in sources:shutil.copyfile(P/'src'/name,candidate/name)
 for name in ('run_quiet_job.py','启动隔离桌面宿主.ps1','IsolatedCadHostNative.cs','prepare_autocad_session.py'):shutil.copyfile(P/name,candidate/name)
 result=dict(nonce=nonce,session=str(session),stagedARX=str(module),stagedINI=str(ini),stagingDirectory=str(trusted),binarySHA256=sha(module),sourceFiles=sources,controllerSHA256=sha(P/'run_quiet_job.py'),launcherSHA256=sha(P/'启动隔离桌面宿主.ps1'),helperSHA256=sha(P/'IsolatedCadHostNative.cs'),profile=profile,template=str(template),templateSHA256=sha(template),hostMode='standard AutoCAD profile, actual loaded modules must be observed',sourceReuseEvidence=str(build_receipt or prior),task='stage1 ordinary AutoCAD host baseline; read-only sources, no COM/CUA/TCH commands/phase2')
 if profile_config:result['dedicatedProfileEvidence']=dict(path=str(profile_config.resolve()),SHA256=sha(profile_config))
 (session/'worker-config.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
 (P/'current-autocad-session.txt').write_text(str(session),encoding='utf-8')
 print(json.dumps(dict(session=str(session),nonce=nonce,profile=profile,binarySHA256=sha(module)),ensure_ascii=False))

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--build-receipt',type=Path);parser.add_argument('--profile-config',type=Path);args=parser.parse_args();main(args.build_receipt,args.profile_config)
