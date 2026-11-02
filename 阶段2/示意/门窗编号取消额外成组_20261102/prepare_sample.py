"""Dissolve only extra CAD groups. Keep genuine Tianzheng opening numbering."""
from pathlib import Path
import json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);previous=ROOT/'阶段2/示意/门窗内部编号_20261102';sourceManifest=read(previous/'manifest-v147.json');delivery=read(previous/'delivery-v147.json');qa=read(previous/'qa-v147.json')
assert all(sha(ROOT/p)==h for p,h in sourceManifest['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==sourceManifest['pluginBinarySHA256']==b['moduleSHA256']
logHelper='(defun bcp-log (s / f) (setq f (open BCPLOG "a")) (write-line s f) (close f) (princ))'
dumpHelper='(defun bcp-dump-groups (file / f gd name data ent) (setq f (open file "w") gd (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP")))) (foreach row (entget gd) (cond ((= (car row) 3) (setq name (cdr row))) ((= (car row) 350) (setq data (entget (cdr row))) (write-line (strcat "GROUP|" name "|" (cdr (assoc 5 data))) f) (foreach member data (if (= (car member) 340) (progn (setq ent (entget (cdr member))) (write-line (strcat "MEMBER|" (cdr (assoc 5 ent)) "|" (cdr (assoc 0 ent))) f))))))) (close f) (princ))'
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)'];m=dict(contract='bcp.native-number-no-extra-group.v1',version=v,session=str(session),nonce=b['nonce'],pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=sourceManifest['sourceFiles'],floors=[])
for f in sourceManifest['floors']:
 n=f['floor'];dd=next(x for x in delivery['DWGs'] if x['floor']==n);old=next(x for x in qa['floors'] if x['floor']==n);source=Path(dd['DWG']);assert sha(source)==dd['SHA256'];folder=job/f'ungroup-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log';groups=folder/'reopened-groups.log';groupNames=[x['group'] for x in f['ops'] if x['group']];assert len(groupNames)==len(set(groupNames))==(10 if n==1 else 9)
 body=[f'(setq BCPLOG {path(log)} BCPGD (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))))',f'(bcp-log "BEGIN|F{n}")']
 for name in groupNames:
  body += [f'(if (not (dictsearch BCPGD {st(name)})) (progn (bcp-log "MISSING_GROUP|{name}") (exit)))',f'(command "_.-GROUP" "_Explode" {st(name)})',f'(if (dictsearch BCPGD {st(name)}) (progn (bcp-log "GROUP_NOT_DISSOLVED|{name}") (exit)))',f'(bcp-log "DISSOLVED|{name}")']
 body+=['(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)'];commands+=['_.OPEN',path(control),logHelper,'(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in body]+["(eval (cons 'defun (cons 'bcp-dissolve-number-groups (cons nil bcp-body))))",'(bcp-dissolve-number-groups)','_.CLOSE','_.OPEN',path(control),dumpHelper,f'(bcp-dump-groups {path(groups)})']
 m['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),sourcePreview=str(Path(old['previewJob'])/'stage2-preview-native.json'),sourceAudit=str(Path(old['previewJob'])/'product-axis-document-audit.json'),control=str(control),log=str(log),groupsLog=str(groups),removedGroups=groupNames,targets=f['targets'],oldGroups=old['groups'],expectedModelCount=old['modelRoots']))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
balance=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:balance+=(c=='(')-(c==')');assert balance>=0
assert balance==0 and not quoted
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(boot);b['scope']='DWG sample only: dissolve 19 extra native CAD groups, keep existing TChWinLab-generated opening numbers; no COM/CUA/TCH command/plugin changes';bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');m['bootSHA256']=sha(boot);(HERE/f'manifest-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,session=str(session),groups=[len(f['removedGroups']) for f in m['floors']]),ensure_ascii=False))
