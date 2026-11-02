"""Verify numbering command structure; actual circle strings require PDF evidence.

Operational verification does not infer numbers from command return, stored
requests, geometry equality or opaque Tianzheng serialization.
"""
import json
import math
import sys
from pathlib import Path


def near(a,b,tol=1e-6):
    if isinstance(a,bool) or isinstance(b,bool):return type(a) is type(b) and a==b
    if isinstance(a,(int,float)) and isinstance(b,(int,float)):
        return math.isfinite(a) and math.isfinite(b) and abs(a-b)<=tol
    if isinstance(a,list) and isinstance(b,list):return len(a)==len(b) and all(near(x,y,tol) for x,y in zip(a,b))
    if isinstance(a,dict) and isinstance(b,dict):return a.keys()==b.keys() and all(near(a[k],b[k],tol) for k in a)
    return type(a) is type(b) and a==b


def check_number_request(record,check,label):
    check(record.get('contract')=='stage1.axis-continuous-number-operation.v1',label+': operation contract')
    check(record.get('api')=='ObjectARX native + public Tianzheng command; no COM',label+': native API')
    check(record.get('error') is None,label+': no native error')
    check(record.get('commandAttempted') is True and record.get('commandName')=='TReAxisNo' and record.get('commandStatus')==5100,label+': documented native command')
    check(record.get('activeTransactionsBefore')==record.get('activeTransactionsAfter')==0,label+': zero transactions across command')
    check(record.get('coordinateTransformStatus')==5100,label+': WCS to UCS')
    check(record.get('pickMatchesActualPublicGrip') is True,label+': real public grip selection point')
    before,after=record.get('before') or {},record.get('after') or {}
    h=record.get('axisHandle')
    check(before.get('handle')==after.get('handle')==h and before.get('type')==after.get('type')=='TDbAxisLabelSet'
          and before.get('proxy') is False and after.get('proxy') is False,label+': same native Tianzheng collection')
    def finite_point(p):
        return isinstance(p,list) and len(p)==3 and all(isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x) for x in p)
    for stage in (before,after):
        for key in ('grip','stretch'):
            raw=stage.get(key) or {};pts=raw.get('points') or []
            check(raw.get('status')==0 and bool(pts) and all(finite_point(v) for v in pts),label+': actual finite native '+key+' arrays')
    pick=record.get('pickWcs')
    check(finite_point(pick) and any(near(pick,v) for v in (before.get('grip') or {}).get('points',[])),label+': pick independently belongs to captured native grip')
    check(finite_point(record.get('pickUcs')),label+': actual UCS point finite')
    for k in ('grip','stretch','curve','layer'):
        check(near(before.get(k),after.get(k)),label+': no geometric mutation '+k)
    check(record.get('tokens')==['TReAxisNo','actual grip WCS converted to UCS',record.get('requestedNumber'),''],label+': supplied documented tokens')
    t=record.get('trace') or {}
    check(t.get('editorAdded') is True and t.get('editorRemoved') is True and t.get('inputAddStatus')==0
          and t.get('inputRemoveStatus') in (0,33) and t.get('retained') is False and t.get('traceDropped') is False,label+': real observer lifecycle')
    events=record.get('commandTrace') or []
    cmds=[v for v in events if v.get('command','').lstrip('_.').upper()=='TREAXISNO']
    check([v.get('event') for v in cmds]==['commandWillStart','commandEnded'],label+': observed start/end')
    check(not any(v.get('event') in ('commandCancelled','commandFailed','unknownCommand') for v in events),label+': no failed native command')
    observed=[]
    for v in record.get('inputTrace') or []:
        vals=([v['selectedHandle']] if v.get('selectedHandle') is not None else [])+(v.get('selectedHandles') or [])
        for item in vals:
            if not observed or observed[-1]!=item:observed.append(item)
    check(observed==[h],label+': actual command selected only the requested collection')
    actual_string=[v.get('value') for v in record.get('inputTrace') or [] if v.get('event')=='endGetString' and isinstance(v.get('value'),str)]
    check(not actual_string or record.get('requestedNumber') in actual_string,label+': any observed input string agrees; missing capture not synthesized')
    check(record.get('commandCompleted') is True and record.get('safeToContinue') is True,label+': native completion classification')
    check(record.get('numberSemantics')=='pending rendered circles or independently confirmed public text evidence',label+': semantic proof not fabricated')


