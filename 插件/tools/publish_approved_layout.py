"""Publish the actual validated native candidate; never updates an installed CAD bundle."""
from pathlib import Path
import hashlib,json,shutil,zipfile,datetime
ROOT=Path(__file__).resolve().parents[2];APP=ROOT/'插件';VERSION='v170'
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
b=read(ROOT/f'阶段2/evidence/native-host-binding-{VERSION}.json');q=read(APP/f'evidence/approved-saved-qa-{VERSION}.json');visual=read(APP/f'evidence/approved-visual-{VERSION}.json');edit=read(APP/f'evidence/approved-edit-qa-{VERSION}.json');legacy=read(APP/f'evidence/approved-legacy-regression-{VERSION}.json');installer=read(APP/f'evidence/approved-installer-qa-{VERSION}.json');review=read(APP/f'evidence/approved-final-review-{VERSION}.json');closure=read(ROOT/f'阶段2/evidence/host-closure-{VERSION}.json');normal=read(Path(b['session'])/'normal-quit-verification.json');unload=read(APP/f'evidence/unload-product-{VERSION}.json')
assert q['passed'] and visual['reviewed'] and visual['samePixelsAsReviewedV169'] and len(edit)==2 and all(x['proof']['allTrialsRestored'] for x in edit)
assert legacy['error'] is None and installer['passed'] and review['outcome']=='No findings';assert normal['completed'] and normal['exitCode']==0 and not normal['forcedTermination'];assert unload['unloaded'] and unload['commandsRemoved'] and closure['stagingRemoved'] and not closure['process']['ownProcessAlive'];assert all(x['snapshotUnchanged'] for x in closure['inputs']) and all(x['LOADCTRLS']==2 for x in closure['sharedRegistrations'])
assert all(sha(APP/p)==h for p,h in b['productSources'].items()) and all(sha(ROOT/'阶段2/src'/p)==h for p,h in b['sourceFiles'].items());assert sha('D:/方案施工图.dwg')==b['inputSHA256']
bundle=APP/'build/approved-013-candidate/BuildingPlan2020.bundle';exe=APP/'build/approved-013-installer/BuildingPlanSetup.exe';assert sha(exe)==installer['installerSHA256'];assert sha(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx')==installer['ARXSHA256']==b['moduleSHA256'];resources=bundle/'Contents/Win64/resources';assert {str(p.relative_to(resources)):sha(p) for p in resources.rglob('*') if p.is_file()}==b['resources'];manifest=read(bundle/'SHA256.json');assert all(sha(bundle/p)==h for p,h in manifest.items())
out=APP/'交付/建筑施工图助手_0.1.3_20261102';assert not out.exists();out.mkdir();shutil.copytree(bundle,out/bundle.name);shutil.copyfile(exe,out/'安装程序.exe');shutil.copyfile(APP/'docs/0.1.3使用说明.md',out/'使用说明.md');shutil.copyfile(APP/'docs/0.1.3验证报告.md',out/'验证报告.md');shutil.copyfile(APP/'构建说明.md',out/'构建说明.md');examples=out/'示例';examples.mkdir();result=read(Path(b['job'])/'product-result.json')
for n,f in enumerate(result['outputs'],1):
 dst=examples/f'方案施工图_{"一层" if n==1 else "二层"}_0.1.3.dwg';shutil.copyfile(f['path'],dst);assert sha(dst)==f['SHA256'];record=read(Path(b['job'])/f'floor-{n}/product-result.json');shutil.copyfile(Path(record['records'])/'待确认清单.csv',examples/f'待确认清单_{n}层.csv')
source=out/'源码'
for rel in ['插件/src','阶段2/src','插件/installer/src']:
 shutil.copytree(ROOT/rel,source/rel)
for rel in ['插件/installer/build_payload.py','插件/installer/previous-version-0.1.2.json','插件/PackageContents.xml']:
 dst=source/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/rel,dst)
