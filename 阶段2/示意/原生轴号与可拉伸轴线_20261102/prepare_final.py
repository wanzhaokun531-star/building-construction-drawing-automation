"""Direct genuine Tianzheng axes and independent lines in the user's DWG copy."""
from pathlib import Path
import importlib.util,json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('axis_spike_helpers',HERE/'prepare_spike.py')
# Only reusable definitions preceding the spike's top-level version binding.
scope={'__file__':str(HERE/'prepare_spike.py')};src=(HERE/'prepare_spike.py').read_text('utf-8');exec(src[:src.index('v=sys.argv[1]')],scope)
read,sha,st,path,pt,literal,helper=[scope[n] for n in ['read','sha','st','path','pt','literal','helper']]
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);source=Path(b['input']);inspection=read(ROOT/'阶段2/示意/用户修改对比_20261102/inspection-v155.json');assert sha(source)==inspection['sourceSHA256'];oldJob=Path(inspection['previewJob']);a=read(oldJob/'product-axis-document-audit.json');p=read(oldJob/'stage2-preview-native.json');roots={e['handle']:e for e in p['originalBefore']};groups=[g['name'] if isinstance(g,dict) else g for g in a['groups']]
# Existing audit groups contain native group summaries, resolved by exact names.
groups=[]
for g in a['groups']:
 if isinstance(g,str):groups.append(g)
 elif isinstance(g,dict):groups.append(g.get('name',g.get('group','')))