def main(path):
    assertions,failures=0,[]
    def check(ok,label):
        nonlocal assertions
        assertions+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file():
        print('FAIL: axis continuous numbering native evidence missing')
        print('assertions=1 passed=0 failed=1 result=FAIL')
        return 1
    d=json.loads(path.read_text('utf-8-sig'))
    check(d.get('contract')=='stage1.axis-continuous-number-native.v1','fixture contract')
    check(d.get('error') is None,'fixture no native error')
    check(d.get('initialModelCount')==d.get('finalModelCount')==d.get('reopenedModelCount')==214,'frozen scheme213 plus one real TCH set')
    check(d.get('sourceCount')==213,'all originals protected')
    original=d.get('originalBefore') or []
    check(len(original)==213 and len({x.get('handle') for x in original})==213,'complete original identity set')
    check(near(original,d.get('originalAfter')) and near(original,d.get('originalSaved')),'original geometry preserved through persistence')
    op=d.get('operation') or {};check_number_request(op,check,'operation')
    check(op.get('axisHandle') not in {v.get('handle') for v in original},'only new axis collection numbered')
    mode=d.get('fixtureMode');spec={'digits':('vertical','1'),'letters':('horizontal','A'),'skip-io':('horizontal','G')}
    if check(mode in spec,'known diagnostic fixture mode'):
        orientation,start=spec[mode]
        check(op.get('requestedNumber')==start and d.get('orientation')==orientation,'explicit fixed start and orientation')
    for k in ('saveStatus','reopenStatus','closeInputStatus'):check(d.get(k)==0,k)
    check(near(d.get('axisAfter'),d.get('axisSaved')),'same native axis persists')
    for key in ('controlDWG','outputDWG'):
        p=Path(d.get(key,''));check(p.is_file() and p.parent.resolve()==path.parent.resolve(),key+' isolated real file')
    check(d.get('semanticVerification')=='pending native PDF circle-number verification','fixture does not invent semantic PASS')
    for failure in failures:print('FAIL:',failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print('structure='+('FAIL' if failures else 'PASS')+' number_semantics=PENDING')
    print('Scope: command structure, selection, retained TCH type, original protection and persistence. Circle text semantics remain pending native PDF inspection.')
    return int(bool(failures))

def check_merge_operation(record,check,label):
    check(record.get('contract')=='stage1.axis-merge-operation.v1',label+': merge contract')
    check(record.get('error') is None,label+': no native merge error')
    check(record.get('commandAttempted') is True and record.get('commandName')=='TLabelMerge' and record.get('commandStatus')==5100,label+': documented merge public command')
    before=record.get('beforeSets') or [];after=record.get('afterSets') or []
    n=len(before);check(n>=2 and record.get('requestedCollectionCount')==n and len(after)==1,label+': N real sets become one')
    handles=[e.get('handle') for e in before]
    check(len(set(handles))==n and handles==record.get('targetHandles'),label+': distinct actual source handles in requested order')
    check(all(e.get('type')=='TDbAxisLabelSet' and e.get('proxy') is False for e in before+after),label+': TCH type retained')
    vertical=record.get('orientation')=='vertical';check(record.get('orientation') in ('vertical','horizontal'),label+': declared orientation')
    primary=0 if vertical else 1
    def finite_point(p):return isinstance(p,list) and len(p)==3 and all(isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x) for x in p)
    def pointbag(a,b):
        if len(a)!=len(b):return False
        remaining=list(b)
        for p in a:
            at=next((i for i,q in enumerate(remaining) if finite_point(p) and finite_point(q) and math.dist(p,q)<=1e-6),None)
            if at is None:return False
            remaining.pop(at)
        return not remaining
    stations=[];expected_stretch=[];expected_grip=[]
    picks=record.get('pickWcs') or []
    for i,e in enumerate(before):
        stretch=(e.get('stretch') or {}).get('points') or [];grip=(e.get('grip') or {}).get('points') or []
        if check(len(stretch)==4 and len(grip)==8 and all(finite_point(p) for p in stretch+grip),label+': real single-station input'+str(i)):
            x=stretch[0][primary];stations.append(x)
            check(all(abs(p[primary]-x)<=1e-6 for p in stretch),label+': no hidden extra source station'+str(i))
            def actual_pick_allowed(pick):
                if not finite_point(pick):return False
                if any(near(pick,p) for p in grip):return True
                along=sorted(p[1-primary] for p in stretch)
                v=pick[1-primary]
                return abs(pick[primary]-x)<=1e-6 and all(abs(p[2]-pick[2])<=1e-6 for p in stretch) and ((along[0]-1e-6<=v<=along[1]+1e-6) or (along[2]-1e-6<=v<=along[3]+1e-6))
            check(i<len(picks) and actual_pick_allowed(picks[i]),label+': source pick bound to public grip or retained exterior leader'+str(i))
            on=[p for p in grip if abs(p[primary]-x)<=1e-6]
            check(len(on)==6,label+': both sides source station grips'+str(i))
            expected_grip+=on;expected_stretch+=stretch
            if i==0:expected_grip += [p for p in grip if abs(p[primary]-x)>1e-6]
    check(len(stations)==n and all(b>a+1e-6 for a,b in zip(stations,stations[1:])),label+': independent ascending station identities')
    if len(after)==1:
        e=after[0];stretch=e.get('stretch') or {};grip=e.get('grip') or {}
        check(stretch.get('status')==grip.get('status')==0,label+': output real public getters')
        check(pointbag(expected_stretch,stretch.get('points') or []),label+': actual output equals complete stretch-point union')
        check(pointbag(expected_grip,grip.get('points') or []),label+': actual complete station grips and canonical base edge grips')
        check(e.get('handle')==record.get('mergedAxisHandle'),label+': actual surviving collection identified')
    protected=record.get('protectedBefore') or []
    check(near(protected,record.get('protectedAfter')),label+': all non-target entities unchanged')
    check(record.get('beforeModelCount')==len(protected)+n and record.get('afterModelCount')==len(protected)+1,label+': full model membership delta')
    check(record.get('pointTransformStatuses')==[5100]*n and record.get('activeTransactionsBefore')==record.get('activeTransactionsAfter')==0,label+': native picks and zero transaction boundary')
    trace=record.get('trace') or {}
    check(trace.get('editorAdded') is True and trace.get('editorRemoved') is True and trace.get('inputAddStatus')==0 and trace.get('inputRemoveStatus') in (0,33) and trace.get('retained') is False and trace.get('traceDropped') is False,label+': merge observers detached')
    events=record.get('commandTrace') or [];calls=[v for v in events if v.get('command','').lstrip('_.').upper()=='TLABELMERGE']
    check([v.get('event') for v in calls]==['commandWillStart','commandEnded'] and not any(v.get('event') in ('commandCancelled','commandFailed','unknownCommand') for v in events),label+': actual merge start/end and no failure')
    selected=[]
    for v in record.get('inputTrace') or []:
        for h in (([v['selectedHandle']] if v.get('selectedHandle') is not None else [])+(v.get('selectedHandles') or [])):
            if not selected or selected[-1]!=h:selected.append(h)
    check(bool(selected) and bool(handles) and selected[0]==handles[0] and set(selected)==set(handles),label+': actual base and all other requested collections selected')
    check(record.get('pointUnionMatched') is True and record.get('commandCompleted') is True and record.get('safeToContinue') is True,label+': actual merge completion classification')
    check(record.get('numberSemantics')=='not checked by merge; renumber and render both sides separately',label+': merge does not invent numbering semantics')


if __name__=='__main__':
    try:raise SystemExit(main(Path(sys.argv[1])))
    except (OSError,ValueError,TypeError,KeyError,AttributeError) as e:
        print('FAIL: invalid numbering evidence:',e);raise SystemExit(1)
