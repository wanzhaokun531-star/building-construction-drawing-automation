"""Expected native failure for an altered cache identity; no output should appear."""
from pathlib import Path
import json,datetime,sys
from run_native import S,read,sha,runner
def main():
 suffix=sys.argv[1] if len(sys.argv)>1 else '';assert suffix in ['', '-v2','-v3','-v4','-v5'];binding=read(S/f'evidence/native-host-binding{suffix}.json');session=Path(binding['session']);ready=read(session/'quiet-worker-ready.json');sidecar=Path(binding['sidecar']);original=sidecar.read_bytes();assert sha(sidecar)==binding['sidecarSHA256']
 case=S.parent/'阶段1/runs'/('stage2-cache-rejection-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));case.mkdir();before={p.name for p in (S/'runs').iterdir()};text=original.decode('utf-16');text=text.replace('cacheHash='+binding['cacheSHA256'],'cacheHash='+'0'*64).replace('receiptDirectory='+binding['job'],'receiptDirectory='+str(case))
 try:
  sidecar.write_text(text,encoding='utf-16');step=runner().dispatch(session,ready,'S1PHASE2EXECUTE',case,'stage2-analysis.error',60);error=(case/'stage2-analysis.error').read_text(encoding='utf-8');assert error=='failure: frozen input/cache hash mismatch';assert before=={p.name for p in (S/'runs').iterdir()};assert not (case/'stage2-execute.done').exists()
 finally:sidecar.write_bytes(original)
 assert sha(sidecar)==binding['sidecarSHA256'];report=S/f'evidence/native-cache-rejection{suffix}.json';assert not report.exists();report.write_text(json.dumps(dict(contract='bcp.stage2.native-input-rejection.v1',case=str(case),step=step,expectedFailure=error,newStage2Outputs=False,sidecarRestored=True,sourceUnchanged=sha(Path(read(S/'inputs/frozen-scheme-v1.json')['source']['path']))==binding['sourceSHA256']),ensure_ascii=False,indent=2),encoding='utf-8');print('PASS altered cache identity rejected; no DWG output; sidecar restored')
if __name__=='__main__':main()
