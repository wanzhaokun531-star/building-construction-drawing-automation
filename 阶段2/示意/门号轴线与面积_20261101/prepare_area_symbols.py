"""One-off DWG typography batch; product C++ and ARX stay unchanged."""
from pathlib import Path
import json,hashlib,shutil
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def string(s):return '"'+str(s).replace('\\','\\\\').replace('"','\\"')+'"'
def path(p):return string(Path(p).as_posix())
def point(p):return "'("+' '.join(format(float(x),'.12f') for x in p[:3])+')'
bpath=ROOT/'阶段2/evidence/native-host-binding-v125.json';b=read(bpath);session=Path(b['session']);job=Path(b['job'])
m=read(HERE/'drawing-preview-manifest.json');prior=read(HERE/'native-drawing-preview-qa.json')
helper='''(defun bcp-area-symbol (handle content anchor mode name / e d h sty width ob bb sb lo hi minx maxx miny maxy bx by sx sy sup p gd members) (setq e (handent handle) d (entget e) h (cdr (assoc 40 d)) sty (cdr (assoc 7 d)) width (cdr (assoc 41 d)) ob (cdr (assoc 51 d))) (if (/= (cdr (assoc 0 d)) "TEXT") (exit)) (if (not width) (setq width 1.0)) (if (not ob) (setq ob 0.0)) (setq d (subst (cons 1 content) (assoc 1 d) d) d (subst '(72 . 0) (assoc 72 d) d) d (subst '(73 . 0) (assoc 73 d) d) d (subst '(10 0.0 0.0 0.0) (assoc 10 d) d) d (subst '(11 0.0 0.0 0.0) (assoc 11 d) d) bb (textbox d)) (setq sup (list '(0 . "TEXT") '(100 . "AcDbEntity") (cons 8 (cdr (assoc 8 d))) (cons 62 (if (assoc 62 d) (cdr (assoc 62 d)) 256)) '(100 . "AcDbText") '(10 0.0 0.0 0.0) (cons 40 (* h 0.5)) '(1 . "2") (cons 41 width) (cons 50 0.0) (cons 51 ob) (cons 7 sty) '(72 . 0) '(11 0.0 0.0 0.0) '(100 . "AcDbText") '(73 . 0)) sb (textbox sup)) (setq sx (- (car (cadr bb)) (car (car sb))) sy (* h 0.6620111731843575) minx (min (car (car bb)) (+ sx (car (car sb)))) maxx (max (car (cadr bb)) (+ sx (car (cadr sb)))) miny (min (cadr (car bb)) (+ sy (cadr (car sb)))) maxy (max (cadr (cadr bb)) (+ sy (cadr (cadr sb))))) (if (= mode "center") (setq bx (- (car anchor) (/ (+ minx maxx) 2.0)) by (- (cadr anchor) (/ (+ miny maxy) 2.0))) (setq bx (car anchor) by (cadr anchor))) (setq p (list bx by 0.0) d (subst (cons 10 p) (assoc 10 d) d) d (subst (cons 11 p) (assoc 11 d) d)) (if (not (entmod d)) (exit)) (entupd e) (setq p (list (+ bx sx) (+ by sy) 0.0) sup (subst (cons 10 p) (assoc 10 sup) sup) sup (subst (cons 11 p) (assoc 11 sup) sup) sup (entmakex sup)) (if (not sup) (exit)) (command "_.-GROUP" "_Create" name "" e sup "") (setq gd (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))) members (dictsearch gd name)) (if (not members) (exit)) (bcp-log (strcat "CREATE|" name "|" handle "|" (cdr (assoc 5 (entget sup))))) (princ))'''
loghelper='(defun bcp-log (txt / f) (setq f (open bcp-logpath "a")) (write-line txt f) (close f))'
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "OSMODE" 0)']
result=dict(contract='bcp.area-symbol-sample.v1',version='v125',pluginCodeChanged=False,sourceManifestSHA256=sha(HERE/'drawing-preview-manifest.json'),sourceFiles=m['sourceHashes'],pluginBinarySHA256=m['pluginBinarySHA256'],session=str(session),nonce=b['nonce'],typography=dict(source='TCH CFCF public explode',superscriptHeightFactor=0.5,baselineRiseFactor=0.6620111731843575,objects='two AcDbText grouped per annotation; ordinary text allowed by user'),floors=[])
for f in m['floors']:
 n=f['floor'];qa=next(x for x in prior['floors'] if x['floor']==n);assert sha(qa['DWG'])==qa['DWGSHA256'];folder=job/f'area-symbol-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(qa['DWG'],control)
 oldaudit=read(Path(qa['previewJob'])/'product-axis-document-audit.json');oldtexts={x['handle']:x for x in oldaudit['nativeAnnotations'] if x['type']=='AcDbText'};tabs=[x.split() for x in Path(f['log']).read_text(encoding='cp936').splitlines()];pairs=[]
 for row in tabs:
  if row[0]=='AREA':
   area=next(x for x in f['areas'] if x['region']==row[1]);pairs.append(dict(id=row[1],bodyHandle=row[3],bodyText=f'{area["expectedM2"]:.2f}m',anchor=area['position'],height=area['height'],mode='center',group='BCP_AREA_'+row[1].replace('-','_'),areaM2=area['expectedM2']))
  elif row[0]=='TOTAL':
   text=oldtexts[row[2]]['text'];assert text.endswith('m2');oldpreview=read(Path(qa['previewJob'])/'stage2-preview-native.json');e=next(x for x in oldpreview['originalBefore'] if x['handle']==row[2]);pairs.append(dict(id=f'F{n}-TOTAL',bodyHandle=row[2],bodyText=text[:-1],anchor=e['stretch']['points'][0],height=300,mode='left',group=f'BCP_AREA_F{n}_TOTAL',areaM2=f['totalM2']))
 log=folder/'symbol-creation.log';commands+=['_.OPEN',path(control),loghelper,helper,f'(setq bcp-logpath {path(log)})']
 for pair in pairs:commands.append(f'(bcp-area-symbol {string(pair["bodyHandle"])} {string(pair["bodyText"])} {point(pair["anchor"])} {string(pair["mode"])} {string(pair["group"])})')
 notes=[]
 for h,e in oldtexts.items():
  if 'm2' in e['text'] and not any(x['bodyHandle']==h for x in pairs):
   new=e['text'].replace('m2','平方米');commands.append(f'(progn (setq bcp-e (handent {string(h)}) bcp-d (entget bcp-e)) (if (= (cdr (assoc 0 bcp-d)) "TEXT") (progn (entmod (subst (cons 1 {string(new)}) (assoc 1 bcp-d) bcp-d)) (entupd bcp-e))))');notes.append(dict(handle=h,text=new))
 commands+=['_.REGEN','_.QSAVE','_.CLOSE','_.OPEN',path(control)]
 readback=folder/'symbol-reopened.log';commands+=[f'(setq bcp-f (open {path(readback)} "w") bcp-gd (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))))']
 for pair in pairs:
  commands.append(f'(progn (write-line {string("GROUP|"+pair["id"]+"|"+pair["group"])} bcp-f) (foreach bcp-r (dictsearch bcp-gd {string(pair["group"])}) (if (= (car bcp-r) 340) (progn (setq bcp-d (entget (cdr bcp-r)) bcp-p (cdr (assoc 10 bcp-d))) (write-line (strcat "TEXT|" (cdr (assoc 5 bcp-d)) "|" (cdr (assoc 1 bcp-d)) "|" (rtos (cdr (assoc 40 bcp-d)) 2 12) "|" (rtos (car bcp-p) 2 12) "|" (rtos (cadr bcp-p) 2 12) "|" (cdr (assoc 7 bcp-d))) bcp-f)))))')
 commands+=['(write-line (strcat "COUNT|" (itoa (sslength (ssget "_X" \'((410 . "Model")))))) bcp-f)','(close bcp-f)']
 result['floors'].append(dict(floor=n,source=qa['DWG'],sourceSHA256=qa['DWGSHA256'],control=str(control),expectedModelCount=qa['modelRoots']+len(pairs),creationLog=str(log),readbackLog=str(readback),pairs=pairs,notes=notes,oldPreview=qa['previewJob']))
commands+=['(princ)'];boot=session/'boot.scr';boot.write_bytes(('\r\n'.join(commands)+'\r\n').encode('cp936'));b['bootSHA256']=sha(boot);b['scope']='One-off area superscript typography sample, no C++ changes, no COM/CUA/TCH business commands';bpath.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');result['bootSHA256']=sha(boot);(HERE/'area-symbol-manifest-v125.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(session=str(session),counts=[f['expectedModelCount'] for f in result['floors']]),ensure_ascii=False))
