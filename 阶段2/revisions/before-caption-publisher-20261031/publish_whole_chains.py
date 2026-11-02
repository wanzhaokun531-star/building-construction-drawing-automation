"""The current dimension-chain DWG publisher. Requires native, visual and closure gates."""
from pathlib import Path
import json,shutil,datetime
from chain_contract import S,read,sha,build_contract,load_v67
from verify_whole_chains import verify,check
VERSION='v74'
P=S.parent/'阶段1/原生接口探测'
REQUIRED_REVIEW_FILES={
    'src/NativeWholeDimensionChains.inc','src/Stage2Arx.cpp','chain_contract.py',
    'verify_whole_chains.py','publish_whole_chains.py','prepare_whole_chain_host.py',
    'run_whole_chain_job.py','preview_whole_chains.py','close_host.py',
    'test_chain_contract.py','test_whole_chain_gate.py','test_whole_chain_publisher.py',
    '../阶段1/原生接口探测/src/NativeStage2Bridge.inc',
    '../阶段1/原生接口探测/启动隔离桌面宿主.ps1',
    '../阶段1/原生接口探测/IsolatedCadHostNative.cs',
}
def validate_review_files(review):
    files=review['reviewedFiles']
    check(REQUIRED_REVIEW_FILES.issubset(files),'mandatory review coverage missing')
    for name,wanted in files.items():check(sha(S/name)==wanted,'reviewed implementation/gate changed: '+name)
def validate_host_records(binding,run,closure,config,launch,actual_final,hash_provider=sha):
    session=Path(binding['session']).resolve();final=closure['final'];path=Path(closure['finalPath'])
    check(path.parent.resolve()==session and path.name in ('isolation-final.json','isolation-host-final.json') and path.is_file(),'actual final evidence path missing/outside session')
    check(actual_final==read(path)==final,'actual final host evidence differs from closure')
    check(launch['nonce']==config['nonce']==binding['nonce']==run['nonce']==final['nonce'],'host nonce differs')
    check(launch['session']==config['session']==binding['session']==run['session']==final['session'],'host session differs')
    check(launch['processId']==run['processId']==final['processId'],'actual launched process differs')
    check(final['workerExited'] and final['readyIdentityConfirmed'] and final['monitorStartedBeforeWorker'] and final['monitorStarted'] and final['monitorStopped'],'actual worker lifecycle monitoring incomplete')
    check(final['inputDesktopReadErrors']==0 and not final['inputDesktopChanged'],'input desktop monitoring incomplete')
    check(final['workerForegroundEvents']==final['workerUserDesktopShowEvents']==final['workerForegroundSamples']==final['droppedEvents']==0,'own user desktop events observed')
    check(not final['monitorError'] and not final['supervisorErrors'] and not final['postExitWindowDiagnostics'] and not final['timedOut'],'actual monitoring error')
    check(not launch['metadataErrors'] and launch['monitorStartedBeforeWorker'] and not launch['switchDesktopCalled'] and not launch['userCadTouched'],'actual launch evidence incomplete')
    check(launch['bootSHA256']==binding['bootSHA256']==hash_provider(session/'boot.scr'),'actual boot script differs')
    for key,cfg_key,name in (('controllerSHA256','launcherSHA256','启动隔离桌面宿主.ps1'),('nativeHelperSHA256','helperSHA256','IsolatedCadHostNative.cs')):
        check(launch[key]==config[cfg_key]==hash_provider(P/name)==hash_provider(session/'candidate'/name),'monitor controller/helper binding differs: '+name)
    check(config['binarySHA256']==binding['bridgeARXSHA256']==hash_provider(P/'bin/NativeAccessProbe.arx')==hash_provider(session/'candidate/NativeAccessProbe.arx'),'Stage1 bridge binary differs')
    for name,wanted in config['sourceFiles'].items():
        check(hash_provider(P/'src'/name)==wanted==hash_provider(session/'candidate'/name),'Stage1 bridge source differs: '+name)
    check(hash_provider(session/'phase2-candidate'/Path(binding['module']).name)==binding['moduleSHA256'],'Stage2 session candidate binary differs')
    for name,wanted in binding['sourceFiles'].items():check(hash_provider(session/'phase2-candidate'/name)==wanted,'Stage2 session candidate source differs: '+name)
