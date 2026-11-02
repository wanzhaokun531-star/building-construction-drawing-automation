"""Index actual run-specific evidence without flattening failures or candidate versions."""
from pathlib import Path
import json
import re
import hashlib
from datetime import datetime
STAGE=Path(__file__).resolve().parents[1]
SPECS=[
("E10","Family","coverage-family-20261028-230732-027","object-family-native.json","verify-strengthened.txt"),
("E11","Geometry","coverage-geometry-20261028-232845-683","standard-geometry-native.json","verify-strengthened.txt"),
("E12","Safety","coverage-safety-20261028-232804-377","native-safety-resources.json","verify-result.txt"),
("E13","Combine","coverage-combine-20261028-234100-432","dimension-bridge-native.json","verify-corrected.txt"),
("E14","Plot","coverage-plot-20261029-010413-409","native-plot.json","verify-result.txt"),
("E15","Text","coverage-text-20261029-003350-351","text-bridge-native.json","verify-strengthened.txt"),
("E16","List","coverage-list-20261029-004034-482","business-list-native.json","verify-corrected.txt"),
("E17","AxisDelete","coverage-axisdelete-20261029-011107-664","axis-template-delete-native.json","verify-result.txt"),
("E18","Merge","coverage-merge-20261029-013543-370","dimension-merge-native.json","verify-result.txt"),
("E19","SymbolText","coverage-symboltext-20261029-013538-618","symbol-text-bridge-native.json","verify-strengthened.txt"),
("E20a","AxisTwo","coverage-axistwo-20261029-015001-923","axis-template-prune-native.json","verify-result.txt"),
("E20b","AxisThree","coverage-axisthree-20261029-015007-563","axis-template-prune-native.json","verify-result.txt"),
("E21a","PreviewText","coverage-previewtext-20261029-012209-901","nativeTextPreview.json","verify-corrected.txt"),
("E21b","PreviewCombine","coverage-previewcombine-20261029-012310-819","nativeCombinePreview.json","verify-corrected.txt"),
("E21c","PreviewGeometry","coverage-previewgeometry-20261029-012502-574","nativeGeometryPreview.json","verify-result.txt"),
("E22","HorizontalAxis","coverage-horizontalaxis-20261029-015012-533","horizontal-axis-native.json","verify-result.txt"),
("E24","VersionAudit","coverage-versionaudit-20261029-020622-986","input-version-audit.json","verify-result.txt"),
]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):
    b=p.read_bytes()
    return b.decode("utf-16") if b.startswith((b"\xff\xfe",b"\xfe\xff")) else b.decode("utf-8-sig")
archives={}
for f in (STAGE/"原生接口探测/revisions").rglob("NativeAccessProbe.arx"):
    archives.setdefault(digest(f),[]).append(str(f))
records=[]
for eid,kind,rname,report,logname in SPECS:
    run=STAGE/"runs"/rname;receipt=json.loads(read(run/"host-receipt.json"));log=run/logname
    matches=re.findall(r"assertions=(\d+) passed=(\d+) failed=(\d+) result=(PASS|FAIL)",read(log))
    if not matches:raise ValueError("no actual verification count: "+str(log))
    a,p,f,result=matches[-1]
    records.append({"id":eid,"kind":kind,"run":str(run),"nativeReport":str(run/report),"nativeReportSHA256":digest(run/report),"currentValidationLog":str(log),"currentValidationLogSHA256":digest(log),"assertions":int(a),"passed":int(p),"failed":int(f),"result":result,"atRunVerificationExitCode":receipt["VerificationExitCode"],"atRunError":receipt.get("Error"),"binarySHA256":receipt["BinarySHA256"],"archivedBinaryMatches":archives.get(receipt["BinarySHA256"],[]),"sourceFilesAtRun":receipt["SourceFiles"],"validatorDependenciesAtRun":receipt["ValidatorDependencies"],"controllerAtRunSHA256":receipt["ControllerSHA256"],"hostReceiptSHA256":digest(run/"host-receipt.json"),"unload":receipt["Unload"],"cleanup":receipt["Cleanup"],"postValidation":str(run/"post-validation.json") if (run/"post-validation.json").is_file() else None})
audit=STAGE/"原生接口探测/全需求验证_输入与模块终核_20261029.json"
state={"contract":"stage1.coverage-evidence-index.v2","recorded":datetime.now().astimezone().isoformat(),"scope":"Run-specific capability evidence; PASS counts are not requirements-completion percentages or a claim that all tests reran on the current ARX. Old failures/at-run receipts remain separate.","phase1FullAcceptance":False,"phase2Started":False,"businessObjectAccess":"ObjectARX native and public Tianzheng commands","externalOrchestration":"COM module loading and command scheduling only","computerUseUsedThisRound":False,"inputVersionAudit":str(audit),"inputVersionAuditSHA256":digest(audit),"inputWarning":"Frozen scheme5BC remains the tested input; current original92E differs. No attribution or private-semantic equivalence is inferred.","fieldExtraction":str(STAGE/"runs/coverage-list-20261029-004034-482/business-fields-extraction.json"),"results":records}
state["retainedHistoricalFailures"]=[str(STAGE/"runs"/name) for name in ["native-open-axislabels2pt-20261028-225717-034","coverage-merge-20261029-013053-695","coverage-axistwo-20261029-013814-133","coverage-axisthree-20261029-013819-079"]]
state["inputDifferenceReview"]=str(STAGE/"runs/coverage-versionaudit-20261029-020622-986/input-version-diff-review.json")
state["inputDifferenceScope"]="213 to217; four added objects and18 anonymous block-reference names differ; original213 public geometry/common/nativeText unchanged; private/nested semantic equality not asserted. Version audit PASS means validated reads and provenance only."
(STAGE/"全需求验证证据索引.json").write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding="utf-8")
print("indexed",len(records),"real results; retained failures",[(r["kind"],r["failed"]) for r in records if r["failed"]])
print("all binaries archived:",all(r["archivedBinaryMatches"] for r in records))
