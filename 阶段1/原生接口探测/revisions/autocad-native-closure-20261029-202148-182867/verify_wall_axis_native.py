"""Validate documented Tianzheng wall-to-axis command on a full scheme copy."""
import json
import math
import sys
from pathlib import Path
from verify_scheme_native import near, point, points_equal


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
        for k in ("grip", "stretch"):
            check(a.get(k, {}).get("status") == b.get(k, {}).get("status") and points_equal(a.get(k, {}).get("points"), b.get(k, {}).get("points")), label + ": " + k)

    if check(path.is_file(), "wall-axis evidence missing"):
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        check(d.get("contract") == "stage1.wall-axis-native.v1", "contract")
        check(d.get("api") == "ObjectARX native; no COM", "native object access")
        check(d.get("commandName") == "TWall2Axis", "documented command")
        check(d.get("commandStatus") == 5100, "native command returned RTNORM")
        check(d.get("error") is None, "no native error")
        check(d.get("initialModelCount") == d.get("sourceCount") == 213, "complete initial scheme")
        for k in ("saveStatus", "reopenStatus", "closeInputStatus"):
            check(d.get(k) == 0, k + " succeeded")
        control = Path(d.get("controlDWG", ""))
        output = Path(d.get("outputDWG", ""))
        check(control.resolve().parent == path.resolve().parent and control.name == "control.dwg", "isolated active control drawing")
        check(output.resolve().parent == path.resolve().parent and output.is_file() and output.stat().st_size > 0, "isolated output drawing")
        source = d.get("sourceGeometry")
        after = d.get("afterOriginalGeometry")
        saved = d.get("savedOriginalGeometry")
        mapping = d.get("sourceMap")
        if check(all(isinstance(v, list) and len(v) == 213 for v in (source, after, saved, mapping)), "all original preservation records"):
            src = {e["handle"].upper(): e for e in source}
            after_map = {e["handle"].upper(): e for e in after}
            saved_map = {e["handle"].upper(): e for e in saved}
            check(len(src) == len(after_map) == len(saved_map) == 213, "unique original records")
            check({m.get("sourceHandle", "").upper() for m in mapping} == set(src), "source map coverage")
            for m in mapping:
                sh, th = m["sourceHandle"].upper(), m["targetHandle"].upper()
                if check(sh in src and th in after_map and th in saved_map, "mapped original exists"):
                    same(src[sh], after_map[th], sh + "/after command")
                    same(src[sh], saved_map[th], sh + "/reopened")
        else:
            src = {}
        controls = d.get("wallControls")
        if check(isinstance(controls, list) and len(controls) == 3, "three selected walls"):
            check({w.get("sourceHandle") for w in controls} == {"CDB4", "CDB0", "CD83"}, "wall fixtures")
            for w in controls:
                h = w.get("sourceHandle", "").upper()
                original = src.get(h, {})
                curve = original.get("curve") or {}
                check(w.get("type") == original.get("type") == "TDbWall", h + ": native wall")
                check(near(w.get("start"), curve.get("start")) and near(w.get("end"), curve.get("end")), h + ": actual baseline")
                if check(point(w.get("start")) and point(w.get("end")), h + ": finite control endpoints"):
                    check(abs(math.dist(w["start"], w["end"])-w.get("lineControlLength", 0)) <= 1e-6, h + ": control length")
        added, persisted = d.get("added"), d.get("savedAdded")
        check(d.get("addedCount") == 3 and d.get("reopenedModelCount") == 216, "exactly three real axis entities added")
        if check(all(isinstance(v, list) and len(v) == 3 for v in (added, persisted)), "created and reopened axis records"):
            new_map = {e["handle"]: e for e in added}
            saved_new_map = {e["handle"]: e for e in persisted}
            check(len(new_map) == len(saved_new_map) == 3 and set(new_map) == set(saved_new_map), "stable new axis handles")
            for h, e in new_map.items():
                check(h.upper() not in src, "new axis is new entity")
                check(e.get("type") == "AcDbLine" and e.get("proxy") is False, h + ": real native axis LINE")
                check(e.get("layer") == "DOTE", h + ": Tianzheng axis layer")
                curve = e.get("curve") or {}
                check(all(curve.get(k) == 0 for k in ("startStatus", "endStatus", "distanceStatus")) and point(curve.get("start")) and point(curve.get("end")) and curve.get("length", 0) > 1e-6, h + ": finite nonzero axis")
                if h in saved_new_map:
                    same(e, saved_new_map[h], h + "/persisted axis")
            if isinstance(controls, list):
                matched_handles = []
                for w in controls:
                    matches = []
                    if not (point(w.get("start")) and point(w.get("end"))):
                        check(False, "finite wall baseline needed for match")
                        continue
                    start, end = w["start"], w["end"]
                    length = math.dist(start, end)
                    if not check(length > 1e-6, "nonzero wall baseline"):
                        continue
                    unit = [(b-a)/length for a, b in zip(start, end)]
                    for e in persisted:
                        curve = e.get("curve") or {}
                        ends = [curve.get("start"), curve.get("end")]
                        if not all(point(p) for p in ends):
                            continue
                        projections = [sum((p[i]-start[i])*unit[i] for i in range(3)) for p in ends]
                        residuals = [math.dist(p, [start[i]+t*unit[i] for i in range(3)]) for p, t in zip(ends, projections)]
                        if max(residuals) <= 1e-6 and min(projections) <= 1e-6 and max(projections) >= length-1e-6:
                            matches.append(e["handle"])
                    check(len(matches) == 1, str(w.get("sourceHandle")) + ": exactly one axis is collinear and covers wall baseline")
                    matched_handles.extend(matches)
                check(len(matched_handles) == len(set(matched_handles)) == 3, "three different axes match three wall controls")
    for label in failures:
        print("FAIL:", label)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: three actual wall-baseline axes through documented command, not full axis design or annotation.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
