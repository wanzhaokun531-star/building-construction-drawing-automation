"""Compare actual PlotSDK ink with N9a public string boxes. No geometric margin."""
from pathlib import Path
import argparse
import csv
import gzip
import hashlib
import json
import math
import re
import subprocess
import sys
import zlib

import numpy as np
from PIL import Image, ImageDraw

ROOT = next(p for p in Path(__file__).resolve().parents
            if (p / '立面插件').is_dir() and (p / '阶段1/原生接口探测').is_dir())
POPPLER = Path('C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/poppler/Library/bin/pdftoppm.exe')
RESOLUTIONS = (144, 288)
IDS = ('digits', 'chinese', 'rotated-backward', 'nested-nonuniform', 'tracking-oblique')


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def write(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)


def render(page, output, dpi):
    source = Path(page['pdf']).resolve(strict=True)
    assert sha(source) == page['pdfSHA256']
    target = output / f'{page["id"]}-{dpi}-ink.png'
    assert not target.exists(), target
    subprocess.run([str(POPPLER), '-f', '1', '-l', '1', '-singlefile', '-r', str(dpi),
                    '-aa', 'yes', '-aaVector', 'yes', '-png', str(source), str(target.with_suffix(''))], check=True)
    with Image.open(target) as image:
        rgb = np.asarray(image.convert('RGB')).copy()
    mask = np.any(rgb < 255, axis=2)  # Every observable non-white antialias coverage.
    yy, xx = np.nonzero(mask)
    assert xx.size > 0, 'blank native page'
    assert xx.min() > 2 and yy.min() > 2 and xx.max() < rgb.shape[1]-3 and yy.max() < rgb.shape[0]-3, 'ink touches device edge'
    return target, rgb, mask, xx, yy


def mapping(page, dpi):
    m = np.asarray(page.get('_measuredWorldToPagePoints', page['worldToPagePoints']), dtype=float).reshape(2, 3) * (dpi / 72)
    assert np.isfinite(m).all() and abs(np.linalg.det(m[:, :2])) > 1e-8
    return m


def project(corners, m):
    points = np.asarray(corners, dtype=float)
    assert points.shape == (4, 3) and np.isfinite(points).all() and np.max(np.abs(points[:, 2])) < 1e-8
    return np.column_stack((points[:, :2], np.ones(4))) @ m.T


def full_cells(polygon, xx, yy):
    # A pixel at (x,y) occupies [x,x+1] x [y,y+1]. All four corners must
    # be inside every oriented half-plane; no AA / lineweight margin is added.
    signed_area = np.sum(polygon[:, 0] * np.roll(polygon[:, 1], -1) - polygon[:, 1] * np.roll(polygon[:, 0], -1))
    assert abs(signed_area) > 1e-8, 'degenerate candidate polygon'
    sign = 1 if signed_area > 0 else -1
    inside = np.ones(xx.shape, dtype=bool)
    max_outside = np.zeros(xx.shape, dtype=float)
    for a, b in zip(polygon, np.roll(polygon, -1, axis=0)):
        edge = b-a
        length = np.linalg.norm(edge)
        assert length > 1e-8
        for dx, dy in ((0, 0), (1, 0), (1, 1), (0, 1)):
            cross = sign * (edge[0] * (yy+dy-a[1]) - edge[1] * (xx+dx-a[0])) / length
            inside &= cross >= -1e-8  # Numerical comparison only, in pixel units.
            max_outside = np.maximum(max_outside, -cross)
    return inside, max_outside


