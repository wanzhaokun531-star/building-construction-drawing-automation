"""Check new Tianzheng axis-label sets from 2 or 3 real scheme-based LINE axes."""
import json
import math
import sys
from pathlib import Path
from verify_scheme_native import near, points_equal


def main(path):
    count, failures = 0, []

    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def same(a, b, label):
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": native type")
        check(a.get("layer") == b.get("layer"), label + ": layer")
        check(near(a.get("min"), b.get("min")) and near(a.get("max"), b.get("max")), label + ": bounds")
        for kind in ("grip", "stretch"):
            aa, bb = a.get(kind, {}), b.get(kind, {})
            check(aa.get("status") == bb.get("status") and points_equal(aa.get("points"), bb.get("points")), label + ": " + kind)

    if check(path.is_file(), "axis-label creation evidence missing"):
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        check(d.get("contract") == "stage1.axis-labels-native.v1", "contract")
        check(d.get("api") == "ObjectARX native; no COM" and d.get("commandName") in {"TMultAxisDim", "GRIDS"}, "native command observed in the host")
        n = d.get("axisCount")
        check(n in (2, 3), "two or three axis fixture")
        check(d.get("initialModelCount") == d.get("sourceCount") == 213, "full frozen scheme start")
        check(d.get("zoomStatus") == 5100 and d.get("error") is None, "native zoom and operation error state")
        if d.get("commandApi") == "sendStringToExecute":
            check(d.get("commandName") in {"GRIDS", "TMultAxisDim"}, "registered native command after precise preselection")
            check(d.get("quiescentObserved") is True and d.get("quiescentCount", 0) >= 1, "native idle observer actually ran")
            idle = d.get("idleCheck") or {}
            check(idle.get("cmdaActiveGetStatus") == 5100 and idle.get("cmdaActive") == 0, "preselection set only while no command executes")
            check(idle.get("pickfirstGetStatus") == 5100 and idle.get("pickfirst") == 1, "existing PICKFIRST enabled without settings change")
            check(idle.get("inputPending") == 0, "no pre-existing document command buffer before selection")
            control = str(Path(d.get("controlDWG", "")).resolve()).casefold()
            check(all(str(Path(idle.get(k, "")).resolve()).casefold() == control for k in ("currentDWG", "mdiActiveDWG")), "idle selection belongs to isolated control document")
            before, readback = d.get("preselectionBefore", {}), d.get("pickfirstReadback", {})
            check(before.get("getStatus") == 5100 and before.get("available") is True and before.get("nullResult") is True and before.get("selectionCount") == 0 and before.get("handles") == [], "existing preselection proven empty before setting axes")
            check(d.get("pickfirstSetStatus") == 5100, "native exact preselection succeeded")
            expected = {a.get("handle") for a in d.get("inputAxes", [])}
            actual = readback.get("handles", [])
            check(readback.get("getStatus") == 5100 and readback.get("available") is True and readback.get("selectionCount") == n and len(actual) == n and set(actual) == expected, "native preselection readback is exactly the real input axes")
            check(d.get("sendStringStatus") == d.get("queueStatus") == d.get("commandStatus") == 0, "native document queue accepted")
            check(d.get("idleObserverAddStatus") == d.get("idleObserverRemoveStatus") == 0, "idle observer detached")
            check(d.get("documentObserverRemoved") is True and d.get("documentInvalidated") is False, "control document remained valid and observer detached")
            check(d.get("finalizerInvoked") is True and d.get("asynchronousCompleted") is True, "native finalizer actually completed")
            done = path.parent / "axis-labels-native.done"
            if check(done.is_file(), "native completion marker"):
                completion = json.loads(done.read_text(encoding="utf-8-sig"))
                check(Path(completion.get("run", "")).resolve() == path.parent.resolve() and completion.get("safeToUnload") is True, "completion belongs to this run and allows unload")
        elif d.get("commandApi") == "acedCommandC":
            # The local 2020 C reference has no documented return-value contract.
            # Preserve the real queue return; completion and actual entities are the oracle.
            check(isinstance(d.get("queueStatus"), int) and d.get("queueStatus") == d.get("commandStatus"), "actual native queue return recorded")
            check(d.get("callbackInvoked") is True and d.get("callbackCancelled") is False and d.get("asynchronousCompleted") is True, "native coroutine callback completed")
            done = path.parent / "axis-labels-native.done"
            if check(done.is_file(), "native completion marker"):
                completion = json.loads(done.read_text(encoding="utf-8-sig"))
                check(Path(completion.get("run", "")).resolve() == path.parent.resolve() and completion.get("safeToUnload") is True, "completion belongs to this run and allows unload")
        else:
            check(d.get("commandStatus") == 5100, "synchronous command return")
        if "reactorRegistration" in d:
            registration = d["reactorRegistration"]
            check(registration.get("editorAddReturned") is True and registration.get("editorRemoveReturned") is True, "editor trace detached")
            check(d.get("inputReactorAddStatus") == d.get("inputReactorRemoveStatus") == 0, "input trace detached")
            check(d.get("traceRetained") is False and d.get("traceDropped") is False, "complete native trace without retained callback")
        for k in ("saveStatus", "reopenStatus", "closeInputStatus"):
            check(d.get(k) == 0, k + " succeeded")
        for k in ("controlDWG", "outputDWG"):
            f = Path(d.get(k, ""))
            check(f.resolve().parent == path.resolve().parent and f.is_file(), k + ": isolated file")
        source, after, saved, mapping = (d.get(k) for k in ("sourceGeometry", "afterOriginalGeometry", "savedOriginalGeometry", "sourceMap"))
        original = {}
        if check(all(isinstance(v, list) and len(v) == 213 for v in (source, after, saved, mapping)), "all original records"):
            original = {e["handle"]: e for e in source}
            a, s = ({e["handle"]: e for e in seq} for seq in (after, saved))
            check(len(original) == len(a) == len(s) == 213, "unique original entities")
            for m in mapping:
                sh, th = m["sourceHandle"], m["targetHandle"]
                if check(sh in original and th in a and th in s, "mapped original"):
                    same(original[sh], a[th], sh + "/after")
                    same(original[sh], s[th], sh + "/saved")
        walls, axes = d.get("wallControls"), d.get("inputAxes")
        xs = []
        if check(isinstance(walls, list) and isinstance(axes, list) and len(walls) == len(axes) == n, "actual wall controls and input axes"):
            handles = {"CDB4", "CDB0"} if n == 2 else {"CDB4", "CDA3", "CDB0"}
            check({w.get("sourceHandle") for w in walls} == handles, "source wall fixtures")
            for w in walls:
                h = w.get("sourceHandle")
                curve = original.get(h, {}).get("curve") or {}
                check(near(w.get("start"), curve.get("start")) and near(w.get("end"), curve.get("end")), str(h) + ": real native wall")
                x = (curve["start"][0] + curve["end"][0])/2
                xs.append(x)
                matches = [a for a in axes if abs(a["curve"]["start"][0]-x) <= 1e-6 and abs(a["curve"]["end"][0]-x) <= 1e-6]
                check(len(matches) == 1, str(h) + ": one real axis at wall centerline")
            for a in axes:
                check(a.get("type") == "AcDbLine" and a.get("layer") == "DOTE" and a.get("proxy") is False, "documented real LINE axis")
                check(a["curve"].get("length", 0) > 1e-6, "nonzero native axis")
        extent = d.get("axisExtentControl", {})
        check(extent.get("wallCount") == 63 and abs(extent.get("minY", 0)-16541.2635519528) <= 1e-6 and abs(extent.get("maxY", 0)-34741.26355195283) <= 1e-6, "axis length from actual source wall range")
        added, saved_added = d.get("added"), d.get("savedAdded")
        if check(isinstance(added, list) and isinstance(saved_added, list) and len(added) == len(saved_added) == d.get("addedCount"), "all added entities recorded"):
            check(d.get("reopenedModelCount") == 213+len(added), "complete output model count")
            aa, ss = ({e["handle"]: e for e in seq} for seq in (added, saved_added))
            check(len(aa) == len(ss) == len(added) and set(aa) == set(ss), "added entity handle identity")
            for h, e in aa.items():
                check(h not in original and e.get("type") in {"AcDbLine", "TDbAxisLabelSet", "TDbDimension2"}, h + ": only axis input or native annotations added")
                if h in ss:
                    same(e, ss[h], h + "/persisted")
        labels, saved_labels = d.get("generatedLabelSets"), d.get("savedGeneratedLabelSets")
        if check(isinstance(labels, list) and isinstance(saved_labels, list) and len(labels) == len(saved_labels) >= 1, "new native axis-label collection created"):
            observed_xs = []
            for label in saved_labels:
                check(label.get("type") == "TDbAxisLabelSet" and label.get("proxy") is False, "native newly created label type")
                check(label.get("stretch", {}).get("status") == 0, "native label control points")
                for p in label.get("stretch", {}).get("points", []):
                    if not any(abs(p[0]-x) <= 1e-6 for x in observed_xs):
                        observed_xs.append(p[0])
            check(len(observed_xs) == n and all(any(abs(x-a) <= 1e-6 for a in observed_xs) for x in xs), "actual new label axis count and wall positions")
    for f in failures:
        print("FAIL:", f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: new labels from real 2/3 LINE axes with current Tianzheng settings; final settings/style/layout remain separate.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
