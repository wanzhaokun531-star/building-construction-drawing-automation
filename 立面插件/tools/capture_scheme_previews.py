"""Read-only native plots of six existing source views, not generated elevations."""
from pathlib import Path
import importlib.util
import json
import subprocess
import sys
import hashlib

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main(binding_path):
    binding = read(binding_path)
    session = Path(binding['session'])
    ready = read(session / 'quiet-worker-ready.json')
    assert binding['nonce'] == ready['nonce'] and ready['error'] is None
    fixture_path = ROOT / '立面插件/tests/fixtures/scheme-scene-manifest.json'
    fixture = read(fixture_path)
    source = next(i for i in binding['inputs'] if i['role'] == 'scheme')
    assert sha(source['inspectionCopy']) == fixture['schemeSHA256'] == source['SHA256']
    spec = importlib.util.spec_from_file_location('bce_native_preview_dispatch', ROOT / '阶段1/原生接口探测/run_quiet_job.py')
    queue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(queue)
    manifest = dict(contract='bce.scheme-native-previews.v1', session=str(session), nonce=binding['nonce'],
                    sourceSHA256=source['SHA256'], fixtureManifest=str(fixture_path), fixtureManifestSHA256=sha(fixture_path),
                    pages=[], error=None, scope='Plot existing scheme views only; not production generation or semantic recognition proof')
    target = session / 'scheme-native-previews.json'
    marked = False
    try:
        for view in fixture['expectedViews']:
            job = session / ('scheme-view-' + view['role'])
            job.mkdir()
            control = job / 'control.dwg'
            control.write_bytes(Path(source['inspectionCopy']).read_bytes())
            assert sha(control) == source['SHA256']
            lo_x, lo_y, hi_x, hi_y = view['bounds']
            window = [lo_x - 800, lo_y - 800, hi_x + 800, hi_y + 800]
            (job / 'annotation-index.tsv').write_text('S2DIMPREVIEW3 0 741 ' + ' '.join(format(x, '.17g') for x in window) + ' 594 420\n', encoding='ascii')
            queue.dispatch(session, ready, 'S1OPENPROBECONTROL', job, 'native-open.done', 120)
            if not marked:
                subprocess.run([
                    'C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe',
                    'C:/Users/PC/.codex/plugins/cache/openai-primary-runtime/pdf/26.1026.10807/skills/pdf/container_tools/mark_artifact_operation_started.mjs',
                    '--operation-kind', 'create', '--expected-output-count', '6', '--output-format', 'pdf'], check=True)
                marked = True
            queue.dispatch(session, ready, 'S1PHASE2PREVIEW', job, 'stage2-preview-native.json', 180)
            report_path = job / 'stage2-preview-native.json'
            report = read(report_path)
            assert report['error'] is None and report['page']['error'] is None
            assert report['initialModelCount'] == report['finalModelCount'] == 741
            assert report['originalBefore'] == report['originalAfter']
            assert report['globalsBefore'] == report['globalsAfter'] and report['layoutBefore'] == report['layoutAfter']
            assert sha(control) == source['SHA256']
            actual = report['page']['validatedSettings']
            pw, ph = actual['paperMM']
            left, bottom, right, top = actual['marginsMM']
            printable = [pw - left - right, ph - bottom - top]
            if actual['rotation'] == 1: printable.reverse()
            needed = [(window[2] - window[0]) / 100, (window[3] - window[1]) / 100]
            assert all(a <= b + 1e-6 for a, b in zip(needed, printable))
            pdf = Path(report['page']['outputPDF'])
            manifest['pages'].append(dict(role=view['role'], kind=view['kind'], titleHandle=view['titleHandle'],
                                          windowWCS=window, diagnosticScale=[1, 100], printableMM=printable, neededMM=needed,
                                          PDF=str(pdf), PDFSHA256=sha(pdf), control=str(control), controlSHA256=sha(control),
                                          nativeReport=str(report_path), nativeReportSHA256=sha(report_path)))
            target.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
            print(json.dumps(dict(role=view['role'], PDF=str(pdf), geometryUnchanged=True)), flush=True)
    except BaseException as failure:
        manifest['error'] = repr(failure)
        raise
    finally:
        target.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main(sys.argv[1])
