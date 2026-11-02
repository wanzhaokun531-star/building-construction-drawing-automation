"""One-off AutoCAD drawing batch. No product source or binary is changed."""
from pathlib import Path
import json, hashlib, shutil, sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
def read(p): return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def s(x): return '"'+str(x).replace('\\','/').replace('"','\\"')+'"'
def point(p): return "'("+' '.join(format(float(x),'.12f') for x in [*p[:2],0])+')'
def datum_point(p): return '('+' '.join(format(float(x),'.12f') for x in p[:2])+')'

version=sys.argv[1] if len(sys.argv)>1 else 'v119'
binding_path=ROOT/f'阶段2/evidence/native-host-binding-{version}.json'
b=read(binding_path);session=Path(b['session']);job=Path(b['job'])
contract=read(HERE/'门号与轴衔接编辑合同_v1.json')
base_boot=(session/'boot.scr').read_text(encoding='cp936').split('S1QUIETSTART',1)[0]+'S1QUIETSTART'
commands=[base_boot,
 '(setq bcp-old-filedia (getvar "FILEDIA") bcp-old-cmddia (getvar "CMDDIA"))',
 '(setvar "FILEDIA" 0)', '(setvar "CMDDIA" 0)', '(setvar "OSMODE" 0)',
 '(defun bcp-layer (n c p) (if (not (tblsearch "LAYER" n)) (entmake (list (cons 0 "LAYER") (cons 100 "AcDbSymbolTableRecord") (cons 100 "AcDbLayerTableRecord") (cons 2 n) (cons 70 0) (cons 62 c) (cons 6 "Continuous") (cons 290 p)))))',
 '(defun bcp-poly (pts) (entmakex (append (list (cons 0 "LWPOLYLINE") (cons 100 "AcDbEntity") (cons 8 "BCP-AREA-CALC") (cons 100 "AcDbPolyline") (cons 90 (length pts)) (cons 70 1)) (mapcar (function (lambda (p) (cons 10 p))) pts))))',
 '(defun bcp-text (txt pt ht) (entmakex (list (cons 0 "TEXT") (cons 100 "AcDbEntity") (cons 8 "A-房间名称") (cons 62 7) (cons 100 "AcDbText") (cons 10 pt) (cons 40 ht) (cons 1 txt) (cons 50 0.0) (cons 41 0.65) (cons 7 bcp-style) (cons 72 1) (cons 11 pt) (cons 100 "AcDbText") (cons 73 2))))',
 '(defun bcp-note (txt pt ht) (entmakex (list (cons 0 "TEXT") (cons 100 "AcDbEntity") (cons 8 "A-房间名称") (cons 62 7) (cons 100 "AcDbText") (cons 10 pt) (cons 40 ht) (cons 1 txt) (cons 50 0.0) (cons 41 0.65) (cons 7 bcp-style))))',
 '(defun bcp-log (a) (setq bcp-file (open bcp-logpath "a")) (write-line a bcp-file) (close bcp-file))',
 '(defun bcp-format (v / old txt) (setq old (getvar "DIMZIN")) (setvar "DIMZIN" 0) (setq txt (rtos v 2 2)) (setvar "DIMZIN" old) txt)']