shutil.copytree(resources,source/'插件/resources/runtime');shutil.copyfile(out/'使用说明.md',source/'插件/使用说明.md')
(source/'构建.ps1').write_text('''$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$taskMSBuild='C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe'
$taskBuild=Join-Path $taskRoot ('构建产物_'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$taskBin=Join-Path $taskBuild 'bin'
$taskObj=Join-Path $taskBuild 'obj'
& $taskMSBuild (Join-Path $taskRoot '插件/src/BuildingPlanPlugin2020.vcxproj') /p:Configuration=Release /p:Platform=x64 "/p:OutDir=$taskBin\\" "/p:IntDir=$taskObj\\" /m /nologo
if($LASTEXITCODE -ne 0){throw 'ARX build failed'}
$taskBundle=Join-Path $taskBuild 'BuildingPlan2020.bundle'
$taskContent=Join-Path $taskBundle 'Contents/Win64'
New-Item -ItemType Directory -Path $taskContent -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $taskBin 'BuildingPlanPlugin2020.arx') -Destination $taskContent
Copy-Item -LiteralPath (Join-Path $taskRoot '插件/PackageContents.xml') -Destination $taskBundle
Copy-Item -LiteralPath (Join-Path $taskRoot '插件/使用说明.md') -Destination $taskBundle
Copy-Item -LiteralPath (Join-Path $taskRoot '插件/resources/runtime') -Destination (Join-Path $taskContent 'resources') -Recurse
[System.IO.File]::WriteAllText((Join-Path $taskContent 'BuildingPlanPlugin2020.ini'),"[product]`r`ntangentDirectory=C:\\Tangent\\TArchT20V8\\SYS23x64`r`n",[System.Text.Encoding]::Unicode)
Write-Host ('ARX and bundle: '+$taskBundle)
''',encoding='utf-8-sig')
(source/'说明.md').write_text('保持插件/src与阶段2/src的目录关系，运行构建.ps1。需要ObjectARX2020、VS2019 v141和Windows SDK10.0.19041；SDK路径可通过ArxSdkRoot覆盖。脚本只生成ARX/bundle，不安装、不启动CAD；重建后须重新验证。运行资源从本次实测bundle复制，未分发Autodesk SDK、天正运行组件和字体。安装器源码也提供，先为新bundle生成SHA256.json，再以build_payload.py <bundle> <previous-version-0.1.2.json>生成载荷并构建其vcxproj。源码包的构建脚本为交付辅助，正式验证对象是包内已实测ARX及安装程序。\n',encoding='utf-8')
records=out/'验证记录';records.mkdir()
for name in ['native-approved-v170.json','approved-saved-qa-v170.json','approved-opening-fields-v170.json','approved-edit-qa-v170.json','approved-visual-v170.json','approved-legacy-regression-v170.json','approved-final-review-v170.json','approved-installer-qa-v170.json','approved-area-wall-link-check.json']:
 shutil.copyfile(APP/'evidence'/name,records/name)
shutil.copyfile(Path(b['session'])/'normal-quit-verification.json',records/'normal-quit-verification.json')
files={str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()};(out/'SHA256.json').write_text(json.dumps(files,ensure_ascii=False,indent=2),encoding='utf-8');zip_path=out.parent/(out.name+'.zip');assert not zip_path.exists()
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in out.rglob('*'):
  if p.is_file():z.write(p,p.relative_to(out))
with zipfile.ZipFile(zip_path) as z:
 assert set(z.namelist())=={p.relative_to(out).as_posix() for p in out.rglob('*') if p.is_file()}
 for rel,digest in files.items():assert hashlib.sha256(z.read(Path(rel).as_posix())).hexdigest().upper()==digest
standalone=APP/'交付/建筑施工图助手_0.1.3_安装程序.exe';assert not standalone.exists();shutil.copyfile(exe,standalone);assert sha(standalone)==sha(exe)
delivery=dict(version='0.1.3',recorded=datetime.datetime.now().astimezone().isoformat(),nativeVersion=VERSION,releaseDirectory=str(out),ZIP=str(zip_path),ZIP_SHA256=sha(zip_path),installer=str(standalone),installerSHA256=sha(standalone),ARXSHA256=b['moduleSHA256'],DWGs=[dict(path=str(examples/f'方案施工图_{"一层" if n==1 else "二层"}_0.1.3.dwg'),SHA256=f['SHA256']) for n,f in enumerate(result['outputs'],1)],finalReview='No findings',normalQUIT=True,actualAppDataInstalled=False,existing012Preserved=True,fullConstructionAutomationComplete=False)
(APP/'evidence/release-approved-013-20261102.json').write_text(json.dumps(delivery,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(delivery,ensure_ascii=False))
