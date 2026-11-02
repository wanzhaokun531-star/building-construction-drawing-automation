"""Verify the phase-one evidence package. Fixed counts bind this sample only."""
from pathlib import Path
import collections
import csv
import datetime
import hashlib
import json
from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[3]
PHASE = ROOT / '立面插件/阶段1'
DATA = PHASE / '数据'
OLD = ROOT / '阶段1/runs/elevation-stage1-20261101-162251-d748fbf0'
NEW = ROOT / '阶段1/runs/elevation-stage1-20261101-163350-07abbb82'


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def identity(path):
    p = Path(path).resolve(strict=True)
    return dict(path=str(p), SHA256=sha(p), bytes=p.stat().st_size)


def main():
    checks, files = [], []
    bindings = [read(DATA / ('reader-binding-' + n + '.json')) for n in ('d748fbf0', '07abbb82')]
    for binding in bindings:
        s = Path(binding['session'])
        raw = read(binding['captureReport'])
        ready = read(s / 'quiet-worker-ready.json')
        assert raw['nonce'] == ready['nonce'] == binding['nonce'] and raw['pid'] == ready['processId']
        assert raw['desktop'] == 'CodexS1_' + binding['nonce'] != ready['inputDesktop']
        assert not raw['sourceDrawingWritten'] and not raw['businessWriteAttempted']
        for source in raw['sources']:
            assert source['publicFingerprintUnchanged'] and source['before'] == source['after']
        for item in binding['inputs']:
            for key in ('source', 'snapshot', 'inspectionCopy'):
                assert sha(item[key]) == item['SHA256']
                files.append(identity(item[key]))
        receipt = read(binding['buildReceipt'])
        assert receipt['exitCode'] == 0 and sha(receipt['binary']) == binding['readerSHA256']
        for path, digest in receipt['sourceFiles'].items():
            compiled = Path(receipt['compiledSource']).parent / Path(path).name
            assert sha(compiled) == digest
            files.append(identity(compiled))
        files.extend(identity(p) for p in (receipt['binary'], receipt['log'], Path(receipt['compiledSource']).parent / 'ReferenceCapturePolicyTests.exe'))
        retirement = read(s / 'reader-retirement.json')
        final = read(s / 'isolation-final.json')
        assert retirement['nonce'] == final['nonce'] == binding['nonce']
        assert retirement['processId'] == raw['pid'] and len(retirement['removedFiles']) == 4
        assert retirement['stagingRemoved'] and not Path(binding['stagingDirectory']).exists()
        for item in retirement['removedFiles']:
            assert not Path(item['path']).exists() and sha(item['archive']) == item['SHA256']
        assert final['workerExited'] and final['desktopClosed'] and final['processHandlesClosed']
        assert final['workerForegroundEvents'] == final['workerUserDesktopShowEvents'] == final['inputDesktopReadErrors'] == 0
        assert final['monitorError'] is None and not final['supervisorErrors'] and not final['postExitWindowDiagnostics']
        assert final['forcedTermination'] and not final['nativeQuitConfirmed']
        events = [json.loads(line) for line in (s / 'isolation-related-cad-events.jsonl').read_text(encoding='utf-8-sig').splitlines()]
        host = read(s / 'isolation-host-receipt.json')
        assert host['relatedProcessIdsAtStart'] == [41908] and not host['metadataErrors']
        assert {e['Pid'] for e in events} == {41908} and all(e['Pid'] != raw['pid'] for e in events)
        checks.append(dict(name='owned-session-protection-and-retirement', nonce=binding['nonce'], processId=raw['pid'], passed=True, normalQuit=False, sourcePublicFingerprintComplete=[s['before']['complete'] for s in raw['sources']], relatedEventPIDs=[41908], relatedEventCount=len(events)))
        files.extend(identity(p) for p in (s / 'reader-retirement.json', s / 'isolation-final.json', s / 'capture/reference-inspection.json', Path(binding['buildReceipt'])))
    current = read(NEW / 'capture/reference-inspection.json')
    current_sha = sha(NEW / 'capture/reference-inspection.json')
    reference = next(s for s in current['sources'] if s['role'] == 'reference')
    scheme = next(s for s in current['sources'] if s['role'] == 'scheme')
    assert scheme['modelCount'] == 741 and reference['modelCount'] == 29535
    assert scheme['complete'] and not reference['complete']
    assert all(s['before']['complete'] and s['after']['complete'] for s in current['sources'])
    roots = [o for o in reference['observations'] if o['type'] == 'TDbDimension2']
    segments = {(o['handle'], n['nodeId']): n for o in roots for n in o['nodes'] if n['kind'] == 'dimension'}
    assert len(roots) == 64 and len(segments) == 246
    for n in segments.values():
        assert n['measurement']['status'] == n['formatted']['status'] == 0
        assert not n['glyphRead'] and not n['formatted']['isActualGlyphRead']
        assert n['style']['reference']['name'] == '_TCH_ARCH&&100'
    hatches = [o for s in current['sources'] for o in s['observations'] if o['type'] == 'AcDbHatch']
    assert len(hatches) == 54 and all(o['complete'] for o in hatches)
    catalog = read(DATA / '原生实测标准.json')
    assert len(catalog['dimensionChains']) == 64 and len(catalog['dimensionSegments']) == 246
    for n in catalog['dimensionSegments']:
        assert n['provenance']['rawSHA256'] == current_sha
        actual = segments[(n['rootHandle'], n['nodeId'])]
        for key in ('measurement', 'formatted', 'definition', 'override', 'textPosition', 'glyphRead'):
            assert n[key] == actual[key]
        assert n['style']['effectiveValues'] == actual['style']['effectiveValues']
    assert catalog['captureHistoryComparison']['matchedSegments'] == 246 and not catalog['captureHistoryComparison']['differences']
    csv_path = DATA / '尺寸分段实测.csv'
    assert csv_path.read_bytes().startswith(b'\xef\xbb\xbf')
    with csv_path.open(encoding='utf-8-sig', newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 246 and {r['rawSHA256'] for r in rows} == {current_sha}
    assert {(r['rootHandle'], int(r['nodeId'])) for r in rows} == set(segments)
    for row in rows:
        actual = segments[(row['rootHandle'], int(row['nodeId']))]
        assert float(row['measurement']) == actual['measurement']['value']
        assert row['formatted'] == actual['formatted']['text']
        assert json.loads(row['definitionJSON']) == actual['definition']
        assert json.loads(row['effectiveValuesJSON']) == actual['style']['effectiveValues']
    all_segments = {(o['handle'], n['nodeId']): n for o in reference['observations'] for n in o['nodes'] if n['kind'] == 'dimension'}
    supplemental = {k: v for k, v in all_segments.items() if k not in segments}
    assert len(all_segments) == 253 and len(supplemental) == 7
    assert {key[0] for key in supplemental} == {'11B93'}
    assert len(catalog['blockContainedDimensionSegments']) == 7
    for node in catalog['blockContainedDimensionSegments']:
        actual = supplemental[(node['rootHandle'], node['nodeId'])]
        assert node['rawNode'] == actual and node['provenance']['rawSHA256'] == current_sha
        assert actual['style']['reference']['name'] == '_TCH_ARCH&&25'
        assert actual['style']['effectiveValues']['dimtxt'] == 74.375
        assert not actual['glyphRead']
    extra_csv = DATA / '块内尺寸补充.csv'
    assert extra_csv.read_bytes().startswith(b'\xef\xbb\xbf')
    with extra_csv.open(encoding='utf-8-sig', newline='') as stream:
        extra_rows = list(csv.DictReader(stream))
    assert len(extra_rows) == 7 and {(r['rootHandle'], int(r['nodeId'])) for r in extra_rows} == set(supplemental)
    for row in extra_rows:
        actual = supplemental[(row['rootHandle'], int(row['nodeId']))]
        assert row['rootType'] == 'AcDbBlockReference' and row['rawSHA256'] == current_sha
        assert float(row['measurement']) == actual['measurement']['value'] and row['formatted'] == actual['formatted']['text']
        assert json.loads(row['definitionJSON']) == actual['definition'] and json.loads(row['effectiveValuesJSON']) == actual['style']['effectiveValues']
    checks.append(dict(name='native-catalog-and-csv', passed=True, topLevelChainCount=64, topLevelSegmentCount=246, blockContainedSegments=7, readableVisitedDimensionSegments=253, hatchesRead=54, actualDimensionGlyphLeavesRead=0))
    preview = read(OLD / 'stage1-preview-run.json')
    assert preview['error'] is None and len(preview['pages']) == 4
    pdfs = []
    for p in preview['pages']:
        assert sha(p['control']) == p['sourceSHA256'] == bindings[0]['inputs'][1]['SHA256']
        assert sha(p['PDF']) == p['PDFSHA256'] and sha(p['nativeReport']) == p['nativeReportSHA256']
        nr = read(p['nativeReport'])
        assert nr['error'] is None and nr['page']['error'] is None
        assert nr['initialModelCount'] == nr['finalModelCount'] == 29535
        assert nr['originalBefore'] == nr['originalAfter'] and nr['globalsBefore'] == nr['globalsAfter'] and nr['layoutBefore'] == nr['layoutAfter']
        assert all(a <= b + 1e-6 for a, b in zip(p['windowPaperMM'], p['printableMM']))
        reader = PdfReader(p['PDF'])
        assert len(reader.pages) == 1
        page = reader.pages[0]
        names = sorted({str(f.get_object()['/BaseFont']) for f in page['/Resources']['/Font'].values()})
        assert names == ['/SimSun'] and not page.get('/Annots')
        text = page.extract_text()
        assert '立面图' in text and '1:100' in text
        pdfs.append(dict(frame=p['frameHandle'], PDF=identity(p['PDF']), fontNames=names, annotations=0, textExtractionContainsSHXDimensionNumbers=False))
        files.append(identity(PHASE / '图面' / (p['frameHandle'] + '.png')))
    atlas = read(PHASE / '图面/尺寸逐链核对/atlas.json')
    visual = read(PHASE / '图面/尺寸逐链核对/visual-review.json')
    assert atlas['count'] == 64 and visual['reviewedChains'] == 64 and visual['reviewedSegments'] == 246
    assert visual['atlasSHA256'] == sha(PHASE / '图面/尺寸逐链核对/atlas.json')
    assert set(visual['handles']) == {o['handle'] for o in roots} and not visual['differencesSeen']
    for item in atlas['entries']:
        assert sha(item['crop']) == item['cropSHA256'] and sha(item['sourcePNG']) == item['sourcePNGSHA256']
    for sheet in atlas['sheets']:
        assert sha(sheet['path']) == sheet['SHA256']
    block_crop_path = PHASE / '图面/11B93-native720.json'
    block_crop = read(block_crop_path)
    assert block_crop['rootHandle'] == '11B93' and sha(block_crop['sourcePDF']) == block_crop['sourcePDFSHA256']
    assert sha(block_crop['output']) == block_crop['outputSHA256'] and block_crop['visualStatus'] == 'partial-visible-with-overlap'
    files.extend(identity(p) for p in (block_crop_path, block_crop['output'], extra_csv))
    fonts_path = OLD / 'fonts/native-actual-fonts.json'
    fonts = read(fonts_path)
    assert sha(fonts_path) == preview['fonts']['SHA256'] and fonts['error'] is None
    assert not fonts['sourceDrawingWritten'] and not fonts['COMUsed'] and not fonts['ComputerUseUsed'] and not fonts['TianzhengBusinessCommandsUsed']
    font_paths = {f['path'] for src in fonts['sources'] for st in src['styles'] for f in st['resolvedFiles'] if f.get('fileExists')}
    font_files = [identity(p) for p in sorted(font_paths)]
    checks.append(dict(name='native-previews-and-visual-provenance', passed=True, frames=4, chains=64, visualComparisonOnly=True, actualGlyphGeometryProgrammaticallyVerified=False))
    for path in [DATA / '静态标准提取.json', DATA / '原生实测标准.json', csv_path, fonts_path, OLD / 'stage1-preview-run.json', PHASE / '图面/尺寸逐链核对/atlas.json', PHASE / '图面/尺寸逐链核对/visual-review.json']:
        files.append(identity(path))
    for path in PHASE.glob('*.md'):
        files.append(identity(path))
    result = dict(contract='bce.stage1.verified-evidence.v1', recorded=datetime.datetime.now().astimezone().isoformat(), status='collected-and-cross-checked-with-explicit-limits', stage=1, stage2Started=False, sourceDrawingWritten=False, businessWriteAttempted=False, checks=checks, diagnosticPDFs=pdfs, resolvedFontFileIdentities=font_files, files=files, limitations=['246 dimension glyph leaves unavailable; visual comparison is not programmatic glyph geometry', 'reference capture remains complete=false with explicit compound/budget limits', 'GS font-field interpretation and formal CTB/lineweight not fully ratified', '800/831.877 baseline variants retained without universal selection rule', 'both own hosts stopped exactly after QUIT withheld; normal QUIT not passed'], verifier=identity(__file__))
    output = DATA / '阶段1核验.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(passed=len(checks), chainCount=64, segmentCount=246, hatches=54, frames=4, output=str(output)), ensure_ascii=False))


if __name__ == '__main__':
    main()
