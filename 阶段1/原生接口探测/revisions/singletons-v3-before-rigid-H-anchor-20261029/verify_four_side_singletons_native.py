"""Strict structural verification of22 independent native two-sided axis sets.

This API never reads semantic sidecars, so Preview can safely bind this source.
"""
import json
import math
import sys
from pathlib import Path
from verify_four_axis_seed_native import near,bag,unique,entity_observation,station_grips,auxiliary_state
from verify_four_side_axis_native import expected_points,observed_radius,check_fit_move
from verify_axis_continuous_number_native import check_number_request


def check_refresh(record,check,label):
    check(record.get("attempted") is True and record.get("commandName")=="_.REGEN" and record.get("commandStatus")==5100,label+": actual public native REGEN completed")
    check(record.get("activeTransactionsBefore")==record.get("activeTransactionsAfter")==0,label+": refresh outside every transaction")


def check_endpoint_index_binding(move,vertical,check,label):
    before=move.get("before") or {};sp=(before.get("stretch") or {}).get("points") or [];gp=(before.get("grip") or {}).get("points") or [];indices=move.get("indices") or []
    valid=len(indices)==1 and isinstance(indices[0],int) and not isinstance(indices[0],bool) and 0<=indices[0]<len(sp)
    if not check(valid,label+": one real semantic stretch endpoint index"):return
    semantic=sp[indices[0]];check(near(move.get("selectedStretchPoint"),semantic),label+": selected semantic endpoint is actual public SP")
    if vertical:
        check(move.get("operationMethod")=="AcDbEntity::moveStretchPointsAt" and move.get("pointIndexSource")=="actual getStretchPoints index" and move.get("operationIndices")==indices and move.get("selectedGripIndex") is None and move.get("selectedGripPoint") is None,label+": verified vertical stretch API/index protocol retained")
    else:
        check(move.get("operationMethod")=="AcDbEntity::moveGripPointsAt(AcDbVoidPtrArray,offset,flags)" and move.get("pointIndexSource")=="actual modern getGripPoints coordinate matched to semantic stretch endpoint",label+": explicit horizontal modern opaque-appData protocol")
        if move.get("moveAttempted") is not True:
            check(move.get("modern") is None and move.get("operationIndices")==[] and move.get("selectedGripIndex") is None and move.get("selectedGripPoint") is None,label+": no modern call or index fabricated for zero offset")
            return
        modern=move.get("modern") or {};rows=modern.get("modernGrips") or [];selected=modern.get("selectedModernGripIndex")
        check(bool(rows) and all(isinstance(g,dict) and g.get("index")==i and isinstance(g.get("gripPoint"),list) and len(g["gripPoint"])==3 and all(isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x) for x in g["gripPoint"]) and isinstance(g.get("appDataNonNull"),bool) for i,g in enumerate(rows)),label+": complete actual modern grip descriptors and indices")
        matches=[i for i,g in enumerate(rows) if isinstance(g,dict) and isinstance(g.get("gripPoint"),list) and len(g["gripPoint"])==len(semantic)==3 and math.dist(g["gripPoint"],semantic)<=1e-6]
        check(len(matches)==1 and selected==matches[0] and move.get("operationIndices")==matches and move.get("selectedGripIndex")==selected and near(move.get("selectedGripPoint"),rows[matches[0]].get("gripPoint")) and rows[matches[0]].get("index")==selected and rows[matches[0]].get("appDataNonNull") is True,label+": real modern gripPoint uniquely matches semantic SP with borrowed nonnull appData")
        check(modern.get("contract")=="stage1.singleton-modern-move.v1" and modern.get("completed") is True and modern.get("safeToContinue") is True and modern.get("error") is None,label+": modern sub-operation completed safely")
        check(modern.get("getFlags")==modern.get("moveFlags")==0 and all(modern.get(k)==0 for k in ("openStatus","getStatus","moveStatus","transactionStatus")) and modern.get("moveAttempted") is True,label+": real public modern get/move and transaction statuses")
        check(near(modern.get("semanticStretchPoint"),semantic) and near(modern.get("selectedGripPoint"),semantic) and near(modern.get("offset"),[move.get("targetAcross",0)-move.get("currentAcross",0),0,0]) and all(modern.get(k)==move.get(k) for k in ("openStatus","moveStatus","transactionStatus")),label+": actual opaque call uses same semantic point, native delta and transaction")
        check(modern.get("opaqueAppDataPassedUnchanged") is True and modern.get("appDataDecoded") is False,label+": appData passed unchanged without private interpretation")
        life=modern.get("lifecycle") or {}
        check(life.get("getCalled") is True and life.get("returnedGripDataCount")==life.get("deletedUniqueGripDataCount")==len(rows) and all(life.get(k) is True for k in ("gripsToBeDeletedCalled","gripsToBeDeletedReturned","gripsDoneCalled","gripsDoneReturned","allGripWrappersReleased","safeToContinue")) and life.get("cleanupThrew") is False and life.get("appDataDecoded") is False and life.get("appDataDeleted") is False,label+": complete documented SDK grip ownership/status lifecycle")
        view=modern.get("view") or {};screen=view.get("screenPixels") or [];direction=view.get("viewDirection") or []
        available=len(screen)==len(direction)==3 and view.get("viewSize",0)>0 and view.get("gripSizePixels",0)>0
        check(available and all(view.get(k)==5100 for k in ("screenStatus","viewSizeStatus","gripSizeStatus","viewDirStatus")) and view.get("screenRestype") in (5002,5009),label+": actual modern view/grip arguments recorded")
        if available:check(near(view.get("pixelsPerDrawingUnit"),screen[1]/view["viewSize"]) and near(direction[:2],[0,0]) and direction[2]>0,label+": modern view pixels per model unit and actual top direction")


