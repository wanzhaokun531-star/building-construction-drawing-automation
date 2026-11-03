from pathlib import Path
import json,sys,math,hashlib,collections,re
R=Path(__file__).resolve().parents[3];T=Path(__file__).parent
def read(p):return json.loads(p.read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
b=read(R/f'阶段2/evidence/native-host-binding-{sys.argv[1]}.json');j=Path(b['job'])
source=read(R/'阶段1/runs/stage2-product-20261103-100031/preview-source-1/stage2-preview-native.json')['originalBefore']
final=read(j/'preview-final-1/stage2-preview-native.json')['originalBefore'];actual={e['handle']:e for e in final};protected=0
for e in source:
 assert e['handle'] in actual
 f=actual[e['handle']];assert e['type']==f['type'] and not f['proxy']
 if e['type'] not in ['TDbWall','TDbOpening','TDbGroupOpening']:continue
 old=e['grip']['points'];now=f['grip']['points'];limit=len(old)-1 if e['type']=='TDbGroupOpening' else len(old)
 assert all(math.dist(old[k],now[k])<1e-5 for k in range(limit)),e['handle'];protected+=1
assert protected==172
counts=collections.Counter(e['type'] for e in final);assert counts['TDbWall']==126 and counts['TDbOpening']==45 and counts['TDbGroupOpening']==1 and counts['TDbAxisLabelSet']==40 and counts.get('AcDbBlockReference',0)==0
assert counts['TDbDimension2']==59
for e in final:
 if e['type']=='TDbDimension2':assert len(e['grip']['points'])>=8 and (len(e['grip']['points'])-6)%2==0
sdk=Path(sys.argv[2]);p=max((sdk/'public-properties').glob('*.log'),key=lambda p:p.stat().st_size);s=p.read_text('cp936');numbers={}
for m in re.finditer(r'<BCP_BEGIN_\d+_([A-F0-9]+)>(.*?)<BCP_END_',s,re.S):
 n=re.search(r'门窗编号[^\S\r\n]*:[^\S\r\n]*([^\r\n]*)',m[2])
 if n:numbers[m[1]]=n[1].strip()
assert numbers==read(T/'native-numbers-v181.json')
assert len(numbers)==46 and sum(bool(x) for x in numbers.values())==43
original=Path('C:/Users/PC/Documents/xwechat_files/w248681827_ca52/msg/file/2026-11/百塘村邹总_按PDF间距_原生墙门窗_v8源线复核.dwg')
assert sha(original)=='0DC6A390DDB022241F69698D5FC18FDEEFAC6DA1303893CAE32BF1D23661E205'
out=dict(sourceSHA256=sha(original),sourceNativeGeometryPreserved=protected,savedRootCounts=dict(counts),nativeOpeningNumbers=numbers,COMUsed=False,ComputerUseUsed=False,apronWorkStopped=True)
(T/f'qa-{sys.argv[1]}.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(dict(counts))
