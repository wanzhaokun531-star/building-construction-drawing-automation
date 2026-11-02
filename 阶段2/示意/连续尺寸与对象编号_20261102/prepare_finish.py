"""One-off native drawing batch: keep the merged chain, move its text grips."""
from pathlib import Path
import ast,json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
def pt(v):return "'("+' '.join(format(float(x),'.12f') for x in v)+')'
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
old=read(HERE/'manifest-v137.json');previews={f['floor']:f for f in read(HERE/'native-previews-v137.json')}
assert all(sha(ROOT/p)==h for p,h in old['sourceFiles'].items())
assert sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==old['pluginBinarySHA256']==b['moduleSHA256']
tree=ast.parse((ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/prepare_door_clearance.py').read_text(encoding='utf-8-sig'))
known=ast.literal_eval(next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='helpers' for t in n.targets)))
helpers=known[:2]+[
 '(defun bcp-prop (h key / e r) (setq e (handent h) r (vl-catch-all-apply \'getpropertyvalue (list e key))) (bcp-log (strcat "PROPERTY|" h "|" key "|" (if (vl-catch-all-error-p r) (strcat "ERROR|" (vl-catch-all-error-message r)) (strcat "VALUE|" (vl-prin1-to-string r))))) (princ))',
]
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)']
m=dict(contract='bcp.continuous-chain-clearance-sample.v1',version=v,session=str(session),nonce=b['nonce'],pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=old['sourceFiles'],floors=[])
for f in old['floors']:
 n=f['floor'];source=Path(f['control']);folder=job/f'continuous-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log';reopened=folder/'drawing-reopened.log';preview=read(Path(previews[n]['job'])/'stage2-preview-native.json');audit=read(Path(previews[n]['job'])/'product-axis-document-audit.json');roots={e['handle']:e for e in preview['originalBefore']};ops=[]
 lines=[f'(setq BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE") BCPOLDLOG (getvar "LOGFILEMODE") BCPOLDLOGPATH (getvar "LOGFILEPATH"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)',f'(setvar "LOGFILEPATH" {st(folder.as_posix()+"/")})','(setvar "LOGFILEMODE" 1)','(bcp-log (strcat "PROPERTY_DUMP_LOG|" (getvar "LOGFILENAME")))']
 candidates=[(h,e) for h,e in roots.items() if e['type'] in ['TDbOpening','TDbGroupOpening']]
 # Probe one door and one window by known source numbering associations, not private data.
 handles=['7F36','7F5F'] if n==1 else ['8015','8017']
 handles=[h for h in handles if h in roots and roots[h]['type'] in ['TDbOpening','TDbGroupOpening']]
 if len(handles)<2:handles += [h for h,e in candidates if h not in handles][:2-len(handles)]
 for h in handles:
  lines += [f'(bcp-prop {st(h)} "LayerId")',f'(bcp-prop {st(h)} "编号")',f'(princ "\\nBCP_PROPERTY_DUMP_BEGIN_{h}\\n")',f'(vl-catch-all-apply \'dumpallproperties (list (handent {st(h)})))',f'(princ "\\nBCP_PROPERTY_DUMP_END_{h}\\n")']
 lines+=['(setvar "LOGFILEMODE" BCPOLDLOG)','(setvar "LOGFILEPATH" BCPOLDLOGPATH)']
 if n==1:
  d=next(e for e in audit['nativeAnnotations'] if e['handle']=='847E');gp=d['grips'];assert len(d['values'])==10 and len(gp)==26
  y=[(gp[2+j][1]+gp[3+j][1])/2 for j in range(10)]
  for j in range(10):
   k=13+j;before=gp[k];x=38121.70152820693 if j==2 else (37751.70152820693 if j in [3,7] else 37301.70152820693);target=[x,y[j],0];delta=[a-z for a,z in zip(target,before)];lo=[before[0]-25,before[1]-25,0];hi=[before[0]+25,before[1]+25,0]
   assert [i for i,p in enumerate(gp) if lo[0]<=p[0]<=hi[0] and lo[1]<=p[1]<=hi[1]]==[k]
   # A displaced text grip need not lie on actual selectable ink. A second
   # narrow window crosses the same chain's baseline, still enclosing only
   # this text grip; explicit target removal guards all other entities.
   lo2=[min(before[0],gp[0][0])-30,before[1]-25,0];hi2=[max(before[0],gp[0][0])+30,before[1]+25,0]
   assert [i for i,p in enumerate(gp) if lo2[0]<=p[0]<=hi2[0] and lo2[1]<=p[1]<=hi2[1]]==[k]
   windows="'(("+pt(lo)[1:]+' '+pt(hi)[1:]+') ('+pt(lo2)[1:]+' '+pt(hi2)[1:]+'))'
   lines.append(f'(bcp-stretch-one "847E" {windows} {pt(delta)} {st("text"+str(k))})');ops.append(dict(handle='847E',gripIndex=k,before=before,target=target,delta=delta));gp[k]=target
  expectedGrips=gp
 else:expectedGrips=None
 lines+=['(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.ZOOM" "_E")','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)']
 body=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-finish (cons nil bcp-body))))",'(bcp-finish)']
 # Only the edited floor enters the boot. The existing SDK queue subsequently
 # opens fresh copies of both saved files for independent readback/rendering.
 # There are no unguarded CLOSE/OPEN commands following a failed edit.
 if n==1:commands+=['_.OPEN',path(control),*[line for h in helpers for line in helper(h)],*body]
 m['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),sourcePreview=str(Path(previews[n]['job'])/'stage2-preview-native.json'),sourceAudit=str(Path(previews[n]['job'])/'product-axis-document-audit.json'),originalSource=f['source'],originalSourceSHA256=f['sourceSHA256'],originalSourcePreview=f['sourcePreview'],originalSourceAudit=f['sourceAudit'],control=str(control),log=str(log),reopenedLog=str(reopened),expectedModelCount=f['expectedModelCount'],ops=ops,expectedGrips=expectedGrips,propertyProbeHandles=handles))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(boot);b['scope']='DWG sample only, native STRETCH on joined dimension text grips; read-only property discovery; no COM/CUA/TCH commands or plugin source edits';bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');m['bootSHA256']=sha(boot);(HERE/f'manifest-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,session=str(session),ops=[len(f['ops']) for f in m['floors']]),ensure_ascii=False))
