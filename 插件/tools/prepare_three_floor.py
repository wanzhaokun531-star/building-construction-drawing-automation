"""Build source associations and native drafting resources from approved v5 evidence.

Development only. Production uses ObjectARX and this declarative contract, not Python.
"""
from pathlib import Path
import json, hashlib, importlib.util, shutil, math
R=Path(__file__).resolve().parents[2]
T=R/'阶段2/项目/百塘村邹总_20261103'
OUT=R/'插件/resources/three-floor'
OUT.mkdir(parents=True,exist_ok=True)
read=lambda p:json.loads(Path(p).read_text('utf-8-sig'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
b=read(R/'阶段2/evidence/native-host-binding-v191.json');j=Path(b['job'])
roots={e['handle']:e for e in read(j/'preview-final-1/stage2-preview-native.json')['originalBefore']}
delivery=read(T/'delivery-reviewfix-v191.json');donor=Path(delivery['path'])
assert sha(donor)==delivery['SHA256']
shutil.copyfile(donor,OUT/'annotations.dwg')
spec=importlib.util.spec_from_file_location('public_fields',R/'阶段2/示意/门窗内部编号_20261102/public_fields.py');pf=importlib.util.module_from_spec(spec);spec.loader.exec_module(pf)
fields=pf.fields_in_job(read(T/'sdk-read-reviewfix-v191.json')['job'])
sourceBinding=read(R/'阶段2/evidence/native-host-binding-v171.json')
sourceFields=pf.fields_in_job(sourceBinding['job'])
original=read(next((Path(sourceBinding['job'])/'diagnostics').glob('*.json')))['input']['nativeGeometry']
if isinstance(original,str):original=json.loads(original)
original={e['handle']:e for e in original}
erased=set('1CDC 1CDD 1CDE 1CDF 1CE0 1CE1'.split())
for name in read(T/'room-text-plan.json'):erased.update(name['erase'])
# Last source entity is the confirmed composite entrance. This is resource extraction,
# not a production rule for identifying newly added user objects.
keep={h for h in roots if int(h,16)<=int('3B33',16)}
assert len(keep)+len(erased)==8616
clone=set(roots)-keep
q=lambda s:json.dumps(s,ensure_ascii=False)
pt=lambda p:' '.join(format(float(v),'.17g') for v in p)
rows=['BCPTHREE1 '+sha(OUT/'annotations.dwg')]
for h,e in sorted(original.items()):
 rows.append('SOURCE '+h+' '+e['type']+' '+str(len(e['grips']))+' '+pt(v for p in e['grips'] for v in p))
 for key in (['左厚度','右厚度'] if e['type']=='TDbWall' else ['宽度','高度','离地高度','转角']):
  rows.append('PARAM '+h+' '+q(key)+' '+format(float(sourceFields[h]['fields'][key]),'.17g'))
for h in sorted(keep):rows.append('KEEP '+h+' '+roots[h]['type'])
for h in sorted(clone):rows.append('CLONE '+h+' '+roots[h]['type'])
for h in sorted(erased):rows.append('ERASE '+h)
for h in sorted(original):
 e=roots[h];points=e['grip']['points']
 if e['type']=='TDbWall':rows.append('WALL '+h+' '+pt(points[0]+points[1]))
 else:
  number=fields[h]['fields']['门窗编号'];core=points[:-1] if number else points
  assert len(core) in (len(original[h]['grips']),len(original[h]['grips'])-1),(h,len(core),len(original[h]['grips']))
  rows.append('OPEN '+h+' '+q(number)+' '+str(len(core))+' '+pt(v for p in core for v in p)+' '+pt(points[-1] if number else [0,0,0]))
# The newly confirmed window is cloned from a native window prototype and is not a source replacement.
added=read(j/'tidy.done')['addedWindow'];e=roots[added];points=e['grip']['points']
rows.append('OPEN '+added+' '+q('LC0918')+' '+str(len(points)-1)+' '+pt(v for p in points[:-1] for v in p)+' '+pt(points[-1]))
for line in (T/'integer-actions.tsv').read_text().splitlines():
 if line.startswith('S '):rows.append('COLUMN '+line[2:])
rows.append('END')
(OUT/'layout.tsv').write_text('\n'.join(rows)+'\n','utf-8')
(OUT/'extraction.json').write_text(json.dumps(dict(source=str(sourceBinding['input']),sourceSHA256=sourceBinding['inputSHA256'],keep=len(keep),clone=len(clone),erase=len(erased),originalNative=len(original),targetRoots=len(roots)),ensure_ascii=False,indent=2),'utf-8')
print('three-floor resources',len(keep),'retained',len(clone),'native additions',len(erased),'superseded roots')