def validate(path):
    count=0;failures=[]
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file():return 1,["native four-side singleton host evidence missing"]
    d=json.loads(path.read_text("utf-8-sig"))
    check(d.get("contract")=="stage1.four-side-singletons-native.v3" and d.get("api")=="ObjectARX native; no COM","explicit native singleton fixture contract")
    check(d.get("completed") is True and d.get("safeToContinue") is True and d.get("error") is None,"all native fixture operations completed safely")
    check(d.get("initialModelCount")==215 and d.get("beforeNumberModelCount")==d.get("finalModelCount")==d.get("reopenedModelCount")==235,"complete213 plus22 native annotations persisted")
    check(d.get("allSingleStationFitsCompleted") is True and d.get("numberingCommandsCompleted") is True and d.get("mergeAttempted") is False,"all22 fits/numbers and no merge substitution")
    for k in ("saveStatus","reopenStatus","closeInputStatus"):check(d.get(k)==0,k)
    check(d.get("firstReaderDestroyed") is True,"first reader destroyed before final fresh database")
    check(d.get("numberingSemanticVerified") is False and "pending" in d.get("numberingSemanticStatus",""),"no invented circle-number observation")
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
    if not check(all(v is not None for v in dims+gaps+radii),"all independent placement inputs known"):return count,failures
    def axis_matches(entity,vertical,stations,label):
        check(entity.get("type")=="TDbAxisLabelSet" and entity.get("proxy") is False,label+": proprietary native class")
        obs=entity_observation(entity,vertical);gp,sp=expected_points(wall,dims,gaps,radii,vertical,stations)
        if not check(obs is not None,label+": actual points available"):return
        check(entity.get("grip",{}).get("status")==entity.get("stretch",{}).get("status")==0,label+": native point getters")
        check(near(obs["stations"],stations) and len(obs["stretch"])==4*len(stations) and len(obs["grips"])==6*len(stations)+2,label+": complete station count without hidden extras")
        check(bag(obs["stretch"],sp) and bag(station_grips(obs,vertical),gp),label+": exact entire external stretch/control/circle point set")
        aux=auxiliary_state(entity,vertical)
        check(aux is not None and aux["boundsValid"] and near(aux["radius"],radii[0 if vertical else 1]),label+": exactly two legal joint auxiliary controls and unchanged reference circle radius/bounds")
        check(len(obs["sideA"])==len(obs["sideB"])==len(stations),label+": every station visible on both opposite sides")
        for point in obs["stretch"]:
            check(point[1]>=wall[3]-1e-6 or point[1]<=wall[1]+1e-6 if vertical else point[0]<=wall[0]+1e-6 or point[0]>=wall[2]-1e-6,label+": native leader control is on/outside body envelope")
    expected_digits=[str(i) for i in range(1,len(xs)+1)]
    expected_letters=[c for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ" if c not in "IOZ"][:len(ys)]
    check(d.get("expectedDigits")==expected_digits and d.get("expectedLetters")==expected_letters,"declared numbering follows all10X/12Y candidates and skipI/O/Z")
    expected_targets=[("vertical",s,n,"X"+str(i+1)) for i,(s,n) in enumerate(zip(xs,expected_digits))]+[("horizontal",s,n,"Y"+str(i+1)) for i,(s,n) in enumerate(zip(ys,expected_letters))]
    targets=d.get("axisTargets") or [];fits=d.get("fitSteps") or [];numbers=d.get("numberSteps") or [];final_axes=d.get("finalAxes") or [];saved_axes=d.get("savedAxes") or []
    check(len(targets)==len(fits)==len(numbers)==len(final_axes)==len(saved_axes)==22,"all22 targets/fits/number commands/final/saved native collections")
    handles=[]
    for i,(orientation,station,text,request_id) in enumerate(expected_targets):
        label="axis"+str(i);vertical=orientation=="vertical"
        if i>=len(targets) or i>=len(fits) or i>=len(numbers) or i>=len(final_axes) or i>=len(saved_axes):continue
        target,fit,number,current,saved=targets[i],fits[i],numbers[i],final_axes[i],saved_axes[i];handle=target.get("axisHandle");handles.append(handle)
        check(target.get("orientation")==orientation and near(target.get("station"),station) and target.get("expectedNumber")==text and target.get("id")==request_id,label+": predeclared actual candidate and final number")
        check(fit.get("orientation")==orientation and near(fit.get("targetStation"),station) and fit.get("completed") is True and fit.get("error") is None,label+": declared singleton fitting completed")
        check(fit.get("safeToContinue") is True,label+": native modern lifetime permits continued work")
        check(handle==fit.get("handle")==fit.get("before",{}).get("handle")==fit.get("after",{}).get("handle")==number.get("axisHandle")==current.get("handle")==saved.get("handle"),label+": one actual native handle through fit/number/save")
        axis_matches(fit.get("after") or {},vertical,[station],label+"/fitted")
        axis_matches(current,vertical,[station],label+"/final");axis_matches(saved,vertical,[station],label+"/saved")
        check_refresh(fit.get("translationRefresh") or {},check,label+"/translation")
        moves=fit.get("moves") or [];check(len(moves)==4 and [m.get("roleIndex") for m in moves]==[1,0,2,3],label+": both sides inner then outer")
        refreshes=fit.get("moveRefreshes") or []
        check(len(refreshes)==4 and [r.get("roleIndex") for r in refreshes]==[1,0,2,3],label+": every refresh is logged before reading potentially invalidated native geometry")
        _,desired=expected_points(wall,dims,gaps,radii,vertical,[station]);target_across=sorted(p[1 if vertical else 0] for p in desired);prior=fit.get("translated")
        for j,move in enumerate(moves):
            ml=label+"/move"+str(j);check(near(move.get("before"),prior),ml+": fresh immediately previous public snapshot")
            check_endpoint_index_binding(move,vertical,check,ml)
            check_fit_move(move,vertical,station,target_across,check,ml)
            check(move.get("endpointReached") is True,ml+": endpoint reaches fixed target after refresh")
            refresh=move.get("regenerate") or {}
            check(j<len(refreshes) and near(refreshes[j].get("regenerate"),refresh),ml+": actual refresh log bound to native operation")
            if move.get("moveAttempted") is True:check_refresh(refresh,check,ml)
            else:check(refresh.get("attempted") is False,ml+": no refresh invented for no-op")
            prior=move.get("after")
        attempted_modern=[move.get("modern") for move in moves if not vertical and move.get("moveAttempted") is True]
        check(near(fit.get("modernAttempts") or [],attempted_modern),label+": complete actual modern-attempt log preserved independently of geometry observation")
        check(near(prior,fit.get("after")),label+": fitted collection is the actual last operation result")
        check_number_request(number,check,label+"/number")
        check(number.get("id")==request_id and number.get("requestedNumber")==text,label+": exact singleton request/start number")
        check(near(number.get("before"),fit.get("after")) and near(number.get("after"),current) and near(current,saved),label+": exact actual fit-number-persistence geometry chain")
    check(len(handles)==len(set(handles))==22 and not(set(handles)&{e.get("handle") for e in originals}),"22 distinct native new axis sets")
    for key,expected_entities in (("added",final_axes),("savedAdded",saved_axes)):
        values=d.get(key) or [];mapping={e.get("handle"):e for e in values}
        check(len(values)==len(mapping)==22 and set(mapping)==set(handles),key+": exactly22 independent native additions")
        for e in expected_entities:check(e.get("type")=="TDbAxisLabelSet" and e.get("proxy") is False and near(mapping.get(e.get("handle")),e),key+": every addition remains the actual proprietary native collection")
    issues={x.get("code") for x in d.get("pendingIssues",[])}
    check({"GEOMETRIC_CANDIDATES_REQUIRE_USE_REVIEW","LOCAL_WALL_USE_CD9A","NEAR_Y_CENTERS100","EXISTING_DIMENSIONS_ONLY_TWO_ROWS"}<=issues,"unconfirmed wall use/offset/circle conflict/two dimension-row limitations retained")
    for key in ("controlDWG","outputDWG"):
        file=Path(d.get(key,""));check(file.is_file() and file.resolve().parent==path.resolve().parent,key+": isolated real run file")
    return count,failures


def main(path):
    try:count,failures=validate(path)
    except (OSError,ValueError,KeyError,TypeError,AttributeError,IndexError) as e:count,failures=1,["invalid/incomplete singleton evidence: "+str(e)]
    for f in failures:print("FAIL:",f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: complete10X/12Y candidate geometry,22 true native collections, public numbering commands and persistence; all44 printed numbers remain pending native PDF inspection.")
    return int(bool(failures))


if __name__=="__main__":raise SystemExit(main(Path(sys.argv[1])))
