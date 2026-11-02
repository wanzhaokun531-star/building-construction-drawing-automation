"""Plot only M1's supported 2000x1000 test panel; this is not the blocked whole wall."""
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
    case = session / 'case'
    report_path = case / 'native-case.json'
    report = read(report_path)
    assert report['caseName'] == 'NativeMaterialPreparation'
    assert report['nonce'] == ready['nonce'] == binding['nonce']
    assert report['pid'] == ready['processId']
    assert report['moduleSHA256'] == binding['candidateSHA256']
    assert ready['desktop'] == 'CodexS1_' + binding['nonce'] != ready['inputDesktop']
    assert ready['error'] is None
    assert not (session / 'isolation-final.json').exists()
    assert not report['sourceDrawingWritten']
    for item in binding['inputs']:
        for key in ('source', 'snapshot', 'inspectionCopy'):
            assert sha(item[key]) == item['SHA256']
    source = case / 'material-roundtrip.dwg'
    assert source.is_file()
    job = session / 'material-small-native-preview'
    job.mkdir()
    control = job / 'control.dwg'
    shutil.copyfile(source, control)
    source_sha = sha(source)
    assert sha(control) == source_sha
    # The small panel is an independent capability control, not the 58.2M wall.
    (job / 'annotation-index.tsv').write_text(
        'S2DIMPREVIEW3 0 1 -200 -200 2200 1200 594 420\n', encoding='ascii')
    module = ROOT / '阶段1/原生接口探测/run_quiet_job.py'
    spec = importlib.util.spec_from_file_location('bce_material_native_dispatch', module)
    queue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(queue)
    manifest = dict(contract='bce.material-small-natural-preview.v1', binding=str(binding_path),
                    bindingSHA256=sha(binding_path), nonce=binding['nonce'], pid=report['pid'],
                    nativeReport=str(report_path), nativeReportSHA256=sha(report_path),
                    source=str(source), sourceSHA256=source_sha, control=str(control),
                    helperSHA256=sha(__file__), dispatcherSHA256=sha(module), error=None,
                    scope='Owned M1 fixture only; normal host drawing, not collector reconstruction or project output')
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
        assert plot['initialModelCount'] == plot['finalModelCount'] == 1
        assert plot['originalBefore'][0]['type'] == 'AcDbHatch'
        assert plot['originalBefore'][0]['min'] == [0, 0, 0]
        assert plot['originalBefore'][0]['max'] == [2000, 1000, 0]
        assert plot['originalBefore'] == plot['originalAfter']
        assert plot['globalsBefore'] == plot['globalsAfter']
        assert plot['layoutBefore'] == plot['layoutAfter']
        assert sha(source) == sha(control) == source_sha
        pdf = Path(plot['page']['outputPDF'])
        manifest.update(nativePlotReport=str(plot_path), nativePlotReportSHA256=sha(plot_path),
                        PDF=str(pdf), PDFSHA256=sha(pdf), controlSHA256=sha(control),
                        drawingStateUnchanged=True)
        print(json.dumps(dict(PDF=str(pdf), PDFSHA256=sha(pdf), plotReport=str(plot_path))))
    except BaseException as error:
        manifest['error'] = repr(error)
        raise
    finally:
        (job / 'material-natural-preview.json').write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')

if __name__ == '__main__':
    main(sys.argv[1])
