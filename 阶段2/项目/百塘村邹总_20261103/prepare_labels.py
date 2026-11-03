from pathlib import Path
import sys,json,hashlib,importlib.util,shutil
R=Path(__file__).resolve().parents[3];T=Path(__file__).parent
spec=importlib.util.spec_from_file_location('prepare',R/'插件/tools/prepare_native.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
v=sys.argv[1];source=R/'阶段1/runs/stage2-product-20261103-110306/百塘村邹总_标注深化确认稿.dwg'
m.main(v,source,'generate',T/'bin/BTNative.arx',R/'插件/build/approved-013-candidate/BuildingPlan2020.bundle/Contents/Win64/resources')
bf=R/f'阶段2/evidence/native-host-binding-{v}.json';b=json.loads(bf.read_text('utf-8'));s=Path(b['session']);j=Path(b['job']);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
ini=Path(b['sidecar']);t=ini.read_text('utf-16');t=t.replace('[product]',f'sourceHash={b["inputSHA256"]}\n[product]');ini.write_text(t,encoding='utf-16');b['sidecarSHA256']=sha(ini)
boot=s/'boot.scr';boot.write_bytes(boot.read_bytes()+('_.OPEN\r\n"'+(j/'control.dwg').as_posix()+'"\r\n(setvar "OSMODE" 0)\r\n(command "BT_LABELS")\r\n(princ)\r\n').encode('cp936'));b['bootSHA256']=sha(boot)
snap=s/'phase2-candidate/project-helper';snap.mkdir();
for p in T.iterdir():
 if p.is_file() and p.suffix in ['.cpp','.inc','.py','.tsv','.json','.vcxproj']:shutil.copyfile(p,snap/p.name)
bf.write_text(json.dumps(b,ensure_ascii=False,indent=2),encoding='utf-8')
