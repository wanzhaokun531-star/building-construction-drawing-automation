"""Verify the owned native startup command without a second queued invocation."""
from pathlib import Path
import json,sys,hashlib,time,subprocess,datetime
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
suffix=sys.argv[1] if len(sys.argv)>1 else '-v8'
binding=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(binding['session']);cfg=read(session/'worker-config.json');job=Path(binding['job'])
assert cfg['nonce']==binding['nonce'] and sha(Path(binding['module']))==binding['moduleSHA256'] and sha(Path(binding['sidecar']))==binding['sidecarSHA256']
for name,h in binding['sourceFiles'].items():assert sha(S/'src'/name)==h
deadline=time.monotonic()+90
while not (job/'stage2-analysis.done').exists():
 if (job/'stage2-analysis.error').exists():raise RuntimeError((job/'stage2-analysis.error').read_text(encoding='utf-8'))
 if time.monotonic()>deadline:raise TimeoutError('startup command did not finish; no dialog clicked or command cancelled')
 time.sleep(.2)
launch=read(session/'isolation-host-receipt.json');assert launch['nonce']==binding['nonce'] and launch['desktopName']=='CodexS1_'+binding['nonce'] and launch['switchDesktopCalled'] is False
receipt=read(job/'stage2-analysis.done');outputs=[p.parent for p in (S/'runs').glob('native-*/native-foundation.json') if read(p)==receipt]
assert len(outputs)==1;output=outputs[0];assert '-'+str(launch['processId'])+'-' in output.name
native=read(output/'native-foundation.json');analysis=read(output/'analysis.json');annotations=read(output/'native-annotations.json');manifest=read(Path(binding['inputManifest']))
offline=S/'runs'/('endpoint-offline-'+str(launch['processId']));args=[str(S/'bin/PlanAnalyse.exe'),manifest['cache']['path'],str(offline),str(output/'endpoint-supplement.tsv')];args+=([str(S/'inputs/user-axis-exclusions-v1.tsv')] if suffix in ['-v9','-v10','-v11','-v12','-v13','-v16','-v17','-v18'] else []);run=subprocess.run(args,capture_output=True,text=True)
assert run.returncode==0,run.stderr;assert read(offline/'analysis.json')==analysis
assert native['sourceObjectsVerified']==native['outputObjectsVerified']==213 and native['semanticGeometryObjectsVerified']==127 and native['allModelHandleTypesRetained'] is True and native['savedReopened'] is True and native['error'] is None
assert all(native[k] is False for k in ['COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','constructionDrawingComplete'])
axes=[a for a in analysis['axisCandidates'] if a['confirmed']];assert annotations['nativeAxisSets']==len(axes)==20
expected=[]
if annotations.get('dimensionSampling')=='retained-axis-per-side':
 for side,vertical,key in [('bottom',True,'showNegative'),('top',True,'showPositive'),('left',False,'showNegative'),('right',False,'showPositive')]:
  positions=[a['station'] for a in annotations['axes'] if a['vertical']==vertical and a[key]]
  expected.extend((side,x,y) for x,y in zip(positions,positions[1:]))
 assert len(expected)==24
 for d,(side,x,y) in zip([d for d in annotations['dimensions'] if d.get('row','axis')=='axis'],expected):assert d['side']==side and abs(d['axisFrom']-x)<1e-6 and abs(d['axisTo']-y)<1e-6 and abs(d['expected']-(y-x))<1e-6
expectedCount=len(expected) if annotations.get('dimensionSampling')=='retained-axis-per-side' else 36
totalCount=annotations.get('nativeTotalDimensions',0);guideCount=annotations.get('trialGuides',0)
assert annotations['nativeAxisIntervalDimensions']==expectedCount and len(annotations['dimensions'])==expectedCount+totalCount and native['annotationsAdded']==20+expectedCount+totalCount+guideCount and native['outputModelCount']==213+20+expectedCount+totalCount+guideCount and len(annotations['axes'])==20
if suffix=='-v18':assert annotations['spacingTrial'] and totalCount==4 and guideCount==4 and native['outputModelCount']==265
assert all(abs(d['expected']-d['liveMeasurement'])<1e-6 for d in annotations['dimensions'])

dwg=output/'方案施工图_阶段2中间图.dwg';assert sha(dwg)==native['outputDWGSHA256'] and sha(Path(manifest['source']['path']))==binding['sourceSHA256'] and sha(Path(manifest['cache']['path']))==binding['cacheSHA256']
result=dict(contract='bcp.stage2.native-run.v2',recorded=datetime.datetime.now().astimezone().isoformat(),session=str(session),nonce=cfg['nonce'],processId=launch['processId'],binding=binding,invocation='own ObjectARX command in startup script before worker queue',output=str(output),DWGSHA256=sha(dwg),analysisSHA256=sha(output/'analysis.json'),nativeReportSHA256=sha(output/'native-foundation.json'),offlineNativeResultsIdentical=True,stage2Complete=False)
out=S/f'evidence/native-run{suffix}.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(output),newNativeAxes=20,newNativeDimensions=expectedCount,savedModelCount=native["outputModelCount"],issues=len(analysis['issues'])),ensure_ascii=False))
