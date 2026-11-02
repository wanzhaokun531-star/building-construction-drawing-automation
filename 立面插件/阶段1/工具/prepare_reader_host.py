"""Prepare a new exclusive reader/preview host without changing shared pointers."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
import sys
import uuid

ROOT = Path(__file__).resolve().parents[3]
PHASE = ROOT / '立面插件/阶段1'
P = ROOT / '阶段1/原生接口探测'
S = ROOT / '阶段1'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def main(receipt_path):
    candidate = read(receipt_path)
    assert candidate['exitCode'] == 0 and sha(candidate['binary']) == candidate['binarySHA256']
    assert sha(candidate['source']) == candidate['sourceSHA256']
    for path, digest in candidate.get('sourceFiles', {}).items():
        assert sha(path) == digest, ('reader source differs', path)
    bridge_build = read(P / 'evidence/build-product-bridge-v5-20261101.json')
    bridge_binary = P / 'bin/NativeAccessProbe.arx'
    assert sha(bridge_binary) == bridge_build['binarySHA256']
    for name, digest in bridge_build['sourceFiles'].items():
        assert sha(P / 'src' / name) == digest, ('bridge source differs', name)
    support = read(P / 'evidence/autocad-support-profile-20261029.json')
    assert sha(support['componentPath']) == support['componentSHA256']
    frozen = read(ROOT / 'outputs/elevation-reading-20261101/reader-binding.json')['inputs']
    for item in frozen:
        assert sha(item['source']) == sha(item['snapshot']) == item['SHA256']
    nonce = uuid.uuid4().hex
    session = S / 'runs' / ('elevation-stage1-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + nonce[:8])
    session.mkdir()
    (session / 'queue').mkdir()
    (session / 'capture').mkdir()
    inputs = []
    for item in frozen:
        copied = session / (item['role'] + '-input.dwg')
        shutil.copyfile(item['snapshot'], copied)
        assert sha(copied) == item['SHA256']
        inputs.append(dict(item, inspectionCopy=str(copied)))
    staging = Path('C:/Program Files/Autodesk/ApplicationPlugins') / ('CodexElevationStage1_' + nonce)
    staging.mkdir()
    bridge = staging / ('S1Read_' + nonce[:8] + '.arx')
    reader = staging / ('BCE1Read_' + nonce[:8] + '.arx')
    shutil.copyfile(bridge_binary, bridge)
    shutil.copyfile(candidate['binary'], reader)
    bridge_ini = bridge.with_suffix('.ini')
    bridge_ini.write_text(
        f'[probe]\nroot={S}\nscheme={frozen[0]["snapshot"]}\nreference={frozen[1]["snapshot"]}\noutput={session}\n'
        f'[worker]\nnonce={nonce}\nsession={session}\nexpectedProfile={support["profile"]}\n'
        f'supportComponent={support["componentPath"]}\nearlyNativeAutoload=1\n', encoding='utf-16')
    reader_ini = reader.with_suffix('.ini')
    reader_ini.write_text(
        f'[inspect]\nnonce={nonce}\nsession={session}\nscheme={inputs[0]["inspectionCopy"]}\n'
        f'reference={inputs[1]["inspectionCopy"]}\noutput={session / "capture"}\n', encoding='utf-16')
    boot = session / 'boot.scr'
    boot.write_bytes(('_.ARX\r\n_L\r\n"' + bridge.as_posix() + '"\r\nS1QUIETSTART\r\n'
                      '_.ARX\r\n_L\r\n"' + reader.as_posix() + '"\r\nBCE1_CAPTURE\r\n').encode('ascii'))
    template = Path('C:/Users/PC/AppData/Local/Autodesk/AutoCAD 2020/R23.1/chs/Template/acadiso.dwt')
    assert template.is_file()
    binding = dict(contract='bce.stage1.reader-binding.v1', session=str(session), nonce=nonce, profile=support['profile'],
                   earlyNativeAutoload=True,
                   template=str(template), stagedARX=str(bridge), stagedINI=str(bridge_ini),
                   stagingDirectory=str(staging), binarySHA256=sha(bridge), sourceFiles=bridge_build['sourceFiles'],
                   readerARX=str(reader), readerINI=str(reader_ini), readerSHA256=sha(reader),
                   readerSource=candidate['source'], readerSourceSHA256=candidate['sourceSHA256'],
                   readerSourceFiles=candidate.get('sourceFiles', {}),
                   buildReceipt=str(Path(receipt_path).resolve()), inputs=inputs, boot=str(boot),
                   captureReport=str(session / 'capture/reference-inspection.json'),
                   scope='Stage1 only: read source copies, collect standards and existing native previews; no business writes')
    (session / 'worker-config.json').write_text(json.dumps(binding, ensure_ascii=False, indent=2), encoding='utf-8')
    output = PHASE / '数据' / ('reader-binding-' + nonce[:8] + '.json')
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(binding, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(binding=str(output), session=str(session), nonce=nonce), ensure_ascii=False))


if __name__ == '__main__':
    main(sys.argv[1])
