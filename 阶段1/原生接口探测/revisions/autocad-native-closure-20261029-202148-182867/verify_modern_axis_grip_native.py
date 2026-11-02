"""Verify the isolated public modern-grip H right-inner round-trip only."""
import json
import math
import sys
from pathlib import Path
from verify_four_axis_seed_native import near,bag,auxiliary_state


def geometry(entity):
    gp=(entity.get('grip') or {}).get('points') or [];sp=(entity.get('stretch') or {}).get('points') or []
    return gp,sp,entity.get('min'),entity.get('max')


def right_inner(entity):
    gp,sp,lo,hi=geometry(entity)
    if len(gp)!=8 or len(sp)!=4:return None
    points=sorted(sp,key=lambda p:p[0])
    if not all(near(p[1],points[0][1]) for p in points) or not all(b[0]>a[0]+1e-6 for a,b in zip(points,points[1:])):return None
    return points[2]


def expected_forward(entity):
    gp,sp,lo,hi=geometry(entity);anchor=right_inner(entity)
    if anchor is None:return None
    def shift(points):return [[p[0]+(100 if p[0]>=anchor[0]-1e-6 else 0),p[1],p[2]] for p in points]
    return shift(gp),shift(sp),list(lo),[hi[0]+100,hi[1],hi[2]]


def matches(entity,expected):
    gp,sp,lo,hi=geometry(entity)
    return expected is not None and bag(gp,expected[0]) and bag(sp,expected[1]) and near(lo,expected[2]) and near(hi,expected[3])


def check_operation(record,expected,check,label):
    before=record.get('before') or {};after=record.get('after') or {};anchor=right_inner(before)
    check(record.get('completed') is True and record.get('safeToContinue') is True and record.get('error') is None,label+': native modern operation completed safely')
    check(record.get('getFlags')==record.get('moveFlags')==0 and all(record.get(k)==0 for k in ('getStatus','moveStatus','openStatus','transactionStatus')) and record.get('moveAttempted') is True,label+': documented modern overloads and actual API status')
    check(anchor is not None and near(anchor,record.get('semanticRightInnerBefore')),label+': semantic right-inner derived from current actual4SP')
    grips=record.get('modernGrips') or [];selected=record.get('selectedModernGripIndex')
    available=isinstance(selected,int) and not isinstance(selected,bool) and 0<=selected<len(grips) and isinstance(grips[selected],dict)
    if check(available,label+': actual selected modern grip exists'):
        actual=[g for g in grips if isinstance(g,dict) and near(g.get('gripPoint'),anchor)]
        check(len(actual)==1 and actual[0].get('index')==selected and grips[selected].get('index')==selected and grips[selected].get('appDataNonNull') is True,label+': unique real gripPoint match with nonnull appData')
    check(record.get('opaqueAppDataPassedUnchanged') is True and record.get('appDataDecoded') is False,label+': opaque application identity passed without interpretation')
    life=record.get('lifecycle') or {}
    check(life.get('getCalled') is True and life.get('returnedGripDataCount')==len(grips) and life.get('deletedUniqueGripDataCount')==len(grips),label+': all returned SDK-owned grip wrappers released')
    check(all(life.get(k) is True for k in ('gripsToBeDeletedCalled','gripsToBeDeletedReturned','gripsDoneCalled','gripsDoneReturned','allGripWrappersReleased','safeToContinue')) and life.get('cleanupThrew') is False and life.get('appDataDecoded') is False and life.get('appDataDeleted') is False,label+': documented public gripStatus lifecycle and borrowed appData ownership')
    refresh=record.get('regenerate') or {}
    check(refresh.get('commandName')=='_.REGEN' and refresh.get('commandStatus')==5100 and refresh.get('activeTransactionsBefore')==refresh.get('activeTransactionsAfter')==0,label+': refresh only after resource and transaction completion')
    check(record.get('expectedGeometryMatched') is True and matches(after,expected),label+': actual complete8GP/4SP/bounds reach independently declared target')
    if expected is not None:check(bag(record.get('expectedGripPoints') or [],expected[0]) and bag(record.get('expectedStretchPoints') or [],expected[1]) and near(record.get('expectedMin'),expected[2]) and near(record.get('expectedMax'),expected[3]),label+': native expected geometry was not derived from unexpected output')
    for snap in (before,after):
        aux=auxiliary_state(snap,False)
        check(snap.get('type')=='TDbAxisLabelSet' and snap.get('proxy') is False and aux is not None and aux['boundsValid'] and near(aux['radius'],400),label+': native class and400-radius complete circle/control geometry')
        check((snap.get('grip') or {}).get('status')==(snap.get('stretch') or {}).get('status')==snap.get('extentStatus')==0,label+': actual diagnostic public geometry getters')
    check(before.get('handle')==after.get('handle'),label+': same native database target')
    protected=record.get('protectedBefore') or []
    check(len(protected)==214 and near(protected,record.get('protectedAfter')),label+': all original213 plus other native seed unchanged')


