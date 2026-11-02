"""One-off DWG sample placement; no plugin source or binary changes."""
from pathlib import Path
import json,hashlib,sys,shutil
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(s):return '"'+str(s).replace('\\','\\\\').replace('"','\\"')+'"'
def path(p):return st(Path(p).as_posix())
def pt(p):return "'("+' '.join(format(float(x),'.12f') for x in p)+')'
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
version=sys.argv[1];plan=HERE/'area-name-placement-final.json';layout=read(plan)
assert layout['all20HaveJointClearCandidates'] and all(f['jointClearSolutionFound'] and not f['newLabelContacts'] for f in layout['floors'])
bindingPath=ROOT/f'阶段2/evidence/native-host-binding-{version}.json';binding=read(bindingPath);session=Path(binding['session']);job=Path(binding['job'])
previous=read(ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/door-clearance-manifest-v134.json')
assert binding['moduleSHA256']==previous['pluginBinarySHA256']
assert all(sha(ROOT/p)==v for p,v in previous['sourceFiles'].items())
helper='(defun bcp-log (txt / f) (setq f (open BCPLOG "a")) (write-line txt f) (close f))'
groupHelper='(defun bcp-area-members (name expected / d out e) (setq d (dictsearch (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))) name) out nil) (foreach row d (if (= (car row) 340) (setq out (cons (cdr row) out)))) (if (/= (length out) 2) (exit)) (foreach h expected (setq e (handent h)) (if (or (not (member e out)) (/= (cdr (assoc 0 (entget e))) "TEXT")) (exit))) (princ))'
unlockHelper='(defun bcp-unlock (h / n row flags) (setq n (cdr (assoc 8 (entget (handent h))))) (if (not (assoc n BCPSTATES)) (progn (setq row (entget (tblobjname "LAYER" n)) flags (cdr (assoc 70 row)) BCPSTATES (cons (list n flags) BCPSTATES)) (if (/= 0 (logand flags 4)) (entmod (subst (cons 70 (logand flags -5)) (assoc 70 row) row))))) (princ))'
restoreHelper='(defun bcp-restore (/ row e d) (foreach row BCPSTATES (setq e (tblobjname "LAYER" (car row)) d (entget e)) (if (not (entmod (subst (cons 70 (cadr row)) (assoc 70 d) d))) (exit))) (foreach row BCPSTATES (if (/= (cdr (assoc 70 (tblsearch "LAYER" (car row)))) (cadr row)) (exit))) (princ))'
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)']
manifest=dict(contract='bcp.area-under-name-sample.v1',version=version,session=str(session),nonce=binding['nonce'],plan=str(plan),planSHA256=sha(plan),pluginCodeChanged=False,pluginBinarySHA256=binding['moduleSHA256'],sourceFiles=previous['sourceFiles'],floors=[])
for f in layout['floors']:
 n=f['floor'];assert len(f['areas'])==10 and sha(f['source'])==f['sourceSHA256'];preview=read(f['sourcePreview']);objects={e['handle']:e for e in preview['originalBefore']};folder=job/f'area-name-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(f['source'],control);log=folder/'drawing-edit.log';reopened=folder/'drawing-reopened.log';moves={}
 for a in f['areas']:
  assert len(a['members'])==2 and near(a['desiredBounds'],[v+a['delta'][i%2] for i,v in enumerate(a['currentBounds'])])
  for h in a['members']:assert h not in moves and objects[h]['type']=='AcDbText';moves[h]=a['delta']
 for move in f.get('nameMoves',[]):assert move['handle'] not in moves;moves[move['handle']]=move['delta']
 lines=[f'(setq BCPLOG {path(log)} BCPSTATES nil BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)']
 for a in f['areas']:lines.append(f'(bcp-area-members {st(a["group"])} \'('+' '.join(st(h) for h in a['members'])+'))')
 for h in moves:lines.append(f'(bcp-unlock {st(h)})')
 for h,delta in moves.items():
  if any(abs(x)>1e-8 for x in delta):lines.append(f'(progn (command "_.MOVE" (handent {st(h)}) "" "_non" \'(0.0 0.0 0.0) "_non" {pt(delta)}) (bcp-log "MOVED|{h}"))')
 lines.append('(setq BCPFRONT (ssadd))')
 for h in moves:lines.append(f'(ssadd (handent {st(h)}) BCPFRONT)')
 lines+=['(command "_.DRAWORDER" BCPFRONT "" "_Front")','(bcp-restore)','(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.ZOOM" "_E")','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)']
 body=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+line+')))' for line in lines]+["(eval (cons 'defun (cons 'bcp-edit-area-name (cons nil bcp-body))))",'(bcp-edit-area-name)']
 commands+=['_.OPEN',path(control),helper,groupHelper,unlockHelper,restoreHelper,*body,'_.CLOSE','_.OPEN',path(control),f'(setq bcp-f (open {path(reopened)} "w") bcp-gd (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))))']
 for a in f['areas']:commands.append(f'(progn (write-line {st("GROUP|"+a["id"]+"|"+a["group"])} bcp-f) (foreach bcp-r (dictsearch bcp-gd {st(a["group"])}) (if (= (car bcp-r) 340) (progn (setq bcp-d (entget (cdr bcp-r))) (write-line (strcat "MEMBER|" (cdr (assoc 5 bcp-d)) "|" (cdr (assoc 0 bcp-d))) bcp-f) (write-line (strcat "AREA-TEXT|" (cdr (assoc 5 bcp-d)) "|" (cdr (assoc 1 bcp-d)) "|" (cdr (assoc 7 bcp-d)) "|" (rtos (cdr (assoc 40 bcp-d)) 2 12) "|" (rtos (if (assoc 41 bcp-d) (cdr (assoc 41 bcp-d)) 1.0) 2 12) "|" (rtos (if (assoc 50 bcp-d) (cdr (assoc 50 bcp-d)) 0.0) 2 12) "|" (rtos (if (assoc 51 bcp-d) (cdr (assoc 51 bcp-d)) 0.0) 2 12)) bcp-f)))))')
 commands+=['(write-line (strcat "COUNT|" (itoa (sslength (ssget "_X" \'((410 . "Model")))))) bcp-f)','(close bcp-f)']
 manifest['floors'].append(dict(floor=n,source=f['source'],sourceSHA256=f['sourceSHA256'],sourcePreview=f['sourcePreview'],control=str(control),expectedModelCount=len(objects),log=str(log),reopenedLog=str(reopened),moves=moves,areas=f['areas'],nameMoves=f.get('nameMoves',[])))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
balance=0;string=False;escaped=False
for c in text:
 if escaped:escaped=False;continue
 if string and c=='\\':escaped=True;continue
 if c=='"':string=not string
 elif not string:balance+=(c=='(')-(c==')');assert balance>=0
assert balance==0 and not string
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));binding['bootSHA256']=sha(boot);binding['scope']='DWG review sample only: room-area MOVE and existing-name MOVE, ordinary AutoCAD commands plus existing ObjectARX readback; no COM/CUA/TCH command or plugin edits';bindingPath.write_text(json.dumps(binding,ensure_ascii=False,indent=2),encoding='utf-8');manifest['bootSHA256']=sha(boot);(HERE/f'area-name-manifest-{version}.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(session=str(session),modelCounts=[f['expectedModelCount'] for f in manifest['floors']],textAndNameMoves=[len(f['moves']) for f in manifest['floors']]),ensure_ascii=False))
