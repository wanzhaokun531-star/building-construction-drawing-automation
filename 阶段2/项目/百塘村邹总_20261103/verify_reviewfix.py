from pathlib import Path
import json,sys,math,hashlib,collections
T=Path(__file__).parent;R=T.parents[2]
read=lambda p:json.loads(p.read_text('utf-8-sig'))
b=read(R/f'阶段2/evidence/native-host-binding-{sys.argv[1]}.json');j=Path(b['job']);report=read(j/'tidy.done')
old={e['handle']:e for e in read(R/'阶段1/runs/stage2-product-20261103-120232/preview-final-1/stage2-preview-native.json')['originalBefore']}
new={e['handle']:e for e in read(j/'preview-final-1/stage2-preview-native.json')['originalBefore']}
near=lambda a,b:math.dist(a,b)<1e-5
same=lambda a,b:len(a)==len(b) and all(near(x,y) for x,y in zip(a,b))
removed={'1CDC','1CDD','1CDE','1CDF','1CE0','1CE1','3C59'}
for line in (T/'room-text-plan.tsv').read_text().splitlines():removed.update(line.split()[3].split(','))
added={report['addedWindow'],report['newTopChain']}|{x['handle'] for x in report['roomTexts']}
assert set(old)-set(new)==removed,(set(old)-set(new),removed)
assert set(new)-set(old)==added
axes={a['handle']:a for a in report['axes']}
changed={'CE2','3C42'}|set(axes)
for h in set(old)&set(new):
 assert (new[h]['type'],new[h]['layer'],new[h]['proxy'])==(old[h]['type'],old[h]['layer'],old[h]['proxy']),h
 if h not in changed:assert same(old[h]['grip']['points'],new[h]['grip']['points']),h
for h,a in axes.items():
 p=new[h]['grip']['points'];o=old[h]['grip']['points'];assert same(p,a['after']);assert same(p[1:],o[1:]);assert abs(p[0][1]-o[0][1])<1e-5
assert same(new['CE2']['grip']['points'],[[p[0],p[1]+19,p[2]] for p in old['CE2']['grip']['points']])
w=new[report['addedWindow']];assert w['type']=='TDbOpening';assert same(w['grip']['points'],[[p[0]+3460,p[1],p[2]] for p in old['ED5']['grip']['points']])
def values(e):
 p=e['grip']['points'];n=int(round(e['curve']['endParam']));return [math.dist(p[k+2],p[k+3]) for k in range(n)]
assert all(abs(a-b)<1e-5 for a,b in zip(values(new['3C42']),[400,1000,400,600,1000,200]))
assert len(values(new[report['newTopChain']]))==16
assert all(abs(a-b)<1e-5 for a,b in zip(values(new[report['newTopChain']]),[120,220,900,2560,900,220,400,1000,400,500,1200,500,851,2500,849,120]))
for n in report['roomTexts']:
 e=new[n['handle']];assert e['type']=='TDbText' and not e['proxy'];assert math.dist([(e['min'][0]+e['max'][0])/2,(e['min'][1]+e['max'][1])/2,0],[n['x'],n['y'],0])<0.08
counts=collections.Counter(e['type'] for e in new.values());assert counts['TDbWall']==126 and counts['TDbDimension2']==59 and counts['TDbAxisLabelSet']==40
assert counts['AcDbWipeout']==0;assert report['roomTextCount']==34
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert sha(Path(b['input']))==b['inputSHA256'];assert sha(j/'百塘村邹总_三层平面_门窗轴线文字修订_v5.dwg')==report['outputSHA256']
result=dict(sourcePreserved=True,unchangedOtherRootGrips=True,wallsPreserved=126,addedNativeWindow=report['addedWindow'],doorOffsets=[600,1000,200],wholeDimensionChains=59,topThirdChainSegments=16,trimmedAxes=len(axes),nativeRoomTexts=34,removedVectorGlyphs=len(removed)-7,noModelWipeout=True,rootCount=len(new))
(T/f'qa-reviewfix-{sys.argv[1]}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(result)