def calibration(page, native, output, dpi):
    path, rgb, mask, xx, yy = render(page, output, dpi)
    m = mapping(page, dpi)
    if '_pdfFrame' in page:
        box=page['_pdfFrame']['mediaBox']
        assert abs(rgb.shape[1]-(box[2]-box[0])*dpi/72) < 1 and abs(rgb.shape[0]-(box[3]-box[1])*dpi/72) < 1
    else:
        paper = page['validatedSettings']['paperMM']
        assert abs(rgb.shape[1]-paper[0]*dpi/25.4) < 2 and abs(rgb.shape[0]-paper[1]*dpi/25.4) < 2
    # Each native line is isolated. Associate by the publicly predicted center,
    # then check the entire ink bbox, both endpoints, and the pixel center.
    remaining = mask.copy()
    checks = []
    for center in native['calibrationCenters']:
        predicted = m @ np.array([center[0], center[1], 1.0])
        px, py = predicted
        half = native['calibrationLineHalfLength']*dpi/25.4
        x0, y0 = max(0, math.floor(px-half-3)), max(0, math.floor(py-3))
        x1, y1 = math.ceil(px+half+3), math.ceil(py+3)
        ry, rx = np.nonzero(remaining[y0:y1, x0:x1])
        assert rx.size > 0, 'predicted native calibration location has no line'
        box = [int(rx.min()+x0), int(ry.min()+y0), int(rx.max()+x0+1), int(ry.max()+y0+1)]
        actual = np.array([(box[0]+box[2])/2, (box[1]+box[3])/2])
        residual = actual-predicted
        assert max(abs(residual)) <= 1.0, ('public projection calibration residual', residual.tolist())
        assert abs((box[2]-box[0])-2*half) <= 2.0 and box[3]-box[1] <= 3, 'unexpected calibration line footprint'
        remaining[y0:y1, x0:x1] = False
        checks.append(dict(modelCenter=center, predictedPixel=predicted.tolist(), observedPixel=actual.tolist(), residualPixel=residual.tolist(), inkBox=box))
    assert not remaining.any(), 'calibration page contains unexpected ink'
    return dict(dpi=dpi, image=str(path), sha256=sha(path), mappingSource='measured native PDF geometry; three fitted, two held out' if '_measuredWorldToPagePoints' in page else 'validated settings; no fitted parameters',
                independentChecks=2 if '_measuredWorldToPagePoints' in page else 5, fittedIndices=[0,1,2] if '_measuredWorldToPagePoints' in page else [],
                heldOutIndices=[3,4] if '_measuredWorldToPagePoints' in page else list(range(5)),pixelQuantizationTolerance=1.0, checks=checks)


PDF_NUMBER=rb'[-+]?(?:\d+(?:\.\d*)?|\.\d+)'


def pdf_frame(page):
    """Read only the observed controlled Plot PDF subset; reject unknown transforms."""
    path=Path(page['pdf']);assert sha(path)==page['pdfSHA256']
    raw=path.read_bytes()
    boxes=re.findall(rb'/MediaBox\s*\[([^]]+)\]',raw)
    assert len(boxes)==1,'unknown/multiple PDF page boxes'
    media=[float(x) for x in boxes[0].split()]
    assert len(media)==4 and media[:2]==[0,0] and media[2]>0 and media[3]>0
    crop=re.findall(rb'/CropBox\s*\[([^]]+)\]',raw)
    assert not crop or len(crop)==1 and [float(x)for x in crop[0].split()]==media,'nontrivial PDF CropBox unsupported'
    rotate=[int(x)for x in re.findall(rb'/Rotate\s+([-+]?\d+)',raw)]
    assert not rotate or rotate==[0],'PDF rotation unsupported'
    streams=[]
    for stream in re.findall(rb'stream\r?\n(.*?)\r?\nendstream',raw,re.S):
        try:streams.append(zlib.decompress(stream))
        except zlib.error:streams.append(stream)
    cm_pattern=rb'\s+'.join([b'('+PDF_NUMBER+b')']*6)+rb'\s+cm\b'
    matrices=[[float(v)for v in match]for stream in streams for match in re.findall(cm_pattern,stream)]
    assert len(matrices)==1,'controlled PDF must have exactly one observed cm transform'
    cm=matrices[0]
    assert cm[0]>0 and cm[3]>0 and cm[1]==cm[2]==cm[4]==cm[5]==0,'unknown PDF basis/translation'
    line_pattern=rb'\s+'.join([b'('+PDF_NUMBER+b')']*2)+rb'\s+m\s+'+rb'\s+'.join([b'('+PDF_NUMBER+b')']*2)+rb'\s+l\s+S\b'
    lines=[[float(v)for v in match]for stream in streams for match in re.findall(line_pattern,stream)]
    return dict(mediaBox=media,cropBox=None if not crop else media,rotate=0,allCm=matrices,
                observedCoordinateUnitPoints=[cm[0],cm[3]],rawSimpleStrokedLines=lines)


