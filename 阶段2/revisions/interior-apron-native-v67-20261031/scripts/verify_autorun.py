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
offline=S/'runs'/('endpoint-offline-'+str(launch['processId']));args=[str(S/'bin/PlanAnalyse.exe'),manifest['cache']['path'],str(offline),str(output/'endpoint-supplement.tsv')];args+=([str(S/'inputs/user-axis-exclusions-v1.tsv')] if suffix in ['-v9','-v10','-v11','-v12','-v13','-v16','-v17','-v18','-v19','-v20','-v21','-v22','-v23','-v24','-v25','-v26','-v27','-v28','-v29','-v30','-v31','-v32','-v33','-v34','-v35','-v36','-v37','-v38','-v39','-v40','-v41','-v42','-v43'] else []);run=subprocess.run(args,capture_output=True,text=True)
assert run.returncode==0,run.stderr;assert read(offline/'analysis.json')==analysis
assert native['sourceObjectsVerified']==native['outputObjectsVerified']==213 and native['semanticGeometryObjectsVerified']==127 and native['allModelHandleTypesRetained'] is True and native['savedReopened'] is True and native['error'] is None
assert all(native[k] is False for k in ['COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','constructionDrawingComplete'])
axes=[a for a in analysis['axisCandidates'] if a['confirmed']];assert len(axes)==20
activeAxes=[a for a in axes if a['showNegative'] or a['showPositive']];axisCount=len(activeAxes);assert annotations['nativeAxisSets']==axisCount
expected=[]
mode=annotations.get('dimensionSampling')
if mode in ('retained-axis-per-side','exterior-faces-and-retained-axes'):
 poly=analysis['wallOuterBodyContours'][0]
 for side,vertical,key in [('bottom',True,'showNegative'),('top',True,'showPositive'),('left',False,'showNegative'),('right',False,'showPositive')]:
  positions=[a['station'] for a in annotations['axes'] if a['vertical']==vertical and a[key]]
  if mode=='exterior-faces-and-retained-axes':
   i=0 if vertical else 1;low=min(p[i] for p in poly);high=max(p[i] for p in poly);positions=[low]+[x for x in positions if low+100+1e-6<x<high-100-1e-6]+[high]
  expected.extend((side,x,y) for x,y in zip(positions,positions[1:]))
 for d,(side,x,y) in zip([d for d in annotations['dimensions'] if d.get('row','axis')=='axis'],expected):assert d['side']==side and abs(d['axisFrom']-x)<1e-6 and abs(d['axisTo']-y)<1e-6 and abs(d['expected']-(y-x))<1e-6
expectedCount=len(expected) if mode in ('retained-axis-per-side','exterior-faces-and-retained-axes') else 36
totalCount=annotations.get('nativeTotalDimensions',0);guideCount=annotations.get('trialGuides',0);logicalFullCount=annotations.get('logicalFullAxes',annotations.get('nativeFullAxisLines',0));fullCount=annotations.get('axisPresentation',{}).get('axisPieceCount',logicalFullCount)
assert annotations['nativeAxisIntervalDimensions']==expectedCount and len(annotations['dimensions'])==expectedCount+totalCount and native['annotationsAdded']==axisCount+expectedCount+totalCount+guideCount+fullCount and native['outputModelCount']==213+axisCount+expectedCount+totalCount+guideCount+fullCount and len(annotations['axes'])==axisCount
if suffix in ('-v24','-v25','-v26','-v27','-v28','-v29','-v30','-v31','-v32','-v33','-v34','-v35','-v36','-v37','-v38','-v39','-v40','-v41','-v42','-v43'):assert mode=='exterior-faces-and-retained-axes' and expectedCount==21 and logicalFullCount==20 and native['outputModelCount']==260+fullCount
if suffix in ['-v18','-v19','-v20','-v21','-v22','-v23','-v24','-v25','-v26','-v27','-v28','-v29','-v30','-v31','-v32','-v33','-v34','-v35','-v36','-v37','-v38','-v39','-v40','-v41','-v42','-v43']:assert annotations['spacingTrial'] and totalCount==4 and guideCount==4 and native['outputModelCount']==(260+fullCount if suffix in ('-v24','-v25','-v26','-v27','-v28','-v29','-v30','-v31','-v32','-v33','-v34','-v35','-v36','-v37','-v38','-v39','-v40','-v41','-v42','-v43') else 259 if suffix=='-v23' else 265)
assert all(abs(d['expected']-d['liveMeasurement'])<1e-6 for d in annotations['dimensions'])

dwg=output/'方案施工图_阶段2中间图.dwg';assert sha(dwg)==native['outputDWGSHA256'] and sha(Path(manifest['source']['path']))==binding['sourceSHA256'] and sha(Path(manifest['cache']['path']))==binding['cacheSHA256']
result=dict(contract='bcp.stage2.native-run.v2',recorded=datetime.datetime.now().astimezone().isoformat(),session=str(session),nonce=cfg['nonce'],processId=launch['processId'],binding=binding,invocation='own ObjectARX command in startup script before worker queue',output=str(output),DWGSHA256=sha(dwg),analysisSHA256=sha(output/'analysis.json'),nativeReportSHA256=sha(output/'native-foundation.json'),offlineNativeResultsIdentical=True,stage2Complete=False)
out=S/f'evidence/native-run{suffix}.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(output),newNativeAxes=axisCount,newNativeDimensions=expectedCount,savedModelCount=native["outputModelCount"],issues=len(analysis['issues'])),ensure_ascii=False))
