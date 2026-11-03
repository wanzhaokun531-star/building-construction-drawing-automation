from pathlib import Path
import json
T=Path(__file__).parent;R=T.parents[2]
scene=json.loads((R/'阶段1/runs/stage2-product-20261103-120232/preview-final-1/stage2-preview-native.json').read_text('utf-8'))['originalBefore']
placement=json.loads((T/'area-name-placement.json').read_text('utf-8'))
names=['茶水间','公卫1','主卧1','内卫1','次卧1','公卫2','茶水间','客厅','主卧2','儿童房','次卧2','内卫2','公卫3','客厅','主卧3','儿童房','茶水间','内卫3','次卧3']
spec=[]
for a,name in zip(placement,names):
 b=a['sourceNameGlyphBounds'].copy()
 if name.startswith('内卫'):b[1]+=180;b[3]+=180
 spec.append((a['floor'],name,[b[0]-8,b[1]-8,b[2]+8,b[3]+8]))
for n,dx in [(1,0),(2,42887.596250418486),(3,86035.314872116665)]:
 extra=[('储物间',[6860,11160,7800,11615]),('电梯',[2160,9360,2910,9805]),('衣帽间',[11040,4235,11970,4685] if n==1 else [11040,4190,11975,4640] if n==2 else [11035,3890,11975,4345])]
 if n>1:extra += [('衣帽间',[1155,5430,2080,5875])]
 if n==1:extra += [('堂屋',[6300,4170,7250,4610]),('客厅',[980,3470,1930,3905])]
 elif n==2:extra += [('阳台',[6525,-505,7115,-60])]
 else:extra += [('露台',[6510,-1050,7120,-600])]
 for name,b in extra:spec.append((n,name,[b[0]+dx,b[1]+(2 if n==3 and name in ['电梯','储物间'] else 0),b[2]+dx,b[3]+(2 if n==3 and name in ['电梯','储物间'] else 0)]))
records=[];used=set()
for n,name,b in spec:
 members=[]
 for e in scene:
  if e['type'] not in ['AcDbHatch','AcDbPolyline','AcDbText']:continue
  lo,hi=e['min'],e['max']
  if lo[0]>=b[0] and lo[1]>=b[1] and hi[0]<=b[2] and hi[1]<=b[3]:members.append(e)
 assert members,(n,name,b)
 handles=[e['handle'] for e in members];assert not used.intersection(handles),(n,name);used.update(handles)
 bounds=[min(e['min'][0] for e in members),min(e['min'][1] for e in members),max(e['max'][0] for e in members),max(e['max'][1] for e in members)]
 records.append(dict(floor=n,name=name,bounds=bounds,center=[(bounds[0]+bounds[2])/2,(bounds[1]+bounds[3])/2],erase=handles))
(T/'room-text-plan.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
(T/'room-text-plan.tsv').write_text('\n'.join(f"{r['name'].encode('utf-8').hex()} {r['center'][0]:.12f} {r['center'][1]:.12f} "+','.join(r['erase']) for r in records)+'\n',encoding='ascii')
print('Names',len(records),'source glyph entities',len(used))
