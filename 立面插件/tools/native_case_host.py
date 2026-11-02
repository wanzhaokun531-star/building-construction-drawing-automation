"""Prepare nonce-bound native test hosts; never launches or stops CAD implicitly."""
from pathlib import Path
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
import re
import shutil
import uuid

ROOT = Path(__file__).resolve().parents[2]
PLUGIN = ROOT / '立面插件'
BRIDGE = ROOT / '阶段1/原生接口探测'


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def save_new(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)


def checked_inputs():
    frozen = read(ROOT / 'outputs/elevation-reading-20261101/reader-binding.json')['inputs']
    assert len(frozen) == 2
    for item in frozen:
        assert sha(item['source']) == sha(item['snapshot']) == item['SHA256']
    return frozen


def prepare(receipt_path, case_name):
    assert re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,79}', case_name)
    receipt_path = Path(receipt_path).resolve(strict=True)
    candidate = read(receipt_path)
    assert candidate['exitCode'] == 0
    candidate_binary = Path(candidate['binary']).resolve(strict=True)
    assert candidate_binary.is_relative_to((PLUGIN / 'build').resolve())
    assert sha(candidate_binary) == candidate['binarySHA256']
    for path, digest in candidate['sourceFiles'].items():
        assert sha(path) == digest, ('source changed after build', path)
    bridge_receipt_path = BRIDGE / 'evidence/build-product-bridge-v5-20261101.json'
    bridge_receipt = read(bridge_receipt_path)
    bridge_binary = BRIDGE / 'bin/NativeAccessProbe.arx'
    assert sha(bridge_binary) == bridge_receipt['binarySHA256']
    for name, digest in bridge_receipt['sourceFiles'].items():
        assert sha(BRIDGE / 'src' / name) == digest, ('bridge source changed', name)
    support_path = BRIDGE / 'evidence/autocad-support-profile-20261029.json'
    support = read(support_path)
    assert sha(support['componentPath']) == support['componentSHA256']
    frozen = checked_inputs()
    nonce = uuid.uuid4().hex
    session = ROOT / '阶段1/runs' / ('elevation-native-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + nonce[:8])
    session.mkdir()
    (session / 'queue').mkdir()
    output = session / 'case'
    output.mkdir()
    inputs = []
    for item in frozen:
        dest = session / (item['role'] + '-input.dwg')
        shutil.copyfile(item['snapshot'], dest)
        assert sha(dest) == item['SHA256']
        inputs.append(dict(item, inspectionCopy=str(dest)))
    # Existing retirement helper verifies this exact prefix and four exact files.
    staging = Path('C:/Program Files/Autodesk/ApplicationPlugins') / ('CodexElevationStage1_' + nonce)
    staging.mkdir()
    bridge = staging / ('S1Read_' + nonce[:8] + '.arx')
    native = staging / ('BCECase_' + nonce[:8] + '.arx')
    shutil.copyfile(bridge_binary, bridge)
    shutil.copyfile(candidate_binary, native)
    bridge_ini, native_ini = bridge.with_suffix('.ini'), native.with_suffix('.ini')
    bridge_ini.write_text(
        f'[probe]\nroot={ROOT / "阶段1"}\nscheme={frozen[0]["snapshot"]}\nreference={frozen[1]["snapshot"]}\noutput={session}\n'
        f'[worker]\nnonce={nonce}\nsession={session}\nexpectedProfile={support["profile"]}\n'
        f'supportComponent={support["componentPath"]}\nearlyNativeAutoload=1\n', encoding='utf-16')
    by_role = {i['role']: i for i in inputs}
    native_ini.write_text(
        f'[case]\nnonce={nonce}\nsession={session}\nscheme={by_role["scheme"]["inspectionCopy"]}\n'
        f'reference={by_role["reference"]["inspectionCopy"]}\noutput={output}\ncaseName={case_name}\n'
        f'expectedModuleSHA256={candidate["binarySHA256"]}\n', encoding='utf-16')
    boot = session / 'boot.scr'
    boot.write_bytes((f'_.ARX\r\n_L\r\n"{bridge.as_posix()}"\r\nS1QUIETSTART\r\n'
                      f'_.ARX\r\n_L\r\n"{native.as_posix()}"\r\nBCE_TEST_RUN\r\n').encode('ascii'))
    template = Path('C:/Users/PC/AppData/Local/Autodesk/AutoCAD 2020/R23.1/chs/Template/acadiso.dwt')
    assert template.is_file()
    cleanup = ROOT / '立面插件/阶段1/工具/retire_reader_staging.py'
    binding = dict(contract='bce.native-case-host.v1', session=str(session), nonce=nonce,
                   profile=support['profile'], template=str(template), earlyNativeAutoload=True,
                   stagedARX=str(bridge), stagedINI=str(bridge_ini), stagingDirectory=str(staging),
                   binarySHA256=sha(bridge), sourceFiles=bridge_receipt['sourceFiles'],
                   readerARX=str(native), readerINI=str(native_ini), readerSHA256=sha(native),
                   candidateARX=str(native), candidateINI=str(native_ini), candidateSHA256=sha(native),
                   candidateSourceFiles=candidate['sourceFiles'], buildReceipt=str(receipt_path),
                   buildReceiptSHA256=sha(receipt_path), bridgeBuildReceipt=str(bridge_receipt_path),
                   bridgeBuildReceiptSHA256=sha(bridge_receipt_path), supportRecord=str(support_path),
                   supportRecordSHA256=sha(support_path), inputs=inputs, boot=str(boot), bootSHA256=sha(boot),
                   caseName=case_name, output=str(output), caseReport=str(output / 'native-case.json'),
                   preparationTool=str(Path(__file__).resolve()), preparationToolSHA256=sha(__file__),
                   retirementHelper=str(cleanup), retirementHelperSHA256=sha(cleanup),
                   scope='New private native test host; no BCP business dispatch; source originals read-only; writes only owned case fixtures',
                   legacyFieldAliases='readerARX/readerINI/readerSHA256 are exact candidate aliases for unchanged retirement helper compatibility')
    save_new(session / 'worker-config.json', binding)
    evidence = PLUGIN / 'evidence'
    evidence.mkdir(exist_ok=True)
    target = evidence / ('native-case-binding-' + nonce[:8] + '.json')
    save_new(target, binding)
    print(json.dumps(dict(binding=str(target), session=str(session), nonce=nonce)))


def queue_exit(binding_path):
    binding = read(binding_path)
    session = Path(binding['session']).resolve(strict=True)
    ready = read(session / 'quiet-worker-ready.json')
    host = read(session / 'isolation-host-receipt.json')
    assert ready['nonce'] == host['nonce'] == binding['nonce']
    assert ready['processId'] == host['processId'] and ready['session'] == str(session)
    assert ready['desktop'] == 'CodexS1_' + binding['nonce'] != ready['inputDesktop']
    assert sha(binding['stagedARX']) == binding['binarySHA256'] and sha(binding['candidateARX']) == binding['candidateSHA256']
    serial = uuid.uuid4().hex
    pending = session / 'queue' / (serial + '.tmp')
    pending.write_text(f'[job]\nnonce={binding["nonce"]}\nserial={serial}\ncommand=S1QUIETEXIT\nrun={session}\n', encoding='utf-16')
    os.replace(pending, pending.with_suffix('.ini'))
    print(json.dumps(dict(queued=True, serial=serial, scope='Owned quiet exit requested; process exit not yet proven')))


def retire(binding_path):
    binding = read(binding_path)
    helper = Path(binding['retirementHelper'])
    assert sha(helper) == binding['retirementHelperSHA256']
    spec = importlib.util.spec_from_file_location('bce_exact_retirement', helper)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main(binding_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='action', required=True)
    p = sub.add_parser('prepare')
    p.add_argument('receipt', type=Path)
    p.add_argument('--case', default='NativePrototypeRoundtrip')
    for name in ('queue-exit', 'retire'):
        sub.add_parser(name).add_argument('binding', type=Path)
    args = parser.parse_args()
    if args.action == 'prepare': prepare(args.receipt, args.case)
    elif args.action == 'queue-exit': queue_exit(args.binding)
    else: retire(args.binding)
