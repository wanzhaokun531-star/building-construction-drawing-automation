from pathlib import Path
import json,sys,hashlib,time,subprocess,datetime
S=Path(__file__).resolve().parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
suffix=sys.argv[1] if len(sys.argv)>1 else '-v36';binding=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(binding['session']);cfg=read(session/'worker-config.json');job=Path(binding['job'])
assert sha(Path(binding['module']))==binding['moduleSHA256'] and sha(Path(binding['sidecar']))==binding['sidecarSHA256']
for name,h in binding['sourceFiles'].items():assert sha(S/'src'/name)==h,name
deadline=time.monotonic()+90
while not (job/'stage2-analysis.done').exists():
 if (job/'stage2-analysis.error').exists():raise RuntimeError((job/'stage2-analysis.error').read_text(encoding='utf-8-sig'))
 if time.monotonic()>deadline:raise TimeoutError('own startup command did not finish')
 time.sleep(.2)
receipt=read(job/'stage2-analysis.done');outputs=[p.parent for p in (S/'runs').glob('native-*/native-foundation.json') if read(p)==receipt];assert len(outputs)==1;output=outputs[0];native=read(output/'native-foundation.json');a=read(output/'native-annotations.json');analysis=read(output/'analysis.json');launch=read(session/'isolation-host-receipt.json');assert cfg['nonce']==launch['nonce']==binding['nonce'] and launch['switchDesktopCalled'] is False
assert native['savedReopened'] and native['error'] is None and native['sourceObjectsVerified']==213 and native['semanticGeometryObjectsVerified']==127 and native['allModelHandleTypesRetained']
assert not any(native[k] for k in ['COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','constructionDrawingComplete'])
assert a['unifiedOpenings'] and a['nativeAxisIntervalDimensions']==28 and len([d for d in a['dimensions'] if d['row']=='axis'])==28
assert a['trialGuides']==0 and a['logicalFullAxes']==20 and a['nativeAxisSets']==18 and len(a['captions'])==(27 if suffix in ('-v44','-v45','-v46','-v47','-v48','-v49','-v50','-v51','-v52','-v53','-v54','-v55','-v56','-v57','-v58','-v59','-v60','-v61','-v62','-v63','-v64','-v65') else 30)
assert len([x for x in a['captions'] if x['pending']])==(0 if suffix in ('-v44','-v45','-v46','-v47','-v48','-v49','-v50','-v51','-v52','-v53','-v54','-v55','-v56','-v57','-v58','-v59','-v60','-v61','-v62','-v63','-v64','-v65') else 13) and len([x for x in a['captions'] if not x['pending']])==(27 if suffix in ('-v44','-v45','-v46','-v47','-v48','-v49','-v50','-v51','-v52','-v53','-v54','-v55','-v56','-v57','-v58','-v59','-v60','-v61','-v62','-v63','-v64','-v65') else 17)
count=213+18+len(a['dimensions'])+len(a['captions'])+a['nativeFullAxisLines']+len(a.get('witnessMasks',[]));assert native['outputModelCount']==count and native['annotationsAdded']==count-213
manifest=read(Path(binding['inputManifest']));offline=S/'runs'/('endpoint-unified-'+str(launch['processId']));run=subprocess.run([str(S/'bin/PlanAnalyse.exe'),manifest['cache']['path'],str(offline),str(output/'endpoint-supplement.tsv'),str(S/'inputs/user-axis-exclusions-v1.tsv')]+([str(S/'inputs/user-opening-prefixes-v1.tsv')] if suffix in ('-v44','-v45','-v46','-v47','-v48','-v49','-v50','-v51','-v52','-v53','-v54','-v55','-v56','-v57','-v58','-v59','-v60','-v61','-v62','-v63','-v64','-v65') else []),capture_output=True,text=True,encoding='utf-8');assert run.returncode==0,run.stderr;assert read(offline/'analysis.json')==analysis
source=output/'方案施工图_阶段2中间图.dwg';assert sha(source)==native['outputDWGSHA256'];assert sha(Path(manifest['source']['path']))==binding['sourceSHA256'];assert sha(Path(manifest['cache']['path']))==binding['cacheSHA256']
r=dict(contract='bcp.stage2.native-run.v2',recorded=datetime.datetime.now().astimezone().isoformat(),session=str(session),nonce=cfg['nonce'],processId=launch['processId'],binding=binding,output=str(output),DWGSHA256=sha(source),analysisSHA256=sha(output/'analysis.json'),nativeReportSHA256=sha(output/'native-foundation.json'),endpointCaptureSHA256=sha(output/'endpoint-capture.json'),offlineNativeResultsIdentical=True,stage2Complete=False)
p=S/f'evidence/native-run{suffix}.json';assert not p.exists();p.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(output),dimensions=len(a['dimensions']),captions=len(a['captions']),formalCaptions=sum(not x["pending"] for x in a["captions"]),pendingCaptions=sum(x["pending"] for x in a["captions"]),modelCount=count),ensure_ascii=False))
