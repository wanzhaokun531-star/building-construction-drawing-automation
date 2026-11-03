"""Package the same native candidate and resources validated in v195."""
from pathlib import Path
import json,hashlib,shutil,zipfile,datetime
R=Path(__file__).resolve().parents[2];A=R/'插件';V='v195'
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
b=read(R/f'阶段2/evidence/native-host-binding-{V}.json');q=read(A/f'evidence/rules-final-qa-{V}.json');review=read(A/'evidence/rules014-final-review.json');session=Path(b['session']);quit=read(session/'normal-quit-verification.json');closed=read(R/f'阶段2/项目/百塘村邹总_20261103/closed-hosts-{V}-{V}.json');unload=read(A/f'evidence/unload-product-{V}.json')
assert q['passed'] and review['allActionableFindingsAddressed'] and unload['unloaded'] and quit['completed'] and quit['exitCode']==0 and not quit['forcedTermination']
assert closed[0]['ownShowEvents']==closed[0]['ownForegroundEvents']==0
for p,h in b['productSources'].items():assert sha(A/p)==h,p
for p,h in b['sourceFiles'].items():assert sha(R/'阶段2/src'/p)==h,p
bundle=A/'build/rules-014-candidate/BuildingPlan2020.bundle';exe=A/'build/rules-014-installer/BuildingPlanSetup.exe';resources=bundle/'Contents/Win64/resources'
assert sha(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx')==b['moduleSHA256']
assert {str(p.relative_to(resources)):sha(p) for p in resources.rglob('*') if p.is_file()}==b['resources']
assert all(sha(bundle/p)==h for p,h in read(bundle/'SHA256.json').items())
for row in read(A/'installer/build/payload.json'):assert sha(bundle/row['relative'])==row['SHA256']
for log in ['installer014-from013.log','installer014-from012.log']:assert (A/'evidence'/log).read_text('utf-8-sig').strip().endswith('OK')
out=A/'交付/建筑施工图助手_0.1.4_20261103';assert not out.exists();out.mkdir();shutil.copytree(bundle,out/bundle.name);shutil.copyfile(exe,out/'安装程序.exe')
for name in ['使用说明','验证报告']:shutil.copyfile(A/f'docs/0.1.4{name}.md',out/f'{name}.md')
shutil.copyfile(A/'构建说明.md',out/'构建说明.md');examples=out/'示例';examples.mkdir();drawings=[]
cases=[('百塘村邹总_三层平面_插件0.1.4.dwg',q['three']),*[(f'方案施工图_{"一层" if n==1 else "二层"}_插件0.1.4.dwg',x) for n,x in enumerate(q['two'],1)],('原方案_基础标注回归_插件0.1.4.dwg',q['legacy'])]
for name,row in cases:
 dst=examples/name;shutil.copyfile(row['DWG'],dst);assert sha(dst)==row['SHA256'];drawings.append(dict(path=str(dst),SHA256=sha(dst)))
source=out/'源码'
for rel in ['插件/src','阶段2/src','插件/installer/src']:
 shutil.copytree(R/rel,source/rel)
for rel in ['插件/installer/build_payload.py',*['插件/installer/previous-version-'+v+'.json' for v in ['0.1.1','0.1.2','0.1.3']],'插件/PackageContents.xml']:
 dst=source/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/rel,dst)
shutil.copytree(resources,source/'插件/resources/runtime');shutil.copyfile(out/'使用说明.md',source/'插件/使用说明.md')
old=A/'交付/建筑施工图助手_0.1.3_20261102/源码';shutil.copyfile(old/'构建.ps1',source/'构建.ps1')
(source/'说明.md').write_text('保持插件/src与阶段2/src目录关系，运行构建.ps1生成ARX及bundle。需要VS2019 v141、Windows SDK10.0.19041、ObjectARX2020。包内已编译ARX是实际验证版本，重新构建后需重新验证。运行资源已包含，不需要Python。安装器重建需为新bundle生成SHA256.json，以build_payload.py <bundle> <previous-version-0.1.1.json> <previous-version-0.1.2.json> <previous-version-0.1.3.json>生成载荷，再构建installer/src/BuildingPlanSetup.vcxproj。开发打包工具使用Python，正式运行不使用。\n','utf-8')
tests=source/'插件/tests';tests.mkdir();
for name in ['ThreeFloorContractTests.cpp','ThreeFloorContractTests.vcxproj','ApprovedAreaTests.cpp','ApprovedLayoutContractTests.cpp']:shutil.copyfile(A/'tests'/name,tests/name)
records=out/'验证记录';records.mkdir()
for name in [f'native-rules-{V}.json',f'native-rules-{V}-three-floor.json',f'native-rules-{V}-legacy.json',f'rules-final-qa-{V}.json','rules014-final-review.json','installer014-from013.log','installer014-from012.log']:
 shutil.copyfile(A/'evidence'/name,records/name)
shutil.copyfile(session/'normal-quit-verification.json',records/'normal-quit-verification.json')
files={str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()};(out/'SHA256.json').write_text(json.dumps(files,ensure_ascii=False,indent=2),'utf-8')
zip_path=out.parent/(out.name+'.zip');assert not zip_path.exists()
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in out.rglob('*'):
  if p.is_file():z.write(p,p.relative_to(out))
with zipfile.ZipFile(zip_path) as z:
 assert set(z.namelist())=={p.relative_to(out).as_posix() for p in out.rglob('*') if p.is_file()}
 for p,h in files.items():assert hashlib.sha256(z.read(Path(p).as_posix())).hexdigest().upper()==h
standalone=out.parent/'建筑施工图助手_0.1.4_安装程序.exe';assert not standalone.exists();shutil.copyfile(exe,standalone)
revision=A/'revisions/pre-rules-014-20261103';revision.mkdir();installed=A/'bin/BuildingPlanPlugin2020.arx';shutil.copyfile(installed,revision/installed.name);shutil.copyfile(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx',installed);assert sha(installed)==b['moduleSHA256']
delivery=dict(version='0.1.4',recorded=datetime.datetime.now().astimezone().isoformat(),nativeVersion=V,releaseDirectory=str(out),ZIP=str(zip_path),ZIP_SHA256=sha(zip_path),installer=str(standalone),installerSHA256=sha(standalone),ARXSHA256=b['moduleSHA256'],DWGs=drawings,installerTestsPassed=11,reviewFindingsFixed=1,normalQUIT=True,actualAppDataInstalled=False,fullConstructionAutomationComplete=False,GitHubPushed=False)
(A/'evidence/release-rules-014-20261103.json').write_text(json.dumps(delivery,ensure_ascii=False,indent=2),'utf-8');print(json.dumps(delivery,ensure_ascii=False,indent=2))
