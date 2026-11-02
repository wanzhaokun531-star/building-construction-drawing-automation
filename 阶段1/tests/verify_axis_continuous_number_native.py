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

def finite_point(p):
    return isinstance(p,list) and len(p)==3 and all(isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x) for x in p)


def subtract_point_bag(actual,required):
    """Multiset subtraction preserves overlapping station/auxiliary coordinates."""
    if not all(finite_point(p) for p in actual+required):return None
    remaining=list(actual)
    for p in required:
        at=next((i for i,q in enumerate(remaining) if math.dist(p,q)<=1e-6),None)
        if at is None:return None
        remaining.pop(at)
    return remaining


def point_bag(a,b):
    remaining=subtract_point_bag(a,b)
    return remaining is not None and not remaining


def single_auxiliary_policy(entity,vertical):
    """Derive two joint +/-R UI-control states from captured single-station geometry.

    These points are not on the circle circumference. The four stretch points,
    two circle centres, finite bounds and both controls are all independently
    checked; no arbitrary off-station point is discarded.
    """
    gs,ss=entity.get('grip') or {},entity.get('stretch') or {}
    grip,stretch=gs.get('points') or [],ss.get('points') or []
    lo,hi=entity.get('min'),entity.get('max')
    primary,along=(0,1) if vertical else (1,0)
    if gs.get('status')!=0 or ss.get('status')!=0 or entity.get('extentStatus')!=0:return None
    if len(grip)!=8 or len(stretch)!=4 or not all(finite_point(p) for p in grip+stretch+[lo,hi]):return None
    station,z=stretch[0][primary],stretch[0][2]
    if any(abs(p[primary]-station)>1e-6 or abs(p[2]-z)>1e-6 for p in stretch):return None
    if any(abs(p[2]-z)>1e-6 for p in grip+[lo,hi]):return None
    if any(any(near(p,q) for q in stretch[:i]) for i,p in enumerate(stretch)):return None
    if any(not any(near(p,q) for q in grip) for p in stretch):return None
    on=[p for p in grip if abs(p[primary]-station)<=1e-6]
    centers=[p for p in on if not any(near(p,q) for q in stretch)]
    aux=[p for p in grip if abs(p[primary]-station)>1e-6]
    if len(on)!=6 or len(centers)!=2 or len(aux)!=2:return None
    low,high=sorted(centers,key=lambda p:p[along])
    ra,rb=low[along]-lo[along],hi[along]-high[along]
    if ra<=1e-6 or not near(ra,rb):return None
    radius=(ra+rb)/2
    if not near(lo[primary],station-radius) or not near(hi[primary],station+radius):return None
    anchors=[list(low),list(high)];anchors[0][along]+=radius;anchors[1][along]-=radius
    if any(not any(near(p,q) for q in stretch) for p in anchors):return None
    negative,positive=([list(p) for p in anchors] for _ in range(2))
    for p in negative:p[primary]=station-radius
    for p in positive:p[primary]=station+radius
    sign=-1 if point_bag(aux,negative) else 1 if point_bag(aux,positive) else 0
    if not sign:return None
    return {'contract':'stage1.axis-auxiliary-control-states.v1','declaredBeforeCommand':True,'valid':True,
            'baseStation':station,'radius':radius,'sourceSign':sign,'negativeState':negative,'positiveState':positive,
            'mixedSignsAllowed':False,'scope':'only two auxiliary controls; all station/circle grips, stretch points and bounds remain strict'}


def check_merge_operation(record,check,label):
    check(record.get('contract')=='stage1.axis-merge-operation.v2',label+': merge contract')
    check(record.get('api')=='ObjectARX native + public Tianzheng command; no COM',label+': merge native API')
    check(record.get('error') is None,label+': no native merge error')
    check(record.get('commandAttempted') is True and record.get('commandName')=='TLabelMerge' and record.get('commandStatus')==5100,label+': documented merge public command')
    before=record.get('beforeSets') or [];after=record.get('afterSets') or []
    n=len(before);check(n>=2 and record.get('requestedCollectionCount')==n and len(after)==1,label+': N real sets become one')
    handles=[e.get('handle') for e in before]
    check(len(set(handles))==n and handles==record.get('targetHandles'),label+': distinct actual source handles in requested order')
    check(all(e.get('type')=='TDbAxisLabelSet' and e.get('proxy') is False for e in before+after),label+': TCH type retained')
    vertical=record.get('orientation')=='vertical';check(record.get('orientation') in ('vertical','horizontal'),label+': declared orientation')
    primary=0 if vertical else 1
    stations=[];expected_stretch=[];expected_grip=[];policies=[];bounds=[]
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
        policy=single_auxiliary_policy(e,vertical);policies.append(policy)
        if check(policy is not None,label+': source auxiliary/radius/bounds valid'+str(i)):
            bounds.append((e['min'],e['max']))
    check(len(stations)==n and all(b>a+1e-6 for a,b in zip(stations,stations[1:])),label+': independent ascending station identities')
    policy=policies[0] if policies else None
    check(policy is not None and near(record.get('auxiliaryControlPolicy'),policy),label+': auxiliary states declared from complete base geometry before command')
    check(policy is not None and all(p is not None and near(p['radius'],policy['radius']) for p in policies),label+': all input circle radii agree')
    expected_bounds=None
    if len(bounds)==n and bounds:
        expected_bounds={'min':[min(b[0][i] for b in bounds) for i in range(3)],'max':[max(b[1][i] for b in bounds) for i in range(3)]}
    check(expected_bounds is not None and near(record.get('expectedMergedBounds'),expected_bounds),label+': native bounds envelope independently bound to all inputs')
    if len(after)==1:
        e=after[0];stretch=e.get('stretch') or {};grip=e.get('grip') or {}
        check(stretch.get('status')==grip.get('status')==0,label+': output real public getters')
        check(point_bag(expected_stretch,stretch.get('points') or []),label+': actual output equals complete stretch-point union')
        aux=subtract_point_bag(grip.get('points') or [],expected_grip)
        check(aux is not None,label+': actual complete six station grips retained per input station')
        sign=0
        if aux is not None and policy is not None:
            sign=-1 if point_bag(aux,policy['negativeState']) else 1 if point_bag(aux,policy['positiveState']) else 0
        check(sign in (-1,1),label+': only two auxiliary points in one declared joint +/-R state')
        check(aux is not None and point_bag(record.get('actualAuxiliaryPoints') or [],aux) and record.get('auxiliaryAfterSign')==sign,label+': auxiliary audit matches actual multiset remainder')
        check(expected_bounds is not None and e.get('extentStatus')==0 and near({'min':e.get('min'),'max':e.get('max')},expected_bounds),label+': actual complete circle bounds equal input envelope')
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
    check(all(record.get(k) is True for k in ('pointUnionMatched','stationGripsMatched','auxiliaryMatched','boundsMatched','commandCompleted','safeToContinue')),label+': actual merge completion classification')
    check(record.get('numberSemantics')=='not checked by merge; renumber and render both sides separately',label+': merge does not invent numbering semantics')


if __name__=='__main__':
    try:raise SystemExit(main(Path(sys.argv[1])))
    except (OSError,ValueError,TypeError,KeyError,AttributeError) as e:
        print('FAIL: invalid numbering evidence:',e);raise SystemExit(1)
