"""Strict native one-station templates; never treat a return code as station creation."""
import json
import math
import sys
from pathlib import Path

STATIONS={"vertical":[35081.381329981,38881.381329633,40281.381329981,41881.381329981,46081.381329981,47481.381329981,47521.381329981,49621.381329981,53081.381329981],"horizontal":[18092.097205169,19392.097205169,20192.097205169,23792.097205229,25592.097204953,27392.097204953,29892.097204953,31392.097204953]}
def near(a,b,tol=1e-6):
    if isinstance(a,bool) or isinstance(b,bool):return type(a) is type(b) and a==b
    if isinstance(a,(int,float)) and isinstance(b,(int,float)):return math.isfinite(a) and math.isfinite(b) and abs(a-b)<=tol
    if isinstance(a,(list,tuple)) and isinstance(b,(list,tuple)):return len(a)==len(b) and all(near(x,y,tol) for x,y in zip(a,b))
    if isinstance(a,dict) and isinstance(b,dict):return a.keys()==b.keys() and all(near(a[k],b[k],tol) for k in a)
    return type(a) is type(b) and a==b
def bag(a,b):
    if len(a)!=len(b):return False
    used=set()
    for p in a:
        index=next((i for i,q in enumerate(b) if i not in used and near(p,q)),None)
        if index is None:return False
        used.add(index)
    return True
def unique(values):
    result=[]
    for v in sorted(values):
        if not result or abs(v-result[-1])>1e-6:result.append(v)
    return result
def entity_observation(e,vertical):
    gs=e.get("grip",{});ss=e.get("stretch",{});gp=gs.get("points",[]);sp=ss.get("points",[])
    along=0 if vertical else 1;across=1-along
    if not gp or not sp:return None
    stations=unique(p[along] for p in sp)
    a=max(p[across] for p in gp) if vertical else min(p[across] for p in gp)
    b=min(p[across] for p in gp) if vertical else max(p[across] for p in gp)
    first=[p for p in gp if abs(p[across]-a)<=1e-6];second=[p for p in gp if abs(p[across]-b)<=1e-6]
    return {"stations":stations,"sideA":first,"sideB":second,"grips":gp,"stretch":sp,"a":a,"b":b}

def station_grips(observed,vertical):
    along=0 if vertical else 1
    return [p for p in observed["grips"] if any(near(p[along],s) for s in observed["stations"])]

def auxiliary_state(entity,vertical):
    """Validate only the two joint +/-R UI-control states, never drop arbitrary grips."""
    o=entity_observation(entity,vertical)
    if not o:return None
    lo,hi=entity.get("min",[]),entity.get("max",[]);along=0 if vertical else 1;across=1-along
    if len(lo)!=3 or len(hi)!=3 or not o["stations"]:return None
    ra=hi[1]-o["a"] if vertical else o["a"]-lo[0]
    rb=o["b"]-lo[1] if vertical else hi[0]-o["b"]
    if not near(ra,rb) or ra<=0:return None
    radius=(ra+rb)/2;first=o["stations"][0]
    points=[p for p in o["grips"] if not any(near(p[along],s) for s in o["stations"])]
    controls=[p for p in o["stretch"] if near(p[along],first)]
    if len(points)!=2 or len(controls)!=4:return None
    lower=min(p[across] for p in controls);upper=max(p[across] for p in controls)
    edges=[p for p in controls if near(p[across],lower) or near(p[across],upper)]
    if len(edges)!=2 or near(lower,upper):return None
    sign=0
    for candidate in (-1,1):
        expected=[list(p) for p in edges]
        for p in expected:p[along]+=candidate*radius
        if bag(points,expected):sign=candidate;break
    if not sign:return None
    expected_lo=[first-radius,o["b"]-radius,0] if vertical else [o["a"]-radius,first-radius,0]
    expected_hi=[o["stations"][-1]+radius,o["a"]+radius,0] if vertical else [o["b"]+radius,o["stations"][-1]+radius,0]
    return {"sign":sign,"radius":radius,"points":points,"boundsValid":near(lo,expected_lo) and near(hi,expected_hi)}

def singleton_controls_preserved(before,after,vertical):
    bo=entity_observation(before,vertical);ao=entity_observation(after,vertical)
    ba=auxiliary_state(before,vertical);aa=auxiliary_state(after,vertical)
    if not all(x is not None for x in (bo,ao,ba,aa)):return False
    along=0 if vertical else 1;first=bo["stations"][0]
    retained_g=[p for p in station_grips(bo,vertical) if near(p[along],first)]
    retained_s=[p for p in bo["stretch"] if near(p[along],first)]
    return (near(ao["stations"],[first]) and len(ao["grips"])==8 and len(ao["stretch"])==4
        and len(station_grips(ao,vertical))==6 and bag(retained_g,station_grips(ao,vertical))
        and bag(retained_s,ao["stretch"]) and near(ba["radius"],aa["radius"]) and aa["boundsValid"])
