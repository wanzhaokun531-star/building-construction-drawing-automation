"""Development only: decode existing N9a PNGs; call C++; reuse frozen full_cells.

No rendering, PDF parsing, CAD, production qualification or geometric margin.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import subprocess

import numpy as np
from PIL import Image

ROOT = next(p for p in Path(__file__).resolve().parents
            if (p / '立面插件').is_dir() and (p / '阶段1/原生接口探测').is_dir())
PLUGIN = ROOT / '立面插件'
CASE = ROOT / '阶段1/runs/elevation-native-20261103-093743-84f838a5/case'
IMAGES = CASE / 'text-range-public-measured-v8'
COMPARATOR = PLUGIN / 'tests/native/text-range/offline-projection-v8/verify_text_range.py'
NATIVE_SHA = '05254B213DC3F1B0C5D20DF1F0C352E8D12D9281DF68B9180D010800EECF6CD1'
COMPARATOR_SHA = '9296A063343919A13C3A954CE855E239C706254A55063AB8B431DF21857B27F3'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def binding(path):
    return dict(path=str(Path(path).resolve()), sha256=sha(path))


def run_adapter(exe, work, rgb, matrix, dpi, identities):
    # RGB order, opaque white background, tight positive rows; byte offset
    # (y*width+x)*3. No overlay and no selective sampling enters this adapter.
    height, width, channels = rgb.shape
    assert channels == 3 and rgb.dtype == np.uint8
    raw_path = work / 'input.rgb'
    raw_path.write_bytes(rgb.tobytes(order='C'))
    command = [str(exe), raw_path.name, str(width), str(height), str(width*3),
               *[format(float(n), '.17g') for n in matrix.reshape(-1)], str(dpi),
               *identities, '0', '0']
    executed = subprocess.run(command, cwd=work, capture_output=True, text=True, check=False)
    assert executed.returncode == 0, (executed.returncode, executed.stderr)
    result = json.loads(executed.stdout)
    raw_binding = dict(sha256=sha(raw_path), bytes=raw_path.stat().st_size,
                       width=width, height=height, rowStride=width*3)
    raw_path.unlink()  # This single owned file only; no directory cleanup.
    assert result['kind'] == 'observed device footprint'
    assert np.array_equal(np.asarray(result['worldToPixel']).reshape(2, 3), matrix)
    assert [result['context'][key] for key in
            ('deviceIdentity', 'frameIdentity', 'fontIdentity', 'sourceIdentity')] == identities
    assert result['context']['dpi'] == [dpi, dpi]
    assert result['context']['pixelBoundaryPhase'] == [0, 0]
    return result, raw_binding


def project(corners, matrix):
    points = np.asarray(corners, dtype=float)
    assert points.shape == (4, 2) and np.isfinite(points).all()
    return np.column_stack((points, np.ones(4))) @ matrix.T


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    receipt_path = args.receipt.resolve(strict=True)
    receipt = read(receipt_path)
    assert receipt['exitCode'] == receipt['buildExitCode'] == receipt['testExitCode'] == 0
    build = receipt_path.parent
    for entry in receipt['inputs']:
        assert sha(build/'input'/entry['path']) == entry['sha256']
    assert sha(build/'input/tools/verify_text_occupancy.py') == sha(__file__)
    exe = build/'out/TextOccupancyAdapter.exe'
    recorded = next(e for e in receipt['outputs'] if Path(e['path']).name == exe.name)
    assert sha(exe) == recorded['sha256']
    assert sha(CASE/'text-range.json') == NATIVE_SHA
    assert sha(COMPARATOR) == COMPARATOR_SHA
    assert sha(PLUGIN/'tools/verify_text_range.py') == COMPARATOR_SHA
    spec = importlib.util.spec_from_file_location('n9a_frozen_full_cells', COMPARATOR)
    old = importlib.util.module_from_spec(spec)
    # Execute the unchanged comparator source without writing __pycache__ into
    # its frozen evidence directory.
    exec(compile(COMPARATOR.read_bytes(), str(COMPARATOR), 'exec'), old.__dict__)
    native = read(CASE/'text-range.json')
    previous = read(IMAGES/'comparison.json')
    assert previous['nativeReportSHA256'] == NATIVE_SHA
    assert previous['comparatorSHA256'] == COMPARATOR_SHA
    assert len(previous['comparisons']) == 10
    assert sum(e['renderContainmentVerified'] for e in previous['comparisons']) == 0
    diagnostics = CASE/'text-range-backward-basis-diagnostic-v8/diagnostic.json'
    diagnostic_binding = binding(diagnostics)
    measured = np.asarray(previous['measuredProjection']['worldToPageMeasured'], dtype=float)
    assert measured.shape == (2, 3)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    samples = {s['id']: s for s in native['samples']}
    results = []
    for prior in previous['comparisons']:
        name, dpi = prior['id'], prior['dpi']
        image_path = IMAGES/f'{name}-{dpi}-ink.png'
        assert sha(image_path) == prior['inkSHA256']
        with Image.open(image_path) as source:
            assert source.mode == 'RGB', 'expected original opaque RGB native render'
            rgb = np.asarray(source.convert('RGB')).copy()
        yy, xx = np.nonzero(np.any(rgb < 255, axis=2))
        assert xx.size == prior['inkPixels'] > 0
        assert xx.min() > 0 and yy.min() > 0 and xx.max() < rgb.shape[1]-1 and yy.max() < rgb.shape[0]-1
        matrix = measured*(dpi/72)
        sample = samples[name]
        identities = ['Poppler-'+previous['rasterizerSHA256'], prior['inkSHA256'],
                      sample['fontSHA256']+'-'+sample['bigfontSHA256'], NATIVE_SHA+'-'+name]
        cpp, raw = run_adapter(exe, output, rgb, matrix, dpi, identities)
        expected_bounds = [[int(xx.min()), int(yy.min())], [int(xx.max()+1), int(yy.max()+1)]]
        assert cpp['inkPixels'] == xx.size and cpp['deviceCellBounds'] == expected_bounds
        polygon = project(cpp['worldCorners'], matrix)
        inside, distances = old.full_cells(polygon, xx, yy)
        assert inside.all(), (name, dpi, int((~inside).sum()))
        world = np.asarray(cpp['worldCorners'])
        assert np.array_equal(cpp['worldBounds'], [world.min(axis=0).tolist(), world.max(axis=0).tolist()])
        negatives = []
        shrunk = world.mean(axis=0)+(world-world.mean(axis=0))*0.35
        bad, bad_dist = old.full_cells(project(shrunk, matrix), xx, yy)
        assert not bad.all(), 'independent comparator accepted a 35% result'
        negatives.append(dict(kind='shrink-world-result-to-35-percent', outsidePixels=int((~bad).sum()), maxOutsidePixel=float(bad_dist.max())))
        # Both are finite, invertible mappings accepted by C++; the independent
        # comparator must reject their WORLD result under the actual mapping.
        wrong = matrix.copy(); wrong[:, 2] += [50, -30]
        for label, incorrect in [('wrong-mapping-translation', wrong), ('omitted-worldToPixel', np.array([[1., 0., 0.], [0., 1., 0.]]))]:
            wrong_cpp, _ = run_adapter(exe, output, rgb, incorrect, dpi, identities)
            bad, bad_dist = old.full_cells(project(wrong_cpp['worldCorners'], matrix), xx, yy)
            assert not bad.all(), (label, 'independent comparator failed to reject')
            negatives.append(dict(kind=label, outsidePixels=int((~bad).sum()), maxOutsidePixel=float(bad_dist.max())))
        results.append(dict(id=name, dpi=dpi, image=binding(image_path), decodedRGB=raw,
                            cpp=cpp, independentlyCheckedInkPixels=int(xx.size), outsidePixels=int((~inside).sum()),
                            maxOutsidePixel=float(distances.max()), negativeControls=negatives,
                            preservedOriginalRangeOutsidePixels=prior['outsidePixels']))
        print(f'{name} {dpi}dpi: {xx.size} ink; cells={expected_bounds}; outside=0; 3 negatives rejected')
    result = dict(contract='bce.text-occupancy-development-check.v1', kind='observed device footprint',
                  buildReceipt=binding(receipt_path), executable=binding(exe), verifier=binding(__file__),
                  nativeReport=binding(CASE/'text-range.json'), preservedN9aComparison=binding(IMAGES/'comparison.json'),
                  preservedBackwardDiagnostic=diagnostic_binding, comparator=binding(COMPARATOR),
                  sources=[dict(path=e['path'], sha256=e['sha256']) for e in receipt['inputs']],
                  pixelContract='RGB any channel <255; complete [x,x+1] by [y,y+1]; y down; top-left boundary phase [0,0]',
                  numericalComparisonTolerancePixel=1e-8, geometricMarginPixel=0,
                  scope='Exact existing 5 native scenes x 2 finite rasters and measured mappings; external scene completeness and Layout binding remain unestablished.',
                  results=results, summary=dict(images=10, containedObservations=10, rejectedNegativeResults=30,
                                               preservedHistoricalStringRangeContainments=0))
    with (output/'comparison.json').open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2, allow_nan=False)
    print('10/10 observed footprints independently contained; 30/30 incorrect results rejected; historical range remains 0/10')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
