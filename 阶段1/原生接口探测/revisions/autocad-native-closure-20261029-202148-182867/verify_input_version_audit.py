"""Validate read-only input snapshots and describe differences, without equality as a pass condition."""
import argparse
import hashlib
import json
import math
from collections import Counter
from pathlib import Path

FROZEN_SHA256 = "5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E"
TOLERANCE = 1e-6
MISSING = object()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def differences(a, b, path=""):
    """Retain every exact difference, including numeric differences within tolerance."""
    if a is MISSING or b is MISSING:
        return [{"path": path, "before": {"missing": True} if a is MISSING else a,
                 "after": {"missing": True} if b is MISSING else b, "withinNumericTolerance": False}]
    if isinstance(a, dict) and isinstance(b, dict):
        result = []
        for key in sorted(a.keys() | b.keys()):
            result.extend(differences(a.get(key, MISSING), b.get(key, MISSING), path+"/"+str(key).replace("~", "~0").replace("/", "~1")))
        return result
    if isinstance(a, list) and isinstance(b, list):
        result = []
        for i in range(max(len(a), len(b))):
            result.extend(differences(a[i] if i < len(a) else MISSING, b[i] if i < len(b) else MISSING, path+"/"+str(i)))
        return result
    if type(a) is type(b) and a == b:
        return []
    numeric = (isinstance(a, (int, float)) and not isinstance(a, bool)
               and isinstance(b, (int, float)) and not isinstance(b, bool))
    near = numeric and math.isfinite(a) and math.isfinite(b) and abs(a-b) <= TOLERANCE
    row = {"path": path, "before": a, "after": b, "withinNumericTolerance": bool(near)}
    if numeric and math.isfinite(a) and math.isfinite(b):
        row["numericDelta"] = b-a
    return [row]


def component(a, b):
    changes = differences(a, b)
    return {"exactChanged": bool(changes), "toleranceEquivalent": all(x["withinNumericTolerance"] for x in changes), "changes": changes}


def entity_map(source):
    return {state.get("geometry", {}).get("handle"): state for state in source.get("entities", [])}


def keyed_diff(before, after, key):
    aa, bb = {x.get(key): x for x in before}, {x.get(key): x for x in after}
    added, removed, changed = sorted(bb.keys()-aa.keys()), sorted(aa.keys()-bb.keys()), []
    for handle in sorted(aa.keys() & bb.keys()):
        delta = component(aa[handle], bb[handle])
        if delta["exactChanged"]:
            changed.append({"key": handle, **delta})
    return {"frozenCount": len(before), "currentCopyCount": len(after), "added": added, "removed": removed, "changed": changed}


