"""Actual native negative probe; do not replace an existing candidate artifact."""
from pathlib import Path
from datetime import datetime
import hashlib,json,sys,uuid

PROJECT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(PROJECT))
from run_quiet_job import dispatch,read,digest,STAGE

def main(session):
 session=session.resolve();cfg=read(session/'worker-config.json');ready=read(session/'quiet-worker-ready.json')
 assert ready['nonce']==cfg['nonce'] and ready['session']==str(session) and digest(Path(cfg['stagedARX']))==cfg['binarySHA256']
 run=STAGE/'runs'/('quiet-preexistingcandidate-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'));run.mkdir()
 marker=run/'validation-failure.candidate.dwg';content=b'S1_EXISTING_EVIDENCE_MUST_SURVIVE_NATIVE_REJECTION'
 with marker.open('xb') as stream:stream.write(content)
 report=dict(contract='stage1.preexisting-candidate-protection.v1',session=str(session),nonce=cfg['nonce'],processId=ready['processId'],binarySHA256=cfg['binarySHA256'],run=str(run),marker=str(marker),sentinelIsDrawing=False,sentinelSHA256=hashlib.sha256(content).hexdigest().upper(),sourceFiles=cfg['sourceFiles'],driverSHA256=digest(Path(__file__)),COMUsed=False,ComputerUseUsed=False,TianzhengCommandsUsed=False)
 report['dispatch']=dispatch(session,ready,'S1NATIVECANDIDATEDISCARD',run,'native-candidate-discard.json',90)
 native=read(run/'native-candidate-discard.json');case=native['cases'][0]
 report.update(nativeAttemptRejected=case['error'] is not None and case['passed'] is False,preserved=marker.is_file() and marker.read_bytes()==content,nativeReport=str(run/'native-candidate-discard.json'),nativeReportSHA256=digest(run/'native-candidate-discard.json'))
 report['attemptDidNotOwnExistingFile']=case.get('candidateOwnedByAttempt') is False
 report['noPublicationForRejectedCase']=case.get('moveAttempted') is False and case.get('published') is False and case.get('publicationExists') is False and case.get('candidateSaveStatus')==-9999
 report['sourcePublicStatesUnchanged']=native.get('originalBefore')==native.get('originalFresh') and len(native.get('originalBefore',[]))==213
 report['completed']=report['nativeAttemptRejected'] and report['preserved'] and report['attemptDidNotOwnExistingFile'] and report['noPublicationForRejectedCase'] and report['sourcePublicStatesUnchanged'];report['phase2Started']=False
 (run/'preexisting-protection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 (run/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
 print(json.dumps(dict(run=str(run),nativeAttemptRejected=report['nativeAttemptRejected'],preserved=report['preserved'],result='PASS' if report['completed'] else 'FAIL'),ensure_ascii=False))
 return 0 if report['completed'] else 1
if __name__=='__main__':raise SystemExit(main(Path(sys.argv[1])))
