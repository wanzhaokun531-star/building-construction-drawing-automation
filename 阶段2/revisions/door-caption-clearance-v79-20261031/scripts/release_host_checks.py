"""Shared read-only host evidence guard, preserved from the reviewed v74 publisher."""
from pathlib import Path
from chain_contract import S,read,sha
from verify_whole_chains import check
P=S.parent/'阶段1/原生接口探测'
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