def compare_sources(frozen, current):
    aa, bb = entity_map(frozen), entity_map(current)
    added, removed = sorted(bb.keys()-aa.keys()), sorted(aa.keys()-bb.keys())
    changed, categories = [], {key: [] for key in ("geometry", "commonStyle", "nativeText", "publicString", "publicDxfAudit")}
    for handle in sorted(aa.keys() & bb.keys()):
        first, second = aa[handle], bb[handle]
        full = component(first, second)
        if not full["exactChanged"]:
            continue
        row = {"handle": handle, "frozenType": first.get("geometry", {}).get("type"),
               "currentCopyType": second.get("geometry", {}).get("type"), **full, "components": {}}
        pairs = {"geometry": (first.get("geometry"), second.get("geometry")),
                 "commonStyle": (first.get("common"), second.get("common")),
                 "nativeText": (first.get("nativeText"), second.get("nativeText")),
                 "publicString": ((first.get("publicDxf") or {}).get("stringGroups"), (second.get("publicDxf") or {}).get("stringGroups")),
                 "publicDxfAudit": (first.get("publicDxf"), second.get("publicDxf"))}
        for key, (a, b) in pairs.items():
            delta = component(a, b)
            row["components"][key] = delta
            if delta["exactChanged"]:
                categories[key].append(handle)
        changed.append(row)
    resource_before, resource_after = frozen.get("resources", {}), current.get("resources", {})
    resource_changes = {table: keyed_diff(resource_before.get(table, []), resource_after.get(table, []), "handle")
                        for table in ("textStyles", "linetypes", "dimStyles", "layers")}
    resource_changes["databaseSettings"] = component(resource_before.get("databaseSettings"), resource_after.get("databaseSettings"))
    count = lambda states: dict(sorted(Counter(x.get("geometry", {}).get("type", "unknown") for x in states).items()))
    counts_a, counts_b = count(frozen.get("entities", [])), count(current.get("entities", []))
    return {"addedHandles": added, "removedHandles": removed, "changedHandles": [x["handle"] for x in changed],
            "addedEntities": [bb[x] for x in added], "removedEntities": [aa[x] for x in removed], "changedEntities": changed,
            "changedHandleCategories": categories,
            "countsByNativeType": {"frozen": counts_a, "currentCopy": counts_b,
                                   "delta": {k: counts_b.get(k, 0)-counts_a.get(k, 0) for k in sorted(counts_a.keys() | counts_b.keys())}},
            "resourceChanges": resource_changes,
            "publicDxfAvailability": {"frozen": dict(Counter(str((s.get("publicDxf") or {}).get("available")) for s in aa.values())),
                                      "currentCopy": dict(Counter(str((s.get("publicDxf") or {}).get("available")) for s in bb.values()))},
            "numericTolerance": TOLERANCE, "exactChangesRetained": True,
            "semanticEquivalence": "not established; private data, nested definitions, all associations and rendered appearance are outside the captured public readout",
            "causeOfFileChange": "not determined"}


