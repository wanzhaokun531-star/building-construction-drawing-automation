"""Attest a completed owned test, then close only its nonce-bound host and staging."""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess
import sys
import time
import command_case_host as host


def main(binding_path):
    binding_path = Path(binding_path).resolve(strict=True)
    binding = host.read(binding_path)
    session = Path(binding['session'])
    deadline = time.monotonic() + 1600
    report_path = Path(binding['caseReport'])
    report = None
    while report is None:
        try:
            report = host.read(report_path)
        except (OSError, ValueError):
            pass
        if report is not None:
            break
        final_path = session / 'isolation-final.json'
        if final_path.exists():
            final = host.read(final_path)
            if final.get('nonce') != binding['nonce']:
                raise RuntimeError('Supervisor final identity differs; no cleanup attempted')
            if final.get('workerExited'):
                raise RuntimeError('Owned host exited before a closed native report; not a business test result. Explicit retirement is required.')
        if time.monotonic() >= deadline:
            raise TimeoutError('No closed readable native result; host retained for explicit diagnosis')
        time.sleep(.3)
    launch = host.read(session / 'isolation-host-receipt.json')
    ready = host.read(session / 'quiet-worker-ready.json')
    assert report['nonce'] == binding['nonce'] == launch['nonce'] == ready['nonce']
    assert report['pid'] == launch['processId'] == ready['processId']
    assert report['moduleSHA256'] == binding['candidateSHA256'] == host.sha(binding['candidateARX'])
    assert host.sha(binding['productARX']) == binding['productSHA256']
    assert len(binding['stagedFiles']) == 5
    for staged in binding['stagedFiles']:
        assert host.sha(staged['path']) == staged['SHA256']
    assert report['desktop'] == 'CodexS1_' + binding['nonce'] != ready['inputDesktop']
    assert not report['sourceDrawingWritten'] and report['status'] in ('passed', 'partial', 'failed')
    for item in binding['inputs']:
        for key in ('source', 'snapshot', 'inspectionCopy'):
            assert host.sha(item[key]) == item['SHA256']
    observed = dict(contract='bce.native-result-and-closure.v1', recorded=datetime.datetime.now().astimezone().isoformat(),
                    binding=str(binding_path), bindingSHA256=host.sha(binding_path), nativeReport=str(report_path),
                    nativeReportSHA256=host.sha(report_path), nativeStatus=report['status'],
                    nonce=binding['nonce'], processId=report['pid'], binarySHA256=report['moduleSHA256'], productSHA256=binding['productSHA256'],
                    sourceSHAsUnchanged=True, businessPassInferred=False,
                    scope='Result identity and closure only; caller must evaluate behavior assertions separately')
    print(json.dumps(dict(nativeStatus=report['status'], nativeReport=str(report_path), processId=report['pid'])), flush=True)
    queue_before = {p.name for p in (session / 'queue').glob('*.ini')}
    host.queue_exit(binding_path)
    added = [p for p in (session / 'queue').glob('*.ini') if p.name not in queue_before]
    assert len(added) == 1
    serial = added[0].stem
    quit_deadline = time.monotonic() + 120
    stop_needed = False
    while not (session / 'isolation-final.json').exists():
        try:
            state = host.read(session / 'worker-state.json')
        except (OSError, ValueError):
            state = {}
        if state.get('serial') == serial and state.get('command') == 'S1QUIETEXIT' and state.get('phase') == 'idle' and state.get('cmdActive') == 0:
            if state.get('error') == 'private worker final document is modified; QUIT withheld':
                stop_needed = True
                break
            if state.get('error'):
                raise RuntimeError('Owned exit failed unexpectedly; host retained: ' + state['error'])
        if time.monotonic() >= quit_deadline:
            stop_needed = True
            break
        time.sleep(.3)
    if stop_needed:
        stop_script = host.BRIDGE / '启动隔离桌面宿主.ps1'
        command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(stop_script),
                   '-RunDirectory', str(session), '-StopOwnedWorker', '-ExpectedNonce', binding['nonce']]
        outcome = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace',
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        observed['explicitStopHelperExitCode'] = outcome.returncode
        assert outcome.returncode == 0, outcome.stdout + outcome.stderr
    end = time.monotonic() + 20
    while not (session / 'isolation-final.json').exists():
        if time.monotonic() >= end:
            raise TimeoutError('Supervisor final not yet closed; no staging removal attempted')
        time.sleep(.2)
    final = host.read(session / 'isolation-final.json')
    assert final['nonce'] == binding['nonce'] and final['processId'] == report['pid']
    assert final['workerExited'] and final['desktopClosed'] and final['processHandlesClosed']
    # A QUIT marker cannot establish normal exit after an explicit stop request.
    # Keep forcedTermination below as the raw supervisor observation.
    observed.update(normalQuit=(final['nativeQuitConfirmed'] and not final['forcedTermination']
                                and not stop_needed and final.get('workerExitCode') == 0),
                    explicitStopRequested=stop_needed,
                    normalQuitUnprovenDueToExplicitStop=stop_needed,
                    forcedTermination=final['forcedTermination'], timedOut=final['timedOut'],
                    supervisorFinalSHA256=host.sha(session / 'isolation-final.json'))
    host.retire(binding_path)
    observed['retirement'] = str(session / 'reader-retirement.json')
    observed['retirementSHA256'] = host.sha(observed['retirement'])
    observed['watcherSHA256'] = host.sha(__file__)
    host.save_new(session / 'native-result-closure.json', observed)
    print(json.dumps(dict(closure=str(session / 'native-result-closure.json'), nativeStatus=report['status'],
                          normalQuit=observed['normalQuit'], stagedFilesRemoved=len(binding['stagedFiles']))), flush=True)


if __name__ == '__main__':
    main(sys.argv[1])
