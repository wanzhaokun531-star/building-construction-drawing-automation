"""Release only the pinned, reviewed and independently checked native candidate."""
from pathlib import Path
import sys,json,shutil,datetime,importlib.util
from verify_native import ROOT,APP,S,read,sha,need,verify_geometry,verify_axes
sys.path.insert(0,str(S));from release_host_checks import validate_host_records
VERSION='v92'
REQUIRED={'插件/src/BuildingPlanPlugin2020.cpp','插件/src/BuildingPlanPlugin2020.vcxproj','插件/src/PluginPolicy.cpp','插件/src/PluginPolicy.h','插件/src/ProductRuntime.inc','插件/src/ProductAudit.inc','插件/tools/verify_native.py','插件/tools/publish.py','插件/tools/prepare_native.py','插件/tools/run_native.py','插件/tools/preview_native.py','插件/build_package.py','插件/PackageContents.xml','插件/使用说明.md','插件/tests/test_native_gate.py','插件/tests/test_package.py','插件/tests/PolicyTests.cpp','阶段2/src/Stage2Arx.cpp','阶段2/src/NativeWholeDimensionChains.inc','阶段2/src/NativeDoorCaptionClearance.inc','阶段1/原生接口探测/src/NativeStage2Bridge.inc','阶段2/release_host_checks.py','阶段2/close_host.py','插件/tools/unload_native.py','插件/验证报告.md','插件/构建说明.md','插件/tests/PolicyTests.vcxproj'}
def main():
 run=read(APP/f'evidence/native-product-{VERSION}.json');result=run['result'];b=run['binding'];preview=read(APP/f'evidence/preview-product-{VERSION}.json');qa=read(APP/f'evidence/qa-product-{VERSION}.json');review=read(APP/'evidence/final-review.json');closure=read(S/f'evidence/host-closure-{VERSION}.json');session=Path(b['session']);ready=read(session/'quiet-worker-ready.json');native=read(Path(b['job'])/'product-result.json')
 need(review['passed'] and review['releaseVersion']==VERSION and review['visualReviewed'] and review['fullyResolved'],'review or actual visual check incomplete')
 need(REQUIRED.issubset(review['releaseFiles']),'mandatory review/release file coverage absent')
 for name,digest in review['releaseFiles'].items():need(sha(ROOT/name)==digest,'implementation or release gate changed '+name)
 need(result==native and result['output']==b['output']==preview['source']==qa['DWG'] and Path(result['output']).resolve()==(Path(b['job'])/'插件生成施工图.dwg').resolve(),'native output path differs')
 recovery=read(Path(b['job'])/'product-recovery-test.json');need(run.get('recovery')==recovery and recovery['realDiagnosticWriteFailure'] and recovery['jobCleared'] and not recovery['retainedJobLock'],'reviewed failure recovery was not verified before the successful same-host build')
 need(sha(result['output'])==result['outputSHA256']==preview['sourceSHA256']==qa['DWGSHA256']==review['DWGSHA256']==sha(Path(preview['job'])/'control.dwg'),'native DWG/preview binding differs')
 need(sha(APP/f'evidence/native-product-{VERSION}.json')==qa['integrationSHA256'] and sha(APP/f'evidence/preview-product-{VERSION}.json')==qa['previewSHA256'],'run evidence changed')
 need(sha(Path(preview['job'])/'stage2-preview-native.json')==preview['nativeReportSHA256'],'native preview changed')
 need(sha(APP/'bin/BuildingPlanPlugin2020.arx')==b['moduleSHA256']==qa['moduleSHA256'],'candidate ARX changed')
 for name,digest in b['productSources'].items():need(sha(APP/name)==digest,'product candidate source changed '+name)
 for name,digest in b['sourceFiles'].items():need(sha(S/'src'/name)==digest,'engine candidate source changed '+name)
 need(ready['nonce']==b['nonce']==preview['nonce'] and ready['processId']==preview['processId'],'native worker differs')
 validate_host_records(b,dict(session=run['session'],nonce=ready['nonce'],processId=ready['processId']),closure,read(session/'worker-config.json'),read(session/'isolation-host-receipt.json'),read(closure['finalPath']))
 need(closure['process']['ownProcessAlive'] is False and closure['stagingRemoved'] and len(closure['files'])==4 and all(x['removed'] for x in closure['files']),'owned staged modules remain')
 need(len(closure['inputs'])==6 and all(x['snapshotUnchanged'] for x in closure['inputs']) and all(x['LOADCTRLS']==2 and x['action']=='read only' for x in closure['sharedRegistrations']),'frozen inputs/common registration changed')
 unloaded=read(APP/f'evidence/unload-product-{VERSION}.json');need(unloaded['unloaded'] and unloaded['commandsRemoved'] and unloaded['error'] is None,'product did not unload normally')
 need(unloaded['moduleSHA256']==b['moduleSHA256'] and unloaded['nonce']==b['nonce'] and unloaded['processId']==ready['processId'] and sha(Path(unloaded['job'])/'native-product-unload.json')==unloaded['reportSHA256'],'unload belongs to another candidate or host')
 record=Path(result['records']);a=read(record/'过程/foundation/annotations.json');w=read(record/'过程/join/whole-dimension-chains.json');c=read(record/'过程/caption/door-caption-clearance.json');p=read(Path(preview['job'])/'stage2-preview-native.json');verify_geometry(a,w,c,p);verify_axes(a,read(Path(preview['job'])/'product-axis-document-audit.json'))
 for name,digest in qa['nativeReports'].items():need(sha(record/name)==digest,'native report changed '+name)
 need(sha(result['source'])==result['sourceSHA256']==b['inputSHA256']==sha(b['input']),'input changed')
 target=APP/'交付/建筑施工图助手_0.1.0_20261031';need(not target.exists(),'never overwrite an earlier release');target.mkdir(parents=True)
 spec=importlib.util.spec_from_file_location('build_native_package',APP/'build_package.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);bundle=target/'BuildingPlan2020.bundle';m.main(bundle)
 need(sha(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx')==b['moduleSHA256'],'packaged ARX differs from tested ARX')
 for name,digest in b['resources'].items():need(sha(bundle/'Contents/Win64/resources'/name)==digest,'packaged resource differs from tested data '+name)
 example=target/'示例';example.mkdir();shutil.copyfile(result['output'],example/'方案施工图_插件生成.dwg');shutil.copyfile(b['input'],example/'定稿方案_只读输入.dwg');shutil.copyfile(record/'待确认清单.csv',example/'待确认清单.csv')
 for name in ('使用说明.md','验证报告.md'):shutil.copyfile(APP/name,target/name)
 source=target/'源码';(source/'插件/src').mkdir(parents=True);(source/'阶段2/src').mkdir(parents=True)
 for folder,destination in ((APP/'src',source/'插件/src'),(S/'src',source/'阶段2/src')):
  for f in folder.iterdir():
   if f.is_file():shutil.copyfile(f,destination/f.name)
 shutil.copyfile(APP/'构建说明.md',source/'构建说明.md')
 manifest={str(f.relative_to(target)):sha(f) for f in target.rglob('*') if f.is_file()};(target/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');archive=shutil.make_archive(str(target),'zip',root_dir=target.parent,base_dir=target.name)
 receipt=dict(contract='bcp.product-release.v1',recorded=datetime.datetime.now().astimezone().isoformat(),version=VERSION,directory=str(target),archive=archive,archiveSHA256=sha(archive),moduleSHA256=b['moduleSHA256'],exampleDWGSHA256=sha(example/'方案施工图_插件生成.dwg'),reviewSHA256=sha(APP/'evidence/final-review.json'),closureSHA256=sha(S/f'evidence/host-closure-{VERSION}.json'),qaSHA256=sha(APP/f'evidence/qa-product-{VERSION}.json'),PDFDelivered=False,constructionDrawingComplete=False)
 out=APP/'evidence/release.json';need(not out.exists(),'release receipt exists');out.write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(receipt,ensure_ascii=False))
if __name__=='__main__':main()
