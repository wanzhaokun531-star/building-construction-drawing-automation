"""Dispatch the owned native adapter, then compare actual SDK output with C++ core."""
from pathlib import Path
import json,sys,hashlib,importlib.util,time,datetime,subprocess
S=Path(__file__).resolve().parent;P=S.parent/'阶段1/原生接口探测'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def runner():
 spec=importlib.util.spec_from_file_location('stage1_native_dispatch',P/'run_quiet_job.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def main():
 suffix=sys.argv[1] if len(sys.argv)>1 else '';assert suffix in ['', '-v2','-v3','-v4','-v5'];binding=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(binding['session']);cfg=read(session/'worker-config.json');m=runner();end=time.monotonic()+90
 while not (session/'quiet-worker-ready.json').exists():
  if time.monotonic()>end:raise RuntimeError('private worker readiness timeout')
  time.sleep(.2)
 ready=read(session/'quiet-worker-ready.json');assert ready['nonce']==binding['nonce']==cfg['nonce'] and ready['desktop']=='CodexS1_'+binding['nonce'] and ready['inputDesktop']!=ready['desktop']
 assert sha(Path(binding['module']))==binding['moduleSHA256'];assert sha(Path(binding['sidecar']))==binding['sidecarSHA256']
 for name,h in binding['sourceFiles'].items():assert sha(S/'src'/name)==h
 before={p for p in (S/'runs').iterdir() if p.name.startswith('native-')};step=m.dispatch(session,ready,'S1PHASE2EXECUTE',Path(binding['job']),'stage2-execute.done',90)
 after={p for p in (S/'runs').iterdir() if p.name.startswith('native-')};new=after-before;assert len(new)==1;output=new.pop();native=read(output/'native-foundation.json');analysis=read(output/'analysis.json');offline_path=Path(binding.get('offlineAnalysis',str(S/'runs/analysis-v2/analysis.json')))
 if suffix in ['-v3','-v4','-v5']:
  offline_dir=S/'runs'/('endpoint-offline-'+str(ready['processId']));run=subprocess.run([str(S/'bin/PlanAnalyse.exe'),str(Path(read(Path(binding.get('inputManifest',str(S/'inputs/frozen-scheme-v1.json'))))['cache']['path'])),str(offline_dir),str(output/'endpoint-supplement.tsv')],capture_output=True,text=True);assert run.returncode==0,run.stderr;offline_path=offline_dir/'analysis.json'
 offline=read(offline_path);assert analysis==offline
 assert native['sourceObjectsVerified']==native['outputObjectsVerified']==213 and native['savedReopened'] is True and native['error'] is None and all(native[k] is False for k in ['COMUsed','ComputerUseUsed','TianzhengBusinessCommandsUsed','constructionDrawingComplete'])
 if suffix:assert native['allModelHandleTypesRetained'] is True and native['semanticGeometryObjectsVerified']==(127 if suffix in ['-v4','-v5'] else 93)
 dwg=output/('方案施工图_阶段2中间图.dwg' if suffix in ['-v4','-v5'] else '方案工作副本_未生成标注.dwg');assert sha(dwg)==native['outputDWGSHA256'];assert sha(Path(read(S/'inputs/frozen-scheme-v1.json')['source']['path']))==binding['sourceSHA256']
 result=dict(contract='bcp.stage2.native-run.v1',recorded=datetime.datetime.now().astimezone().isoformat(),session=str(session),nonce=ready['nonce'],processId=ready['processId'],binding=binding,step=step,output=str(output),analysisSHA256=sha(output/'analysis.json'),nativeReportSHA256=sha(output/'native-foundation.json'),DWGSHA256=sha(dwg),offlineNativeResultsIdentical=True,stage2Complete=False)
 report=S/f'evidence/native-run{suffix}.json';assert not report.exists();report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(output),nativeObjects=213,savedReopened=True,axisCandidates=len(analysis['axisCandidates']),issues=len(analysis['issues'])),ensure_ascii=False))
if __name__=='__main__':main()
