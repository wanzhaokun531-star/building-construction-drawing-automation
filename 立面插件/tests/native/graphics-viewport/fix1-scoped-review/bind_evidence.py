"""Read already completed native runs and freeze scoped-review evidence; never launch CAD."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
ARCHIVE = Path('D:/Codex/项目清理归档/建筑施工图自动化_20261102-181100')
def read(p): return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def save(name, data): (OUT/name).write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
files = set()
def bind(p):
    p = Path(p).resolve()
    assert p.is_file(), p
    files.add(p)

builds = [ARCHIVE/'立面插件/build/n7-20261101-221056-470-owned-file-idle-frozen-b293e40a',
          ROOT/'立面插件/build/n7-20261102-184823-160-replay-bound-ownership-red-15018a85',
          ROOT/'立面插件/build/n7-20261102-190707-579-fix1-persistent-ownership-green-fd78fb67']
for build in builds:
    receipt = read(build/'receipt.json')
    for row in receipt['inputOrigins']:
        path = build/'src'/Path(row['compiledCopy']).name
        assert sha(path)==row['sha256']
        bind(path)
    for name in ['receipt.json','build-command.cmd','build.log','build_graphics_viewport.ps1']:
        bind(build/name)
    binary = build/'out/ElevationViewportCases.arx'
    assert sha(binary)==receipt['binarySHA256']
    bind(binary)
candidate = read(builds[-1]/'receipt.json')
for row in candidate['inputOrigins']:
    assert sha(row['origin'])==row['sha256']
    bind(row['origin'])
for name in ['NativeEvidence.h','NativeEvidence.cpp']:
    assert sha(builds[0]/'src'/name)==sha(builds[-1]/'src'/name)
for row in candidate['officialContracts']:
    assert sha(row['path'])==row['sha256'];bind(row['path'])
bind('C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit/inc/dbmain.h')

observations=[]
for runName, expectedErrors in [('elevation-native-20261102-190426-5bad165f',4),('elevation-native-20261102-191042-5b64a6f1',0)]:
    run = ROOT/'阶段1/runs'/runName
    native=read(run/'case/native-case.json'); closure=read(run/'native-result-closure.json'); retire=read(run/'reader-retirement.json')
    assert len(native['steps'])==78 and len(native['errors'])==expectedErrors
    assert sum(x['status']=='passed' for x in native['steps'])==78-expectedErrors
    for path, digest in [(run/'case/native-case.json',closure['nativeReportSHA256']),
                         (run/'reader-retirement.json',closure['retirementSHA256']),
                         (run/'isolation-final.json',closure['supervisorFinalSHA256']),
                         (closure['binding'],closure['bindingSHA256'])]:
        assert sha(path)==digest;bind(path)
    assert closure['normalQuit'] is False and retire['normalQuit'] is False
    assert closure['forcedTermination'] is True and retire['forcedTermination'] is True
    assert len(retire['removedFiles'])==4 and len(retire['coreInputChecks'])==6
    for row in retire['removedFiles']:
        assert row['removed'] and not Path(row['path']).exists() and sha(row['archive'])==row['SHA256'];bind(row['archive'])
    for row in retire['coreInputChecks']:
        assert sha(row['path'])==row['SHA256'];bind(row['path'])
    trace=[json.loads(s) for s in (run/'case/viewport-command-trace.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    protocol=[x for x in trace if x['event']=='prepareReplay.control']
    assert len(protocol)==7 and all(x['passed'] for x in protocol)
    callbacks=[json.loads(s) for s in (run/'case/viewport-callbacks.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    controls=[x for x in callbacks if x.get('kind')=='persistentDrawableControl']
    assert len(controls)==7 and sum(x['passed'] for x in controls)==7-expectedErrors
    contexts=[x for x in callbacks if x.get('kind')=='hostDrawContext']
    assert len(contexts)==2668 and all(x['contextAddress'] and x['contextDatabaseMatchesTarget'] for x in contexts)
    texts=[x for x in callbacks if x.get('request',{}).get('kind')=='text']
    assert len(texts)==72
    graphics=read(run/'case/graphics-viewport.json')
    assert graphics['glyphComplete'] is False and graphics['capabilityUpgradeAllowed'] is False
    assert all(x['stable'] for x in graphics['records'] if 'sample' in x)
    last=graphics['records'][-1]
    assert not last['sourceEvents'] and not last['ambientEvents'] and not last['targetEvents'] and last['referenceFingerprintUnchanged']
    stopFiles=list(run.glob('isolation-stop-*.json'));assert len(stopFiles)==1
    stop=read(stopFiles[0]);assert stop['exitCode']==70 and stop['forcedTermination']
    observations.append({'run':str(run),'nativeStatus':native['status'],'steps':78,'passed':78-expectedErrors,'errors':native['errors'],
        'protocolControls':protocol,'replayAcceptedEvents':[x for x in trace if x['event']=='handler.prepare.replayAccepted'],
        'ownershipControls':controls,'contextCount':len(contexts),'textRequests':texts,'sampleResults':graphics['records'][:12],
        'unloadEvents':[x for x in trace if x['event']=='module.commandsRemoved' or (x['event']=='module.message' and x.get('message')==2)],
        'normalQuit':False,'explicitStopExitCode':70,'forcedTermination':True,'retirementSHA256':sha(run/'reader-retirement.json'),
        'sixSourceHashesRechecked':True,'fourRetirementArchivesRechecked':True,'inputDesktopReadErrors':retire['inputDesktopReadErrors'],
        'ownedUserShows':retire['ownedUserShows'],'ownedForeground':retire['ownedForeground']})
    for p in run.rglob('*'):
        if p.is_file(): bind(p)
save('observations.json',observations)
bind(ROOT/'立面插件/执行/N7-真实宿主视口采集报告.md')
for p in OUT.iterdir():
    if p.is_file() and p.name not in ['manifest.json','verification-output.json']: bind(p)
save('manifest.json',{'contract':'bce.n7-fix1-scoped-review.v1','scope':'Original reviewer P2 only plus necessary precise prepare replay protocol; not a second full-task review.',
    'candidateSHA256':candidate['binarySHA256'],'validRED':'5bad165f','nativeGREEN':'5b64a6f1','glyphComplete':False,'productionIntegrated':False,
    'files':[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(files)]})
save('verification-output.json',{'command':'python 立面插件/tests/native/graphics-viewport/fix1-scoped-review/bind_evidence.py',
    'noTestsReexecuted':True,'noCadLaunched':True,'compiledInputsRechecked':19,'RED':[78,74,4],'GREEN':[78,78,0],
    'protocolChecksPerRun':7,'retiredFilesPerRun':4,'inputHashesPerRun':6,'manifestSHA256':sha(OUT/'manifest.json')})
print(json.dumps(read(OUT/'verification-output.json'),ensure_ascii=True))