def validate(path):
    count=0;failures=[]
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file():return 1,['native modern axis grip host evidence missing']
    d=json.loads(path.read_text('utf-8-sig'))
    check(d.get('contract')=='stage1.modern-axis-grip-native.v1' and d.get('api')=='ObjectARX2020 public modern grip overloads; no COM','public modern grip contract')
    check(d.get('completed') is True and d.get('safeToContinue') is True and d.get('error') is None,'native full round-trip completion')
    check(d.get('initialModelCount')==d.get('finalModelCount')==d.get('reopenedModelCount')==215 and d.get('frozenOriginal213Verified') is True,'frozen original213 and two real native seeds retained')
    before=d.get('axisBefore') or {};after=d.get('axisAfter') or {};saved=d.get('axisSaved') or {};original=geometry(before)
    check(before.get('handle')==after.get('handle')==saved.get('handle')==d.get('axisHandle') and right_inner(before) is not None,'one actual horizontal single-station target')
    check(near(before,after) and near(after,saved),'whole target public snapshot restored and persisted')
    protected=d.get('protectedBefore') or []
    check(len(protected)==214 and len({e.get('handle') for e in protected})==214 and sum(e.get('type')=='TDbAxisLabelSet' for e in protected)==1,'exact original213 plus one protected peer seed')
    check(near(protected,d.get('protectedAfter')) and near(protected,d.get('protectedSaved')),'complete protected cohort unchanged through fresh reopen')
    view=d.get('view') or {};screen=view.get('screenPixels') or [];direction=view.get('viewDirection') or []
    check(all(view.get(k)==5100 for k in ('screenStatus','viewSizeStatus','gripSizeStatus','viewDirStatus')) and view.get('screenRestype') in (5002,5009),'actual public view parameters obtained')
    if check(len(screen)==len(direction)==3 and view.get('viewSize',0)>0 and view.get('gripSizePixels',0)>0,'actual viewport and grip-size values available'):
        check(near(view.get('pixelsPerDrawingUnit'),screen[1]/view['viewSize']) and near(direction[:2],[0,0]) and direction[2]>0,'modern getGripPoints uses actual pixels per drawing unit and top direction')
    operations=d.get('operations') or [];check(len(operations)==2,'one modern forward and one inverse operation without fallback')
    if operations:
        check(near(operations[0].get('offset'),[100,0,0]) and near(operations[0].get('before'),before),'exact forward instruction and actual baseline')
        check_operation(operations[0],expected_forward(before),check,'forward')
        check(near(operations[0].get('protectedBefore'),protected) and near(operations[0].get('protectedAfter'),protected),'forward protected cohort binds to top-level original214')
    if len(operations)>=2:
        check(near(operations[1].get('offset'),[-100,0,0]) and near(operations[1].get('before'),operations[0].get('after')) and near(operations[1].get('after'),after),'actual inverse continues forward output and produces final target')
        check_operation(operations[1],original,check,'inverse')
        check(near(operations[1].get('protectedBefore'),protected) and near(operations[1].get('protectedAfter'),protected),'inverse protected cohort binds to top-level original214')
    for k in ('saveStatus','reopenStatus','closeInputStatus'):check(d.get(k)==0,k)
    check(d.get('firstReaderDestroyed') is True,'first independent reader destroyed before final new database')
    for k in ('controlDWG','outputDWG'):
        file=Path(d.get(k,''));check(file.is_file() and file.resolve().parent==path.resolve().parent,k+': actual isolated file')
    return count,failures


def main(path):
    try:count,failures=validate(path)
    except (OSError,ValueError,KeyError,TypeError,IndexError,AttributeError) as e:count,failures=1,['invalid/incomplete modern grip evidence: '+str(e)]
    for f in failures:print('FAIL:',f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print('Scope: one H seed modern-grip right-inner+100/-100 with all8GP/4SP, SDK lifecycle, original213/peer protection and persistence. No full-generation claim.')
    return int(bool(failures))


if __name__=='__main__':raise SystemExit(main(Path(sys.argv[1])))
