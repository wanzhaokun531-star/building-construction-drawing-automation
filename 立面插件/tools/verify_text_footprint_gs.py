"""Offline N9c evidence verifier; never used by the ARX runtime."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path


def require(ok, message):
    if not ok:
        raise ValueError(message)


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def apply(m, p):
    a, b, tx, c, d, ty = m
    return a * p[0] + b * p[1] + tx, c * p[0] + d * p[1] + ty


def inside(point, polygon):
    signs = []
    for i in range(4):
        a, b = polygon[i], polygon[(i + 1) % 4]
        signs.append((b[0] - a[0]) * (point[1] - a[1]) -
                     (b[1] - a[1]) * (point[0] - a[0]))
    return min(signs) >= -1e-8 or max(signs) <= 1e-8


def verify_frame(frame, rgb, calibration_map):
    w, h = frame['width'], frame['height']
    require(frame['rowStride'] == w * 3 and len(rgb) == w * h * 3, 'RGB shape')
    m = frame['worldToPixel']
    require(len(m) == 6 and all(math.isfinite(x) for x in m), 'mapping missing/nonfinite')
    require(max(abs(a - b) for a, b in zip(m, calibration_map)) <= 1e-10,
            'mapping differs from independent calibration')
    polygon = frame['worldCorners']
    require(len(polygon) == 4, 'quadrilateral shape')
    lo, hi = frame['deviceCellBounds']
    expected = [(lo[0], lo[1]), (hi[0], lo[1]), (hi[0], hi[1]), (lo[0], hi[1])]
    for p, e in zip(polygon, expected):
        v = apply(m, p)
        require(max(abs(v[k] - e[k]) for k in (0, 1)) <= 1e-8, 'corner/mapping mismatch')
    determinant = m[0] * m[4] - m[1] * m[3]
    require(abs(determinant) > 1e-12, 'singular mapping')

    def inverse(p):
        x, y = p[0] - m[2], p[1] - m[5]
        return ((m[4] * x - m[1] * y) / determinant,
                (-m[3] * x + m[0] * y) / determinant)

    ink, minx, miny, maxx, maxy = 0, w, h, -1, -1
    for y in range(h):
        for x in range(w):
            k = (y * w + x) * 3
            if rgb[k:k+3] == b'\xff\xff\xff':
                continue
            ink += 1
            minx, miny, maxx, maxy = min(minx, x), min(miny, y), max(maxx, x), max(maxy, y)
            for cell in ((x, y), (x+1, y), (x+1, y+1), (x, y+1)):
                require(inside(inverse(cell), polygon), 'nonwhite complete cell outside C++ world quad')
    require(ink == frame['inkPixels'] and ink > 0, 'ink count')
    require(minx > 0 and miny > 0 and maxx < w-1 and maxy < h-1, 'blank or edge')
    require(lo == [minx, miny] and hi == [maxx+1, maxy+1], 'complete device bounds')
    xs, ys = [p[0] for p in polygon], [p[1] for p in polygon]
    expected_aabb = [[min(xs), min(ys)], [max(xs), max(ys)]]
    require(all(abs(frame['worldAABB'][i][j]-expected_aabb[i][j]) <= 1e-10
                for i in (0, 1) for j in (0, 1)), 'world AABB')
    return {'inkPixels': ink, 'allNonwhiteFullCellsCovered': True}


def negatives(frame, rgb, m):
    results = []
    for name in ('shrunken-quad', 'missing-mapping', 'wrong-mapping'):
        bad = copy.deepcopy(frame)
        if name == 'shrunken-quad':
            # Preserve internal corner/mapping consistency, but omit part of the observed cells.
            lo, hi = bad['deviceCellBounds']
            lo[0] += .25
            a, b, tx, c, d, ty = m
            det = a*d-b*c
            def inv(p):
                x, y = p[0]-tx, p[1]-ty
                return [(d*x-b*y)/det, (-c*x+a*y)/det]
            bad['worldCorners'] = [inv(p) for p in ((lo[0], lo[1]), (hi[0], lo[1]),
                                                   (hi[0], hi[1]), (lo[0], hi[1]))]
            bad['worldAABB'] = [[min(p[k] for p in bad['worldCorners']) for k in (0, 1)],
                                [max(p[k] for p in bad['worldCorners']) for k in (0, 1)]]
        elif name == 'missing-mapping':
            del bad['worldToPixel']
        else:
            bad['worldToPixel'][2] += 3
        try:
            verify_frame(bad, rgb, m)
        except (ValueError, KeyError) as e:
            results.append({'name': name, 'rejected': True, 'reason': str(e)})
        else:
            raise ValueError('negative control accepted: '+name)
    return results


def verify_directory(folder):
    result = load(folder / 'gs-result.json')
    require(result['status'] == 'native-buffer-complete', 'native GS execution did not complete')
    require(result['cleanupComplete'] and result['ambientPointersUnchanged'], 'cleanup/ambient identity')
    require(all(v == 0 for v in result['ambientWriteEvents'].values()), 'ambient write event')
    require(len(result['frames']) == 10, 'expected 2 x 5 frames')
    checks = []
    for frame in result['frames']:
        path = Path(frame['rgbPath'])
        require(path.parent.resolve() == folder.resolve(), 'raw RGB must be in this case output')
        rgb = path.read_bytes()
        require(hashlib.sha256(rgb).hexdigest().upper() == frame['rgbSHA256'], 'RGB SHA')
        calibration = load(folder / ('calibration-%d.json' % frame['width']))
        m = calibration['selectedWorldToPixel']
        require(calibration['selectionMaxResidual'] <= .55 and
                calibration['heldOutMaxResidual'] <= .55, 'calibration residual')
        verified = verify_frame(frame, rgb, m)
        checks.append({'id': frame['id'], **verified, 'negativeControls': negatives(frame, rgb, m)})
    return {'status': 'passed', 'checks': checks, 'glyphComplete': False, 'layoutIntegrated': False}


def self_test():
    # Verifier-only fixture. Not a native test or a second N9b implementation.
    rgb = bytearray(b'\xff' * 8 * 8 * 3)
    for y in (2, 3):
        for x in (2, 3, 4):
            rgb[(y*8+x)*3:(y*8+x)*3+3] = b'\0\0\0'
    f = dict(width=8, height=8, rowStride=24, worldToPixel=[1, 0, 0, 0, 1, 0],
             worldCorners=[[2, 2], [5, 2], [5, 4], [2, 4]],
             worldAABB=[[2, 2], [5, 4]], deviceCellBounds=[[2, 2], [5, 4]], inkPixels=6)
    positive = verify_frame(f, bytes(rgb), f['worldToPixel'])
    return {'kind': 'offline-verifier-only', 'positive': positive,
            'negativeControls': negatives(f, bytes(rgb), f['worldToPixel'])}


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('case_output', nargs='?', type=Path)
    p.add_argument('--self-test', action='store_true')
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    try:
        require(args.self_test or args.case_output is not None, 'case_output or --self-test required')
        report = self_test() if args.self_test else verify_directory(args.case_output)
        code = 0
    except (ValueError, KeyError, OSError) as e:
        report, code = {'status': 'failed', 'error': str(e)}, 1
    text = json.dumps(report, ensure_ascii=False, indent=2)
    if args.output:
        require(not args.output.exists(), 'output already exists')
        args.output.write_text(text+'\n', encoding='utf-8')
    print(text)
    raise SystemExit(code)
