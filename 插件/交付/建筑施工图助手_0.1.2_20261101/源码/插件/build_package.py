"""Developer packaging only; the installed ARX never executes Python."""
from pathlib import Path
import hashlib,shutil,json,sys
P=Path(__file__).resolve().parent;ROOT=P.parent
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
RESOURCES={
 'confirmed-input.tsv':P/'inputs/entry-failure-20261101/confirmed-input.tsv',
 'reference.dwg':ROOT/'阶段0/输入快照/施工图平面参考图.dwg',
 'axes.dwg':ROOT/'阶段2/resources/native-axis-number-templates.dwg',
 'axes.tsv':ROOT/'阶段2/resources/native-axis-number-templates.tsv',
 'layers.dwg':ROOT/'阶段2/inputs/layer-standard-20261031/FYHZJZ-A3-2610.dwg',
 'scheme.tsv':ROOT/'阶段2/inputs/frozen-scheme-v2.tsv',
 'axis-exclusions.tsv':ROOT/'阶段2/inputs/user-axis-exclusions-v1.tsv',
 'interior.tsv':ROOT/'阶段2/inputs/interior-dimension-decisions-v1.tsv',
 'prefixes.tsv':ROOT/'阶段2/inputs/user-opening-prefixes-v1.tsv',
}
FLAGS=dict(perimeterSpacingTrial=1,confirmedDimensionGap=1,exteriorDimensionEndpoints=1,fullInteriorAxes=1,axisPresentation=1,wallFollowingAxes=1,breakUnsupportedSpans=1,unifiedOpeningAnnotations=1,simplifyOpeningAnnotations=1,anchorCaptionsAndDirectThird=1,interiorDimensionReview=1,drawApron=0)
def stage_resources(destination):
 destination=Path(destination);destination.mkdir(parents=True,exist_ok=True);rows=[]
 for name,source in RESOURCES.items():
  target=destination/name
  if target.exists():raise FileExistsError(target)
  shutil.copyfile(source,target);rows.append(dict(relative=name,SHA256=sha(target)))
 (destination/'resources.tsv').write_text('BCPRESOURCES1 '+str(len(rows))+'\n'+''.join(f'{r["relative"]} {r["SHA256"]}\n' for r in rows),encoding='ascii')
 (destination/'profile.ini').write_text('[project]\nid=confirmed-plan-20261031\nunits=mm\nsourceCount=213\nbaseSourceHash=5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E\napron=paused\n[stage2]\n'+''.join(f'{k}={v}\n' for k,v in FLAGS.items()),encoding='utf-16')
 verify_resources(destination);return rows
def verify_resources(destination):
 destination=Path(destination);lines=(destination/'resources.tsv').read_text(encoding='ascii').splitlines()
 if lines[0]!='BCPRESOURCES1 9':raise ValueError('resource contract header')
 seen=set()
 for line in lines[1:]:
  name,digest=line.split()
  if name not in RESOURCES or name in seen or sha(destination/name)!=digest:raise ValueError('resource fingerprint differs')
  seen.add(name)
 if seen!=set(RESOURCES):raise ValueError('resource missing')
def main(destination=None):
 destination=Path(destination) if destination else P/'build/BuildingPlan2020.bundle';destination.mkdir(parents=True,exist_ok=False);content=destination/'Contents/Win64';content.mkdir(parents=True);stage_resources(content/'resources')
 shutil.copyfile(P/'bin/BuildingPlanPlugin2020.arx',content/'BuildingPlanPlugin2020.arx')
 (content/'BuildingPlanPlugin2020.ini').write_text('[product]\ntangentDirectory=C:\\Tangent\\TArchT20V8\\SYS23x64\n',encoding='utf-16')
 for name in ('PackageContents.xml','使用说明.md'):shutil.copyfile(P/name,destination/name)
 manifest={str(f.relative_to(destination)):sha(f) for f in destination.rglob('*') if f.is_file()};(destination/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(destination)
if __name__=='__main__':main()
