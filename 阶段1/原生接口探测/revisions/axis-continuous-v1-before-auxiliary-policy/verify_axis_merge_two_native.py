"""Two real numeric single stations merge then numbering; circle semantics separate."""
import json
import sys
from pathlib import Path
from verify_axis_continuous_number_native import near,check_number_request,check_merge_operation

def main(path):
    assertions,failures=0,[]
    def check(ok,label):
        nonlocal assertions
        assertions+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file():
        print('FAIL: two-axis merge native host evidence missing')
        print('assertions=1 passed=0 failed=1 result=FAIL')
        return 1
    d=json.loads(path.read_text('utf-8-sig'))
    check(d.get('contract')=='stage1.axis-merge-two-native.v1' and d.get('error') is None,'native precursor contract/no error')
    check(d.get('initialModelCount')==215 and d.get('preparedModelCount')==216 and d.get('finalModelCount')==d.get('reopenedModelCount')==215,'full seed215 to216 to215 and save reopen')
    original=d.get('originalBefore') or []
    check(len(original)==213 and len({e.get('handle') for e in original})==213,'complete protected original213')
    check(near(original,d.get('originalAfter')) and near(original,d.get('originalSaved')),'all original captured fields preserved')
    other=d.get('otherAxisBefore') or {}
    check(other.get('type')=='TDbAxisLabelSet' and other.get('proxy') is False and near(other,d.get('otherAxisAfter')) and near(other,d.get('otherAxisSaved')),'unselected opposite-direction seed unchanged')
    merge=d.get('merge') or {};check_merge_operation(merge,check,'merge')
    number=d.get('numbering') or {};check_number_request(number,check,'numbering')
    check(number.get('requestedNumber')=='1' and number.get('axisHandle')==merge.get('mergedAxisHandle'),'renumber real surviving merged collection1')
    check(near(number.get('after'),d.get('axisAfter')) and near(d.get('axisAfter'),d.get('axisSaved')),'actual numbered result persists')
    walls=d.get('wallControls') or []
    if check(len(walls)==2,'two actual source wall controls'):
        by={e.get('handle'):e for e in original}
        for w in walls:
            e=by.get(w.get('sourceHandle')) or {};c=e.get('curve') or {};a,b=c.get('start'),c.get('end')
            check(e.get('type')=='TDbWall' and near(a,w.get('start')) and near(b,w.get('end')),'wall source endpoint bound to captured original')
        x=[w.get('axisX') for w in walls]
        for e,value in zip(merge.get('beforeSets') or [],x):
            check(all(abs(p[0]-value)<=1e-6 for p in (e.get('stretch') or {}).get('points',[])),'numeric source set on actual wall X')
    for key in ('saveStatus','reopenStatus','closeInputStatus'):check(d.get(key)==0,key)
    for key in ('controlDWG','outputDWG'):
        p=Path(d.get(key,''));check(p.is_file() and p.parent.resolve()==path.parent.resolve(),key+' isolated file')
    check(d.get('semanticVerification')=='pending native PDF bottom/top circles1/2','does not invent circle digits')
    for failure in failures:print('FAIL:',failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print('structure='+('FAIL' if failures else 'PASS')+' number_semantics=PENDING')
    print('Scope: actual two native sets merged into one at scheme wall X, actual numbering command, retained original/other seed and persistence. PDF circles1/2 on both sides remain a separate gate.')
    return int(bool(failures))
if __name__=='__main__':
    try:raise SystemExit(main(Path(sys.argv[1])))
    except (OSError,ValueError,TypeError,KeyError,AttributeError) as e:
        print('FAIL: invalid merge-two evidence:',e);raise SystemExit(1)
