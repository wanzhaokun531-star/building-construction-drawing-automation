"""DWG-only bath semantic merge, name placement and requested dimension layer."""
from pathlib import Path
import ast,json,hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def st(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
def path(v):return st(Path(v).as_posix())
def pt(v):return "'("+' '.join(format(float(x),'.12f') for x in v)+')'
def literal(file,name):
 tree=ast.parse(Path(file).read_text(encoding='utf-8-sig'));return ast.literal_eval(next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets)))
v=sys.argv[1];bp=ROOT/f'阶段2/evidence/native-host-binding-{v}.json';b=read(bp);session=Path(b['session']);job=Path(b['job']);previous=ROOT/'阶段2/示意/门窗分类编号新规则_20261102';base=read(previous/'manifest-v152.json');qa=read(previous/'qa-v152.json');delivery=read(previous/'delivery-v152.json');areas=read(ROOT/'阶段2/示意/房间面积随房名_20261101/area-name-placement-final.json')
assert all(sha(ROOT/p)==h for p,h in base['sourceFiles'].items()) and sha(ROOT/'插件/bin/BuildingPlanPlugin2020.arx')==b['moduleSHA256']==base['pluginBinarySHA256']
ref=ROOT/'阶段0/输入快照/参考图对象属性.json';refData=read(ref);layers=[r for r in refData['Layers'] if r['Name'] in ['PUB_DIM','A-PUB_DIM']];assert len(layers)==2;target=next(r for r in layers if r['Name']=='PUB_DIM');assert target['Color']==3 and target['Linetype']=='Continuous' and target['Lineweight']==-3
helpersFile=ROOT/'阶段2/示意/房间面积随房名_20261101/prepare_area_name_sample.py';helpers=[literal(helpersFile,k) for k in ['helper','unlockHelper','restoreHelper']];dump=literal(ROOT/'阶段2/示意/门窗编号取消额外成组_20261102/prepare_sample.py','dumpHelper')
boot=(session/'boot.scr').read_bytes();marker=b'S1QUIETSTART\r\n';assert boot.count(marker)==1;commands=[boot[:boot.index(marker)+len(marker)].decode('ascii').rstrip(),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "LOGFILEMODE" 0)'];m=dict(contract='bcp.bath-functional-area-merge.v1',version=v,session=str(session),nonce=b['nonce'],pluginCodeChanged=False,pluginBinarySHA256=b['moduleSHA256'],sourceFiles=base['sourceFiles'],referenceLayerEvidence=str(ref),referenceLayerEvidenceSHA256=sha(ref),referenceLayers=layers,chosenLayer=target,userLayerChoice='PUB_DIM without A- prefix',floors=[])
for old in base['floors']:
 n=old['floor'];fqa=next(f for f in qa['floors'] if f['floor']==n);sourceRow=next(f for f in delivery['DWGs'] if f['floor']==n);source=Path(sourceRow['DWG']);assert sha(source)==sourceRow['SHA256'];preview=Path(fqa['previewJob'])/'stage2-preview-native.json';audit=Path(fqa['previewJob'])/'product-axis-document-audit.json';roots={e['handle']:e for e in read(preview)['originalBefore']};ann=read(audit);aa=next(f for f in areas['floors'] if f['floor']==n)['areas'];left,right=aa[-2:];assert left['id'].endswith('R09') and right['id'].endswith('R10')
 x0=min(p[0] for p in left['netRegionVertices']);x1=max(p[0] for p in right['netRegionVertices']);y0=min(p[1] for p in left['netRegionVertices']);y1=max(p[1] for p in right['netRegionVertices']);boundary=[[x0,y0,0],[x1,y0,0],[x1,y1,0],[x0,y1,0]];area=(x1-x0)*(y1-y0);assert abs(area-5197600)<0.01
 opening='824B' if n==1 else '8218';g=roots[opening]['grip']['points'];assert abs(abs(g[1][1]-g[2][1])-(y1-y0))<1e-5 and abs(min(g[1][1],g[2][1])-y0)<1e-5 and abs(max(g[1][1],g[2][1])-y1)<1e-5
 members=right['members'];name=left['nameHandle'];nameRoot=roots[name];nameWidth=nameRoot['max'][0]-nameRoot['min'][0];nameHeight=nameRoot['max'][1]-nameRoot['min'][1];cx=49700.0+(46422 if n==2 else 0);nameBottom=30940.0;nameDelta=[cx-(nameRoot['min'][0]+nameRoot['max'][0])/2,nameBottom-nameRoot['min'][1],0]
 combined=[min(roots[h]['min'][i] for h in members) for i in [0,1]]+[max(roots[h]['max'][i] for h in members) for i in [0,1]];height=combined[3]-combined[1];areaTarget=[cx-(combined[2]-combined[0])/2,nameBottom-50-height,cx+(combined[2]-combined[0])/2,nameBottom-50];areaDelta=[areaTarget[0]-combined[0],areaTarget[1]-combined[1],0];moves={name:nameDelta,**{h:areaDelta for h in members}}
 dimensionHandles=[e['handle'] for e in ann['nativeAnnotations'] if e['type']=='TDbDimension2' and e['visible'] and e['layer']=='A-PUB_DIM'];assert len(dimensionHandles)==(17 if n==1 else 19)
 folder=job/f'bath-merge-floor-{n}';folder.mkdir();control=folder/'control.dwg';shutil.copyfile(source,control);log=folder/'drawing-edit.log';groups=folder/'reopened-groups.log';metadata=folder/'reopened-area-text.log'
 lines=[f'(setq BCPLOG {path(log)} BCPSTATES nil BCPOLDPICK (getvar "PICKSTYLE") BCPOLDOS (getvar "OSMODE") BCPOLDZIN (getvar "DIMZIN"))',f'(bcp-log "BEGIN|F{n}")','(setvar "PICKSTYLE" 0)','(setvar "OSMODE" 0)','(setvar "DIMZIN" 0)']
 for h in [*moves,*left['members'],*dimensionHandles]:lines.append(f'(bcp-unlock {st(h)})')
 lines+=['(if (not (tblsearch "LAYER" "PUB_DIM")) (entmake \'((0 . "LAYER") (100 . "AcDbSymbolTableRecord") (100 . "AcDbLayerTableRecord") (2 . "PUB_DIM") (70 . 0) (62 . 3) (6 . "Continuous") (370 . -3) (290 . 1))))','(setq BCPLD (entget (tblobjname "LAYER" "PUB_DIM")))']
 for code,value in [(62,'3'),(6,'"Continuous"'),(370,'-3'),(290,'1')]:lines.append(f'(setq BCPLD (if (assoc {code} BCPLD) (subst (cons {code} {value}) (assoc {code} BCPLD) BCPLD) (append BCPLD (list (cons {code} {value})))))')
 lines+=['(if (not (entmod BCPLD)) (exit))', '(command "_.AREA" '+ ' '.join('"_non" '+pt(p) for p in boundary)+' "")',f'(bcp-log (strcat "MERGED_AREA_MM2|" (rtos (getvar "AREA") 2 10)))',f'(if (> (abs (- (getvar "AREA") {area:.10f})) 0.01) (exit))',f'(setq BCPAD (entget (handent {st(members[0])})))','(if (/= (cdr (assoc 0 BCPAD)) "TEXT") (exit))','(if (not (entmod (subst (cons 1 (strcat (rtos (/ (getvar "AREA") 1000000.0) 2 2) "m")) (assoc 1 BCPAD) BCPAD))) (exit))',f'(command "_.-GROUP" "_Explode" {st(left["group"])})']
 for h in left['members']:lines.append(f'(command "_.ERASE" (handent {st(h)}) "")')
 for h,delta in moves.items():lines.append(f'(command "_.MOVE" (handent {st(h)}) "" "_non" \'(0 0 0) "_non" {pt(delta)})')
 for h in dimensionHandles:lines.append(f'(command "_.CHPROP" (handent {st(h)}) "" "_LA" "PUB_DIM" "")')
 lines+=['(setq BCPFRONT (ssadd))']+[f'(ssadd (handent {st(h)}) BCPFRONT)' for h in moves]+['(command "_.DRAWORDER" BCPFRONT "" "_Front")','(bcp-restore)','(setvar "PICKSTYLE" BCPOLDPICK)','(setvar "OSMODE" BCPOLDOS)','(setvar "DIMZIN" BCPOLDZIN)','(command "_.REGEN")','(command "_.QSAVE")',f'(bcp-log "SAVED|F{n}")','(princ)']
 commands+=['_.OPEN',path(control),*helpers,'(setq bcp-body nil)']+["(setq bcp-body (append bcp-body (list '"+x+')))' for x in lines]+["(eval (cons 'defun (cons 'bcp-bath-merge (cons nil bcp-body))))",'(bcp-bath-merge)','_.CLOSE','_.OPEN',path(control),dump,f'(bcp-dump-groups {path(groups)})',f'(setq BCPMF (open {path(metadata)} "w"))']
 for h in members:commands.append(f'(progn (setq BCPMD (entget (handent {st(h)}))) (write-line (strcat {st(h+"|")} (cdr (assoc 1 BCPMD)) "|" (cdr (assoc 7 BCPMD)) "|" (rtos (cdr (assoc 40 BCPMD)) 2 12) "|" (rtos (cdr (assoc 41 BCPMD)) 2 12)) BCPMF))')
 commands+=['(write-line (vl-prin1-to-string (entget (tblobjname "LAYER" "PUB_DIM"))) BCPMF)','(close BCPMF)']
 m['floors'].append(dict(floor=n,source=str(source),sourceSHA256=sha(source),sourcePreview=str(preview),sourceAudit=str(audit),sourcePublicRead=str(previous.parent/f'门窗内部编号_20261102/sdk-read-v152-floor{n}.json'),control=str(control),log=str(log),groupsLog=str(groups),metadataLog=str(metadata),expectedModelCount=len(roots)-2,removed=left['members'],removedGroup=left['group'],retainedGroup=right['group'],retainedAreaMembers=members,bodyHandle=members[0],nameHandle=name,nameText=left['nameText'],newText='5.20m',moves=moves,dimensionHandles=dimensionHandles,dimensionTargetLayer='PUB_DIM',boundary=boundary,netAreaMm2=area,oldAreaSumMm2=(left['areaM2']+right['areaM2'])*1e6,reconnectedInternalDoorStripMm2=area-(left['areaM2']+right['areaM2'])*1e6,internalDoor=opening,targetNameBounds=[cx-nameWidth/2,nameBottom,cx+nameWidth/2,nameBottom+nameHeight],targetAreaBounds=areaTarget,oldGroups=fqa['groups']))
text='\r\n'.join(commands)+'\r\n';assert max(len(x.encode('cp936')) for x in commands)<1800
bal=0;quoted=False;escape=False
for c in text:
 if escape:escape=False;continue
 if quoted and c=='\\':escape=True;continue
 if c=='"':quoted=not quoted
 elif not quoted:bal+=(c=='(')-(c==')');assert bal>=0
assert bal==0 and not quoted
out=session/'boot.scr';out.write_bytes(text.encode('cp936'));b['bootSHA256']=sha(out);b['scope']='DWG sample only: bathroom functional net-area merge, room-name/area MOVE, 36 whole dimensions to user-confirmed PUB_DIM; native AutoCAD, no COM/CUA/TCH commands/plugin source changes';bp.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');m['bootSHA256']=sha(out);(HERE/f'manifest-{v}.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(version=v,areas=[f['netAreaMm2']/1e6 for f in m['floors']],layerCounts=[len(f['dimensionHandles']) for f in m['floors']]),ensure_ascii=False))
