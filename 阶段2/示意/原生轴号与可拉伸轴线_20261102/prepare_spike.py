"""One-off native-axis DWG spike; no product/source implementation edits."""
from pathlib import Path
import ast,json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
def pt(v):return "'("+' '.join(format(float(x),'.12f') for x in v)+')'
def literal(f,name):
 tree=ast.parse(Path(f).read_text('utf-8-sig'));return ast.literal_eval(next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets)))
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
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);source=Path(b['input']);inspection=read(ROOT/'阶段2/示意/用户修改对比_20261102/inspection-v155.json');assert sha(source)==inspection['sourceSHA256'];control=job/'axis-spike/control.dwg';control.parent.mkdir();shutil.copyfile(source,control);log=control.parent/'drawing-edit.log';numbered=control.parent/'axis-renumber-21.dwg';shifted=control.parent/'axis-line-stretch-123.dwg'
treeHelpers=literal(ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/prepare_door_clearance.py','helpers')[:2];boot=(session/'boot.scr').read_bytes();marker=b'S1QUIETSTART\r\n';assert boot.count(marker)==1;commands=[boot[:boot.index(marker)+len(marker)].decode('ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)','_.OPEN',path(control),*[x for h in treeHelpers for x in helper(h)]]
lines=[f'(setq BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE") BCPOLDPATH (getvar "LOGFILEPATH"))','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)',f'(setvar "LOGFILEPATH" {st(control.parent.as_posix()+"/")})','(setvar "LOGFILEMODE" 1)','(bcp-log (strcat "COMMAND_LOG|" (getvar "LOGFILENAME")))']
for component in ['tch_kerncmd.arx','tch_annotate.arx']:
 lines.append(f'(if (or (not (getcname "TReAxisNo")) (not (getcname "TObjEdit"))) (vl-catch-all-apply \'arxload (list {path(Path("C:/Tangent/TArchT20V8/sys23x64")/component)})))')
for name,carrier,var in [('1','83CA','BCPAX1'),('2','83D1','BCPAX2')]:
 lines += [f'(command "_.-GROUP" "_Explode" "BCP_GRID_V_{name}")',f'(command "_.EXPLODE" (handent "{carrier}") "")',f'(setq {var} (entlast))',f'(bcp-log (strcat "DIRECT_AXIS|{name}|" (cdr (assoc 5 (entget {var}))) "|" (cdr (assoc 0 (entget {var})))))']
x,y=78453.70152820693,40825.213801957034
lines += [f'(command "_.ZOOM" "_W" "_non" {pt([x-800,y-800,0])} "_non" {pt([x+800,y+800,0])})',f'(command "TReAxisNo" {pt([x+400,y,0])} "21" "")','(bcp-log "RENUMBER_RETURNED|21")','(command "_.QSAVE")',f'(if (not (vl-file-copy {path(control)} {path(numbered)})) (exit))',f'(command "TReAxisNo" {pt([x+400,y,0])} "1" "")','(bcp-log "RENUMBER_RESTORED|1")']
x=80853.70152820693
lines += [f'(command "_.ZOOM" "_W" "_non" {pt([x-800,y-800,0])} "_non" {pt([x+800,y+800,0])})',f'(command "TObjEdit" {pt([x+400,y,0])} "M" "")','(bcp-log "OBJECT_EDIT_M_RETURNED|2")','(command "_.ZOOM" "_E")']
x,y=78453.70152820693,37225.213801957034
for offset,delta in [(0,123),(123,-123)]:
 lo=[x-25,y+offset-25,0];hi=[x+25,y+offset+25,0];wins="'(("+pt(lo)[1:]+' '+pt(hi)[1:]+'))';lines += [f'(bcp-stretch-one "841B" {wins} {pt([0,delta,0])} "native-axis-line")']
 if delta>0:lines += ['(command "_.QSAVE")',f'(if (not (vl-file-copy {path(control)} {path(shifted)})) (exit))']
lines += ['(setvar "LOGFILEMODE" 0)','(setvar "LOGFILEPATH" BCPOLDPATH)','(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.QSAVE")','(bcp-log "SAVED|SPIKE")','(princ)'];commands+=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-axis-spike (cons nil bcp-body))))",'(bcp-axis-spike)']
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
out=session/'boot.scr';out.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(out);b['scope']='DWG copy only: expose two genuine TCH axis labels, TReAxisNo 21 then1, TObjEdit M, line stretch123 then restore; no COM/CUA/product code changes';bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');m=dict(version=v,session=str(session),nonce=b['nonce'],control=str(control),log=str(log),source=str(source),sourceSHA256=sha(source),numbered=str(numbered),shifted=str(shifted),expectedModelCount=241);(HERE/f'spike-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print('AXIS_SPIKE_PREPARED')
