from pathlib import Path
import sys,json,hashlib,importlib.util,shutil
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('prepare',R/'插件/tools/prepare_native.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
v=sys.argv[1];source='C:/Users/PC/Documents/xwechat_files/w248681827_ca52/msg/file/2026-11/百塘村邹总_按PDF间距_原生墙门窗_v8源线复核.dwg'
m.main(v,source,'generate',T/'bin/BTNative.arx',R/'插件/build/approved-013-candidate/BuildingPlan2020.bundle/Contents/Win64/resources')
bf=R/f'阶段2/evidence/native-host-binding-{v}.json';b=json.loads(bf.read_text('utf-8'));s=Path(b['session']);j=Path(b['job']);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
ini=Path(b['sidecar']);text=ini.read_text('utf-16');at=text.index('[product]');text=text[:at]+f'taskRoot={T}\nsourceHash={b["inputSHA256"]}\nallowTMergeDim=1\nchainComponent=C:/Tangent/TArchT20V8/SYS23x64/tch_annotate.arx\nchainComponentHash={sha(Path("C:/Tangent/TArchT20V8/SYS23x64/tch_annotate.arx"))}\n'+text[at:];ini.write_text(text,encoding='utf-16');b['sidecarSHA256']=sha(ini)
q=lambda p:'"'+Path(p).as_posix()+'"'
bank=j/'bank';bank.mkdir();shutil.copyfile(R/'插件/build/approved-013-candidate/BuildingPlan2020.bundle/Contents/Win64/resources/axes.dwg',bank/'control.dwg')
cmd=['_.OPEN',q(bank/'control.dwg'),'(command "BT_BANK")','_.OPEN',q(j/'control.dwg'),'(setvar "FILEDIA" 0)','(setvar "CMDDIA" 0)','(setvar "OSMODE" 0)','(setvar "PICKSTYLE" 0)',f'(setq BTLOG (open {q(j/"boot-command-check.txt")} "w"))','(write-line (vl-prin1-to-string (getcname "BT_PREP")) BTLOG)','(close BTLOG)','(command "BT_PREP")','(command "BT_JOIN")','(princ)'];boot=s/'boot.scr';boot.write_bytes(boot.read_bytes()+('\r\n'.join(cmd)+'\r\n').encode('cp936'));b['bootSHA256']=sha(boot);b['projectHelperSources']={str(p.relative_to(T)):sha(p) for p in T.glob('*.cpp')};b['scope']='Project drafting copy only; real TCH dimensions/axis/numbers, native SDK and public commands, no COM/CUA; apron paused';bf.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8');print(str(j))

snapshot=s/'phase2-candidate/project-helper';snapshot.mkdir();
for p in T.iterdir():
 if p.is_file() and p.suffix in ['.cpp','.h','.py','.tsv','.ini','.json','.vcxproj','.txt']:shutil.copyfile(p,snapshot/p.name)
