"""Verify complete geometric candidates and literal-number evidence separately."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from verify_four_axis_seed_native import near,bag,unique,entity_observation
from verify_axis_continuous_number_native import check_merge_operation,check_number_request

def digest(path):
    with path.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest().upper()
def public_points(entity,name):return entity.get(name,{}).get("points",[])
def observed_radius(entity,vertical):
    o=entity_observation(entity,vertical)
    if not o:return None
    lo,hi=entity.get("min",[]),entity.get("max",[])
    if len(lo)!=3 or len(hi)!=3:return None
    return ((hi[1]-o["a"])+(o["b"]-lo[1]))/2 if vertical else ((o["a"]-lo[0])+(hi[0]-o["b"]))/2

def expected_points(wall,dimensions,gaps,radii,vertical,stations):
    l,b,r,t=wall;ld,bd,rd,td=dimensions;lg,bg,rg,tg=gaps;vr,hr=radii;sp=[];gp=[]
    for s in stations:
        if vertical:
            sp += [[s,t,0],[s,td+tg,0],[s,b,0],[s,bd-bg,0]]
            gp += [[s,t,0],[s,td+tg+vr,0],[s,td+tg,0],[s,b,0],[s,bd-bg-vr,0],[s,bd-bg,0]]
        else:
            sp += [[l,s,0],[ld-lg,s,0],[r,s,0],[rd+rg,s,0]]
            gp += [[l,s,0],[ld-lg-hr,s,0],[ld-lg,s,0],[r,s,0],[rd+rg+hr,s,0],[rd+rg,s,0]]
    if stations:
        gp += [[stations[0]-vr,td+tg,0],[stations[0]-vr,bd-bg,0]] if vertical else [[ld-lg,stations[0]-hr,0],[rd+rg,stations[0]-hr,0]]
    return gp,sp

def validate(path,require_numbering=False):
    count=0;failures=[]
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file():return 1,["native full four-side axis host evidence missing"],False
    d=json.loads(path.read_text(encoding="utf-8-sig"));run=path.resolve().parent
    check(d.get("contract")=="stage1.four-side-axis-native.v1","contract")
    check(d.get("api")=="ObjectARX native; no COM" and d.get("error") is None,"native API and infrastructure completion")
    check(d.get("completed") is True and d.get("safeToContinue") is True,"native generation commands and declared geometry completed")
    check(d.get("initialModelCount")==215 and d.get("beforeMergeModelCount")==235,"two native seeds copied into22 individual native collections")
    check(d.get("afterMergeModelCount")==d.get("finalModelCount")==d.get("reopenedModelCount")==215,"two final native collections plus original213 after merge/save/reopen")
    for k in ("saveStatus","reopenStatus","closeInputStatus"):check(d.get(k)==0,k)
    check(d.get("firstReaderDestroyed") is True,"first reader destroyed before final new database")
    for k in ("allSingleStationFitsCompleted","allNativeMergesCompleted","numberingCommandsCompleted"):check(d.get(k) is True,k)
    check(d.get("numberingSemanticVerified") is False and "pending" in d.get("numberingSemanticStatus",""),"native probe does not invent literal-number observations")
    originals=d.get("originalBefore",[]);check(len(originals)==213 and len({e.get("handle") for e in originals})==213,"complete protected original set")
    for k in ("originalAfter","originalSaved"):check(near(originals,d.get(k)),k+" preserved")
    walls=[e for e in originals if e.get("type")=="TDbWall"]
    check(len(walls)==63 and not any(e.get("type")=="TDbColumn" for e in originals),"bound source wall/column inventory")
    wall=[min(e["min"][0] for e in walls),min(e["min"][1] for e in walls),max(e["max"][0] for e in walls),max(e["max"][1] for e in walls)] if walls else [0]*4
    xvalues=[];yvalues=[];candidate_handles=set()
    for e in walls:
        curve=e.get("curve") or {};a,b=curve.get("start",[]),curve.get("end",[])
        if not check(len(a)==len(b)==3 and curve.get("startStatus")==curve.get("endStatus")==0,"native source wall curve:"+str(e.get("handle"))):continue
        vertical=abs(a[0]-b[0])<=1e-6 and abs(a[1]-b[1])>1e-6;horizontal=abs(a[1]-b[1])<=1e-6 and abs(a[0]-b[0])>1e-6
        if not(vertical or horizontal):continue
        axis=0 if vertical else 1;thickness=e["max"][axis]-e["min"][axis]
        if abs(thickness-200)<=1e-6:
            station=(a[axis]+b[axis])/2;check(near(station,(e["min"][axis]+e["max"][axis])/2),"curve and200-body midpoint:"+e["handle"]);(xvalues if vertical else yvalues).append(station);candidate_handles.add(e["handle"])
    xs,ys=unique(xvalues),unique(yvalues)
    check(len(xs)==10 and len(ys)==12,"all geometric200-wall centers10X/12Y retained, not a reduced subset")
    check(near(d.get("targetXStations"),xs) and near(d.get("targetYStations"),ys),"target stations independently derived from all original walls")
    check(near(d.get("wallBodyBounds"),wall),"body envelope uses actual source wall extents")
    controls=d.get("wallControls",[]);check(len(controls)==63 and {e.get("handle") for e in controls}=={e["handle"] for e in walls},"all wall candidate-source records retained")
    check({e.get("handle") for e in controls if e.get("geometric200Candidate") is True}==candidate_handles,"candidate flags independently match source thickness and orientation")
    dims=[None]*4;side_counts=[0]*4
    for e in originals:
        if e.get("type")!="TDbDimension2":continue
        c=e.get("curve") or {};a,b=c.get("start",[]),c.get("end",[])
        if len(a)!=3 or len(b)!=3:continue
        side=None;value=None
        if abs(a[1]-b[1])<=1e-6:
            if a[1]>wall[3]:side=3;value=a[1]
            elif a[1]<wall[1]:side=1;value=a[1]
        elif abs(a[0]-b[0])<=1e-6:
            if a[0]<wall[0]:side=0;value=a[0]
            elif a[0]>wall[2]:side=2;value=a[0]
        if side is not None:
            side_counts[side]+=1
            dims[side]=value if dims[side] is None else (max(dims[side],value) if side in (2,3) else min(dims[side],value))
    check(side_counts==[2]*4,"existing source dimensions honestly2 rows per outer side")
    check(near(d.get("outerDimensionBaselines"),dims),"all outermost baselines derived from actual native curves")
    refs=d.get("referenceControls",[]);check(len(refs)==4 and {r.get("side") for r in refs}=={"left","bottom","right","top"},"four independently measured reference sides")
    gaps=[None]*4;radii=[None,None]
    handles={"left":"19A4","bottom":"19A0","right":"19AB","top":"19A7"}
    for r in refs:
        side=r.get("side");vertical=side in ("top","bottom");axis=r.get("sourceAxisEntity",{});obs=entity_observation(axis,vertical);radius=observed_radius(axis,vertical)
        if not check(obs is not None and radius is not None,"actual reference collection raw geometry:"+str(side)):continue
        check(axis.get("type")=="TDbAxisLabelSet" and axis.get("proxy") is False,"native reference proprietary class:"+side)
        check(r.get("dimensionHandle")==handles.get(side),"bound reference dimension handle:"+side)
        a,b=r.get("curveStart",[]),r.get("curveEnd",[])
        if not check(len(a)==len(b)==3,"actual reference curve endpoints:"+side):continue
        baseline=a[1] if vertical else a[0]
        check(abs((a[1]-b[1]) if vertical else (a[0]-b[0]))<=1e-6,"reference baseline orientation:"+side)
        gap=obs["a"]-radius-baseline if side=="top" else baseline-obs["b"]-radius if side=="bottom" else baseline-obs["a"]-radius if side=="left" else obs["b"]-radius-baseline
        check(gap>=0 and near(r.get("curveBaseline"),baseline) and near(r.get("nearCircleGapModel"),gap),"reference gap computed independently:"+side)
        check(near(r.get("circleRadiusModel"),radius) and near(radius,400) and near(r.get("circleRadiusPaperMM"),radius/100) and near(r.get("nearCircleGapPaperMM"),gap/100),"paper-mm conversion and observed radius:"+side)
        gaps[{"left":0,"bottom":1,"right":2,"top":3}[side]]=gap;radii[0 if vertical else 1]=radius
    check(near(d.get("referenceNearCircleGaps"),gaps) and near(d.get("referenceRadii"),radii) and d.get("paperScale")==100,"target radius/gap values bound to original measured geometry at1:100")
    if not check(all(v is not None for v in dims+gaps+radii),"all independent placement inputs known"):return count,failures,False
    def axis_matches(entity,vertical,stations,label):
        check(entity.get("type")=="TDbAxisLabelSet" and entity.get("proxy") is False,label+": proprietary native class")
        obs=entity_observation(entity,vertical);gp,sp=expected_points(wall,dims,gaps,radii,vertical,stations)
        if not check(obs is not None,label+": actual points available"):return
        check(entity.get("grip",{}).get("status")==entity.get("stretch",{}).get("status")==0,label+": native point getters")
        check(near(obs["stations"],stations) and len(obs["stretch"])==4*len(stations) and len(obs["grips"])==6*len(stations)+2,label+": complete station count without hidden extras")
        check(bag(obs["stretch"],sp) and bag(obs["grips"],gp),label+": exact entire external control/circle/auxiliary point set")
        check(len(obs["sideA"])==len(obs["sideB"])==len(stations),label+": every station visible on both opposite sides")
        for point in obs["stretch"]:
            check(point[1]>=wall[3]-1e-6 or point[1]<=wall[1]+1e-6 if vertical else point[0]<=wall[0]+1e-6 or point[0]>=wall[2]-1e-6,label+": native leader control is on/outside body envelope")
    steps=d.get("fitSteps",[]);check(len(steps)==22,"all22 native single-station fits executed")
    seen={True:[],False:[]};fit_handles=[]
    for i,step in enumerate(steps):
        vertical=step.get("orientation")=="vertical";station=step.get("targetStation");label="fit"+str(i)
        check(step.get("completed") is True and step.get("error") is None,label+": completed")
        check(any(near(station,x) for x in (xs if vertical else ys)),label+": actual wall candidate")
        if isinstance(station,(int,float)):seen[vertical].append(station)
        after=step.get("after",{});fit_handles.append(after.get("handle"));axis_matches(after,vertical,[station],label)
        check(step.get("handle")==after.get("handle")==step.get("before",{}).get("handle"),label+": same native collection")
        moves=step.get("moves",[]);check(len(moves)==4 and [m.get("roleIndex") for m in moves]==list(range(4)),label+": all four source endpoint roles audited")
        for move in moves:
            delta=(move.get("targetAcross",0)-move.get("currentAcross",0));attempted=move.get("moveAttempted") is True
            check((attempted and abs(delta)>1e-6 and all(move.get(k)==0 for k in ("openStatus","moveStatus","transactionStatus"))) or (not attempted and abs(delta)<=1e-6 and all(move.get(k) is None for k in ("openStatus","moveStatus","transactionStatus"))),label+": real native operation or explicitly unattempted zero offset")
            before=move.get("before",{});indices=move.get("indices",[]);sp=public_points(before,"stretch")
            check(len(indices)==1 and isinstance(indices[0],int) and 0<=indices[0]<len(sp),label+": actual current stretch index")
    check(near(sorted(seen[True]),xs) and near(sorted(seen[False]),ys) and len(set(fit_handles))==22,"all candidates fitted exactly once in distinct collections")
    before_merge=d.get("addedBeforeMerge",[]);check(len(before_merge)==22 and {e.get("handle") for e in before_merge}==set(fit_handles),"complete actual pre-merge addition cohort")
    merge_steps=d.get("mergeSteps",[]);check(len(merge_steps)==2,"one native merge per orientation")
    for i,merge in enumerate(merge_steps):
        vertical=i==0;n=len(xs if vertical else ys);label="merge"+str(i)
        check_merge_operation(merge,check,label+"/independent-helper")
        check(merge.get("commandName")=="TLabelMerge" and merge.get("commandAttempted") is True and merge.get("commandStatus")==5100 and merge.get("commandCompleted") is True and merge.get("safeToContinue") is True and merge.get("error") is None,label+": native public command completion")
        check(merge.get("requestedCollectionCount")==n and merge.get("beforeModelCount")-merge.get("afterModelCount")==n-1,label+": actualN-to1 model delta")
        check(merge.get("pointUnionMatched") is True and near(merge.get("protectedBefore"),merge.get("protectedAfter")),label+": all non-targets preserved and declared union matched")
        check(merge.get("activeTransactionsBefore")==merge.get("activeTransactionsAfter")==0,label+": no transaction across command")
        for key in ("beforeSets","afterSets"):
            values=merge.get(key,[]);check(len(values)==(n if key=="beforeSets" else 1) and all(e.get("type")=="TDbAxisLabelSet" and e.get("proxy") is False for e in values),label+": actual native cohort:"+key)
    numbers=d.get("numberSteps",[]);check(len(numbers)==2,"both native numbering commands executed")
    final_axes=d.get("finalAxes",[]);saved_axes=d.get("savedAxes",[])
    check(len(final_axes)==len(saved_axes)==2,"two final orientation collections")
    if len(final_axes)==len(saved_axes)==2:
        for i,(current,saved) in enumerate(zip(final_axes,saved_axes)):
            axis_matches(current,i==0,xs if i==0 else ys,"final"+str(i));axis_matches(saved,i==0,xs if i==0 else ys,"saved"+str(i));check(near(current,saved),"new-reader persisted entire native collection"+str(i))
    for i,number in enumerate(numbers):
        label="number"+str(i)
        check_number_request(number,check,label+"/independent-helper")
        check(number.get("commandName")=="TReAxisNo" and number.get("commandAttempted") is True and number.get("commandStatus")==5100 and number.get("commandCompleted") is True and number.get("safeToContinue") is True and number.get("error") is None,label+": public command completed")
        check(number.get("requestedNumber")==("1" if i==0 else "A") and number.get("pickMatchesActualPublicGrip") is True,label+": requested start and actual native grip")
        check(number.get("activeTransactionsBefore")==number.get("activeTransactionsAfter")==0,label+": no transaction across numbering")
        check(near(number.get("before"),number.get("after")),label+": numbering preserves native geometry")
    for key in ("addedAfterMerge","added","savedAdded"):
        values=d.get(key,[]);check(len(values)==2 and all(e.get("type")=="TDbAxisLabelSet" and e.get("proxy") is False for e in values),key+": exactly two proprietary new annotations; no substitute blocks/text or full-length lines")
    issues={x.get("code") for x in d.get("pendingIssues",[])}
    check({"GEOMETRIC_CANDIDATES_REQUIRE_USE_REVIEW","LOCAL_WALL_USE_CD9A","NEAR_Y_CENTERS100","EXISTING_DIMENSIONS_ONLY_TWO_ROWS"}<=issues,"short-wall/offset/circle-conflict and dimension-row limitations retained")
    expected_digits=[str(i) for i in range(1,len(xs)+1)];expected_letters=[c for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ" if c not in "IOZ"][:len(ys)]
    check(d.get("expectedDigits")==expected_digits and d.get("expectedLetters")==expected_letters,"number expectations derive from candidate count and skipI/O/Z policy, not readback claims")
    observed=False;observation=run/"four-side-numbering-observation.json"
    if observation.is_file():
        o=json.loads(observation.read_text(encoding="utf-8-sig"));source=Path(d.get("outputDWG",""));pdf=Path(o.get("pdfPath",""))
        check(o.get("contract")=="stage1.four-side-axis-numbering-observation.v1","external native-render number observation contract")
        check(Path(o.get("sourceDWG","")).resolve()==source.resolve() and o.get("sourceDWGSHA256")==digest(source),"number observations bound to this exact saved DWG")
        check(pdf.is_file() and o.get("pdfSHA256")==digest(pdf) and o.get("method")=="manual inspection of native PDF rendering","real PDF/inspection-method binding")
        observed=o.get("topDigits")==o.get("bottomDigits")==expected_digits and o.get("leftLetters")==o.get("rightLetters")==expected_letters
        check(observed,"all44 circles explicitly observed with correct continuous numbers on both sides")
    if require_numbering:check(observed,"literal numbering remains unverified until independent native LIST/PDF observation is provided")
    return count,failures,observed

def main(path,require_numbering=False):
    try:count,failures,observed=validate(path,require_numbering)
    except (OSError,ValueError,KeyError,TypeError,AttributeError,IndexError) as e:count,failures,observed=1,["invalid/incomplete four-side evidence: "+str(e)],False
    for f in failures[:100]:print("FAIL:",f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("number_semantics="+("PASS" if observed else "UNVERIFIED"))
    print("Scope: complete10X/12Y geometric candidate carrier only; source wall usage/offset decisions and circle collisions remain pending; literal labels need independent observation.")
    return int(bool(failures))
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("report",type=Path);p.add_argument("--require-numbering",action="store_true");a=p.parse_args();raise SystemExit(main(a.report,a.require_numbering))
