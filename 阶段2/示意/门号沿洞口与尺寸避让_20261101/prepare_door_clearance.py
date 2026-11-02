"""One-off drawing edits, not plugin source. Same TCH dimensions remain whole."""
from pathlib import Path
import json,hashlib,shutil,copy,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def lisp_forms(src):
 s=src.strip();assert s[0]=='(' and s[-1]==')';s=s[1:-1];out=[];start=0;depth=0;quoted=False;escape=False
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
def script_helper(src):
 if len(src.encode('cp936'))<1800:return [src]
 parts=lisp_forms(src);assert parts[0]=='defun'
 return ["(setq bcp-definition '("+' '.join(parts[:3])+'))']+["(setq bcp-definition (append bcp-definition (list '"+p+')))' for p in parts[3:]]+['(eval bcp-definition)']
def st(s):return '"'+str(s).replace('\\','\\\\').replace('"','\\"')+'"'
def path(p):return st(Path(p).as_posix())
def pt(p):return "'("+' '.join(format(float(x),'.12f') for x in p[:3])+')'
def near(a,b):return all(abs(x-y)<1e-6 for x,y in zip(a,b))
def inside(p,w):return all(w['min'][i]-1e-7<=p[i]<=w['max'][i]+1e-7 for i in [0,1])
version=sys.argv[1] if len(sys.argv)>1 else 'v128'
bp=ROOT/f'阶段2/evidence/native-host-binding-{version}.json';binding=read(bp);session=Path(binding['session']);job=Path(binding['job'])
door=read(HERE/'door-caption-complete-and-dimension-plan.json');six=read(HERE/'dimension-text-escape-candidates.json');three=read(HERE/'three-other-chains-text-grips.json');merged=read(HERE/'merged-dimension-glyph-check.json')
extraPath=HERE/sys.argv[2] if len(sys.argv)>2 else None
extra=read(extraPath) if extraPath else dict(moves=[])
for name,h in merged['sourcePlans'].items():assert sha(HERE/name)==h
assert read(HERE/'stretch-trial-qa-v127.json')['completed']
helpers=[
 '(defun bcp-log (txt / f) (setq f (open BCPLOG "a")) (write-line txt f) (close f))',
 '''(defun bcp-stretch-one (handle windows delta kind / e data targetLayer states row name flags layer lo hi ss chosen selected i removeList oldpick oldos) (setq e (handent handle) data (entget e) targetLayer (cdr (assoc 8 data)) oldpick (getvar "PICKSTYLE") oldos (getvar "OSMODE") row (tblnext "LAYER" T) states nil) (while row (setq name (cdr (assoc 2 row)) flags (cdr (assoc 70 row)) states (cons (list name flags) states) layer (tblobjname "LAYER" name) data (entget layer)) (if (not (entmod (subst (cons 70 (if (= name targetLayer) (logand flags -5) (logior flags 4))) (assoc 70 data) data))) (exit)) (setq row (tblnext "LAYER"))) (setvar "PICKSTYLE" 0) (setvar "OSMODE" 0) (setq lo (car (car windows)) hi (cadr (car windows))) (command "_.ZOOM" "_W" "_non" (trans (mapcar '- lo '(600.0 600.0 0.0)) 0 1) "_non" (trans (mapcar '+ hi '(600.0 600.0 0.0)) 0 1)) (command "_.REGEN") (setq chosen nil) (foreach win windows (if (not chosen) (progn (setq lo (car win) hi (cadr win) ss (ssget "_C" (trans hi 0 1) (trans lo 0 1))) (if (and ss (ssmemb e ss)) (setq chosen win))))) (if (not chosen) (progn (bcp-log (strcat "FAIL_SELECTION|" handle "|" kind)) (exit))) (setq lo (car chosen) hi (cadr chosen) removeList nil i 0) (repeat (sslength ss) (setq selected (ssname ss i) data (entget selected) flags (cdr (assoc 70 (tblsearch "LAYER" (cdr (assoc 8 data)))))) (if (and (= 0 (logand flags 4)) (not (eq selected e))) (setq removeList (cons selected removeList))) (setq i (1+ i))) (bcp-log (strcat "SELECT|" handle "|" kind "|REMOVE=" (itoa (length removeList)))) (command "_.STRETCH" "_C" "_non" (trans hi 0 1) "_non" (trans lo 0 1)) (if removeList (progn (command "_Remove") (foreach other removeList (command other)))) (command "" "_non" '(0.0 0.0 0.0) "_non" (trans delta 0 1 T)) (foreach row states (setq layer (tblobjname "LAYER" (car row)) data (entget layer)) (if (not (entmod (subst (cons 70 (cadr row)) (assoc 70 data) data))) (exit))) (foreach row states (if (/= (cdr (assoc 70 (tblsearch "LAYER" (car row)))) (cadr row)) (exit))) (setvar "PICKSTYLE" oldpick) (setvar "OSMODE" oldos) (bcp-log (strcat "STRETCHED_LAYERS_RESTORED|" handle "|" kind)) (princ))''',
 '''(defun bcp-mask-template (name handle base / e last copy oldmode data modelOwner) (if (tblsearch "BLOCK" name) (exit)) (setq e (handent handle) modelOwner (cdr (assoc 330 (entget e))) last (entlast) oldmode (getvar "COPYMODE")) (setvar "COPYMODE" 1) (command "_.COPY" e "" "_non" '(0.0 0.0 0.0) "_non" '(0.0 0.0 0.0)) (setvar "COPYMODE" oldmode) (setq copy (entlast)) (if (or (eq copy last) (/= (cdr (assoc 0 (entget copy))) "WIPEOUT")) (exit)) (command "_.-BLOCK" name "_non" (trans base 0 1) copy "") (setq last (entlast) data (entget last)) (if (and (= (cdr (assoc 0 data)) "INSERT") (= (strcase (cdr (assoc 2 data))) (strcase name))) (command "_.ERASE" last "")) (if (and (entget copy) (eq (cdr (assoc 330 (entget copy))) modelOwner)) (command "_.ERASE" copy "")) (if (not (tblsearch "BLOCK" name)) (exit)) (bcp-log (strcat "MASK_TEMPLATE|" name)) (princ))''',
 '''(defun bcp-mask-number (name point sx sy caption group / last ref) (setq last (entlast)) (command "_.-INSERT" name "_non" (trans point 0 1) sx sy 0.0) (setq ref (entlast)) (if (or (eq ref last) (/= (cdr (assoc 0 (entget ref))) "INSERT") (/= (strcase (cdr (assoc 2 (entget ref)))) (strcase name))) (exit)) (ssadd ref BCPMASKS) (command "_.-GROUP" "_Create" group "Door number and dimension clearance" (handent caption) ref "") (bcp-log (strcat "MASK|" caption "|" (cdr (assoc 5 (entget ref))) "|" group)) (princ))'''
]
manifest=dict(contract='bcp.door-caption-clearance-sample.v1',version=version,session=str(session),nonce=binding['nonce'],pluginCodeChanged=False,pluginBinarySHA256=binding['moduleSHA256'],sourcePlans=merged['sourcePlans'],sourceFiles={**{str((ROOT/'阶段2/src'/p).relative_to(ROOT)):h for p,h in binding['sourceFiles'].items()},**{str((ROOT/'插件'/p).relative_to(ROOT)):h for p,h in binding['productSources'].items()}},floors=[],scope='Ordinary AutoCAD private-desktop drawing batch, native STRETCH/MOVE; no COM/CUA/TCH business command, no C++ edits')
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)']
for f in door['floors']:
 n=f['floor'];assert sha(f['source'])==f['sourceSHA256'];folder=job/f'clearance-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(f['source'],control);p=read(f['actualPreview']);idx={e['handle']:e for e in p['originalBefore']};audit=read(Path(f['actualPreview']).parent/'product-axis-document-audit.json');dims={e['handle']:e for e in audit['nativeAnnotations'] if e['type']=='TDbDimension2' and e['visible']};glyphs={x['handle']:x for x in read(f['captionWitness'])['dimensionAudit']};state={h:copy.deepcopy(d['grips']) for h,d in dims.items()};floorSix=next(x for x in six['floors'] if x['floor']==n);ops=[]
 for baseline in floorSix['baselineAssumptions']:
  h=baseline['handle'];d=dims[h];gp=state[h];delta=baseline['delta'];point=gp[0][:];w=dict(min=[point[0]-30,point[1]-30,0],max=[point[0]+30,point[1]+30,0]);assert [i for i,x in enumerate(gp) if inside(x,w)]==[0]
  ops.append(dict(kind='baseline',handle=h,gripIndex=0,before=point,delta=delta,windows=[w]));vertical=abs(gp[0][0]-gp[1][0])<1e-6;along=1 if vertical else 0;segments=(len(gp)-6)//2
  for j in range(segments):
   k=segments+3+j;mid=(gp[2+j][along]+gp[3+j][along])/2
   if abs(gp[k][along]-mid)<1e-6:gp[k]=[a+b for a,b in zip(gp[k],delta)]
  gp[0]=[a+b for a,b in zip(gp[0],delta)];gp[1]=[a+b for a,b in zip(gp[1],delta)]
 textRows=[]
 for t in three['plans']:
  if t['floor']==n:textRows.append(dict(handle=t['handle'],segment=t['segment'],gripIndex=t['textGripIndex'],before=t['selectionPoint'],target=t['desiredTextPoint'],delta=t['delta']))
 for t in floorSix['textMoves']:
  selected=t['selected'];assert selected is not None;textRows.append(dict(handle=t['handle'],segment=t['segment'],gripIndex=t['textIndex'],before=t['gripAfterPrerequisiteBaseline'],target=selected['target'],delta=selected['deltaFromAfterPrerequisite']))
 for t in extra['moves']:
  if t['floor']==n:textRows.append(t)
 for t in textRows:
  h=t['handle'];k=t['gripIndex'];gp=state[h];assert near(gp[k],t['before']) and near([a+b for a,b in zip(t['before'],t['delta'])],t['target']);point=t['before'];vertical=abs(gp[0][0]-gp[1][0])<1e-6;along=1 if vertical else 0;normal=1-along;w=dict(min=[point[0]-30,point[1]-30,0],max=[point[0]+30,point[1]+30,0]);assert [i for i,x in enumerate(gp) if inside(x,w)]==[k]
  oldPoint=dims[h]['grips'][k];bounds=glyphs[h]['texts'][t['segment']]['fontBounds'];normalLo=bounds[normal]+point[normal]-oldPoint[normal]-2;normalHi=bounds[normal+2]+point[normal]-oldPoint[normal]+2;others=[abs(x[along]-point[along]) for i,x in enumerate(gp) if i!=k and normalLo<=x[normal]<=normalHi and abs(x[along]-point[along])>1e-6];half=min([100]+[v*.4 for v in others]);fallback=copy.deepcopy(w);fallback['min'][normal]=normalLo;fallback['max'][normal]=normalHi;fallback['min'][along]=point[along]-half;fallback['max'][along]=point[along]+half;windows=[w]
  if [i for i,x in enumerate(gp) if inside(x,fallback)]==[k]:windows.append(fallback)
  ops.append(dict(kind='text',handle=h,gripIndex=k,segment=t['segment'],before=point,target=t['target'],delta=t['delta'],windows=windows));gp[k]=t['target'][:]
 sourceMasks=[e for e in p['originalBefore'] if e['type']=='AcDbWipeout'];assert len(sourceMasks)==4;template=sourceMasks[0];base=template['min'];width=template['max'][0]-base[0];height=template['max'][1]-base[1];assert width>0 and height>0
 log=folder/'drawing-edit.log';reopened=folder/'drawing-reopened.log';templateName=f'BCP_DOOR_NUMBER_CLEARANCE_F{n}';masks=[]
 for c in f['candidates']:
  b=c['desiredBounds'];lo=[b[0]-60,b[1]-60,0];hi=[b[2]+60,b[3]+60,0];masks.append(dict(caption=c['handle'],opening=c['opening'],group=f'BCP_DOOR_LABEL_F{n}_{c["opening"]}',min=lo,max=hi,scaleX=(hi[0]-lo[0])/width,scaleY=(hi[1]-lo[1])/height))
 lines=[f'(setq BCPLOG {path(log)})',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)']
 for c in f['candidates']:
  if any(abs(x)>1e-8 for x in c['delta']):lines.append(f'(progn (command "_.MOVE" (handent {st(c["handle"])}) "" "_non" \'(0.0 0.0 0.0) "_non" {pt(c["delta"])}) (bcp-log "DOOR|{c["opening"]}|{c["handle"]}"))')
 for a in f['annotationMoves']:lines.append(f'(progn (command "_.MOVE" (handent {st(a["handle"])}) "" "_non" \'(0.0 0.0 0.0) "_non" {pt(a["delta"])}) (bcp-log "NAME|{a["handle"]}"))')
 for op in ops:
  wins="'("+' '.join('('+pt(w['min'])[1:]+' '+pt(w['max'])[1:]+')' for w in op['windows'])+')'
  lines.append(f'(bcp-stretch-one {st(op["handle"])} {wins} {pt(op["delta"])} {st(op["kind"]+str(op["gripIndex"]))})')
 lines += [f'(bcp-mask-template {st(templateName)} {st(template["handle"])} {pt(base)})','(setq BCPMASKS (ssadd) BCPOLDLAYER (getvar "CLAYER"))','(setvar "CLAYER" "A-WINDOW-TEXT")']
 for mask in masks:lines.append(f'(bcp-mask-number {st(templateName)} {pt(mask["min"])} {mask["scaleX"]:.15f} {mask["scaleY"]:.15f} {st(mask["caption"])} {st(mask["group"])})')
 for item in extra.get('frontDimensions',[]):
  if item['floor']==n:
   assert item['handle'] in dims and any(t['handle']==item['handle'] for t in textRows)
   lines.append(f'(command "_.DRAWORDER" (handent {st(item["handle"])}) "" "_Front")')
 lines += ['(command "_.DRAWORDER" BCPMASKS "" "_Front")','(setq BCPCAPTIONS (ssadd))']
 for c in f['candidates']:lines.append(f'(ssadd (handent {st(c["handle"])}) BCPCAPTIONS)')
 lines += ['(command "_.DRAWORDER" BCPCAPTIONS "" "_Front")','(setvar "CLAYER" BCPOLDLAYER)','(command "_.ZOOM" "_E")','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")']
 # Feed only complete single-line forms to the script reader; assemble the atomic function as data.
 body=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+line+')))' for line in lines+['(princ)']]+["(eval (cons 'defun (cons 'bcp-edit-floor (cons nil bcp-body))))"]
 commands+=['_.OPEN',path(control),*[line for h in helpers for line in script_helper(h)],*body,'(bcp-edit-floor)','_.CLOSE','_.OPEN',path(control),f'(setq bcp-f (open {path(reopened)} "w") bcp-gd (cdr (assoc -1 (dictsearch (namedobjdict) "ACAD_GROUP"))))']
 for mask in masks:commands.append(f'(progn (write-line {st("GROUP|"+mask["caption"]+"|"+mask["group"])} bcp-f) (foreach bcp-r (dictsearch bcp-gd {st(mask["group"])}) (if (= (car bcp-r) 340) (progn (setq bcp-d (entget (cdr bcp-r))) (write-line (strcat "MEMBER|" (cdr (assoc 5 bcp-d)) "|" (cdr (assoc 0 bcp-d))) bcp-f)))))')
 commands += ['(write-line (strcat "COUNT|" (itoa (sslength (ssget "_X" \'((410 . "Model")))))) bcp-f)','(write-line (strcat "FRAME|" (itoa (getvar "WIPEOUTFRAME"))) bcp-f)','(close bcp-f)']
 manifest['floors'].append(dict(floor=n,source=f['source'],sourceSHA256=f['sourceSHA256'],control=str(control),baseModelCount=len(idx),expectedModelCount=len(idx)+len(masks),log=str(log),reopenedLog=str(reopened),doors=f['candidates'],annotationMoves=f['annotationMoves'],dimensionOps=ops,expectedGrips=state,sourcePreview=f['actualPreview'],captionWitness=f['captionWitness'],masks=masks,maskTemplate=templateName,templateSourceHandle=template['handle'],visibleDimensionHandles=list(dims)))
commands+=['(princ)'];text='\r\n'.join(commands)+'\r\n'
assert max(len(line.encode('cp936')) for line in commands)<1800
balance=0;string=False;escape=False
for c in text:
 if escape:escape=False;continue
 if string and c=='\\':escape=True;continue
 if c=='"':string=not string
 elif not string:
  balance+=(c=='(')-(c==')');assert balance>=0
assert balance==0 and not string
if extraPath:manifest['additionalDimensionPlan']=dict(path=str(extraPath),SHA256=sha(extraPath),data=extra)
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));binding['bootSHA256']=sha(boot);binding['scope']=manifest['scope'];bp.write_text(json.dumps(binding,ensure_ascii=False,indent=2),encoding='utf-8');manifest['bootSHA256']=sha(boot);(HERE/f'door-clearance-manifest-{version}.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(session=str(session),modelCounts=[f['expectedModelCount'] for f in manifest['floors']],operations=[len(f['dimensionOps']) for f in manifest['floors']],bootBytes=len(boot.read_bytes())),ensure_ascii=False))
