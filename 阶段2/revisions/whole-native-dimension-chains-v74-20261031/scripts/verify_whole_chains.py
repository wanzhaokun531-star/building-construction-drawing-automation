"""Independent saved-object gate. Separate dimensions or CAD wrappers are rejected."""
import json,math,sys
from pathlib import Path
from chain_contract import read,sha,S,load_v67,build_contract
def check(ok,message):
    if not ok:raise ValueError(message)
def point_equal(a,b):return len(a)==len(b)==3 and math.dist(a,b)<1e-6
def verify(payload,contracts,preview=None):
    check(payload.get('contract')=='bcp.whole-tch-dimension-chains.v1','requires actual whole native chain report; old separate dimensions rejected')
    check(payload['error'] is None and payload['savedReopened'],'native save/reopen failed')
    check(payload['command']=='TMergeDim' and payload['commandCount']==82,'unexpected command bridge')
    check(payload['sourceSegments']==106 and payload['chains']==24 and payload['outputModelCount']==322,'lost or split chain')
    check(payload['protectedAfterIdentical'] and payload['apronWorkStopped'],'protected drawing changed')
    check(not payload['COMUsed'] and not payload['ComputerUseUsed'],'unapproved automation')
    check(len(payload['protectedBefore'])==298,'non-target roots not all protected')
    expected={c['key']:c for c in contracts}
    live={x['key']:x for x in payload['live']};saved={x['key']:x for x in payload['saved']}
    check(len(live)==len(saved)==len(expected)==24,'duplicate or missing chain')
    check(sum(x.get('sharedNodeChecks',0) for x in live.values())==82,'not all shared nodes exercised')
    handles=set()
    for key,c in expected.items():
        x=live[key];y=saved[key];n=len(c['segments'])
        check(x['handle']==y['handle'] and x['handle'] not in handles,'chain is not one unique persisted entity');handles.add(x['handle'])
        for v in (x,y):
            check(v['singleNativeEntity'] and v['ownerModel'] and v['originalStyleAndTextVerified'],'wrapped/non-native/style changed')
            check(v['segments']==n and len(v['grips'])==2*n+6,'native node count differs')
            ns=c['nodes'] if v['forward'] else c['nodes'][::-1];vals=c['values'] if v['forward'] else c['values'][::-1];ds=c['segments'] if v['forward'] else c['segments'][::-1]
            check(all(point_equal(v['grips'][2+i],p) for i,p in enumerate(ns)),'actual measurement nodes changed')
            check(len(v['values'])==n and all(abs(a-b)<1e-6 for a,b in zip(v['values'],vals)),'actual values changed')
            check(all(point_equal(v['grips'][n+3+i],d['text']) for i,d in enumerate(ds)),'door/window dimension text placement changed')
            normal=0 if c['vertical'] else 1
            check(all(abs(v['grips'][i][normal]-c['baseline'])<1e-6 for i in (0,1)),'baseline split')
        check(x['exercisePerformed'] and x['singleGripWholeBaseline'] and x['singleSharedNodeLinked'],'one-operation edit proof absent')
        check(all(point_equal(a,b) for a,b in zip(x['grips'],y['grips'])),'persisted grip positions changed')
    check(len(payload['events'])==82 and all(x['status']==5100 and x['rootsBefore']==x['rootsAfter']+1 and x['candidates']==1 for x in payload['events']),'actual merges not observed')
    for event in payload['events']:
        selections=[];controls=''
        for x in event['inputTrace']:
            if x['event']=='beginSSGet':controls=x['controls']
            elif x['event']=='endSSGet':
                if x['status']==5100:selections.append(x)
                else:check(controls=='I' and x['status']==-5001 and x['handles']==[],'unexpected failed selection')
        check(len(selections)==2,'exact native selections failed')
        check(selections[0]['handles']==[event['firstHandle']] and selections[1]['handles']==[event['secondHandle']],'command selected unintended entities')
    if preview is not None:
        check(preview['error'] is None and preview['initialModelCount']==preview['finalModelCount']==322,'preview failed')
        check(preview['originalBefore']==preview['originalAfter'],'preview modified drawing')
        actual={x['handle']:x for x in preview['originalBefore']}
        check(len(actual)==322,'preview duplicate/missing roots')
        for x in saved.values():
            e=actual[x['handle']]
            check(e['type']=='TDbDimension2' and not e['proxy'],'saved root is a wrapper/proxy')
            check(len(e['grip']['points'])==len(x['grips']) and all(point_equal(a,b) for a,b in zip(e['grip']['points'],x['grips'])),'independent saved node read differs')
        check(len([x for x in actual.values() if x['type']=='TDbDimension2'])==32,'extra separate dimensions remain')
    return dict(chains=24,segments=106,mergedOperations=82,protectedRoots=298,savedReopened=True)
def main():
    version=sys.argv[1];b=read(S/f'evidence/native-whole-chain-run-{version}.json');check(b['error'] is None,'run failed');job=Path(b['job']);r=read(job/'whole-dimension-chains.json');check(sha(job/'方案施工图_整体连续天正尺寸.dwg')==b['DWGSHA256']==r['outputSHA256'],'DWG fingerprint changed');check(sha(job/'whole-chain-contract.tsv')==r['contractSHA256'],'contract fingerprint changed')
    c=build_contract(*load_v67());preview=None
    if len(sys.argv)>2:preview=read(sys.argv[2])
    print(json.dumps(verify(r,c,preview),ensure_ascii=False))
if __name__=='__main__':main()