def main(path, receipt=None, output=None):
    assertions, failures = 0, []

    def check(ok, label):
        nonlocal assertions
        assertions += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    if not check(path.is_file(), "native read-only input-version audit evidence missing"):
        print("FAIL:", failures[0]); print("assertions=1 passed=0 failed=1 result=FAIL"); return 1
    path = path.resolve(); run = path.parent
    receipt = (receipt or run/"input-version-bindings.json").resolve()
    output = (output or run/"input-version-diff-review.json").resolve()
    if not check(receipt.is_file(), "source/copy SHA binding receipt missing"):
        print("FAIL:", failures[-1]); return 1
    check(output.parent == run and output != path and output != receipt, "new analysis stays inside run and separate from immutable evidence")
    data = json.loads(path.read_text(encoding="utf-8-sig")); bindings = json.loads(receipt.read_text(encoding="utf-8-sig"))
    check(data.get("contract") == "stage1.input-version-audit.v1" and data.get("api") == "ObjectARX native; no COM", "contract and native API")
    check(data.get("drawingWritten") is False and data.get("workingDatabaseRestored") is True and data.get("error") is None, "read-only audit completed and working database restored")
    check(bindings.get("contract") == "stage1.input-version-bindings.v1", "version bindings contract")
    frozen_path = Path(__file__).resolve().parents[2]/"阶段0"/"输入快照"/"方案平面图.dwg"
    observed_path = run/"observed-original.dwg"
    for key, target in (("frozenPath", frozen_path), ("observedCopyPath", observed_path)):
        check(Path(bindings.get(key, "")).resolve() == target.resolve(), "receipt path: "+key)
    check(bindings.get("frozenSHA256", "").upper() == FROZEN_SHA256, "frozen scheme SHA is the accepted immutable baseline")
    copy_sha, original_sha = bindings.get("observedCopySHA256", "").upper(), bindings.get("observedSourceSHA256", "").upper()
    check(len(copy_sha) == 64 and all(c in "0123456789ABCDEF" for c in copy_sha) and copy_sha == original_sha, "observed copy equals receipt's observed source SHA")
    check(isinstance(bindings.get("observedSourcePath"), str) and bool(bindings.get("observedSourcePath")), "original source identified as receipt metadata only; verifier does not open it")
    for target, expected in ((frozen_path, FROZEN_SHA256), (observed_path, copy_sha)):
        if check(target.is_file(), "bound input file exists: "+target.name):
            check(sha256(target) == expected, "actual bound input bytes match receipt: "+target.name)
    sources = data.get("sources", [])
    check(len(sources) == 2 and [s.get("role") for s in sources] == ["frozen", "currentCopy"], "two explicit input roles")
    by_role = {s.get("role"): s for s in sources}
    for role, target in (("frozen", frozen_path), ("currentCopy", observed_path)):
        source = by_role.get(role, {})
        check(Path(source.get("path", "")).resolve() == target.resolve(), role+": native input path")
        check(source.get("readStatus") == source.get("closeInputStatus") == 0 and source.get("error") is None and source.get("readOnly") is True, role+": native read/close completed")
        states = source.get("entities", []); mapped = entity_map(source)
        check(isinstance(source.get("modelCount"), int) and source.get("modelCount") == len(states) == len(mapped) and None not in mapped, role+": every actual Model entity recorded uniquely")
        if role == "frozen":
            check(source.get("modelCount") == 213, "frozen model count")
        for handle, state in mapped.items():
            check(isinstance(handle, str) and bool(handle) and isinstance(state.get("geometry", {}).get("type"), str), role+"/"+str(handle)+": native identity")
            check(isinstance(state.get("common"), dict) and isinstance(state.get("nativeText"), dict), role+"/"+str(handle)+": public state snapshots")
            public = state.get("publicDxf", {})
            check(public.get("method") == "acdbEntGet" and isinstance(public.get("available"), bool) and public.get("truncated") is False, role+"/"+str(handle)+": actual complete public DXF availability observation")
        resources = source.get("resources", {})
        check(resources.get("role") == role and Path(resources.get("path", "")).resolve() == target.resolve(), role+": separately bound resource input")
        check(resources.get("readOnly") is True and resources.get("readStatus") == resources.get("closeInputStatus") == 0 and resources.get("error") is None and resources.get("status") == "Complete", role+": resources fully read")
        for table in ("textStyles", "linetypes", "dimStyles", "layers"):
            rows = resources.get(table, [])
            check(resources.get("counts", {}).get(table) == len(rows) and resources.get("tableErrors", {}).get(table) is None
                  and len({x.get("handle") for x in rows}) == len(rows) and all(x.get("error") is None for x in rows), role+": complete "+table)
    comparison = compare_sources(by_role.get("frozen", {}), by_role.get("currentCopy", {}))
    analysis = {"contract": "stage1.input-version-diff.v1", "validationPassed": not failures,
                "validationFailures": failures, "nativeReport": str(path), "nativeReportSHA256": sha256(path),
                "bindingReceipt": str(receipt), "bindingReceiptSHA256": sha256(receipt), "inputBindings": bindings,
                "byteIdenticalInputs": FROZEN_SHA256 == copy_sha, "comparison": comparison,
                "passMeaning": "source binding and legal read completion only; unequal inputs are allowed and no private semantic equivalence is asserted"}
    if output.parent == run and output not in (path, receipt):
        if output.exists():
            check(json.loads(output.read_text(encoding="utf-8-sig")) == analysis, "existing analysis is identical; no evidence overwrite")
        else:
            with output.open("x", encoding="utf-8", newline="\n") as stream:
                json.dump(analysis, stream, ensure_ascii=False, indent=2, allow_nan=False); stream.write("\n")
    for failure in failures[:100]: print("FAIL:", failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Differences: added=%d removed=%d changed=%d; equality is not the validation target." % (len(comparison["addedHandles"]), len(comparison["removedHandles"]), len(comparison["changedHandles"])))
    return int(bool(failures))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path); parser.add_argument("--receipt", type=Path); parser.add_argument("--diff", type=Path)
    args = parser.parse_args()
    try:
        raise SystemExit(main(args.report, args.receipt, args.diff))
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
        print("FAIL: incomplete input-version evidence:", error); raise SystemExit(1)