def measured_maps(pages,native):
    frames={name:pdf_frame(page)for name,page in pages.items()}
    reference=frames['projection']
    for name,frame in frames.items():
        assert frame['mediaBox']==reference['mediaBox'] and frame['allCm']==reference['allCm'],'text/calibration PDF basis differs'
        assert pages[name]['validatedSettings']==pages['projection']['validatedSettings'],'text/calibration plot settings differ'
    lines=np.asarray(reference['rawSimpleStrokedLines'],dtype=float).reshape(-1,2,2)
    assert lines.shape==(5,2,2),'projection PDF must contain five independent simple stroked lines'
    assert np.max(abs(lines[:,0,1]-lines[:,1,1]))<1e-10,'projection line direction changed'
    cm=reference['allCm'][0]
    actual=lines.mean(axis=1)*np.array([cm[0],cm[3]])
    actual[:,1]=reference['mediaBox'][3]-actual[:,1]
    world=np.asarray(native['calibrationCenters'],dtype=float)[:,:2]
    nominal=np.asarray(pages['projection']['worldToPagePoints']).reshape(2,3)
    predicted=np.column_stack((world,np.ones(5)))@nominal.T
    indices=np.argmin(np.linalg.norm(predicted[:,None,:]-actual[None,:,:],axis=2),axis=1)
    assert len(set(indices.tolist()))==5,'unknown calibration correspondence'
    assert np.max(np.linalg.norm(predicted-actual[indices],axis=1))<1.0,'calibration not near the nominal plot mapping'
    actual=actual[indices]
    fit=np.column_stack((world[:3],np.ones(3)))
    assert np.linalg.matrix_rank(fit)==3
    matrix=np.linalg.solve(fit,actual[:3]).T
    residual=actual-np.column_stack((world,np.ones(5)))@matrix.T
    # This checks the two held-out calibration points, not a universal mapping
    # error bound. It NEVER enlarges the string range or the pixel-cell test.
    tolerance=min(cm[0],cm[3])/2
    assert np.max(abs(residual[3:]))<=tolerance,'held-out native projection residual exceeds half observed coordinate unit'
    for name,page in pages.items():
        page['_measuredWorldToPagePoints']=matrix.reshape(-1).tolist()
        page['_pdfFrame']=frames[name]
    return dict(method='three noncollinear actual native PDF line centers fitted; two independent held-out points',
                fitIndices=[0,1,2],heldOutIndices=[3,4],pdfFrames=frames,correspondenceIndices=indices.tolist(),
                modelCenters=world.tolist(),actualPageCentersYDown=actual.tolist(),worldToPageMeasured=matrix.tolist(),
                residualPoints=residual.tolist(),independentTolerancePoints=tolerance,maxHeldOutResidualPoint=float(np.max(abs(residual[3:]))),
                limitation='finite five-point calibration of this exact Plot output; not a universal affine or infinite-resolution guarantee',
                geometricContainmentMarginPixel=0)


