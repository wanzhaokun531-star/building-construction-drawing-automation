"""Authorized one-off DWG command batch. Never changes plugin C++ or ARX."""
from pathlib import Path
import json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
version=sys.argv[1];mode=sys.argv[2] if len(sys.argv)>2 else 'trial'
bp=ROOT/f'阶段2/evidence/native-host-binding-{version}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);old=read(ROOT/'阶段2/示意/连续尺寸与对象编号_20261102/qa-v141.json');prior=read(ROOT/'阶段2/示意/连续尺寸与对象编号_20261102/manifest-v141.json')
assert all(sha(ROOT/p)==h for p,h in prior['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==prior['pluginBinarySHA256']==b['moduleSHA256']
generation=ROOT/'阶段1/runs/stage2-product-20261101-143628';allCaptions=[]
for p in generation.rglob('annotations.json'):
 d=read(p);floor=1 if '_一层_' in str(p) else 2;allCaptions.append((floor,p,d['captions']))
assert sorted(n for n,p,d in allCaptions)==[1,2]
commands=[(session/'boot.scr').read_text(encoding='ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)']
manifest=dict(contract='bcp.internal-opening-number-sample.v1',version=version,mode=mode,session=str(session),nonce=b['nonce'],pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=prior['sourceFiles'],authorization='User explicitly allows only DWG sample copies, isolated desktop, public MCBH/TChWinLab; no COM/CUA/plugin source changes',floors=[])
for f in old['floors']:
 n=f['floor'];source=Path(next(x for x in old['DWGs'] if x['floor']==n)['DWG']);assert sha(source)==f['SHA256'];preview=read(Path(f['previewJob'])/'stage2-preview-native.json');audit=read(Path(f['previewJob'])/'product-axis-document-audit.json');roots={e['handle']:e for e in preview['originalBefore']};texts={e['handle']:e for e in audit['nativeAnnotations'] if e['type']=='AcDbText'}
 p,captions=next((p,d) for k,p,d in allCaptions if k==n);targets=[];pending=[]
 for c in captions:
  assert roots[c['opening']]['type'] in ['TDbOpening','TDbGroupOpening'] and texts[c['handle']]['text']==c['text']
  if c['pending']:pending.append(c);continue
  assert c['text'][0] in ['M','C'] and c['text'][1:].isdigit();targets.append(dict(opening=c['opening'],caption=c['handle'],number=c['text'],captionRotation=texts[c['handle']]['rotation']))
 folder=job/f'number-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log'
 selected=[t for t in targets if t['opening']=='7F39'] if mode=='trial' else targets
 manifest['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),sourcePreview=str(Path(f['previewJob'])/'stage2-preview-native.json'),sourceAudit=str(Path(f['previewJob'])/'product-axis-document-audit.json'),control=str(control),log=str(log),sourceCaptionReport=str(p),sourceCaptionSHA256=sha(p),targets=targets,pending=pending,selected=selected,expectedOldRoots=len(roots)))
 if not selected:continue
 commands+=['_.OPEN',path(control),'(defun bcp-log (s / f) (setq f (open BCPLOG "a")) (write-line s f) (close f) (princ))']
 body=[f'(setq BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE") BCPOLDLOG (getvar "LOGFILEMODE") BCPOLDPATH (getvar "LOGFILEPATH"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)',f'(setvar "LOGFILEPATH" {st(folder.as_posix()+"/")})','(setvar "LOGFILEMODE" 1)','(bcp-log (strcat "COMMAND_LOG|" (getvar "LOGFILENAME")))']
 for component in ['tch_kerncmd.arx','tch_annotate.arx']:
  body.append(f'(if (not (getcname "TChWinLab")) (progn (setq BCPLOAD (vl-catch-all-apply \'arxload (list {path(Path("C:/Tangent/TArchT20V8/sys23x64")/component)}))) (bcp-log (strcat "LOAD|{component}|" (if (vl-catch-all-error-p BCPLOAD) (vl-catch-all-error-message BCPLOAD) (vl-prin1-to-string BCPLOAD))))))')
 body+=['(if (not (getcname "TChWinLab")) (progn (bcp-log "NUMBER_COMMAND_NOT_REGISTERED") (exit)))','(command "_.ZOOM" "_E")']
 for t in selected:
  h=t['opening'];number=t['number']
  body += [f'(setq BCPOPENING (handent {st(h)}))',f'(if (not BCPOPENING) (progn (bcp-log "MISSING|{h}") (exit)))',f'(bcp-log "NUMBER_BEGIN|{h}|{number}")',f'(command "TChWinLab" (ssadd BCPOPENING) "" {st(number)})',f'(bcp-log (strcat "NUMBER_COMMAND_STATE|{h}|" (itoa (getvar "CMDACTIVE"))))',f'(bcp-log "NUMBER_RETURNED|{h}|{number}")']
 body+=['(setvar "LOGFILEMODE" BCPOLDLOG)','(setvar "LOGFILEPATH" BCPOLDPATH)','(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)']
 commands+=['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in body]+["(eval (cons 'defun (cons 'bcp-number (cons nil bcp-body))))",'(bcp-number)']
 assert mode in ['trial','all']
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
boot=session/'boot.scr';boot.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(boot);b['scope']=manifest['authorization'];bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');manifest['bootSHA256']=sha(boot);(HERE/f'manifest-{version}.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=version,session=str(session),targets=[len(f['targets']) for f in manifest['floors']],pending=[len(f['pending']) for f in manifest['floors']]),ensure_ascii=False))
