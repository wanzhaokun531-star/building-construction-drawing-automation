"""Render the real M2 PlotSDK PDF and make unmodified-pixel QA crops."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
POPPLER = Path('C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/poppler/Library/bin/pdftoppm.exe')


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main(binding_path):
    binding = read(binding_path)
    session = Path(binding['session']).resolve(strict=True)
    assert session.is_relative_to((ROOT / '阶段1/runs').resolve())
    job = session / 'material-tiles-native-preview'
    preview_path = job / 'material-tiles-natural-preview.json'
    preview = read(preview_path)
    assert preview['nonce'] == binding['nonce'] and preview['error'] is None
    pdf = Path(preview['PDF']).resolve(strict=True)
    assert pdf.is_relative_to(job) and sha(pdf) == preview['PDFSHA256']
    png = job / 'material-tiles-natural.png'
    assert not png.exists()
    subprocess.run([str(POPPLER), '-f', '1', '-singlefile', '-r', '300',
                    '-png', str(pdf), str(png.with_suffix(''))], check=True)
    with Image.open(png) as loaded:
        rgb = loaded.convert('RGB')
    mask = ImageChops.difference(rgb, Image.new('RGB', rgb.size, 'white'))
    mask = mask.convert('L').point(lambda value: 255 if value > 10 else 0)
    ink = mask.getbbox()
    assert ink is not None, 'Native plot is blank'
    left, top, right, bottom = ink
    assert right - left > 100 and bottom - top > 100

    def clip(box):
        return (max(0, int(box[0])), max(0, int(box[1])),
                min(rgb.width, int(box[2]) + 1), min(rgb.height, int(box[3]) + 1))

    # This mapping selects visual crops; it is not a substitute for the native
    # coordinate/coverage assertions. Sparse outer hatch ink can shift extents.
    def model_box(x0, y0, x1, y1):
        return clip((left + x0 * (right-left) / 10000,
                     bottom - y1 * (bottom-top) / 6000,
                     left + x1 * (right-left) / 10000,
                     bottom - y0 * (bottom-top) / 6000))

    crop_boxes = {
        'whole-wall': clip((left-20, top-20, right+20, bottom+20)),
        'opening': model_box(600, 300, 3000, 3000),
    }
    seams = []
    tiles = preview['nativeWall']['tiles']
    for index, first in enumerate(tiles):
        ax0, ay0, ax1, ay1 = first['box']
        for second in tiles[index+1:]:
            bx0, by0, bx1, by1 = second['box']
            lo_x, hi_x = max(ax0, bx0), min(ax1, bx1)
            lo_y, hi_y = max(ay0, by0), min(ay1, by1)
            if hi_y > lo_y and (ax1 == bx0 or bx1 == ax0):
                seams.append((hi_y-lo_y, ax1 if ax1 == bx0 else bx1, (lo_y+hi_y)/2))
            if hi_x > lo_x and (ay1 == by0 or by1 == ay0):
                seams.append((hi_x-lo_x, (lo_x+hi_x)/2, ay1 if ay1 == by0 else by1))
    assert seams
    length, center_x, center_y = max(seams)
    crop_boxes['seam'] = model_box(max(0, center_x-1000), max(0, center_y-800),
                                   min(10000, center_x+1000), min(6000, center_y+800))
    crops = {}
    for name, box in crop_boxes.items():
        path = job / ('material-' + name + '-crop.png')
        assert not path.exists()
        rgb.crop(box).save(path)
        crops[name] = dict(path=str(path), SHA256=sha(path), pixelBox=box)
    interior = model_box(1500, 1200, 2100, 2100)
    interior_mask = mask.crop(interior)
    histogram = interior_mask.histogram()
    result = dict(contract='bce.material-tiles-raster-qa.v1',
                  nonce=binding['nonce'], preview=str(preview_path),
                  previewSHA256=sha(preview_path), PDF=str(pdf), PDFSHA256=sha(pdf),
                  PNG=str(png), PNGSHA256=sha(png), size=rgb.size, inkBox=ink,
                  nonWhitePixels=sum(mask.histogram()[1:]), crops=crops,
                  openingInteriorPixelBox=interior,
                  openingInteriorInkPixels=sum(histogram[1:]),
                  selectedSeamModelCenter=[center_x, center_y], selectedSeamLength=length,
                  cropMapping='Approximate from visible ink bounds; for visual inspection only',
                  visualInspectionCompleted=False, helperSHA256=sha(__file__))
    output = job / 'root-raster-qa.json'
    with output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
    print(json.dumps(result, ensure_ascii=True))


if __name__ == '__main__':
    main(sys.argv[1])