def compare(sample, page, rendered, output, dpi, candidate='public'):
    path, rgb, mask, xx, yy = rendered
    m = mapping(page, dpi)
    polygon = project(sample['worldCorners'], m)
    if candidate == 'shrink':
        polygon = polygon.mean(axis=0)+(polygon-polygon.mean(axis=0))*0.35
    elif candidate == 'omit-net':
        polygon = project(sample['omittedNetTransformCorners'], m)
    inside, distances = full_cells(polygon, xx, yy)
    bad_x, bad_y = xx[~inside], yy[~inside]
    inverse = np.linalg.inv(m[:, :2])
    # Preserve every outlier pixel, all four world-space cell corners, and
    # signed edge-distance violation. No sample cap discards failures.
    outliers = output / f'{sample["id"]}-{dpi}-{candidate}-outside.csv.gz'
    with gzip.open(outliers, 'xt', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['pixelX', 'pixelY', 'outsidePixel', 'worldCellCornersXY'])
        for x, y, distance in zip(bad_x, bad_y, distances[~inside]):
            cell = np.array([[x,y],[x+1,y],[x+1,y+1],[x,y+1]], dtype=float)
            world = (cell-m[:, 2]) @ inverse.T
            writer.writerow([int(x), int(y), float(distance), json.dumps(world.tolist(), separators=(',', ':'))])
    overlay = Image.fromarray(rgb)
    draw = ImageDraw.Draw(overlay)
    draw.line([tuple(p) for p in polygon]+[tuple(polygon[0])], fill=(0,100,255), width=1)
    for x,y in zip(bad_x, bad_y):
        draw.point((int(x),int(y)), fill=(255,0,0))
    preview = output / f'{sample["id"]}-{dpi}-{candidate}-overlay.png'
    overlay.save(preview)
    return dict(id=sample['id'], dpi=dpi, candidate=candidate, inkImage=str(path), inkSHA256=sha(path),
                inkPixels=int(xx.size), outsidePixels=int(bad_x.size), maxOutsidePixel=float(distances.max()),
                candidatePixelCorners=polygon.tolist(), pixelCell='[x,x+1] by [y,y+1]; y down; top-left phase 0',
                numericalTolerancePixel=1e-8, geometricMarginPixel=0, backgroundRGB=[255,255,255],
                inkRule='any channel <255 including antialias coverage', outlierFile=str(outliers), outlierSHA256=sha(outliers),
                overlay=str(preview), overlaySHA256=sha(preview), renderContainmentVerified=bool(inside.all()))


