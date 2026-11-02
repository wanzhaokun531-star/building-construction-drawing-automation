"""Two independent actual TCH dimensions must become one real two-segment object."""
import json
import math
import sys
from pathlib import Path
from verify_native_plot import near

def main(path):
    checks, failures = 0, []
    def check(ok, label):
        nonlocal checks
        checks += 1
        if not ok: failures.append(label)
        return bool(ok)
    if not path.is_file():
        print("FAIL: native merge host evidence missing")
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d=json.loads(path.read_text(encoding="utf-8-sig"))
    check(d.get("contract")=="stage1.dimension-merge-native.v1", "contract")
    check(d.get("api")=="ObjectARX native; no COM" and d.get("error") is None,"native API and infrastructure completion")
    for k in ("saveStatus","reopenStatus","closeInputStatus"):check(d.get(k)==0,k)
    check(d.get("commandStatus")==5100,"actual public command return")
    check(d.get("initialModelCount")==213 and d.get("beforeMergeModelCount")==215,"two independent dimensions in full scheme")
    check(d.get("finalModelCount")==d.get("reopenedModelCount")==214,"one dimension after merge and reopen")
    a=d.get("originalBefore",[])
    check(len(a)==213 and len({v["handle"] for v in a})==213,"full protected original set")
    for k in ("originalAfter","originalSaved"):check(near(a,d.get(k)),k+" unchanged")
    wall=d.get("wallControl",{});lo,hi=wall.get("left",[]),wall.get("right",[]);split=d.get("splitPoint",[])
    if check(len(lo)==len(hi)==len(split)==3,"actual wall and opening points"):
        check(near(hi[0]-lo[0],3600) and near(split[0]-lo[0],3000) and near(hi[0]-split[0],600),"source geometry independently gives3000/600")
        by_handle={v.get("handle"):v for v in a}
        curve=by_handle.get("CD83",{}).get("curve",{})
        actual_wall=sorted([curve.get("start",[]),curve.get("end",[])],key=lambda v:v[0] if v else 0)
        check(near([lo,hi],actual_wall),"wall control bound to actual protected native curve")
        raw=d.get("rawOpeningGrip",[])
        actual_opening=by_handle.get("CD84",{}).get("grip",{}).get("points",[])
        check(any(near(raw,p) for p in actual_opening) and len(raw)==3 and near(split,[raw[0],lo[1],lo[2]]),"opening actual grip projected into wall drawing plane")
        def derived(entity):
            grip=entity.get("grip",{})
            check(grip.get("status")==0,"actual native grip read succeeded")
            values=[]
            for p in grip.get("points",[]):
                if isinstance(p,list) and len(p)==3 and all(isinstance(x,(int,float)) and math.isfinite(x) for x in p) and abs(p[1]-lo[1])<=1e-6 and abs(p[2]-lo[2])<=1e-6:
                    if not any(near(p,q) for q in values):values.append(p)
            return sorted(values,key=lambda p:p[0])
        expected=[[lo,split],[split,hi]]
        before=d.get("beforeMeasurements",[])
        check(len(before)==2,"two independent real measurements")
        for i,m in enumerate(before):check(m.get("queryStatus")==0 and near(m.get("points"),expected[i]) and near(m.get("segments"),[[3000],[600]][i]),"pre-merge actual points"+str(i))
        for i,e in enumerate(d.get("addedBefore",[])):
            check(i<2 and near(derived(e),expected[i]),"before actual native entity grip data"+str(i))
            check(near(e.get("curve",{}).get("length"),[3000,600][i]),"before actual native entity curve length"+str(i))
        for key in ("addedAfter","addedSaved"):
            entities=d.get(key,[])
            if len(entities)==1:
                check(near(derived(entities[0]),[lo,split,hi]),key+" actual native grip-derived measures")
                check(near(entities[0].get("curve",{}).get("length"),3600),key+" actual full curve length")
        for key in ("afterMeasurements","savedMeasurements"):
            m=d.get(key) or {}
            check(m.get("queryStatus")==0 and near(m.get("points"),[lo,split,hi]) and near(m.get("segments"),[3000,600]),key+" real combined points")
    for key in ("addedBefore","addedAfter","addedSaved"):
        values=d.get(key,[]);expected=2 if key=="addedBefore" else 1
        check(len(values)==expected and all(v.get("type")=="TDbDimension2" and v.get("proxy") is False for v in values),key+" exact native dimension cohort")
    check(near(d.get("addedAfter"),d.get("addedSaved")),"actual final dimension persisted")
    trace=d.get("trace",{})
    check(trace.get("editorAdded") is True and trace.get("editorRemoved") is True and trace.get("retained") is False and trace.get("traceDropped") is False,"observers detached")
    check(trace.get("inputAddStatus")==0 and trace.get("inputRemoveStatus") in (0,33),"native input observer lifecycle")
    originals={v.get("handle") for v in a}
    before_handles=[v.get("handle") for v in d.get("addedBefore",[])]
    check(len(before_handles)==2 and len(set(before_handles))==2 and not (set(before_handles)&originals),"distinct added dimension identities")
    observed=[]
    for event in d.get("inputTrace",[]):
        if event.get("event")=="endSSGet":
            for handle in event.get("selectedHandles",[]):
                if not observed or observed[-1]!=handle:observed.append(handle)
    check(observed==before_handles,"actual command picked main then distinct secondary fixture")
    events=d.get("commandTrace",[])
    check([e.get("event") for e in events if e.get("command","").upper()=="TMERGEDIM"]==["commandWillStart","commandEnded"],"real public TMergeDim start/end")
    check(not any(e.get("event") in ("commandCancelled","commandFailed","unknownCommand") for e in events),"no failed/cancelled native command")
    check(d.get("activeTransactionsBeforeCommand")==d.get("activeTransactionsAfterCommand")==0,"no transaction across command")
    check(d.get("pointTransformStatuses")==[5100,5100],"WCS to UCS picks")
    for k in ("outputDWG","controlDWG"):
        p=Path(d.get(k,""));check(p.parent.resolve()==path.parent.resolve() and p.is_file(),k+" isolated file")
    for f in failures:print("FAIL:",f)
    print(f"assertions={checks} passed={checks-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: two native dimensions merged from actual source points; no full perimeter or visual layout claim.")
    return int(bool(failures))
if __name__=="__main__":raise SystemExit(main(Path(sys.argv[1])))
