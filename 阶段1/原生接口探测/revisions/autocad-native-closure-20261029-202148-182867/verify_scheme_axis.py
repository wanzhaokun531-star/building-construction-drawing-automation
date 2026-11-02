"""Check native axis-template trials in a complete scheme work copy.

This checks template transfer/editing, not automatic axis design.
"""
import json
import math
import sys
from pathlib import Path

from verify_scheme_native import near, point, points_equal


def main(path):
    count, failures = 0, []

    def check(ok, message):
        nonlocal count
        count += 1
        if not ok:
            failures.append(message)
        return bool(ok)

    def geometry_equal(a, b, label):
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": native type")
        check(a.get("layer") == b.get("layer"), label + ": layer")
        check(near(a.get("min"), b.get("min")) and near(a.get("max"), b.get("max")), label + ": bounds")
        for kind in ("grip", "stretch"):
            aa, bb = a.get(kind, {}), b.get(kind, {})
            check(aa.get("status") == bb.get("status") and points_equal(aa.get("points"), bb.get("points")), label + ": " + kind)

    def translated(p, delta):
        return [x + d for x, d in zip(p, delta)]

    if check(path.is_file(), "scheme-axis result missing: " + str(path)):
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        check(data.get("contract") == "stage1.scheme-axis.v1", "axis contract")
        check(data.get("api") == "ObjectARX native; no COM", "native object operations")
        check(data.get("schemeInputPath", "").endswith("方案平面图.dwg"), "scheme is building input")
        cases = data.get("cases")
        expected = {"scheme-axis-transfer", "scheme-axis-adjust"}
        if check(isinstance(cases, list) and len(cases) == 2, "two independent scheme-axis trials"):
            seen = []
            for c in cases:
                name = c.get("id")
                if not check(name in expected, "unknown axis trial"):
                    continue
                seen.append(name)
                for key in ("loadCopyStatus", "copyInputCloseStatus", "templateCloneStatus", "transformStatus", "transformTransactionStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
                    check(c.get(key) == 0, name + ": " + key)
                check(c.get("error") is None, name + ": native error")
                check(c.get("copyMethod") == "readDwgFile separate working database", name + ": independent work copy")
                check(c.get("sourceCount") == 213 and c.get("mappedCount") == 213, name + ": all originals mapped")
                check(c.get("templateHandle") == "199F", name + ": frozen axis template")
                check(c.get("addedObjectCount") == 1 and c.get("reopenedModelCount") == 214, name + ": one collection added")
                check(c.get("allowedSourceHandles") == [], name + ": no original edits allowed")
                source, saved, mapping = (c.get(k) for k in ("sourceGeometry", "savedOriginalGeometry", "sourceMap"))
                if check(all(isinstance(v, list) and len(v) == 213 for v in (source, saved, mapping)), name + ": complete preservation records"):
                    original = {e["handle"].upper(): e for e in source}
                    result = {e["handle"].upper(): e for e in saved}
                    check(len(original) == len(result) == 213, name + ": unique records")
                    check({m.get("sourceHandle", "").upper() for m in mapping} == set(original), name + ": source mapping coverage")
                    check({m.get("targetHandle", "").upper() for m in mapping} == set(result), name + ": target mapping coverage")
                    for m in mapping:
                        sh, th = m["sourceHandle"].upper(), m["targetHandle"].upper()
                        if sh in original and th in result:
                            geometry_equal(original[sh], result[th], name + "/" + sh)
                else:
                    original = {}
                states = [c.get(k) for k in ("templateBefore", "before", "transformed", "after", "saved")]
                if not check(all(isinstance(v, dict) for v in states), name + ": axis observations"):
                    continue
                template, before, shifted, after, persisted = states
                check(all(s.get("type") == "TDbAxisLabelSet" and s.get("proxy") is False for s in states), name + ": native axis collection")
                geometry_equal(after, persisted, name + ": persisted axis")
                check(c.get("targetHandle") == persisted.get("handle"), name + ": persisted target handle")
                delta = c.get("translation")
                if check(point(delta), name + ": finite translation"):
                    for kind in ("grip", "stretch"):
                        src = before.get(kind, {}).get("points", [])
                        dst = shifted.get(kind, {}).get("points", [])
                        check(points_equal(src, template.get(kind, {}).get("points")), name + ": clone " + kind)
                        check(points_equal([translated(p, delta) for p in src], dst), name + ": translation preserves all " + kind)
                    check(near(translated(before["min"], delta), shifted["min"]) and near(translated(before["max"], delta), shifted["max"]), name + ": translation bounds")
                if name == "scheme-axis-transfer":
                    geometry_equal(shifted, after, name + ": transfer only")
                    check(c.get("steps") == [], name + ": transfer has no local edits")
                else:
                    walls = c.get("wallControls")
                    if check(isinstance(walls, list) and len(walls) == 2, name + ": two scheme wall controls"):
                        xs = []
                        for w in walls:
                            h = w.get("sourceHandle", "").upper()
                            wall = original.get(h, {})
                            curve = wall.get("curve") or {}
                            check(h in {"CDB4", "CDB0"} and wall.get("type") == "TDbWall", name + ": actual wall source")
                            check(near(w.get("start"), curve.get("start")) and near(w.get("end"), curve.get("end")), name + ": actual wall endpoints")
                            if check(point(w.get("start")) and point(w.get("end")), name + ": valid wall control"):
                                check(abs(w["start"][0] - w["end"][0]) <= 1e-6, name + ": vertical wall")
                                xs.append(w["start"][0])
                        if check(len(xs) == 2, name + ": two wall centerlines"):
                            left, right = sorted(xs)
                            check(abs(right-left-17500) <= 1e-6, name + ": scheme wall spacing 17500")
                            shifted_stretch = shifted.get("stretch", {}).get("points", [])
                            check(bool(shifted_stretch) and abs(min(p[0] for p in shifted_stretch)-left) <= 1e-6, name + ": leftmost template axis aligned with left wall")
                            for kind in ("grip", "stretch"):
                                pts = persisted.get(kind, {}).get("points", [])
                                shifted_pts = shifted.get(kind, {}).get("points", [])
                                original_right_x = max(p[0] for p in shifted.get("stretch", {}).get("points", []))
                                expected_pts = [[right if abs(p[0]-original_right_x) <= 1e-6 else p[0], p[1], p[2]] for p in shifted_pts]
                                check(points_equal(expected_pts, pts), name + ": only chosen right axis X changes/" + kind)
                                for x in (left, right):
                                    aligned = [p for p in pts if abs(p[0]-x) <= 1e-6]
                                    check(len(aligned) >= 4 and max(p[1] for p in aligned)-min(p[1] for p in aligned) > 10000, name + ": top and bottom controls on wall x/" + kind)
                    steps = c.get("steps")
                    if check(isinstance(steps, list) and len(steps) > 0, name + ": native local edits"):
                        for s in steps:
                            check(all(s.get(k) == 0 for k in ("queryStatus", "moveStatus", "transactionStatus")), name + ": successful edit status")
                            check(point(s.get("currentPoint")) and point(s.get("targetPoint")), name + ": edit coordinates")
                            observed = s.get("after", {}).get(s.get("mode", "grip"), {}).get("points", [])
                            check(any(near(s.get("targetPoint"), p) for p in observed), name + ": actual target reached after edit")
                    # Radial circle/control distance is 400 in the frozen template;
                    # compare each top/bottom visible circle before and after editing.
                    source_points = shifted.get("grip", {}).get("points", [])
                    output_points = persisted.get("grip", {}).get("points", [])
                    check(len(source_points) == len(output_points) == 41, name + ": visible grip count")
                    for center, edge in [(1, 2), (4, 5), (7, 8), (10, 11), (13, 14), (16, 17), (19, 20), (23, 24), (26, 27), (29, 30), (32, 33), (35, 36), (38, 39)]:
                        if center < len(output_points) and edge < len(output_points):
                            a = math.dist(source_points[center], source_points[edge])
                            b = math.dist(output_points[center], output_points[edge])
                            check(abs(a-400) <= 1e-6 and abs(b-a) <= 1e-6, name + ": circle/control radius preserved")
                output = c.get("outputDWG")
                if check(isinstance(output, str) and bool(output), name + ": output path"):
                    f = Path(output)
                    check(f.resolve().parent == path.resolve().parent and f.is_file() and f.stat().st_size > 0, name + ": isolated saved output")
            check(sorted(seen) == sorted(expected), "complete axis case set")
    for failure in failures:
        print("FAIL:", failure)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: native template transfer and controlled editing in scheme input; not complete axis generation.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_scheme_axis.py <scheme-axis.json>")
    raise SystemExit(main(Path(sys.argv[1])))