helpers=commands[5:];commands=commands[:5]
manifest=dict(contract='bcp.one-off-drawing-preview.v1',pluginCodeChanged=False,pluginBinarySHA256=sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx'),route='AutoCAD native drawing script, no COM/CUA/TCH business commands; not an ObjectARX product implementation',session=str(session),nonce=b['nonce'],floors=[],sourceHashes={str(p.relative_to(ROOT)):sha(p) for folder in ['插件/src','阶段2/src'] for p in (ROOT/folder).iterdir() if p.is_file()})
layout=read(HERE/'area-caption-layout.json') if (HERE/'area-caption-layout.json').is_file() else None
for floor in contract['floors']:
 n=floor['floor'];areas=read(HERE/f'room-areas-floor-{n}.json');src=Path(floor['source']);assert sha(src)==floor['sourceSHA256']==areas['sourceDWGSHA256']
 folder=job/f'drawing-floor-{n}';folder.mkdir(exist_ok=True);control=folder/'control.dwg'
 assert not (folder/'native-drawing-edit.tsv').exists()
 if control.exists():assert sha(control)==sha(src)
 shutil.copyfile(src,control)
 log=folder/'native-drawing-edit.tsv'
 commands+=['_.OPEN',s(control),*helpers,f'(setq bcp-ok T bcp-logpath {s(log)} bcp-file (open bcp-logpath "w") bcp-style (cdr (assoc 7 (entget (handent {s(floor["captionPlacements"][0]["handle"])})))))', '(close bcp-file)', '(bcp-log "START")',
 '(bcp-layer "BCP-AREA-CALC" 8 0)', '(bcp-layer "BCP-OLD-AREA" 8 0)', '(bcp-layer "A-房间名称" 7 1)', '(bcp-log "LAYERS")']
 for move in floor['moves']:
  commands.append(f'(progn (command "_.MOVE" (handent {s(move["handle"])}) "" "_non" \'(0 0 0) "_non" {point(move["delta"])}) (bcp-log {s("MOVE"+chr(9)+move["handle"])}))')
 for line in floor['lineAdditions']:
  commands.append(f'(progn (setq bcp-e (entmakex (list \'(0 . "LINE") \'(100 . "AcDbEntity") \'(8 . "A-AXIS") \'(62 . 3) \'(6 . "Continuous") \'(100 . "AcDbLine") (cons 10 {point(line["start"])}) (cons 11 {point(line["end"])})))) (if (null bcp-e) (setq bcp-ok nil)) (bcp-log (strcat "LINE\t{line["id"]}\t" (cdr (assoc 5 (entget bcp-e))))))')
 for label in areas['excludedOutsideOrNonRoomLabels']:
  if '面积' in label['text']:
   commands.append(f'(progn (command "_.CHPROP" (handent {s(label["handle"])}) "" "_LA" "BCP-OLD-AREA" "") (bcp-log "OLD_AREA_LAYER_{label["handle"]}"))')
 area_rows=[]
 for region in areas['regions']:
  rid=region['id'];pos=region['recommendedAnnotationPoint'];height=240
  if layout:
   lr=next(x for x in layout['floors'] if x['floor']==n)
   proposed=next(x for x in lr['captions'] if x['region']==rid);pos=proposed['position'];height=proposed['height']
  if region['areaM2']<2: height=200
  prefix='连通净' if len(region['labels'])>1 else '净'
  commands.append('(setq bcp-net 0.0)')
  for sign,vs in [(1,region['vertices'])]+[(-1,h) for h in region['holes']]:
   pts="'("+' '.join(datum_point(p) for p in vs)+')'
   commands.append(f'(progn (setq bcp-e (bcp-poly {pts})) (command "_.AREA" "_O" bcp-e) (setq bcp-net (+ bcp-net (* {sign} (getvar "AREA")))))')
  commands.append(f'(if (> (abs (- bcp-net {region["areaMm2"]:.8f})) 1.0) (setq bcp-ok nil))')
  commands.append(f'(progn (setq bcp-e (bcp-text (strcat {s(prefix)} (bcp-format (/ bcp-net 1000000.0)) "㎡") {point(pos)} {height})) (if (null bcp-e) (setq bcp-ok nil)) (bcp-log (strcat "AREA\t{rid}\t" (rtos bcp-net 2 8) "\t" (cdr (assoc 5 (entget bcp-e))))))')
  area_rows.append(dict(region=rid,expectedM2=region['areaM2'],position=pos,height=height,text=prefix+f'{region["areaM2"]:.2f}㎡'))
 commands.append('(setq bcp-total 0.0)')
 for contour in areas['exteriorContours']:
  pts="'("+' '.join(datum_point(p) for p in contour)+')'
  commands.append(f'(progn (setq bcp-e (bcp-poly {pts})) (command "_.AREA" "_O" bcp-e) (setq bcp-total (+ bcp-total (getvar "AREA"))))')
 porch_m2=0
 if n==1:
  # User confirmed: upper platform 4600 x 1380 only, excluding the 600-deep steps.
  px=42551.70152821252;py=24145.21380196116
  vs=[[px,py],[px+4600,py],[px+4600,py+1380],[px,py+1380]]
  pts="'("+' '.join(datum_point(p) for p in vs)+')'
  commands.append(f'(progn (setq bcp-e (bcp-poly {pts})) (command "_.AREA" "_O" bcp-e) (setq bcp-porch (getvar "AREA") bcp-total (+ bcp-total (* 0.5 bcp-porch))) (bcp-log (strcat "PORCH\t" (rtos bcp-porch 2 8))))')
  porch_m2=6.348
 x=39252.53685493558+(46422 if n==2 else 0);y=19600
 commands.append(f'(progn (setq bcp-e (bcp-note (strcat "本层总建筑面积：" (rtos (/ bcp-total 1000000.0) 2 2) "㎡") {point([x,y])} 300)) (bcp-log (strcat "TOTAL\t" (rtos bcp-total 2 8) "\t" (cdr (assoc 5 (entget bcp-e))))))')
 notes=([f'外墙外轮廓{areas["totalExteriorContourAreaM2"]:.2f}㎡＋门廊6.348㎡×1/2；台阶、露台不计。'] if n==1 else ['按外墙闭合外轮廓计算；露台、下层门廊投影不计。'])+['房间按墙内净投影；相连开放空间合并标注。本图为确认样张。']
 for k,text in enumerate(notes):commands.append(f'(bcp-note {s(text)} {point([x,y-550*(k+1)])} 220)')
 commands+=['(foreach bcp-n \'("BCP-AREA-CALC" "BCP-OLD-AREA") (setq bcp-e (tblobjname "LAYER" bcp-n) bcp-d (entget bcp-e)) (entmod (subst (cons 62 (- (abs (cdr (assoc 62 bcp-d))))) (assoc 62 bcp-d) bcp-d)))',
 '(bcp-log (strcat "COUNT\t" (itoa (sslength (ssget "_X" \'((410 . "Model")))))))',
 '(bcp-log (if bcp-ok "SUCCESS" "FAILED"))', '(if bcp-ok (command "_.QSAVE"))', '_.CLOSE']
 manifest['floors'].append(dict(floor=n,source=str(src),sourceSHA256=sha(src),control=str(control),log=str(log),areas=area_rows,exteriorM2=areas['totalExteriorContourAreaM2'],porchM2=porch_m2,totalM2=areas['totalExteriorContourAreaM2']+porch_m2/2,moves=floor['moves'],lines=floor['lineAdditions'],unresolved=floor['unresolved']))
commands+=['(setvar "FILEDIA" bcp-old-filedia)','(setvar "CMDDIA" bcp-old-cmddia)','(princ)']
boot=session/'boot.scr';boot.write_bytes(('\r\n'.join(commands)+'\r\n').encode('cp936'))
b['bootSHA256']=sha(boot);b['scope']=manifest['route'];binding_path.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8')
manifest['bootSHA256']=sha(boot);manifest['boot']=str(boot);manifest['version']=version
out=HERE/'drawing-preview-manifest.json'
if out.exists():shutil.copyfile(out,HERE/('drawing-preview-manifest-'+read(out).get('version','v119')+'-history.json'))
out.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(boot=str(boot),bytes=boot.stat().st_size),ensure_ascii=False))
