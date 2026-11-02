"""Advance only an already bound N7 GREEN host through its public SDK open/capture protocol."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def main(binding_path):
    binding_path = Path(binding_path).resolve(strict=True)
    binding = read(binding_path)
    assert binding['caseName'] == 'PublicGraphicsViewport'
    session = Path(binding['session']).resolve(strict=True)
    assert session.is_relative_to((ROOT / '阶段1/runs').resolve())
    ready = read(session / 'quiet-worker-ready.json')
    assert ready['nonce'] == binding['nonce'] and ready['error'] is None
    assert ready['desktop'] == 'CodexS1_' + binding['nonce'] != ready['inputDesktop']
    assert not (session / 'isolation-final.json').exists()
    inputs = {item['role']: item for item in binding['inputs']}
    assert set(inputs) == {'scheme', 'reference'}
    for item in inputs.values():
        for key in ('source', 'snapshot', 'inspectionCopy'):
            assert sha(item[key]) == item['SHA256']
    module = ROOT / '阶段1/原生接口探测/run_quiet_job.py'
    spec = importlib.util.spec_from_file_location('bce_viewport_native_dispatch', module)
    queue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(queue)
    case = session / 'case'
    assert Path(binding['output']).resolve(strict=True) == case.resolve(strict=True)
    assert sha(binding['candidateARX']) == binding['candidateSHA256']
    signal = case / 'prepare-ready.json'
    def prepared():
        if signal.exists():
            return read(signal)
        # A completed early failure must not be mistaken for an unfinished prepare.
        try:
            result = read(case / 'native-case.json')
        except (OSError, ValueError):
            result = None
        if result and result.get('status') == 'failed':
            raise RuntimeError('N7 completed before prepare-ready; inspect native-case.json')
        return None
    prepared_info = queue.wait_until(prepared, session, ready['processId'], 180)
    assert set(prepared_info) == {'contract', 'nonce', 'pid', 'targetPath', 'targetSHA256',
                                  'modelRootCount', 'rootHandles', 'sourceSHA'}
    assert prepared_info['contract'] == 'bce.graphics-viewport-prepare.v1'
    assert prepared_info['nonce'] == binding['nonce'] and prepared_info['pid'] == ready['processId']
    assert prepared_info['sourceSHA'] == {role: item['SHA256'] for role, item in inputs.items()}
    assert prepared_info['modelRootCount'] == 12
    handles = prepared_info['rootHandles']
    assert len(handles) == 12 and len(set(handles)) == 12
    assert all(isinstance(h, str) and re.fullmatch(r'[0-9A-F]{1,16}', h) and int(h, 16) for h in handles)
    assert len({int(h, 16) for h in handles}) == 12
    source = Path(prepared_info['targetPath']).resolve(strict=True)
    assert source == (case / 'viewport-target.dwg').resolve(strict=True)
    assert sha(source) == prepared_info['targetSHA256']
    job = session / 'viewport-native-job'
    job.mkdir()
    control = job / 'control.dwg'
    shutil.copyfile(source, control)
    assert sha(control) == prepared_info['targetSHA256']
    manifest = dict(contract='bce.viewport-host-advance.v1', binding=str(binding_path),
                    bindingSHA256=sha(binding_path), nonce=binding['nonce'], pid=ready['processId'],
                    candidateSHA256=binding['candidateSHA256'], prepareReady=str(signal),
                    prepareReadySHA256=sha(signal), source=str(source),
                    sourceSHA256=sha(source), control=str(control), controlSHA256=sha(control),
                    helperSHA256=sha(__file__), dispatcherSHA256=sha(module), error=None,
                    scope='Own N7 target only; ordinary public SDK open and exact capture command; no CAD launch/stop or user drawing save')
    try:
        queue.dispatch(session, ready, 'S1OPENPROBECONTROL', job, 'native-open.done', 120)
        marker = read(job / 'native-open.done')
        opened = read(job / 'native-open.json')
        assert marker['safeToUnload'] is True and opened['openStatus'] == 0 and opened['error'] is None
        assert sha(source) == sha(control) == prepared_info['targetSHA256']
        manifest.update(openReport=str(job / 'native-open.json'),
                        openReportSHA256=sha(job / 'native-open.json'))
        # The frozen S1 bridge deliberately accepts its own S1 callbacks only.
        # N7's reactor queues its own capture command after this exact control
        # document becomes current. Do not weaken the bridge's command filter.
        def capture_complete():
            try:
                result = read(case / 'native-case.json')
            except (OSError, ValueError):
                return None
            return result if result.get('status') in ('passed', 'partial', 'failed') else None
        result = queue.wait_until(capture_complete, session, ready['processId'], 180)
        assert result['nonce'] == binding['nonce'] and result['pid'] == ready['processId']
        assert result['moduleSHA256'] == binding['candidateSHA256']
        assert result['caseName'] == 'PublicGraphicsViewport' and not result['sourceDrawingWritten']
        assert sha(source) == sha(control) == prepared_info['targetSHA256']
        for item in inputs.values():
            for key in ('source', 'snapshot', 'inspectionCopy'):
                assert sha(item[key]) == item['SHA256']
        manifest.update(nativeReport=str(case / 'native-case.json'),
                        nativeReportSHA256=sha(case / 'native-case.json'), nativeStatus=result['status'])
        print(json.dumps(dict(nativeStatus=result['status'], report=str(case / 'native-case.json'))))
    except BaseException as error:
        manifest['error'] = repr(error)
        raise
    finally:
        (job / 'viewport-host-advance.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')

if __name__ == '__main__':
    main(sys.argv[1])
