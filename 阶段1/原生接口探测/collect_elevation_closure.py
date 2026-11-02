"""Bind elevation evidence and archive only exited, identity-bound staging files."""
from pathlib import Path
import argparse
import configparser
import ctypes
from datetime import datetime
import hashlib
import json
import shutil

PROJECT = Path(__file__).resolve().parent
STAGE = PROJECT.parent
TRUSTED = Path('C:/Users/PC/AppData/Roaming/Tangent/TArch20V8/sys').resolve()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest().upper()


def process_state(pid):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x00100000, False, pid)
    if not handle:
        return False if ctypes.get_last_error() == 87 else None
    try:
        return kernel.WaitForSingleObject(handle, 0) == 258
    finally:
        kernel.CloseHandle(handle)


def main(cleanup):
    result = dict(contract='stage1.elevation-evidence-closure.v1', recorded=datetime.now().astimezone().isoformat(),
                  inputs=[], runs=[], staging=[], phase2Started=False, cleanupRequested=cleanup)
    for path, expected in [
        (STAGE.parent/'阶段0/输入快照/方案平面图.dwg', '5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'),
        (STAGE.parent/'阶段0/输入快照/施工图平面参考图.dwg', '8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'),
        (Path('D:/cad插件/方案平面图.dwg'), '92E01E8DD5BD4BA2B65CA48C6A3E8F6C38E3CB5F7EF3C3C36E8B692980B8BDD6'),
        (Path('D:/cad插件/施工图平面参考图.dwg'), '8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'),
    ]:
        actual = sha(path)
        result['inputs'].append(dict(path=str(path), sha256=actual, expected=expected, unchanged=actual == expected))
        if actual != expected:
            raise RuntimeError('Input identity changed; preserve and investigate: ' + str(path))
    for run in sorted((STAGE/'runs').glob('quiet-elevation*')):
        entry = dict(run=str(run), reports=[])
        for report in sorted(run.glob('*.json')):
            if report.name == 'elevation-ui-bridge-progress.json':
                continue
            if report.name.startswith('elevation-ui-defaults-attempt-'):
                continue
            data = read(report)
            entry['reports'].append(dict(path=str(report), sha256=sha(report), contract=data.get('contract'),
                                         completed=data.get('completed'), result=data.get('result'),
                                         verificationExitCode=data.get('verificationExitCode'), error=data.get('error')))
        result['runs'].append(entry)
    for session in sorted((STAGE/'runs').glob('isolated-axis-20261029-*')):
        if session.name < 'isolated-axis-20261029-115000':
            continue
        entry = dict(session=str(session), deleted=[], status='PRESERVED')
        try:
            config = read(session/'worker-config.json')
            launch = read(session/'isolation-host-receipt.json')
            nonce = config['nonce']
            assert launch['nonce'] == nonce and launch['origin'] == 'CreateProcessW'
            assert Path(config['session']).resolve() == session.resolve()
            module, ini = Path(config['stagedARX']).resolve(), Path(config['stagedINI']).resolve()
            assert module.parent == TRUSTED and ini == module.with_suffix('.ini')
            assert module.name == 'S1Quiet_' + config['binarySHA256'][:12] + '_' + nonce[:8] + '.arx'
            alive = process_state(launch['processId'])
            entry.update(processId=launch['processId'], processAliveOrUnknown=alive)
            if alive is not False:
                raise RuntimeError('Process alive or absence unproven')
            entry['status'] = 'EXITED_STAGING_IDENTIFIED'
            if cleanup:
                archive = session/'staging-final-archive'
                archive.mkdir(exist_ok=True)
                for path in (module, ini):
                    if not path.exists():
                        continue
                    if path == module:
                        assert sha(path) == config['binarySHA256']
                    else:
                        parser = configparser.ConfigParser()
                        parser.read(path, encoding='utf-16')
                        assert parser['worker']['nonce'] == nonce
                        assert Path(parser['worker']['session']).resolve() == session.resolve()
                    target = archive/path.name
                    if not target.exists():
                        shutil.copyfile(path, target)
                    assert sha(target) == sha(path)
                    path.unlink()
                    entry['deleted'].append(str(path))
                entry['status'] = 'EXACT_OWNED_STAGING_ARCHIVED_AND_REMOVED'
            final = session/'isolation-final.json'
            if final.exists():
                entry['monitorFinal'] = dict(path=str(final), sha256=sha(final), evidence=read(final))
        except (OSError, ValueError, KeyError, AssertionError, RuntimeError) as error:
            entry['error'] = str(error)
        result['staging'].append(entry)
    output = PROJECT/('标高最小试验_输入与模块终核_' + datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.json')
    with output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
    print(json.dumps(dict(path=str(output), inputsUnchanged=True, runs=len(result['runs']),
                          staging=[dict(session=x['session'], status=x['status'], error=x.get('error')) for x in result['staging']]), ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cleanup', action='store_true')
    main(parser.parse_args().cleanup)
