"""Evidence packaging only. Does not build, launch CAD, or modify frozen sources."""
from pathlib import Path
import collections
import difflib
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
PROBE = OUT.parent
BUILD = ROOT / '立面插件/build/n7-20261101-221056-470-owned-file-idle-frozen-b293e40a'
BASE = ROOT / '立面插件/build/n7-20261101-211856-983-stub-red-805dc0d3'
RUN = ROOT / '阶段1/runs/elevation-native-20261101-221520-2b412dba'

def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()

def write(name, value):
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

receipt = read(BUILD / 'receipt.json')
for row in receipt['inputOrigins']:
    assert sha(row['origin']) == sha(row['compiledCopy']) == row['sha256'], row
for p, h in receipt['sourceFiles'].items():
    assert sha(p) == h, p
assert sha(receipt['binary']) == receipt['binarySHA256']
for name in ['NativeEvidence.cpp', 'NativeEvidence.h']:
    assert sha(BASE/'src'/name) == sha(BUILD/'src'/name)

native = read(RUN/'case/native-case.json')
graphics = read(RUN/'case/graphics-viewport.json')
closure = read(RUN/'native-result-closure.json')
retire = read(RUN/'reader-retirement.json')
helper = read(RUN/'viewport-native-job/viewport-host-advance.json')
binding = ROOT/'立面插件/evidence/native-case-binding-2b412dba.json'
assert native['moduleSHA256'] == receipt['binarySHA256'] == helper['candidateSHA256']
assert native['status'] == graphics['status'] == 'partial'
assert len(native['steps']) == 70 and all(x['status']=='passed' for x in native['steps'])
assert not native['errors'] and graphics['failedAssertions']==0
assert graphics['glyphComplete'] is False and graphics['capabilityUpgradeAllowed'] is False
assert sha(binding)==closure['bindingSHA256']==retire['bindingSHA256']==helper['bindingSHA256']
assert sha(RUN/'case/native-case.json')==closure['nativeReportSHA256']==helper['nativeReportSHA256']
assert sha(RUN/'isolation-final.json')==closure['supervisorFinalSHA256']==retire['supervisorFinalSHA256']
assert sha(RUN/'reader-retirement.json')==closure['retirementSHA256']
assert closure['normalQuit'] is False and closure['forcedTermination'] is True
assert retire['normalQuit'] is False and retire['forcedTermination'] is True
assert len(retire['removedFiles']) == 4 and len(retire['coreInputChecks']) == 6
for row in retire['removedFiles']:
    assert row['removed'] and not Path(row['path']).exists() and sha(row['archive'])==row['SHA256']
for row in retire['coreInputChecks']:
    assert sha(row['path'])==row['SHA256']
external=ROOT/'立面插件/tools/capture_viewport_case.py'
assert sha(external)==helper['helperSHA256']
for field, h in [('source','sourceSHA256'),('control','controlSHA256'),('prepareReady','prepareReadySHA256'),('openReport','openReportSHA256')]:
    assert sha(helper[field])==helper[h]

callbacks=[]
for line, text in enumerate((RUN/'case/viewport-callbacks.jsonl').read_text(encoding='utf-8-sig').splitlines(),1):
    callbacks.append((line,json.loads(text)))
contexts=[(n,x) for n,x in callbacks if x.get('kind')=='hostDrawContext']
texts=[{'rawLine':n,**x} for n,x in callbacks if x.get('request',{}).get('kind')=='text']
assert len(contexts)==2668 and all(x['contextAddress'] and x['contextDatabaseMatchesTarget'] and x['regenType']==2 for _,x in contexts)
assert len(texts)==72
for sample in range(12):
    a=[x['request'] for x in texts if x['sample']==sample and x['round']==0]
    b=[x['request'] for x in texts if x['sample']==sample and x['round']==1]
    assert a==b
    assert len(a)==graphics['records'][sample]['textCalls']
    for q in a:
        encoded=q['text'].encode('utf-16-le')
        assert q['utf16']==[int.from_bytes(encoded[k:k+2],'little') for k in range(0,len(encoded),2)]
        assert q['copiedCodeUnits']==len(q['utf16'])
        assert q['rawLength']==-1 or q['rawLength']==q['copiedCodeUnits']
write('actual-text-requests.json', {'source':str(RUN/'case/viewport-callbacks.jsonl'),'sourceSHA256':sha(RUN/'case/viewport-callbacks.jsonl'),'rows':texts,'scope':'Exact public request strings/styles/coordinates; no glyph outline or rendered ink claim.'})
traces=[json.loads(x) for x in (RUN/'case/viewport-command-trace.jsonl').read_text(encoding='utf-8-sig').splitlines()]
selected=[x for x in traces if x['event'].startswith(('ownedFile.','idle.','prepare.','capture.','openGate.'))]
write('actual-context-file-observation.json', {'contextCount':len(contexts),'contextSamples':sorted(set(x['sample'] for _,x in contexts)),'contextRounds':sorted(set(x['round'] for _,x in contexts)),'allNonzeroAndTargetDb':True,'viewportContexts':[{'rawLine':n,**x} for n,x in contexts if x['viewportDraw']],'commandFileObservations':selected,'callbackKindCounts':dict(collections.Counter(x.get('kind','request') for _,x in callbacks))})

