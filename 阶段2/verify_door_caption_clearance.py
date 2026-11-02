"""Conservative saved-ink gate for door captions, including the opened door graphic."""
from pathlib import Path
import math,sys,json
from chain_contract import S,read,sha
def overlaps(a,b,gap=80):
    return a[0]<b[2]+gap-1e-6 and a[2]>b[0]-gap+1e-6 and a[1]<b[3]+gap-1e-6 and a[3]>b[1]-gap+1e-6
def box(e):return [*e['min'][:2],*e['max'][:2]]
def caption_source():return read(Path(read(S/'evidence/native-run-v67.json')['output'])/'native-annotations.json')['captions']
def old_geometry():
    p=read(S/'evidence/whole-chain-preview-v74.json');return read(Path(p['job'])/'stage2-preview-native.json')['originalBefore']
def own_door_collisions(geometry,captions):
    idx={e['handle']:e for e in geometry}
    return [c['opening'] for c in captions if c['text'].startswith('M') and overlaps(box(idx[c['handle']]),box(idx[c['opening']]))]
def dimension_obstacles(report,geometry):
    run=read(S/'evidence/native-whole-chain-run-v74.json');source=Path(run['job'])/'whole-dimension-chains.json'
    if sha(source)!=run['reportSHA256']:raise ValueError('whole-chain baseline report changed')
    expected={x['handle']:x['segments'] for x in read(source)['saved']};records=report.get('dimensionAudit',[]);idx={e['handle']:e for e in geometry}
    if len(records)!=24 or {x['handle'] for x in records}!=set(expected):raise ValueError('dimension font/baseline audit missing')
    obstacles=[]
    for row in records:
        e=idx[row['handle']];n=expected[row['handle']];gp=e['grip']['points']
        if e['type']!='TDbDimension2' or row['segments']!=n or len(gp)!=2*n+6 or len(row['texts'])!=n:raise ValueError('dimension audit shape differs')
        if len(row['baseline'])!=2 or any(math.dist(a,b)>1e-6 for a,b in zip(row['baseline'],gp[:2])):raise ValueError('dimension baseline audit differs from saved object')
        a,b=gp[:2];obstacles.append([min(a[0],b[0])-10,min(a[1],b[1])-10,max(a[0],b[0])+10,max(a[1],b[1])+10])
        for j,t in enumerate(row['texts']):
            p=t['position'][0];bounds=t['fontBounds']
            if math.dist(p,gp[n+3+j])>1e-6 or abs(t['value']-math.dist(gp[2+j],gp[3+j]))>1e-6:raise ValueError('dimension text audit differs from actual saved node/value')
            if not (t['height']>0 and bounds[0]<bounds[2] and bounds[1]<bounds[3] and bounds[0]<=p[0]<=bounds[2] and bounds[1]<=p[1]<=bounds[3]):raise ValueError('invalid native font bounds')
            obstacles.append(bounds)
    return obstacles
def verify_new(report,preview):
    if report['error'] is not None or not report['savedReopened'] or not report['allProtectedUnchanged']:raise ValueError('native save/protection failed')
    if report['contract']!='bcp.door-caption-clearance.v1' or report['targetCount']!=14:raise ValueError('wrong door caption scope')
    if report['COMUsed'] or report['ComputerUseUsed'] or report['TianzhengBusinessCommandsUsed'] or not report['apronWorkStopped']:raise ValueError('unapproved route')
    if preview['error'] is not None or preview['initialModelCount']!=322 or preview['finalModelCount']!=322 or preview['originalBefore']!=preview['originalAfter']:raise ValueError('saved preview failed')
    old={e['handle']:e for e in old_geometry()};new={e['handle']:e for e in preview['originalBefore']};captions=caption_source();targets={c['handle'] for c in captions if c['text'].startswith('M')}
    if len(new)!=322 or len(targets)!=14:raise ValueError('missing objects')
    for h,e in old.items():
        if h not in targets and e!=new[h]:raise ValueError('changed protected geometry '+h)
    if own_door_collisions(list(new.values()),captions):raise ValueError('door number overlaps door graphics')
    rows={x['handle']:x for x in report['targets']}
    if len(report['targets'])!=14 or set(rows)!=targets:raise ValueError('missing/duplicate target')
    dimension_boxes=dimension_obstacles(report,list(new.values()))
    other_openings=[e for e in new.values() if e['type'] in ('TDbOpening','TDbGroupOpening')]
    fixed_text=[e for e in new.values() if e['type'] in ('TDbText','TDbSymbElevation','AcDbText','AcDbMText') and e['handle'] not in targets]
    for h in targets:
        a,b=old[h],new[h];row=rows[h]
        if b['type']!='AcDbText' or b['proxy'] or a['layer']!=b['layer'] or not row['styleUnchanged']:raise ValueError('door number text/style changed')
        if any(abs((a['max'][i]-a['min'][i])-(b['max'][i]-b['min'][i]))>1e-6 for i in (0,1)):raise ValueError('door number size/rotation changed')
        p=b['stretch']['points'][0];anchor_a,anchor_b=row['anchorA'][0],row['anchorB'][0];vertical=abs(anchor_a[0]-anchor_b[0])<1e-6;along=int(vertical);normal=1-along
        if row['side']*(p[normal]-anchor_a[normal])<=0 or not min(anchor_a[along],anchor_b[along])-1e-6<=p[along]<=max(anchor_a[along],anchor_b[along])+1e-6:raise ValueError('door number changed side or escaped corresponding opening')
        old_side=1 if a['stretch']['points'][0][normal]>(anchor_a[normal]+anchor_b[normal])/2 else -1
        if old_side!=row['side']:raise ValueError('changed default side')
        if math.dist(p,row['after'][0])>1e-6:raise ValueError('saved text position differs')
        if any(overlaps(box(b),box(e)) for e in other_openings) or any(overlaps(box(b),box(e),60) for e in fixed_text):raise ValueError('door number overlaps other opening or fixed text')
        if any(overlaps(box(b),box(new[k]),60) for k in targets if k!=h):raise ValueError('door numbers overlap each other')
        if any(overlaps(box(b),d,60) for d in dimension_boxes):raise ValueError('door number overlaps dimension text or baseline')
    return dict(doorNumbers=14,doorGraphicsOverlaps=0,protectedRoots=308,wholeDimensionsUnchanged=True)
if __name__=='__main__':
    bad=own_door_collisions(old_geometry(),caption_source())
    print(json.dumps(dict(oldV74DoorCaptionOverlaps=bad,count=len(bad)),ensure_ascii=False))
    if bad:raise SystemExit(1)
