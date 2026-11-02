"""Join only contiguous existing dimension runs; the frozen v67 is the input."""
import json, math, hashlib
from collections import defaultdict
from pathlib import Path
S=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def load_v67():
    n=read(S/'evidence/native-run-v67.json');p=read(S/'evidence/native-preview-run-v67.json')
    assert sha(Path(n['output'])/'方案施工图_阶段2中间图.dwg')==n['DWGSHA256']
    return read(Path(n['output'])/'native-annotations.json'), read(Path(p['job'])/'stage2-preview-native.json')
def build_contract(a,p):
    details={x['handle']:x for x in a['dimensionDetails']}
    actual={x['handle']:x for x in p['dimensions']};seen=set();groups=defaultdict(list)
    for d in a['dimensions']:
        h=d['handle']
        if h in seen:raise ValueError('duplicate dimension handle')
        seen.add(h);q=details[h];v=actual[h];gp=v['geometry'][0]['grip']['points'];axis=int(q['vertical']);normal=1-axis
        if len(gp)!=8:raise ValueError('input is not one native dimension')
        if any(abs(gp[i][normal]-q['baseline'])>1e-6 for i in (0,1)):raise ValueError('baseline differs')
        x,y=gp[2:4]
        if x[axis]>y[axis]:x,y=y,x
        if abs(abs(y[axis]-x[axis])-d['expected'])>1e-6:raise ValueError('measurement differs')
        kind='third' if d['row'] in ('opening-wall-chain','opening-end-face') else d['row']
        location=q['sourceWall'] if kind in ('internal-opening-chain','local-opening-chain') else d['side']
        groups[(kind,location)].append(dict(handle=h,a=x,b=y,text=gp[4],expected=d['expected'],vertical=q['vertical'],baseline=q['baseline'],layer=v['geometry'][0]['layer']))
    chains=[]
    for (kind,location),segments in groups.items():
        axis=int(segments[0]['vertical']);segments.sort(key=lambda x:x['a'][axis]);runs=[]
        for seg in segments:
            if runs and math.dist(runs[-1][-1]['b'],seg['a'])<1e-6:
                if abs(runs[-1][0]['baseline']-seg['baseline'])>1e-6:raise ValueError('contiguous baseline differs')
                if seg['layer']!=runs[-1][0]['layer']:raise ValueError('contiguous layer differs')
                runs[-1].append(seg)
            else:runs.append([seg])
        for i,run in enumerate(runs):
            chains.append(dict(key=f'{kind}:{location}:{i}',kind=kind,location=location,vertical=run[0]['vertical'],baseline=run[0]['baseline'],segments=run,nodes=[run[0]['a']]+[x['b'] for x in run],values=[x['expected'] for x in run]))
    return chains
def write_contract(path):
    a,p=load_v67();chains=build_contract(a,p);assert len(chains)==24 and sum(map(lambda c:len(c['segments']),chains))==106
    rows=['BCPCHAIN1 404 106 '+str(len(chains))]
    for c in chains:
        rows.append(f"CHAIN {c['key']} {int(c['vertical'])} {len(c['segments'])}")
        for x in c['segments']:
            rows.append('D '+x['handle']+' '+' '.join(format(z,'.17g') for z in [x['expected'],*x['a'],*x['b'],*x['text'],x['baseline']]))
    Path(path).write_text('\n'.join(rows)+'\n',encoding='ascii')
    return chains