def main():
    b=read(S/f'evidence/native-host-binding-{VERSION}.json');run=read(S/f'evidence/native-whole-chain-run-{VERSION}.json');pr=read(S/f'evidence/whole-chain-preview-{VERSION}.json');closure=read(S/f'evidence/host-closure-{VERSION}.json');qa=read(S/f'evidence/whole-chain-release-{VERSION}.json');review=read(S/f'evidence/whole-chain-independent-review-{VERSION}.json')
    check(run['error'] is None and pr['error'] is None,'native run or preview failed')
    check(b['nonce']==run['nonce']==pr['nonce']==closure['final']['nonce']==qa['nonce'],'session nonce mismatch')
    check(run['processId']==pr['processId']==closure['final']['processId'],'process identity mismatch')
    check(b['session']==run['session']==pr['session']==closure['session'],'session path mismatch')
    check(closure['process']['ownProcessAlive'] is False and closure['stagingRemoved'],'owned host not cleaned')
    check(len(closure['files'])==4 and all(x['removed'] for x in closure['files']),'staged modules not cleaned')
    check(len(closure['inputs'])==6 and all(x['snapshotUnchanged'] for x in closure['inputs']),'frozen input changed')
    f=closure['final'];check(f['workerForegroundEvents']==f['workerUserDesktopShowEvents']==f['workerForegroundSamples']==f['droppedEvents']==0,'owned host disturbed user desktop')
    check(not f['monitorError'] and not f['supervisorErrors'] and not f['postExitWindowDiagnostics'] and not f['timedOut'],'window monitoring failed')
    check(all(x['LOADCTRLS']==2 and x['action']=='read only' for x in closure['sharedRegistrations']),'shared registration changed')
    check(review['passed'] and qa['visualReviewed'] and qa['protectedIndependentComparisonPassed'],'final review not passed')
    session=Path(b['session']);config=read(session/'worker-config.json');launch=read(session/'isolation-host-receipt.json')
    validate_host_records(b,run,closure,config,launch,read(Path(closure['finalPath'])))
    validate_review_files(review)
    for name,wanted in b['sourceFiles'].items():check(sha(S/'src'/name)==wanted,'candidate source changed: '+name)
    check(sha(S/'bin/BuildingPlan2020.arx')==b['moduleSHA256'],'candidate binary changed')
    job=Path(run['job']);source=job/'方案施工图_整体连续天正尺寸.dwg';report=job/'whole-dimension-chains.json';preview=Path(pr['job'])/'stage2-preview-native.json'
    check(sha(source)==run['DWGSHA256']==qa['DWGSHA256']==review['DWGSHA256'],'DWG fingerprint mismatch')
    check(sha(report)==run['reportSHA256']==qa['reportSHA256'],'native report fingerprint mismatch')
    check(sha(preview)==pr['nativeReportSHA256']==qa['previewReportSHA256'],'preview fingerprint mismatch')
    audit=read(Path(pr['job'])/'native-mask-document-audit.json');check(audit['error'] is None and audit['masks']==4 and audit['frame70']==audit['WIPEOUTFRAME']==audit['publicVisibleFrames']==0,'saved document frame changed')
    result=verify(read(report),build_contract(*load_v67()),read(preview))
    destination=S/'交付/整体连续天正尺寸_20261031';check(not destination.exists(),'delivery already exists');destination.mkdir();target=destination/'方案施工图_整体连续天正尺寸.dwg';shutil.copyfile(source,target);check(sha(target)==run['DWGSHA256'],'delivery copy differs')
    receipt=dict(contract='bcp.whole-chain-delivery.v1',recorded=datetime.datetime.now().astimezone().isoformat(),version=VERSION,file=str(target),SHA256=sha(target),verification=result,reviewSHA256=sha(S/f'evidence/whole-chain-independent-review-{VERSION}.json'),closureSHA256=sha(S/f'evidence/host-closure-{VERSION}.json'),publisherSHA256=sha(Path(__file__)))
    (S/f'evidence/whole-chain-delivery-{VERSION}.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(receipt,ensure_ascii=False))
if __name__=='__main__':main()
