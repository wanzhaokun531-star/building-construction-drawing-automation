"""One-off authorized Tianzheng numbering batch; product sources stay frozen."""
from pathlib import Path
import ast,json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
def pt(v):return "'("+' '.join(format(float(x),'.12f') for x in v)+')'
def literal(file,name):
 t=ast.parse(Path(file).read_text(encoding='utf-8-sig'))
 return ast.literal_eval(next(n.value for n in t.body if isinstance(n,ast.Assign) and any(isinstance(x,ast.Name) and x.id==name for x in n.targets)))
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
 p=forms(src);return ["(setq bcp-def '("+' '.join(p[:3])+'))']+["(setq bcp-def (append bcp-def (list '"+x+')))' for x in p[3:]]+['(eval bcp-def)']
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);m=read(HERE/'number-plan.json')
assert all(sha(ROOT/p)==h for p,h in m['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==m['pluginBinarySHA256']==b['moduleSHA256']
helpers=literal(ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/prepare_door_clearance.py','helpers')[:2]
dump=literal(ROOT/'阶段2/示意/门窗编号取消额外成组_20261102/prepare_sample.py','dumpHelper')
boot=(session/'boot.scr').read_bytes();marker=b'S1QUIETSTART\r\n';assert boot.count(marker)==1
commands=[boot[:boot.index(marker)+len(marker)].decode('ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)']
m.update(version=v,session=str(session),nonce=b['nonce'],authorization='DWG sample copies only; explicitly allowed public TChWinLab numbering and native STRETCH; no COM/CUA/plugin changes')
for f in m['floors']:
 n=f['floor'];source=Path(f['source']);assert sha(source)==f['sourceSHA256'];folder=job/f'renumber-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log';groups=folder/'reopened-groups.log';roots={x['handle']:x for x in read(f['sourcePreview'])['originalBefore']}
 lines=[f'(setq BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE") BCPOLDPATH (getvar "LOGFILEPATH"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)',f'(setvar "LOGFILEPATH" {st(folder.as_posix()+"/")})','(setvar "LOGFILEMODE" 1)','(bcp-log (strcat "COMMAND_LOG|" (getvar "LOGFILENAME")))']
 for component in ['tch_kerncmd.arx','tch_annotate.arx']:
  lines.append(f'(if (not (getcname "TChWinLab")) (progn (setq BCPLOAD (vl-catch-all-apply \'arxload (list {path(Path("C:/Tangent/TArchT20V8/sys23x64")/component)}))) (bcp-log (strcat "LOAD|{component}|" (if (vl-catch-all-error-p BCPLOAD) (vl-catch-all-error-message BCPLOAD) (vl-prin1-to-string BCPLOAD))))))')
 lines+=['(if (not (getcname "TChWinLab")) (progn (bcp-log "NUMBER_COMMAND_NOT_REGISTERED") (exit)))','(command "_.ZOOM" "_E")']
 for o in f['openings']:
  if not o['changed']:continue
  h=o['opening'];before=o['labelGripBefore'];target=o['labelTarget'];delta=[x-y for x,y in zip(target,before)]
  if any(abs(x)>1e-6 for x in delta):
   gp=roots[h]['grip']['points'];windows=[]
   for dx,dy in [(25,25),(100,100),(180,300),(300,180),(300,300)]:
    lo=[before[0]-dx,before[1]-dy,0];hi=[before[0]+dx,before[1]+dy,0]
    if [i for i,p in enumerate(gp) if lo[0]<=p[0]<=hi[0] and lo[1]<=p[1]<=hi[1]]==[len(gp)-1]:windows.append((lo,hi))
   assert windows
   wins="'("+' '.join('('+pt(lo)[1:]+' '+pt(hi)[1:]+')' for lo,hi in windows)+')'
   lines.append(f'(bcp-stretch-one {st(h)} {wins} {pt(delta)} "opening-number-label")')
  lines += [f'(setq BCPOPENING (handent {st(h)}))',f'(if (not BCPOPENING) (progn (bcp-log "MISSING|{h}") (exit)))',f'(bcp-log "NUMBER_BEGIN|{h}|{o["newNumber"]}")',f'(command "TChWinLab" (ssadd BCPOPENING) "" {st(o["newNumber"])})',f'(bcp-log "NUMBER_RETURNED|{h}|{o["newNumber"]}")']
 if f.get('noteText'):
  lines += [f'(setq BCPND (entget (handent {st(f["noteStyleSource"])})))', '(if (/= (cdr (assoc 0 BCPND)) "TEXT") (exit))',f'(setq BCPNOTE (entmakex (list \'(0 . "TEXT") \'(100 . "AcDbEntity") (assoc 8 BCPND) \'(100 . "AcDbText") (cons 10 {pt(f["notePosition"])}) (assoc 40 BCPND) (cons 1 {st(f["noteText"])}) (assoc 7 BCPND) (assoc 41 BCPND) \'(50 . 0.0) \'(72 . 0) \'(73 . 0) (cons 11 {pt(f["notePosition"])}))))','(if (not BCPNOTE) (exit))','(bcp-log (strcat "NOTE|" (cdr (assoc 5 (entget BCPNOTE)))))']
 lines+=['(setvar "LOGFILEMODE" 0)','(setvar "LOGFILEPATH" BCPOLDPATH)','(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)']
 commands+=['_.OPEN',path(control),*[x for h in helpers for x in helper(h)],'(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-renumber (cons nil bcp-body))))",'(bcp-renumber)','_.CLOSE','_.OPEN',path(control),dump,f'(bcp-dump-groups {path(groups)})']
 f.update(control=str(control),log=str(log),groupsLog=str(groups))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
out=session/'boot.scr';out.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(out);b['scope']=m['authorization'];bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');m['bootSHA256']=sha(out);(HERE/f'manifest-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,changed=m['changedNumbers'],totals=m['totals']),ensure_ascii=False))
