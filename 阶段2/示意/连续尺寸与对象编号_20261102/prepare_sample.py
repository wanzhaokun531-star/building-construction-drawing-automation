"""One-off DWG drawing batch. Does not modify/build plugin source or binaries."""
from pathlib import Path
import json, hashlib, shutil, sys
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
def forms(src):
 s=src.strip()[1:-1];out=[];start=0;depth=0;quoted=False;escape=False
 for i,c in enumerate(s):
  if quoted:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c=='"':quoted=False
  elif c=='"':quoted=True
  elif c=='(':depth+=1
  elif c==')':depth-=1;assert depth>=0
  if c.isspace() and not quoted and depth==0:
   if s[start:i].strip():out.append(s[start:i].strip())
   start=i+1
 if s[start:].strip():out.append(s[start:].strip())
 assert depth==0 and not quoted
 return out
def helper(src):
 if len(src.encode('cp936'))<1600:return [src]
 p=forms(src);assert p[0]=='defun'
 return ["(setq bcp-def '("+' '.join(p[:3])+'))']+["(setq bcp-def (append bcp-def (list '"+x+')))' for x in p[3:]]+['(eval bcp-def)']
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job'])
prior=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-qa-v136.json')
baseline=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-manifest-v136.json')
assert sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==baseline['pluginBinarySHA256']==b['moduleSHA256']
assert all(sha(ROOT/p)==h for p,h in baseline['sourceFiles'].items())
helpers=[
 '(defun bcp-log (s / f) (setq f (open BCPLOG "a")) (write-line s f) (close f) (princ))',
 '(defun bcp-roots (/ s i out) (setq s (ssget "_X" \'((410 . "Model"))) i 0 out nil) (repeat (sslength s) (setq out (cons (ssname s i) out) i (1+ i))) out)',
 '(defun bcp-prop (h key / e r) (setq e (handent h) r (vl-catch-all-apply \'getpropertyvalue (list e key))) (bcp-log (strcat "PROPERTY|" h "|" key "|" (if (vl-catch-all-error-p r) (strcat "ERROR|" (vl-catch-all-error-message r)) (strcat "VALUE|" (vl-prin1-to-string r))))) (princ))',
 '(defun bcp-merge (first second / before after a c candidates e) (setq before (bcp-roots) a (handent first) c (handent second)) (if (or (not a) (not c) (eq a c)) (exit)) (bcp-log (strcat "MERGE_BEGIN|" first "|" second)) (command "TMergeDim" (ssadd a) (ssadd c) "") (setq after (bcp-roots) candidates nil) (foreach e after (if (or (not (member e before)) (eq e a) (eq e c)) (setq candidates (cons e candidates)))) (if (or (/= (length candidates) 1) (/= (length after) (1- (length before)))) (progn (bcp-log (strcat "MERGE_FAILED|" (itoa (length before)) "|" (itoa (length after)) "|" (itoa (length candidates)))) (exit))) (setq BCPRESULT (cdr (assoc 5 (entget (car candidates))))) (bcp-log (strcat "MERGED|" first "|" second "|" BCPRESULT "|" (itoa (length after)))) BCPRESULT)',
]
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)']
manifest=dict(contract='bcp.continuous-dimension-number-sample.v1',version=v,session=str(session),nonce=b['nonce'],pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=baseline['sourceFiles'],floors=[])
for f in prior['floors']:
 n=f['floor'];source=Path(f['DWG']);assert sha(source)==f['DWGSHA256'];folder=job/f'continuous-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log'
 audit=read(Path(f['previewJob'])/'product-axis-document-audit.json');dims={e['handle']:e for e in audit['nativeAnnotations'] if e['type']=='TDbDimension2' and e['visible']}
 opening='7F36' if n==1 else '824B'
 preview=read(Path(f['previewJob'])/'stage2-preview-native.json');roots={e['handle']:e for e in preview['originalBefore']}
 if opening not in roots or roots[opening]['type'] not in ('TDbOpening','TDbGroupOpening'):
  opening=next(h for h,e in roots.items() if e['type']=='TDbOpening')
 lines=[f'(setq BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)',f'(bcp-prop {st(opening)} "LayerId")',f'(bcp-prop {st(opening)} "编号")',f'(setq bcp-dump-result (vl-catch-all-apply \'dumpallproperties (list (handent {st(opening)}))))',f'(bcp-log (strcat "DUMP_PROPERTIES|{opening}|" (if (vl-catch-all-error-p bcp-dump-result) (vl-catch-all-error-message bcp-dump-result) "DONE")))']
 if n==1:
  # The three source chains measure the same x wall face; their two gaps are real 240mm wall widths.
  chain=[dims[h] for h in ('847E','8498','8485')];nodes=[]
  for d in chain:nodes+=d['grips'][2:len(d['values'])+3]
  assert all(abs(p[0]-nodes[0][0])<1e-6 for p in nodes)
  stations=sorted({p[1] for p in nodes});assert len(stations)==11
  lines+=['(setq BCPLAYER (cdr (assoc 8 (entget (handent "847E")))) BCPLAYERENTITY (tblobjname "LAYER" BCPLAYER) BCPLAYERDATA (entget BCPLAYERENTITY) BCPLAYERFLAGS (cdr (assoc 70 BCPLAYERDATA)))','(if (/= 0 (logand BCPLAYERFLAGS 4)) (entmod (subst (cons 70 (logand BCPLAYERFLAGS -5)) (assoc 70 BCPLAYERDATA) BCPLAYERDATA)))','(command "_.ZOOM" "_E")','(bcp-merge "847E" "8498")','(bcp-merge BCPRESULT "8485")','(setq BCPLAYERDATA (entget BCPLAYERENTITY))','(entmod (subst (cons 70 BCPLAYERFLAGS) (assoc 70 BCPLAYERDATA) BCPLAYERDATA))', '(bcp-log (strcat "RESULT|" BCPRESULT))']
 else:chain=[];stations=[]
 lines+=['(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)']
 body=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-edit-sample (cons nil bcp-body))))",'(bcp-edit-sample)']
 commands+=['_.OPEN',path(control),*[line for h in helpers for line in helper(h)],*body,'_.CLOSE','_.OPEN',path(control),f'(setq BCPLOG {path(log)})',helpers[0],helpers[1],f'(bcp-log (strcat "REOPENED|F{n}|" (itoa (length (bcp-roots)))))']
 manifest['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),control=str(control),log=str(log),sourcePreview=str(Path(f['previewJob'])/'stage2-preview-native.json'),sourceAudit=str(Path(f['previewJob'])/'product-axis-document-audit.json'),expectedModelCount=len(roots)-(2 if n==1 else 0),oldChains=chain,stations=stations,propertyProbeOpening=opening))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
balance=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:balance+=(c=='(')-(c==')');assert balance>=0
assert balance==0 and not quoted
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(boot);b['scope']='DWG sample only; authorized TMergeDim joins existing chains; read-only native getpropertyvalue/dumpallproperties probe; no COM/CUA or plugin source changes';bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');manifest['bootSHA256']=sha(boot);(HERE/f'manifest-{v}.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,session=str(session),job=str(job),stations=manifest['floors'][0]['stations']),ensure_ascii=False))
