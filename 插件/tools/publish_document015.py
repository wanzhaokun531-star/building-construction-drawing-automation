"""Deliver the actual v196 candidate; do not touch the user's installed module."""
from pathlib import Path
import json,hashlib,shutil,zipfile,datetime
R=Path(__file__).resolve().parents[2];A=R/'插件'
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
b=read(R/'阶段2/evidence/native-host-binding-v196.json');s=Path(b['session']);j=Path(b['job'])
result=read(A/'evidence/native-rules-v196.json')['result'];positive=read(j/'document-state-probe.json');negative=read(A/'evidence/document-negative-v196.json')
quit=read(s/'normal-quit-verification.json');closed=read(R/'阶段2/项目/百塘村邹总_20261103/closed-hosts-v196-v196.json')[0]
assert positive['after']==20 and positive['aliasSameFunction'] and result['completed'] and result['sourceUnchanged']
assert negative['passed'] and negative['probe']['after']&1 and not negative['result']['completed']
assert quit['completed'] and quit['exitCode']==0 and not quit['forcedTermination']
assert closed['ownShowEvents']==closed['ownForegroundEvents']==0 and read(A/'evidence/unload-product-v196.json')['unloaded']
for p,h in b['productSources'].items():assert sha(A/p)==h,p
for p,h in b['sourceFiles'].items():assert sha(R/'阶段2/src'/p)==h,p
assert sha(b['input'])==b['inputSHA256'] and sha(result['output'])==result['outputSHA256']
bundle=A/'build/document-015-candidate/BuildingPlan2020.bundle';resources=bundle/'Contents/Win64/resources';exe=A/'build/document-015-installer/BuildingPlanSetup.exe'
assert sha(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx')==b['moduleSHA256']
assert {str(p.relative_to(resources)):sha(p) for p in resources.rglob('*') if p.is_file()}==b['resources']
assert all(sha(bundle/p)==h for p,h in read(bundle/'SHA256.json').items())
for row in read(A/'installer/build/payload.json'):assert sha(bundle/row['relative'])==row['SHA256']
assert (A/'evidence/installer015-from014.log').read_text('utf-8-sig').strip().endswith('OK')
out=A/'交付/建筑施工图助手_0.1.5_20261103';assert not out.exists();out.mkdir();shutil.copytree(bundle,out/bundle.name);shutil.copyfile(exe,out/'安装程序.exe')
for label in ['使用说明','验证报告']:shutil.copyfile(A/f'docs/0.1.5{label}.md',out/f'{label}.md')
examples=out/'示例';examples.mkdir();dwg=examples/'百塘村邹总_三层平面_插件0.1.5.dwg';shutil.copyfile(result['output'],dwg)
source=out/'源码'
for rel in ['插件/src','阶段2/src','插件/installer/src']:shutil.copytree(R/rel,source/rel)
for rel in ['插件/installer/build_payload.py',*['插件/installer/previous-version-'+v+'.json' for v in ['0.1.1','0.1.2','0.1.3','0.1.4']],'插件/PackageContents.xml']:
 dst=source/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/rel,dst)
shutil.copytree(resources,source/'插件/resources/runtime');shutil.copyfile(out/'使用说明.md',source/'插件/使用说明.md')
old=A/'交付/建筑施工图助手_0.1.4_20261103/源码';shutil.copyfile(old/'构建.ps1',source/'构建.ps1');shutil.copyfile(A/'构建说明.md',out/'构建说明.md')
(source/'说明.md').write_text('保持插件/src与阶段2/src目录关系，运行构建.ps1。需要VS2019 v141、Windows SDK10.0.19041、ObjectARX2020。运行资源已包含，不需要Python。安装器重建使用installer/build_payload.py为bundle及0.1.1至0.1.4清单生成载荷，再构建installer/src/BuildingPlanSetup.vcxproj。重新构建后需重新验证。\n','utf-8')
tests=source/'插件/tests';tests.mkdir()
for name in ['PolicyTests.cpp','PolicyTests.vcxproj','ThreeFloorContractTests.cpp','ThreeFloorContractTests.vcxproj','ApprovedAreaTests.cpp','ApprovedLayoutContractTests.cpp']:shutil.copyfile(A/'tests'/name,tests/name)
records=out/'验证记录';records.mkdir()
for name in ['native-rules-v196.json','document-negative-v196.json','installer015-from014.log','build-document-015.log']:shutil.copyfile(A/'evidence'/name,records/name)
shutil.copyfile(j/'document-state-probe.json',records/'document-state-20.json');shutil.copyfile(s/'normal-quit-verification.json',records/'normal-quit-verification.json')
files={str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()};(out/'SHA256.json').write_text(json.dumps(files,ensure_ascii=False,indent=2),'utf-8')
zip_path=out.parent/(out.name+'.zip');assert not zip_path.exists()
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in out.rglob('*'):
  if p.is_file():z.write(p,p.relative_to(out))
with zipfile.ZipFile(zip_path) as z:
 assert set(z.namelist())=={p.relative_to(out).as_posix() for p in out.rglob('*') if p.is_file()}
 for p,h in files.items():assert hashlib.sha256(z.read(Path(p).as_posix())).hexdigest().upper()==h
standalone=out.parent/'建筑施工图助手_0.1.5_安装程序.exe';assert not standalone.exists();shutil.copyfile(exe,standalone)
revision=A/'revisions/pre-document-015-20261103';revision.mkdir();installed=A/'bin/BuildingPlanPlugin2020.arx';shutil.copyfile(installed,revision/installed.name);shutil.copyfile(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx',installed)
delivery=dict(version='0.1.5',recorded=datetime.datetime.now().astimezone().isoformat(),nativeVersion='v196',releaseDirectory=str(out),ZIP=str(zip_path),ZIP_SHA256=sha(zip_path),installer=str(standalone),installerSHA256=sha(standalone),ARXSHA256=b['moduleSHA256'],DWG=str(dwg),DWGSHA256=sha(dwg),DBMOD20Generated=True,unsavedEntityBlocked=True,shortCommand='BCP',policyChecks=41,installerTestsPassed=9,normalQUIT=True,actualAppDataInstalled=False,GitHubPushed=False)
(A/'evidence/release-document-015-20261103.json').write_text(json.dumps(delivery,ensure_ascii=False,indent=2),'utf-8');print(json.dumps(delivery,ensure_ascii=False,indent=2))
