"""Move the native opening label grip, replace duplicate standalone text only."""
from pathlib import Path
import ast,json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
def pt(v):return "'("+' '.join(format(float(x),'.12f') for x in v)+')'
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
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
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);assignment=read(HERE/'manifest-v145.json');previews={f['floor']:f for f in read(HERE/'assignment-previews-v145.json')}
assert all(sha(ROOT/p)==h for p,h in assignment['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==assignment['pluginBinarySHA256']==b['moduleSHA256']
tree=ast.parse((ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/prepare_door_clearance.py').read_text(encoding='utf-8-sig'));helpers=ast.literal_eval(next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='helpers' for t in n.targets)))[:2]
area=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-qa-v136.json');door=read(ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/door-clearance-manifest-v134.json')
originalBoot=(session/'boot.scr').read_bytes();marker=b'S1QUIETSTART\r\n';assert originalBoot.count(marker)==1
commands=[originalBoot[:originalBoot.index(marker)+len(marker)].decode('ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)']
m=dict(contract='bcp.native-opening-label-placement.v1',version=v,session=str(session),nonce=b['nonce'],pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=assignment['sourceFiles'],floors=[])
for f in assignment['floors']:
 n=f['floor'];source=Path(f['control']);folder=job/f'number-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log';groupsLog=folder/'groups.log';p=read(Path(previews[n]['job'])/'stage2-preview-native.json');a=read(Path(previews[n]['job'])/'product-axis-document-audit.json');roots={e['handle']:e for e in p['originalBefore']};texts={e['handle']:e for e in a['nativeAnnotations'] if e['type']=='AcDbText'};masks={c['opening']:c for c in next(z for z in door['floors'] if z['floor']==n)['masks']};ops=[];lines=[f'(setq BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)']
 for t in f['targets']:
  h=t['opening'];caption=t['caption'];gp=roots[h]['grip']['points'];k=len(gp)-1;before=gp[k];target=texts[caption]['position'][0];delta=[x-y for x,y in zip(target,before)];windows=[]
  for dx,dy in [(25,25),(100,100),(180,300),(300,180),(300,300)]:
   lo=[before[0]-dx,before[1]-dy,0];hi=[before[0]+dx,before[1]+dy,0]
   if [i for i,p in enumerate(gp) if lo[0]<=p[0]<=hi[0] and lo[1]<=p[1]<=hi[1]]==[k]:windows.append((lo,hi))
  assert windows and all(abs(v)<30000 for v in delta)
  if not near(before,target):
   wins="'("+' '.join('('+pt(lo)[1:]+' '+pt(hi)[1:]+')' for lo,hi in windows)+')';lines.append(f'(bcp-stretch-one {st(h)} {wins} {pt(delta)} {st("opening-label"+str(k))})')
  if h in masks:
   g=masks[h]['group'];lines += [f'(command "_.-GROUP" "_Remove" {st(g)} (handent {st(caption)}) "")',f'(command "_.-GROUP" "_Add" {st(g)} (handent {st(h)}) "")']
  lines += [f'(command "_.ERASE" (handent {st(caption)}) "")',f'(if (and (handent {st(caption)}) (entget (handent {st(caption)}))) (progn (bcp-log "OLD_CAPTION_NOT_ERASED|{caption}") (exit)))',f'(command "_.DRAWORDER" (handent {st(h)}) "" "_Front")',f'(bcp-log "PLACED|{h}|{t["number"]}|{caption}")']
  ops.append(dict(**t,gripIndex=k,before=before,target=target,delta=delta,oldCaptionErased=True,group=masks[h]['group'] if h in masks else None))
 lines+=['(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(setvar "LOGFILEMODE" 0)','(command "_.ZOOM" "_E")','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")',f'(setq BCPGF (open {path(groupsLog)} "w") BCPGD (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))))']
 names=[x['group'] for x in masks.values()]+[x['name'] for x in next(z for z in area['floors'] if z['floor']==n)['groups'].values()]
 for name in names:lines.append(f'(progn (write-line {st("GROUP|"+name)} BCPGF) (foreach row (dictsearch BCPGD {st(name)}) (if (= (car row) 340) (write-line (strcat "MEMBER|" (cdr (assoc 5 (entget (cdr row)))) "|" (cdr (assoc 0 (entget (cdr row))))) BCPGF))))')
 lines+=['(close BCPGF)','(princ)'];body=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-place-labels (cons nil bcp-body))))",'(bcp-place-labels)'];commands+=['_.OPEN',path(control),*[line for h in helpers for line in helper(h)],*body]
 m['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),sourcePreview=str(Path(previews[n]['job'])/'stage2-preview-native.json'),sourceAudit=str(Path(previews[n]['job'])/'product-axis-document-audit.json'),originalSource=f['source'],originalSourceSHA256=f['sourceSHA256'],originalSourcePreview=f['sourcePreview'],originalSourceAudit=f['sourceAudit'],control=str(control),log=str(log),groupsLog=str(groupsLog),targets=f['targets'],ops=ops,expectedModelCount=len(roots)-len(ops)))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(boot);b['scope']='DWG sample only; native opening label grip STRETCH, remove obsolete independent Text, update native door groups; no COM/CUA/plugin source edits';bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');m['bootSHA256']=sha(boot);(HERE/f'manifest-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,session=str(session),labels=[len(f['ops']) for f in m['floors']],rootCounts=[f['expectedModelCount'] for f in m['floors']]),ensure_ascii=False))