assert len(groups)==15 and all(g.startswith('BCP_GRID_') for g in groups)
folder=job/'direct-axis-sample';folder.mkdir(exist_ok=True);control=folder/'control.dwg';assert not (folder/'drawing-edit.log').exists();shutil.copyfile(source,control);log=folder/'drawing-edit.log';number21=folder/'axis-number-21-test.dwg';letterP=folder/'axis-letter-P-test.dwg';shifted=folder/'axis-line-stretch-123-test.dwg';groupsLog=folder/'reopened-groups.log';propertyLog=folder/'reopened-properties.tsv'
stretch=literal(ROOT/'阶段2/示意/门号沿洞口与尺寸避让_20261101/prepare_door_clearance.py','helpers')[:2];dump=literal(ROOT/'阶段2/示意/门窗编号取消额外成组_20261102/prepare_sample.py','dumpHelper');propBoot=(Path(read(ROOT/'阶段2/evidence/native-host-binding-v156.json')['session'])/'boot.scr').read_text('cp936');propHelpers=list(dict.fromkeys(line for line in propBoot.splitlines() if line.startswith('(defun bcp-')));assert len(propHelpers)==3
probe=Path(b['module']).parent/('AXLead_'+b['nonce'][:8]+'.arx');probeIni=probe.with_suffix('.ini');assert not probe.exists() and not probeIni.exists();shutil.copyfile(HERE/'probe/bin/AxisLeadProbe.arx',probe);probeIni.write_text('[probe]\ncontrol='+str(control)+'\noutput='+str(folder)+'\n',encoding='utf-16')
boot=(session/'boot.scr').read_bytes();marker=b'S1QUIETSTART\r\n';assert boot.count(marker)==1;commands=[boot[:boot.index(marker)+len(marker)].decode('ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)','_.OPEN',path(control),*[x for h in stretch for x in helper(h)]]
lines=[f'(setq BCPAXOK nil BCPLOG {path(log)} BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE") BCPOLDPATH (getvar "LOGFILEPATH"))','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)',f'(setvar "LOGFILEPATH" {st(folder.as_posix()+"/")})','(setvar "LOGFILEMODE" 1)','(bcp-log (strcat "COMMAND_LOG|" (getvar "LOGFILENAME")))',f'(setq BCPALOAD (vl-catch-all-apply \'arxload (list {path(probe)})))','(if (vl-catch-all-error-p BCPALOAD) (progn (bcp-log (vl-catch-all-error-message BCPALOAD)) (exit)))','(bcp-log "NATIVE_LEAD_PROBE_LOADED")']
for c in ['tch_kerncmd.arx','tch_annotate.arx']:lines.append(f'(if (or (not (getcname "TReAxisNo")) (not (getcname "TObjEdit"))) (vl-catch-all-apply \'arxload (list {path(Path("C:/Tangent/TArchT20V8/sys23x64")/c)})))')
for g in groups:lines.append(f'(command "_.-GROUP" "_Explode" {st(g)})')
axisPlan=[];priorQA=read(HERE/'qa-v158.json');priorNative=read(Path(priorQA['captures']['final']['job'])/'stage2-preview-native.json');priorRoots={e['handle']:e for e in priorNative['originalBefore']};priorIds={e['name']:e['handle'] for e in priorQA['axes']}
for i,axis in enumerate(a['axes']):
 name=axis['name'];var=f'BCPAX{i}';vertical='_V_' in name;label=name.rsplit('_',1)[1];side='positive' if name in ['BCP_AXIS_V_2','BCP_AXIS_V_3'] else 'negative' if name in ['BCP_AXIS_H_C','BCP_AXIS_H_E'] else 'both';centres=[x['centre'][0] for x in axis['circles']];assert len(centres)==2
 lines += [f'(command "_.EXPLODE" (handent {st(axis["container"])}) "")',f'(setq {var} (entlast))',f'(if (/= (cdr (assoc 0 (entget {var}))) "TCH_AXIS_LABEL") (exit))',f'(command "_.CHPROP" {var} "" "_LA" "A-AXIS" "")',f'(bcp-log (strcat "DIRECT_AXIS|{name}|" (cdr (assoc 5 (entget {var})))))']
 if side!='both':
  centre=centres[1 if side=='positive' else 0];pick=[centre[0]+(400 if vertical else 0),centre[1]+(0 if vertical else 400),0];lines += [f'(command "_.ZOOM" "_W" "_non" {pt([centre[0]-800,centre[1]-800,0])} "_non" {pt([centre[0]+800,centre[1]+800,0])})',f'(command "TObjEdit" {pt(pick)} "M" "")',f'(bcp-log "NATIVE_SIDE_EDIT|{name}|{side}")']
 leadEdits=[]
 if not vertical:
  native=priorRoots[priorIds[name]];oldPoints=native['stretch']['points'];assert len(oldPoints) in [2,4] and (side!='both' or len(oldPoints)==4)
  for index,targetX in [(0,axis['min'][0] if axis['inverted'] else axis['max'][0])]+([(2,axis['max'][0])] if side=='both' else []):
   before=oldPoints[index];target=[targetX,before[1],before[2]];delta=[x-y for x,y in zip(target,before)];lo=[before[0]-25,before[1]-25,0];hi=[before[0]+25,before[1]+25,0];wins="'(("+pt(lo)[1:]+' '+pt(hi)[1:]+'))'
   case=name+'_'+str(index);values=' '.join(format(float(x),'.12f') for x in [*before,*target]);lines.append(f'(if (/= (AXLEAD_FIX {var} {values} {st(case)}) 1) (progn (bcp-log "MODERN_LEAD_FAILED|{case}") (exit)))');lines.append(f'(bcp-log "MODERN_LEAD_COMPLETE|{case}")');leadEdits.append(dict(before=before,target=target,delta=delta,nativeReport=str(folder/f'native-lead-{case}.json')))
 axisPlan.append(dict(name=name,container=axis['container'],oldNativeHandle=axis['nativeHandle'],label=label,vertical=vertical,side=side,variable=var,centres=centres,leadEdits=leadEdits))
# New lower label 1 retains the source native location and gains the matching connector.
start=[78453.70152820693,24525.213801956088,0];end=[78453.7015282064,21325.213801956088,0]
lines += ['(setq BCPSD (entget (handent "8605")))',f'(setq BCPNEWLINE (entmakex (append \'((0 . "LINE") (100 . "AcDbEntity")) (vl-remove nil (mapcar \'(lambda (k) (assoc k BCPSD)) \'(8 62 6 370 48))) (list \'(100 . "AcDbLine") (cons 10 {pt(start)}) (cons 11 {pt(end)})))))','(if (not BCPNEWLINE) (exit))','(bcp-log (strcat "NEW_AXIS1_CONNECTOR|" (cdr (assoc 5 (entget BCPNEWLINE)))))',f'(command "_.MOVE" (handent "8073") "" "_non" \'(0 0 0) "_non" {pt([1000,-1200,0])})','(bcp-log "TITLE_MOVED|8073|1000|-1200")']
for wanted,testNumber,testPath in [('BCP_AXIS_V_1','21',number21),('BCP_AXIS_H_A','P',letterP)]:
 axis=next(x for x in axisPlan if x['name']==wanted);centre=axis['centres'][1 if axis['vertical'] else 0];pick=[centre[0]+(400 if axis['vertical'] else 0),centre[1]+(0 if axis['vertical'] else 400),0];lines += [f'(command "_.ZOOM" "_W" "_non" {pt([centre[0]-800,centre[1]-800,0])} "_non" {pt([centre[0]+800,centre[1]+800,0])})',f'(command "TReAxisNo" {pt(pick)} {st(testNumber)} "")',f'(bcp-log "RENUMBER_RETURNED|{wanted}|{testNumber}")','(command "_.QSAVE")',f'(if (not (vl-file-copy {path(control)} {path(testPath)})) (exit))',f'(command "TReAxisNo" {pt(pick)} {st(axis["label"])} "")',f'(bcp-log "RENUMBER_RESTORED|{wanted}|{axis["label"]}")']
lines += ['(command "_.ZOOM" "_E")'];x,y=78453.70152820693,37225.213801957034
for offset,delta in [(0,123),(123,-123)]:
 lo=[x-25,y+offset-25,0];hi=[x+25,y+offset+25,0];wins="'(("+pt(lo)[1:]+' '+pt(hi)[1:]+'))';lines += [f'(bcp-stretch-one "841B" {wins} {pt([0,delta,0])} "native-axis-line")']
 if delta>0:lines+=['(command "_.QSAVE")',f'(if (not (vl-file-copy {path(control)} {path(shifted)})) (exit))']
lines += ['(setvar "LOGFILEMODE" 0)','(setvar "LOGFILEPATH" BCPOLDPATH)','(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(command "_.QSAVE")','(setq BCPAXOK T)','(bcp-log "SAVED|FINAL")','(princ)'];commands += ['(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-direct-axes (cons nil bcp-body))))",'(bcp-direct-axes)',dump,f'(if BCPAXOK (bcp-dump-groups {path(groupsLog)}))',*propHelpers,f'(if BCPAXOK (bcp-dump {path(propertyLog)}))',f'(setq BCPUNLOAD (vl-catch-all-apply \'arxunload (list {path(probe)})))','(bcp-log (strcat "LEAD_PROBE_UNLOAD|" (if (vl-catch-all-error-p BCPUNLOAD) (vl-catch-all-error-message BCPUNLOAD) (vl-prin1-to-string BCPUNLOAD))))','(setvar "LOGFILEMODE" 0)']
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800;bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
out=session/'boot.scr';out.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(out);b['scope']='DWG sample only: real direct TCH axes, public modern grip endpoint edits, TObjEdit side edit; independent lines and restored number/stretch trials; no COM/CUA/product changes';b['auxiliaryProbe']=dict(module=str(probe),moduleSHA256=sha(probe),sidecar=str(probeIni),sidecarSHA256=sha(probeIni));bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');baseline=read(ROOT/'阶段2/示意/卫生间合并面积与房名避让_20261102/manifest-v154.json');m=dict(version=v,session=str(session),nonce=b['nonce'],source=str(source),sourceSHA256=sha(source),sourcePreview=str(oldJob/'stage2-preview-native.json'),sourceAudit=str(oldJob/'product-axis-document-audit.json'),control=str(control),log=str(log),groupsLog=str(groupsLog),propertyLog=str(propertyLog),axes=axisPlan,dissolvedGroups=groups,addedConnector=dict(start=start,end=end,sourceTemplate='8605'),titleMove=dict(handle='8073',delta=[1000,-1200,0]),numberTrial=str(number21),letterTrial=str(letterP),lineTrial=str(shifted),expectedModelCount=242,pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=baseline['sourceFiles'],auxiliaryProbe=b['auxiliaryProbe']);(HERE/f'manifest-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,axes=len(axisPlan),groupsRemoved=len(groups),expectedModelRoots=242),ensure_ascii=True))