# Freeze all final native inputs and the controller-owned external helper, including shared read-only dependencies.
for row in receipt['inputOrigins']:
    dest=OUT/'candidate-source'/Path(row['compiledCopy']).name
    dest.parent.mkdir(exist_ok=True)
    shutil.copyfile(row['compiledCopy'],dest)
(OUT/'external-controller').mkdir(exist_ok=True)
shutil.copyfile(external,OUT/'external-controller'/external.name)
shutil.copyfile(BUILD/'build_graphics_viewport.ps1',OUT/'candidate-source/build_graphics_viewport.ps1')
diff=[]
for p in sorted((OUT/'candidate-source').iterdir()):
    old=BASE/'src'/p.name
    if p.name=='build_graphics_viewport.ps1': old=BASE/p.name
    before=old.read_text(encoding='utf-8-sig').splitlines(True) if old.exists() else []
    after=p.read_text(encoding='utf-8-sig').splitlines(True)
    diff.extend(difflib.unified_diff(before,after,fromfile=str(old) if old.exists() else '/dev/null',tofile=str(p)))
(OUT/'stub-to-final.diff').write_text(''.join(diff),encoding='utf-8')

paths=set()
def bind(p):
    p=Path(p).resolve()
    if p.is_file(): paths.add(p)
for p in RUN.rglob('*'): bind(p)
bind(binding); bind(external)
for p in [BASE,BUILD, ROOT/'立面插件/build/n7-20261101-220604-269-owned-file-reader-valid-red-30e4ce2e',ROOT/'立面插件/build/n7-20261101-220658-774-owned-file-reader-unit-green-1f9b80ba']:
    for name in ['receipt.json','build-command.cmd','build.log','build_graphics_viewport.ps1']: bind(p/name)
for row in receipt['inputOrigins']: bind(row['origin']); bind(row['compiledCopy'])
for row in receipt['officialContracts']: assert sha(row['path'])==row['sha256']; bind(row['path'])
for p in PROBE.glob('sdk-public-contracts/*'): bind(p)
bind(receipt['binary'])
for pattern in ['*596805fa','*b9becee9','*9c4d780f']:
    for oldrun in (ROOT/'阶段1/runs').glob(pattern):
        for p in oldrun.rglob('*'):
            if p.suffix.lower() in ('.json','.jsonl','.scr','.ini'): bind(p)
for folder in ['review-7dfdec74','diagnosis-b9becee9','fix-9c4d780f']:
    for p in (PROBE/folder).glob('*'): bind(p)
for p in (ROOT/'docs/superpowers').rglob('2026-11-01-建筑施工图立面自动化*.md'): bind(p)
for name in ['N7-真实宿主视口采集简报.md','N7-真实宿主视口采集报告.md','N5-公开图形采集探针报告.md']: bind(ROOT/'立面插件/执行'/name)
for row in retire['coreInputChecks']: bind(row['path'])
for p in OUT.rglob('*'):
    if p.name not in ('manifest.json','verification-output.json'): bind(p)
write('manifest.json',{'contract':'bce.n7-first-fulltask-review.v1','reviewStatus':'ready-for-first-independent-fulltask-review-not-reviewed','baseline':str(BASE),'candidate':str(BUILD),'candidateSHA256':receipt['binarySHA256'],'nativeRun':str(RUN),'scope':'All N7 code from initial stub to b293e40a; frozen NativeEvidence read-only; controller helper included as externally authored dependency; no production integration or glyph completeness.','files':[{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(paths)]})
write('verification-output.json',{'nativeSteps':70,'passed':70,'errors':[],'contexts':2668,'textRequests':72,'twoRoundsEqualPerSample':True,'utf16AndLengthMatch':True,'compiledInputsMatch':len(receipt['inputOrigins']),'strictNativeEvidenceUnchangedFromStub':True,'controllerMatchesExecutedSHA':True,'sixInputSHAsRechecked':True,'fourRetiredFilesArchiveSHARechecked':True,'closureReferencesRechecked':True,'normalQuit':False,'forcedExit':70,'manifestSHA256':sha(OUT/'manifest.json'),'commands':'python 立面插件/tests/native/graphics-viewport/review-fulltask-b293e40a/assemble_review.py','rawResultsReusedNoCadLaunch':True})
print(json.dumps(read(OUT/'verification-output.json'),ensure_ascii=True))
