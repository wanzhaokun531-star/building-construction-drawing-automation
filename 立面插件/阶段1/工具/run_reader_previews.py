"""Use the unchanged native preview command on exact owned copies, never originals."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
PHASE = ROOT / '立面插件/阶段1'
P = ROOT / '阶段1/原生接口探测'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def main(binding_path):
    binding = read(binding_path)
    session = Path(binding['session'])
    deadline = time.monotonic() + 180
    capture = None
    while capture is None:
        try:
            capture = read(session / 'capture/reference-inspection.json')
        except (OSError, ValueError):
            pass
        if capture is not None:
            break
        if time.monotonic() > deadline:
            raise TimeoutError('read-only capture has not closed a complete readable report')
        time.sleep(.25)
    ready = read(session / 'quiet-worker-ready.json')
    assert ready['nonce'] == binding['nonce'] == capture['nonce']
    assert ready['timerStarted'] and ready['error'] is None
    assert not capture['sourceDrawingWritten'] and not capture['businessWriteAttempted']
    spec = importlib.util.spec_from_file_location('owned_stage1_queue', P / 'run_quiet_job.py')
    queue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(queue)
    result = dict(contract='bce.stage1.reference-preview.v1', session=str(session), nonce=binding['nonce'],
                  capture=str(session / 'capture/reference-inspection.json'), captureError=capture.get('error'),
                  pages=[], fonts=None, error=None)
    output = session / 'stage1-preview-run.json'
    try:
        fonts = session / 'fonts'
        fonts.mkdir()
        queue.dispatch(session, ready, 'S1ACTUALFONTS', fonts, 'native-actual-fonts.json', 120)
        actual_fonts = read(fonts / 'native-actual-fonts.json')
        assert actual_fonts['error'] is None
        result['fonts'] = dict(path=str(fonts / 'native-actual-fonts.json'), SHA256=sha(fonts / 'native-actual-fonts.json'))
        input_item = next(x for x in binding['inputs'] if x['role'] == 'reference')
        source = Path(input_item['inspectionCopy'])
        assert sha(source) == input_item['SHA256']
        baseline = read(ROOT / '阶段1/runs/elevation-readonly-20261101-122344/input-version-audit.json')
        reference = baseline['sources'][1]
        frames = [x['geometry'] for x in reference['entities']
                  if x['geometry']['layer'] == '图框固定块' and x['geometry']['type'] == 'AcDbPolyline']
        assert len(frames) == 4
        marked = False
        for frame in frames:
            job = session / ('view-' + frame['handle'])
            job.mkdir()
            control = job / 'control.dwg'
            control.write_bytes(source.read_bytes())
            assert sha(control) == input_item['SHA256']
            window = [frame['min'][0] - 500, frame['min'][1] - 500,
                      frame['max'][0] + 500, frame['max'][1] + 500]
            (job / 'annotation-index.tsv').write_text(
                'S2DIMPREVIEW3 0 29535 ' + ' '.join(format(v, '.17g') for v in window) + ' 594 420\n', encoding='ascii')
            queue.dispatch(session, ready, 'S1OPENPROBECONTROL', job, 'native-open.done', 120)
            if not marked:
                node = Path('C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe')
                marker = Path('C:/Users/PC/.codex/plugins/cache/openai-primary-runtime/pdf/26.1026.10807/skills/pdf/container_tools/mark_artifact_operation_started.mjs')
                subprocess.run([str(node), str(marker), '--operation-kind', 'create', '--expected-output-count', '4', '--output-format', 'pdf'], check=True)
                marked = True
            queue.dispatch(session, ready, 'S1PHASE2PREVIEW', job, 'stage2-preview-native.json', 180)
            report = read(job / 'stage2-preview-native.json')
            assert report['error'] is None and report['page']['error'] is None
            assert report['initialModelCount'] == report['finalModelCount'] == 29535
            assert report['originalBefore'] == report['originalAfter']
            assert report['globalsBefore'] == report['globalsAfter'] and report['layoutBefore'] == report['layoutAfter']
            assert sha(control) == input_item['SHA256']
            actual = report['page']['validatedSettings']
            width, height = actual['paperMM']
            left, bottom, right, top = actual['marginsMM']
            printable = [width - left - right, height - bottom - top]
            if actual['rotation'] == 1:
                printable.reverse()
            demand = [(window[2] - window[0]) / 100, (window[3] - window[1]) / 100]
            assert all(need <= have + 1e-6 for need, have in zip(demand, printable)), ('plot clipping risk', demand, printable)
            pdf = Path(report['page']['outputPDF'])
            result['pages'].append(dict(frameHandle=frame['handle'], job=str(job), control=str(control),
                                       sourceSHA256=input_item['SHA256'], windowWCS=window, diagnosticScale=[1, 100],
                                       printableMM=printable, windowPaperMM=demand, PDF=str(pdf), PDFSHA256=sha(pdf),
                                       nativeReport=str(job / 'stage2-preview-native.json'), nativeReportSHA256=sha(job / 'stage2-preview-native.json')))
            output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
            print(json.dumps(dict(frame=frame['handle'], PDF=str(pdf), fitsPrintable=True), ensure_ascii=False), flush=True)
    except BaseException as error:
        result['error'] = repr(error)
        raise
    finally:
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    main(sys.argv[1])
