"""Crop source-native PDF renders for visual review; never alters CAD data."""
from pathlib import Path
import hashlib
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
PHASE = ROOT / '立面插件/阶段1'
SESSION = ROOT / '阶段1/runs/elevation-stage1-20261101-162251-d748fbf0'


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()


def main():
    raw_path = SESSION / 'capture/reference-inspection.json'
    raw = json.loads(raw_path.read_text(encoding='utf-8-sig'))
    previews = json.loads((SESSION / 'stage1-preview-run.json').read_text(encoding='utf-8-sig'))
    target = PHASE / '图面/尺寸逐链核对'
    target.mkdir(parents=True, exist_ok=True)
    images = {p['frameHandle']: Image.open(PHASE / '图面' / (p['frameHandle'] + '.png')).convert('RGB') for p in previews['pages']}
    font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 19)
    entries, panels = [], []
    source = next(s for s in raw['sources'] if s['role'] == 'reference')
    dims = sorted((o for o in source['observations'] if o['type'] == 'TDbDimension2'), key=lambda o: int(o['handle'], 16))
    for obj in dims:
        bounds = obj['bounds']
        lo, hi = bounds['min'], bounds['max']
        centre = [(lo[i] + hi[i]) / 2 for i in range(2)]
        page = next(p for p in previews['pages'] if p['windowWCS'][0] <= centre[0] <= p['windowWCS'][2] and p['windowWCS'][1] <= centre[1] <= p['windowWCS'][3])
        img = images[page['frameHandle']]
        win = page['windowWCS']
        cx, cy = (win[0] + win[2]) / 2, (win[1] + win[3]) / 2
        # Native plots are centred at strict 1:100 on A2 594 x 420 mm.
        sx, sy = img.width / 594 / 100, img.height / 420 / 100
        def point(x, y):
            return img.width / 2 + (x - cx) * sx, img.height / 2 - (y - cy) * sy
        left, top = point(lo[0] - 650, hi[1] + 650)
        right, bottom = point(hi[0] + 650, lo[1] - 650)
        box = (max(0, int(left)), max(0, int(top)), min(img.width, int(right + 1)), min(img.height, int(bottom + 1)))
        crop = img.crop(box)
        rotated = crop.height > crop.width
        if rotated:
            crop = crop.transpose(Image.Transpose.ROTATE_270)
        crop_path = target / (obj['handle'] + '.png')
        crop.save(crop_path)
        nodes = [n for n in obj['nodes'] if n['kind'] == 'dimension']
        values = [n['formatted']['text'].replace('\\A1;', '') for n in nodes]
        label = obj['handle'] + ' / ' + page['frameHandle'] + ' / ' + ', '.join(values)
        # Labels identify the public API formatting result, not OCR evidence.
        panel = Image.new('RGB', (1500, max(150, min(crop.height, 300) + 58)), 'white')
        draw = ImageDraw.Draw(panel)
        draw.text((12, 5), label, fill='black', font=font)
        if crop.width > 1470 or crop.height > 300:
            display = crop.copy()
            display.thumbnail((1470, 300))
        else:
            display = crop
        panel.paste(display, (12, 40))
        panels.append(panel)
        entries.append(dict(handle=obj['handle'], frame=page['frameHandle'], crop=str(crop_path), cropSHA256=sha(crop_path), sourcePNG=str(PHASE / '图面' / (page['frameHandle'] + '.png')), sourcePNGSHA256=sha(PHASE / '图面' / (page['frameHandle'] + '.png')), pixelCrop=box, rotatedClockwise=rotated, formattedValues=values, labelIsActualGlyphRead=False, visualStatus='pending'))
    sheets = []
    for start in range(0, len(panels), 8):
        group = panels[start:start + 8]
        sheet = Image.new('RGB', (1500, sum(p.height + 12 for p in group)), '#dddddd')
        y = 0
        for p in group:
            sheet.paste(p, (0, y))
            y += p.height + 12
        out = target / ('sheet-%02d.png' % (start // 8 + 1))
        sheet.save(out)
        sheets.append(dict(path=str(out), SHA256=sha(out), handles=[e['handle'] for e in entries[start:start + 8]]))
    manifest = dict(contract='bce.stage1.dimension-visual-atlas.v1', raw=str(raw_path), rawSHA256=sha(raw_path), previewManifest=str(SESSION / 'stage1-preview-run.json'), previewManifestSHA256=sha(SESSION / 'stage1-preview-run.json'), count=len(entries), entries=entries, sheets=sheets, method='Crops from actual source-native 1:100 plots; centred-A2 mapping; API labels require visual comparison and are not OCR')
    (target / 'atlas.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(chains=len(entries), sheets=len(sheets)), ensure_ascii=False))


if __name__ == '__main__':
    main()

