"""Audit this frozen project's explicit floor datum; not a general recognition algorithm."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main():
    manifest_path = ROOT / '立面插件/tests/fixtures/scheme-scene-manifest.json'
    manifest = read(manifest_path)
    audit_path = Path(manifest['sourceAudit'])
    capture_path = Path(manifest['supplementCapture'])
    assert sha(audit_path) == manifest['sourceAuditSHA256']
    assert sha(capture_path) == manifest['supplementCaptureSHA256']
    geometry_source = next(x for x in read(audit_path)['sources'] if x['modelCount'] == 741)
    captured = next(x for x in read(capture_path)['sources'] if x['role'] == 'scheme')
    assert captured['publicFingerprintUnchanged'] and captured['error'] is None
    roots = {x['geometry']['handle']: x for x in geometry_source['entities']}
    observations = {x['handle']: x for x in captured['observations']}
    views = []
    for view in manifest['expectedViews']:
        if view['kind'] != 'elevation':
            continue
        members = [roots[h] for h in view['memberHandles']]
        lower = [x for x in members if x['nativeText']['value'] == '一层平面图 +0 mm']
        upper = [x for x in members if x['nativeText']['value'] == '二层平面图 +3000 mm']
        assert len(lower) == len(upper) == 1
        low_y, high_y = lower[0]['nativeText']['position'][1], upper[0]['nativeText']['position'][1]
        matched = []
        for root in members:
            handle = root['geometry']['handle']
            if root['geometry']['type'] != 'AcDbAlignedDimension':
                continue
            node = next(n for n in observations[handle]['nodes'] if n.get('kind') == 'dimension')
            a, b = node['definition']['xLine1Point'], node['definition']['xLine2Point']
            if a[1] != low_y or b[1] != high_y or a[0] != b[0] or a[2] != b[2]:
                continue
            assert node['measurement']['status'] == 0 and node['complete']
            # Comparison only. Keep source doubles and the independent SDK measurement verbatim.
            assert abs(node['measurement']['value'] - (high_y - low_y)) <= 1e-8
            matched.append((handle, a, b, node['measurement']['value']))
        assert len(matched) == 1
        floor_edges = []
        for root in members:
            geometry = root['geometry']
            if geometry['type'] != 'AcDbLine':
                continue
            curve = geometry['curve']
            if curve['start'][1] == low_y and curve['end'][1] == low_y and curve['length'] > 1:
                floor_edges.append(dict(handle=geometry['handle'], start=curve['start'], end=curve['end']))
        assert floor_edges
        handle, a, b, measurement = matched[0]
        views.append(dict(role=view['role'], titleHandle=view['titleHandle'],
                          datumPoint=a, datumSourceHandle=handle, datumGetter='AcDbAlignedDimension::xLine1Point',
                          upperPoint=b, upperGetter='AcDbAlignedDimension::xLine2Point',
                          SDKMeasurement=measurement, measuredDifference=high_y-low_y,
                          datumMeters=0.0, heightDirection=[0, 1],
                          labelHandles=[lower[0]['geometry']['handle'], upper[0]['geometry']['handle']],
                          corroboratingHorizontalSourceEdges=floor_edges))
    assert len(views) == 4
    confirmation_path = ROOT / '立面插件/inputs/current-project-confirmation.json'
    confirmation = read(confirmation_path)
    assert confirmation['zeroLevel']['confirmedByUser'] and confirmation['zeroLevel']['elevationMetres'] == 0
    result = dict(contract='bce.current-project-datum-audit.v1',
                  schemeSHA256=manifest['schemeSHA256'],
                  sourceAudit=str(audit_path), sourceAuditSHA256=sha(audit_path),
                  publicGetterCapture=str(capture_path), publicGetterCaptureSHA256=sha(capture_path),
                  viewFixture=str(manifest_path), viewFixtureSHA256=sha(manifest_path),
                  userConfirmationBeforeAuditSHA256=sha(confirmation_path),
                  floorRuleConfirmedByUser=True, datumWorldCoordinatesVerified=True, views=views,
                  drawingWritten=False,
                  scope='This frozen project only: user-approved first-floor zero associated with SDK dimension extension points and actual horizontal source edges; not text ink bounds; no plan-axis, opening-role or material confirmation inferred')
    result_path = ROOT / '立面插件/evidence/current-project-datum-audit.json'
    assert not result_path.exists()
    result_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    confirmation['zeroLevel'].update(worldDatumCoordinateVerified=True,
                                     evidence=str(result_path), evidenceSHA256=sha(result_path),
                                     scope='four source elevations; first-floor baseline only')
    confirmation_path.write_text(json.dumps(confirmation, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(views=len(views), verified=True, evidence=str(result_path)), ensure_ascii=False))


if __name__ == '__main__':
    main()