def validate(path):
    count=0;failures=[]
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file():return 1,["native four-axis seed host evidence missing"]
    d=json.loads(path.read_text(encoding="utf-8-sig"))
    check(d.get("contract")=="stage1.four-axis-seed-native.v2","contract")
    check(d.get("api")=="ObjectARX native; no COM" and d.get("error") is None,"native API and operation completion")
    check(d.get("completed") is True and d.get("safeToContinue") is True,"both single-station semantic outcomes")
    check(d.get("initialModelCount")==213 and d.get("finalModelCount")==d.get("reopenedModelCount")==215,"full scheme plus exactly two native seed collections")
    for k in ("saveStatus","reopenStatus","closeInputStatus"):check(d.get(k)==0,k)
    check(d.get("firstReaderDestroyed") is True,"first independent reader destroyed before final fresh read")
    original=d.get("originalBefore",[])
    check(len(original)==213 and len({e.get("handle") for e in original})==213,"complete protected original set")
    for key in ("originalAfter","originalSaved"):check(near(original,d.get(key)),key+" public geometry unchanged")
    cases=d.get("seeds",[]);check(len(cases)==2 and {x.get("orientation") for x in cases}==set(STATIONS),"one template of each orientation")
    axis_handles=[]
    for c in cases:
        name=c.get("orientation");vertical=name=="vertical";axis=0 if vertical else 1
        if not check(name in STATIONS,"known seed orientation"):continue
        expected=STATIONS[name];label=name+":";before=c.get("before",{});after=c.get("after",{});saved=c.get("saved",{})
        bo=entity_observation(before,vertical);ao=entity_observation(after,vertical);zo=entity_observation(saved,vertical)
        if not check(all(x is not None for x in (bo,ao,zo)),label+" actual raw point observations"):continue
        check(c.get("sourceHandle")== ("199F" if vertical else "19A3"),label+" frozen template identity")
        check(c.get("completed") is True and c.get("error") is None,label+" actual semantic result")
        check(near(bo["stations"],expected),label+" complete bound original station baseline")
        check(near(c.get("declaredDeleteStations"),expected[1:]) and near(c.get("retainedStation"),expected[0]),label+" delete mask declared independently before commands")
        for snap in (before,after,saved):
            check(snap.get("type")=="TDbAxisLabelSet" and snap.get("proxy") is False,label+" native proprietary class retained")
            check(snap.get("grip",{}).get("status")==snap.get("stretch",{}).get("status")==0,label+" public native points read")
        check(near(after,saved),label+" exact saved native collection geometry")
        for observed in (ao,zo):
            check(near(observed["stations"],[expected[0]]) and len(observed["stretch"])==4 and len(observed["grips"])==8,label+" one station4stretch8grips without hidden remainder")
            check(len(observed["sideA"])==len(observed["sideB"])==1 and near(observed["sideA"][0][axis],expected[0]) and near(observed["sideB"][0][axis],expected[0]),label+" visible symbol on both opposite sides")
            snap=after if observed is ao else saved
            check(singleton_controls_preserved(before,snap,vertical),label+" all six retained station controls/four stretch points exact; only two declared joint auxiliary states")
        ba=auxiliary_state(before,vertical);aa=auxiliary_state(after,vertical);audit=c.get("auxiliaryControlAudit",{})
        if check(ba is not None and aa is not None,label+" actual before/after auxiliary state available"):
            check(audit.get("beforeSign")==ba["sign"] and audit.get("afterSign")==aa["sign"] and bag(audit.get("beforePoints",[]),ba["points"]) and bag(audit.get("afterPoints",[]),aa["points"]),label+" auxiliary audit independently matches raw controls")
            check(audit.get("canonicalMirrorObserved") is (ba["sign"]!=aa["sign"]) and audit.get("afterBoundsValid") is True and audit.get("numberDirectionVerified") is False,label+" canonical-state observation separate from unverified printed numbering direction")
        attempts=c.get("attempts",[]);check(1<=len(attempts)<=2,label+" at most one attempt per opposite side")
        for attempt in attempts:
            check(attempt.get("commandName")=="TDelLabel" and attempt.get("commandAttempted") is True and attempt.get("commandStatus")==5100,label+" public documented command called")
            check(near(attempt.get("declaredDeleteStations"),expected[1:]),label+" command whitelist not expanded from actual differences")
            check(attempt.get("activeTransactionsBeforeCommand")==attempt.get("activeTransactionsAfterCommand")==0,label+" no transaction across command")
            check(attempt.get("transformStatuses")==[5100,5100],label+" WCS-to-UCS window conversion")
            trace=attempt.get("trace",{});check(trace.get("editorAdded") is True and trace.get("editorRemoved") is True and trace.get("retained") is False and trace.get("traceDropped") is False,label+" observers safely detached")
            events=[e.get("event") for e in attempt.get("commandTrace",[]) if e.get("command","").upper()=="TDELLABEL"]
            check(events==["commandWillStart","commandEnded"],label+" actual native start/end without cancellation")
        axis_handles.append(after.get("handle"))
    for key in ("added","savedAdded"):
        values=d.get(key,[]);check(len(values)==2 and {v.get("handle") for v in values}==set(axis_handles),key+" exact native addition cohort")
    check(len(set(axis_handles))==2 and not (set(axis_handles)&{v.get("handle") for v in original}),"two distinct added seed identities")
    for key in ("controlDWG","outputDWG"):
        file=Path(d.get(key,""));check(file.is_file() and file.resolve().parent==path.resolve().parent,key+" task-owned isolated file")
    return count,failures

def main(path):
    try:count,failures=validate(path)
    except (OSError,ValueError,KeyError,TypeError,AttributeError) as e:count,failures=1,["invalid/incomplete seed evidence: "+str(e)]
    for f in failures:print("FAIL:",f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: two single-station TDbAxisLabelSet carriers only; numbering/four-side project axis selection/rendering not yet established.")
    return int(bool(failures))
if __name__=="__main__":raise SystemExit(main(Path(sys.argv[1])))
