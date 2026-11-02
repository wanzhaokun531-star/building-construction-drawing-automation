"""Verify actual TConbineDim measurement deletion, persistence and scheme protection."""
import json
import math
import sys
from pathlib import Path
from verify_scheme_native import points_equal


def near(a, b):
    if isinstance(a, (int, float)) and isinstance(b, (int, float)) and not isinstance(a, bool) and not isinstance(b, bool):
        return math.isfinite(a) and math.isfinite(b) and abs(a-b) <= 1e-6
    return isinstance(a, list) and isinstance(b, list) and len(a) == len(b) and all(near(x,y) for x,y in zip(a,b))


def main(path):
    assertions, failures = 0, []

    def check(ok, label):
        nonlocal assertions
        assertions += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def same(a, b, label):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": records"):
            return
        for key in ("type", "layer", "proxy", "extentStatus"):
            check(a.get(key) == b.get(key), label + ": " + key)
        for key in ("min", "max"):
            check(near(a.get(key), b.get(key)), label + ": " + key)
        for key in ("grip", "stretch"):
            aa, bb = a.get(key, {}), b.get(key, {})
            check(aa.get("status") == bb.get("status") and points_equal(aa.get("points"), bb.get("points")), label + ": " + key)
        ca, cb = a.get("curve"), b.get("curve")
        if ca is None:
            check(cb is None, label + ": noncurve preserved")
        elif check(isinstance(cb, dict), label + ": curve preserved"):
            for key in ("start", "end", "length"):
                check(near(ca.get(key), cb.get(key)), label + ": curve " + key)

    if not check(path.is_file(), "native dimension-combine host evidence missing"):
        print("FAIL:", failures[0])
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    run = path.resolve().parent
    check(d.get("contract") == "stage1.dimension-bridge-native.v1", "contract")
    check(d.get("api") == "ObjectARX native; no COM" and d.get("commandApi") == "acedCommandS", "native object and command API")
    check(d.get("commandName") == "TConbineDim", "documented command spelling")
    check(d.get("operation") == "combine-first-interior-measurement" and d.get("dimensionHandle") == "D0D3", "fixed operation and target")
    check(d.get("commandStatus") == 5100, "command returned RTNORM; geometry checked separately")
    check(d.get("actualReached") is True, "native first-interior measurement was actually removed")
    check(d.get("error") is None, "no native operation error")
    check(d.get("initialModelCount") == d.get("finalModelCount") == d.get("reopenedModelCount") == 214, "same one added dimension in full scheme")
    check(d.get("sourceCount") == 213, "213 protected original objects")
    for key in ("saveStatus", "reopenStatus", "closeInputStatus"):
        check(d.get(key) == 0, key)
    for key, name in (("controlDWG", "control.dwg"), ("outputDWG", "dimension-combine-generated.dwg")):
        f = Path(d.get(key, ""))
        check(f.is_file() and f.resolve().parent == run and f.name == name, key + ": isolated file")
    originals = [d.get(k, []) for k in ("sourceGeometry", "afterOriginalGeometry", "savedOriginalGeometry")]
    mapping = d.get("sourceMap", [])
    original = {}
    if check(all(len(seq) == 213 for seq in originals) and len(mapping) == 213, "all original entities and mappings"):
        original, changed, persisted = [{x.get("handle"): x for x in seq} for seq in originals]
        check(len(original) == 213 and set(original) == set(changed) == set(persisted) and "D0D3" not in original, "protected identity coverage")
        check({m.get("sourceHandle") for m in mapping} == set(original), "mapping covers original handles")
        for m in mapping:
            h = m.get("sourceHandle")
            check(m.get("targetHandle") == h and m.get("type") == original.get(h, {}).get("type"), "identity mapping " + str(h))
        for h in original.keys() & changed.keys() & persisted.keys():
            same(original[h], changed[h], "original-after/" + h)
            same(original[h], persisted[h], "original-saved/" + h)
    wall = d.get("wallControl", {})
    check(wall.get("sourceHandle") == "CD83" and wall.get("type") == "TDbWall", "native wall control identity")
    wall_start, wall_end = wall.get("start", []), wall.get("end", [])
    if not check(len(wall_start) == len(wall_end) == 3, "native wall endpoints available"):
        wall_start, wall_end = [0, 0, 0], [3600, 0, 0]
    baseline, drawing_z = wall_start[1], wall_start[2]
    check(near(wall_start[1:], wall_end[1:]) and near(abs(wall_start[0]-wall_end[0]), 3600), "horizontal 3600 wall")
    opening = d.get("openingControl", {})
    check(opening.get("sourceHandle") == "CD84" and opening.get("type") == "TDbOpening" and opening.get("gripQueryStatus") == 0, "actual native opening side grips")
    sides = opening.get("sidePoints", [])
    if not check(len(sides) == 2 and all(len(p) == 3 for p in sides), "two actual side controls"):
        sides = [[600, baseline, 900], [3000, baseline, 900]]
    left_x, right_x = sides[0][0], sides[1][0]
    check(near(right_x-left_x, 2400), "real opening width")
    if "CD84" in original:
        actual = original["CD84"].get("grip", {}).get("points", [])
        check(all(any(near(p, q) for q in actual) for p in sides), "controls occur in original opening grips")
    wall_x = sorted([wall_start[0], wall_end[0]])
    expected_before = [[x, baseline, drawing_z] for x in (wall_x[0], left_x, right_x, wall_x[1])]
    expected_after = [expected_before[i] for i in (0, 2, 3)]
    before, after, saved = [d.get(k, {}) for k in ("before", "after", "saved")]
    for key, entity, want in (("measurementBefore", before, expected_before), ("measurementAfter", after, expected_after), ("measurementSaved", saved, expected_after)):
        check(entity.get("type") == "TDbDimension2" and entity.get("handle") == "D0D3" and entity.get("proxy") is False, key + ": same native dimension")
        check(entity.get("grip", {}).get("status") == 0, key + ": actual grip query")
        actual_points = [p for p in entity.get("grip", {}).get("points", []) if len(p) == 3 and near(p[1], baseline) and near(p[2], drawing_z)]
        unique = []
        for p in sorted(actual_points, key=lambda p: p[0]):
            if not unique or not near(unique[-1], p):
                unique.append(p)
        check(points_equal(unique, want), key + ": actual measurement deletion, no expected-only inference")
        measurement = d.get(key, {})
        check(measurement.get("queryStatus") == 0 and points_equal(measurement.get("points"), unique), key + ": actual measurement evidence")
        actual_segments = [b[0]-a[0] for a, b in zip(unique, unique[1:])]
        check(near(measurement.get("segments"), actual_segments), key + ": segments calculated from actual native points")
        check(near(actual_segments, [600, 2400, 600] if key == "measurementBefore" else [3000, 600]), key + ": required segmentation")
        check(near(entity.get("curve", {}).get("length"), 3600), key + ": total remains 3600")
    check(before.get("layer") == after.get("layer") == saved.get("layer"), "target layer preserved")
    same(after, saved, "target persisted")
    for key in ("start", "end"):
        check(near(before.get("curve", {}).get(key), after.get("curve", {}).get(key)), "dimension line " + key + " retained")
    selection = d.get("selectionWindow", {})
    line_y = before.get("curve", {}).get("start", [0, 0, 0])[1]
    check(points_equal(selection.get("worldPoints"), [[left_x-20, line_y-100, drawing_z], [left_x+20, line_y+100, drawing_z]]), "only first interior dimension-line arrow selected")
    check(selection.get("transformStatuses") == [5100, 5100] and len(selection.get("userPoints", [])) == 2, "both WCS points converted to UCS")
    trace = d.get("trace", {})
    check(trace.get("inputAddStatus") == trace.get("inputRemoveStatus") == 0 and trace.get("editorRemoved") is True, "native command/input observers added and removed")
    check(trace.get("traceDropped") is False and trace.get("retained") is False, "complete trace and no retained callbacks")
    commands = d.get("commandTrace", [])
    check(isinstance(d.get("inputTrace"), list), "real input trace recorded, including an empty observation")
    for event in ("commandWillStart", "commandEnded"):
        check(any(x.get("event") == event and (x.get("command") or "").upper() == "TCONBINEDIM" for x in commands), event + ": actual command observed")
    check(not any(x.get("event") in ("commandCancelled", "commandFailed", "unknownCommand") for x in commands), "no command failure/cancel observation")
    for failure in failures[:100]:
        print("FAIL:", failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: actual native measurement removal and persistence. Rendered labels 3000/600 require independent native PDF inspection.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_dimension_bridge_native.py <run/dimension-bridge-native.json>")
    try:
        result = main(Path(sys.argv[1]))
    except (OSError, ValueError, TypeError, AttributeError, KeyError, IndexError) as failure:
        print("FAIL: unreadable or incomplete native dimension-combine evidence:", failure)
        result = 1
    raise SystemExit(result)
