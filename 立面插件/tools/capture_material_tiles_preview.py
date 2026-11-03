"""Plot the owned M2 whole-wall fixture using its actual verified tile count."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main(binding_path):
    binding_path = Path(binding_path).resolve(strict=True)
    binding = read(binding_path)
    session = Path(binding['session']).resolve(strict=True)
    assert session.is_relative_to((ROOT / '阶段1/runs').resolve())
    ready = read(session / 'quiet-worker-ready.json')
    report_path = session / 'case/native-case.json'
    report = read(report_path)
    assert binding['caseName'] == report['caseName'] == 'NativeMaterialTiles'
    assert report['status'] == 'passed' and not report['errors']
    assert report['steps'] and all(s['status'] == 'passed' for s in report['steps'])
    assert report['nonce'] == ready['nonce'] == binding['nonce']
    assert report['pid'] == ready['processId']
    assert report['moduleSHA256'] == binding['candidateSHA256']
    assert ready['desktop'] == 'CodexS1_' + binding['nonce'] != ready['inputDesktop']
    assert ready['error'] is None and not report['sourceDrawingWritten']
    assert not (session / 'isolation-final.json').exists()
    for item in binding['inputs']:
        for key in ('source', 'snapshot', 'inspectionCopy'):
            assert sha(item[key]) == item['SHA256']
    walls = [c for c in report['capabilities'] if c.get('scenario') == 'WholeWall58M']
    assert len(walls) == 1
    wall = walls[0]
    tiles = wall['tiles']
    count = len(tiles)
    assert count > 1 and wall['nativeArea'] == 58200000
    assert wall['seams'] > 0 and wall['contacts'] > 0
    assert [t['tile'] for t in tiles] == list(range(count))
    assert all(t['area'] > 0 and t['lines'] > 0 for t in tiles)
    assert sum(t['lines'] for t in tiles) == wall['nativeLines']
    source = session / 'case/material-tiles-0.dwg'
    source_sha = sha(source)
    job = session / 'material-tiles-native-preview'
    job.mkdir()
    control = job / 'control.dwg'
    shutil.copyfile(source, control)
    assert sha(control) == source_sha
    (job / 'annotation-index.tsv').write_text(
        f'S2DIMPREVIEW3 0 {count} -500 -500 10500 6500 594 420\n', encoding='ascii')
    module = ROOT / '阶段1/原生接口探测/run_quiet_job.py'
    spec = importlib.util.spec_from_file_location('bce_material_tiles_dispatch', module)
    queue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(queue)
    manifest = dict(contract='bce.material-tiles-natural-preview.v1', binding=str(binding_path),
                    bindingSHA256=sha(binding_path), nonce=binding['nonce'], pid=report['pid'],
                    nativeReport=str(report_path), nativeReportSHA256=sha(report_path),
                    source=str(source), sourceSHA256=source_sha, control=str(control),
                    helperSHA256=sha(__file__), dispatcherSHA256=sha(module),
                    tileCount=count, nativeWall=wall, error=None,
                    scope='Owned M2 whole-wall fixture; actual host PlotSDK; internal visual verification')
    try:
        queue.dispatch(session, ready, 'S1OPENPROBECONTROL', job, 'native-open.done', 120)
        subprocess.run([
            'C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe',
            'C:/Users/PC/.codex/plugins/cache/openai-primary-runtime/pdf/26.1026.10807/skills/pdf/container_tools/mark_artifact_operation_started.mjs',
            '--operation-kind', 'create', '--expected-output-count', '1', '--output-format', 'pdf'], check=True)
        queue.dispatch(session, ready, 'S1PHASE2PREVIEW', job, 'stage2-preview-native.json', 180)
        plot_path = job / 'stage2-preview-native.json'
        plot = read(plot_path)
        assert plot['error'] is None and plot['page']['error'] is None
        assert plot['initialModelCount'] == plot['finalModelCount'] == count
        assert len(plot['originalBefore']) == count
        assert all(r['type'] == 'AcDbHatch' for r in plot['originalBefore'])
        actual_boxes = sorted(tuple(r['min'][:2] + r['max'][:2]) for r in plot['originalBefore'])
        assert actual_boxes == sorted(tuple(t['box']) for t in tiles)
        assert plot['originalBefore'] == plot['originalAfter']
        assert plot['globalsBefore'] == plot['globalsAfter']
        assert plot['layoutBefore'] == plot['layoutAfter']
        assert sha(source) == sha(control) == source_sha
        for item in binding['inputs']:
            for key in ('source', 'snapshot', 'inspectionCopy'):
                assert sha(item[key]) == item['SHA256']
        pdf = Path(plot['page']['outputPDF'])
        manifest.update(nativePlotReport=str(plot_path), nativePlotReportSHA256=sha(plot_path),
                        PDF=str(pdf), PDFSHA256=sha(pdf), controlSHA256=sha(control),
                        drawingStateUnchanged=True)
        print(json.dumps(dict(PDF=str(pdf), PDFSHA256=sha(pdf), plotReport=str(plot_path), tileCount=count)))
    except BaseException as error:
        manifest['error'] = repr(error)
        raise
    finally:
        (job / 'material-tiles-natural-preview.json').write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main(sys.argv[1])