def bound_inputs(binding_path):
    binding = read(binding_path)
    session = Path(binding['session']).resolve(strict=True)
    assert session.is_relative_to((ROOT/'阶段1/runs').resolve())
    assert binding['caseName'] == 'ControlledTextRange'
    assert sha(binding['buildReceipt']) == binding['buildReceiptSHA256']
    for path, digest in binding['candidateSourceFiles'].items():
        assert sha(path) == digest
    for item in binding['inputs']:
        for key in ('source','snapshot','inspectionCopy'):
            assert sha(item[key]) == item['SHA256']
    native_path = session/'case/text-range.json'
    native = read(native_path)
    case = read(session/'case/native-case.json')
    ready = read(session/'quiet-worker-ready.json')
    assert case['status'] == 'partial' and not case['errors']
    assert native['nonce'] == case['nonce'] == ready['nonce'] == binding['nonce']
    assert native['pid'] == case['pid'] == ready['processId']
    assert native['moduleSHA256'] == case['moduleSHA256'] == binding['candidateSHA256']
    assert native['glyphComplete'] is False and native['capabilityUpgradeAllowed'] is False
    assert native['ownedControlDocumentModifiedInMemory'] is True and native['dwgSaved'] is False
    assert native['overrulingRestored'] and native['backgroundPlotRestored']
    assert sha(native['controlPath']) == native['controlDiskSHA256']
    assert [s['id'] for s in native['samples']] == list(IDS)
    for sample in native['samples']:
        assert sample['loadStyleRecBits'] & 1 and sample['loadStyleRecBits'] & 30 == 0
        assert sample['contextDatabaseMatchesTarget'] and sample['stringRangeComputed']
        assert sample['raw'] and sample['penups'] and sample['upsideDown'] is False
        assert sample['fontBeforeLoad'].casefold() == sample['fontAfterLoad'].casefold() == sample['fontPath'].casefold()
        assert sample['bigfontBeforeLoad'].casefold() == sample['bigfontAfterLoad'].casefold() == sample['bigfontPath'].casefold()
        assert sha(sample['fontPath']) == sample['fontSHA256'] and sha(sample['bigfontPath']) == sample['bigfontSHA256']
        n,d = np.asarray(sample['normal']), np.asarray(sample['direction'])
        assert abs(np.linalg.norm(n)-1)<1e-8 and abs(np.linalg.norm(d)-1)<1e-8 and abs(n@d)<1e-8, 'unknown basis'
        assert sample['lengthCodeUnits'] == len(sample['utf16'])
        assert sample['utf16'] == list(np.frombuffer(sample['text'].encode('utf-16le'), dtype='<u2').astype(int))
        lo,hi=sample['rangeMin'],sample['rangeMax']
        local=np.array([[lo[0],lo[1]],[hi[0],lo[1]],[hi[0],hi[1]],[lo[0],hi[1]]])
        position=np.asarray(sample['position'])
        xyz=position+local[:,0,None]*d+local[:,1,None]*np.cross(n,d)
        net=np.asarray(sample['modelToWorld']).reshape(4,4)
        expected=(np.column_stack((xyz,np.ones(4)))@net.T)[:,:3]
        assert np.allclose(expected,sample['worldCorners'],rtol=0,atol=1e-8), 'four-corner/net transform mismatch'
    return binding,native,native_path


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('binding',type=Path)
    parser.add_argument('--mode',choices=('negative','public'),required=True)
    parser.add_argument('--projection',choices=('nominal','measured-pdf'),default='nominal')
    parser.add_argument('--output-tag',default='')
    args=parser.parse_args()
    binding,native,native_path=bound_inputs(args.binding)
    assert re.fullmatch(r'[a-zA-Z0-9_-]*',args.output_tag)
    output=native_path.parent/(f'text-range-{args.mode}'+('-'+args.output_tag if args.output_tag else ''))
    output.mkdir()
    result=dict(contract='bce.text-range-comparison.v1', mode=args.mode, binding=str(args.binding.resolve()),
                bindingSHA256=sha(args.binding), nativeReport=str(native_path),nativeReportSHA256=sha(native_path),
                comparator=str(Path(__file__).resolve()), comparatorSHA256=sha(__file__), rasterizer=str(POPPLER),rasterizerSHA256=sha(POPPLER),
                glyphComplete=False,capabilityUpgradeAllowed=False, renderContainmentVerified=False,
                resolutionScope='finite native Plot vector output rasterized by Poppler at 144 and 288 dpi; no infinite-resolution proof',
                unsupported=['upsideDown','outerReflection','missingGlyphs','fallback','TTF','vertical','decorations','fullMText','unknownClip','unknownAnnotationScale','originalTCHGlyphs'],
                projection=[],comparisons=[],error=None)
    try:
        pages={p['id']:p for p in native['pages']}
        assert set(pages)==set(IDS)|{'projection'}
        if args.projection=='measured-pdf':result['measuredProjection']=measured_maps(pages,native)
        for dpi in RESOLUTIONS:
            result['projection'].append(calibration(pages['projection'],native,output,dpi))
            for sample in native['samples']:
                rendered=render(pages[sample['id']],output,dpi)
                if args.mode=='negative':
                    if sample['id']=='digits':
                        comparison=compare(sample,pages[sample['id']],rendered,output,dpi,'shrink')
                    elif sample['id']=='nested-nonuniform':
                        comparison=compare(sample,pages[sample['id']],rendered,output,dpi,'omit-net')
                    else:
                        continue
                    assert not comparison['renderContainmentVerified'] and comparison['outsidePixels']>0,'real negative not rejected'
                else:
                    comparison=compare(sample,pages[sample['id']],rendered,output,dpi)
                result['comparisons'].append(comparison)
        if args.mode=='negative':
            assert len(result['comparisons'])==4
            result['status']='real-native-negative-controls-rejected'
        else:
            assert len(result['comparisons'])==10
            result['renderContainmentVerified']=all(x['renderContainmentVerified'] for x in result['comparisons'])
            result['status']='finite-controlled-containment-verified' if result['renderContainmentVerified'] else 'actual-outside-pixel-cells-preserved'
    except BaseException as error:
        result['error']=repr(error)
        raise
    finally:
        write(output/'comparison.json',result)
    print(json.dumps(dict(status=result['status'],report=str(output/'comparison.json'),renderContainmentVerified=result['renderContainmentVerified'])))
    return 0 if args.mode=='negative' or result['renderContainmentVerified'] else 1


if __name__=='__main__':
    sys.exit(main())
