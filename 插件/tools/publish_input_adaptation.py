"""Publish only the reviewed 0.1.2 candidate and actual separately generated floor DWGs."""
from pathlib import Path
import hashlib,json,shutil,zipfile,sys
from verify_dynamic import ROOT,read,need,verify_run
APP=ROOT/'插件'
NATIVE_VERSION='v118'
EXTRA_VERSION='v117'
sys.path.insert(0,str(ROOT/'阶段2'))
from release_host_checks import validate_host_records
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def validate_runtime_resources(binding,bundle,payload):
 runtime=bundle/'Contents/Win64/resources';actual={str(f.relative_to(runtime)):sha(f) for f in runtime.rglob('*') if f.is_file()};need(actual==binding['resources'],'bundle differs from actual tested runtime resources')
 manifest=read(bundle/'SHA256.json')
 for name,digest in manifest.items():need(sha(bundle/name)==digest,'bundle resource changed '+name)
 payloadRows={row['relative']:row['SHA256'] for row in payload};bundleRows={str(f.relative_to(bundle)):sha(f) for f in bundle.rglob('*') if f.is_file()};need(len(payloadRows)==len(payload) and payloadRows==bundleRows,'bundle and actual installer payload differ')

def validate_host_evidence(binding,closure):
 session=Path(binding['session']);ready=read(session/'quiet-worker-ready.json');validate_host_records(binding,dict(session=str(session),nonce=ready['nonce'],processId=ready['processId']),closure,read(session/'worker-config.json'),read(session/'isolation-host-receipt.json'),read(closure['finalPath']))

def validate():
 review=read(APP/'evidence/final-review-input-adaptation-20261101.json');need(review['passed'] and review['fullyResolved'],'final review unresolved')
 for name,digest in review['releaseFiles'].items():need(sha(ROOT/name)==digest,'reviewed file changed '+name)
 qa=read(APP/f'evidence/qa-input-adaptation-{NATIVE_VERSION}.json');need(qa['passed'] and qa['visualReviewed'] and qa['summaries']==verify_run(NATIVE_VERSION),'native/visual QA failed')
 extra=read(APP/f'evidence/qa-extra-root-{EXTRA_VERSION}.json');need(extra['passed'] and extra['actualLineRetained'] and extra['summary']['modelRoots']==309,'extra root regression missing')
 b=read(ROOT/f'阶段2/evidence/native-host-binding-{NATIVE_VERSION}.json');need(sha(APP/'bin/BuildingPlanPlugin2020.arx')==b['moduleSHA256']==qa['moduleSHA256']==extra['moduleSHA256'],'candidate ARX differs')
 for name,digest in b['sourceFiles'].items():need(sha(ROOT/'阶段2/src'/name)==digest,'native engine source changed '+name)
 for name,digest in b['productSources'].items():need(sha(APP/name)==digest,'native product source changed '+name)
 for v in [EXTRA_VERSION,NATIVE_VERSION]:
  closure=read(ROOT/f'阶段2/evidence/host-closure-{v}.json');validate_host_evidence(read(ROOT/f'阶段2/evidence/native-host-binding-{v}.json'),closure);final=read(closure['finalPath']);unload=read(APP/f'evidence/unload-product-{v}.json');need(unload['unloaded'] and unload['commandsRemoved'] and unload['moduleSHA256']==qa['moduleSHA256'],'candidate not normally unloaded')
  need(not closure['process']['ownProcessAlive'] and closure['stagingRemoved'] and len(closure['files'])==4 and all(x['removed'] for x in closure['files']),'owned host/staging still active')
  need(len(closure['inputs'])==6 and all(x['snapshotUnchanged'] for x in closure['inputs']) and all(x['LOADCTRLS']==2 and x['action']=='read only' for x in closure['sharedRegistrations']),'source/common registry changed')
  need(final['workerUserDesktopShowEvents']==final['workerForegroundEvents']==final['inputDesktopReadErrors']==0 and not final['monitorError'] and not final['supervisorErrors'],'owned window isolation incomplete')
 bundle=APP/'build/input-adaptation-reviewed-candidate/BuildingPlan2020.bundle';manifest=read(bundle/'SHA256.json')
 for name,digest in manifest.items():need(sha(bundle/name)==digest,'bundle resource changed '+name)
 need(sha(bundle/'Contents/Win64/BuildingPlanPlugin2020.arx')==qa['moduleSHA256'],'packaged ARX differs')
 installer=read(APP/'evidence/installer-input-adaptation-reviewed-qa.json');validate_runtime_resources(b,bundle,installer['payload']);need(installer['passed'] and installer['tests']==6 and installer['EXESHA256']==sha(APP/'installer/bin/BuildingPlanSetup.exe'),'installer not verified')
 return qa,b,bundle

def main():
 qa,b,bundle=validate();destination=APP/'交付/建筑施工图助手_0.1.2_20261101';archive=Path(str(destination)+'.zip');setup=APP/'交付/建筑施工图助手_0.1.2_安装程序.exe';need(not destination.exists() and not archive.exists() and not setup.exists(),'release destination already exists');destination.mkdir();shutil.copytree(bundle,destination/'BuildingPlan2020.bundle');shutil.copyfile(APP/'installer/bin/BuildingPlanSetup.exe',destination/'安装程序.exe');shutil.copyfile(APP/'installer/bin/BuildingPlanSetup.exe',setup)
 for name in ['使用说明.md','验证报告.md','构建说明.md']:shutil.copyfile(APP/name,destination/name)
 examples=destination/'示例';examples.mkdir()
 for row in qa['summaries']:
  floor=row['floor'];shutil.copyfile(row['DWG'],examples/f'方案施工图_{"一层" if floor==1 else "二层"}.dwg');r=read(Path(b['job'])/f'floor-{floor}/product-result.json');shutil.copyfile(Path(r['records'])/'待确认清单.csv',examples/f'待确认清单_{floor}层.csv')
 sources=destination/'源码'
 for rel in ['插件/src','阶段2/src','插件/tests','插件/installer/src']:
  dst=sources/rel;dst.mkdir(parents=True)
  for f in (ROOT/rel).iterdir():
   if f.is_file() and f.suffix in ['.h','.cpp','.inc','.vcxproj','.py']:shutil.copyfile(f,dst/f.name)
 for rel in ['插件/build_package.py','插件/PackageContents.xml','插件/installer/build_payload.py','插件/installer/previous-version-0.1.1.json','插件/inputs/entry-failure-20261101/confirmed-input.tsv']:
  dst=sources/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/rel,dst)
 manifest={str(f.relative_to(destination)):sha(f) for f in destination.rglob('*') if f.is_file()};(destination/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
 with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
  for f in destination.rglob('*'):
   if f.is_file():z.write(f,f.relative_to(destination))
 with zipfile.ZipFile(archive) as z:need(z.testzip() is None and all(not x.lower().endswith('.pdf') for x in z.namelist()),'archive damaged or contains QA PDF')
 receipt=dict(contract='bcp.input-adaptation-release.v1',version='0.1.2',directory=str(destination),ZIP=str(archive),ZIPSHA256=sha(archive),installer=str(setup),installerSHA256=sha(setup),moduleSHA256=qa['moduleSHA256'],drawings=qa['summaries'],constructionDrawingComplete=False)
 (APP/'evidence/release-input-adaptation-20261101.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(receipt,ensure_ascii=False))
if __name__=='__main__':main()
