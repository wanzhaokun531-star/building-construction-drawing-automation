"""Verify native dimension subdivision using actual scheme opening endpoints."""
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
        for kind in ("grip", "stretch"):
            aa, bb = a.get(kind, {}), b.get(kind, {})
            check(aa.get("status") == bb.get("status") and points_equal(aa.get("points"), bb.get("points")), label + ": " + kind)

    if check(path.is_file(), "dimension-point evidence missing"):
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        check(d.get("contract") == "stage1.dim-points-native.v1", "contract")
        check(d.get("api") == "ObjectARX native; no COM" and d.get("commandName") == "TBreakDim", "documented native command")
        check(d.get("commandStatus") == d.get("zoomStatus") == 5100, "commands returned RTNORM")
        check(d.get("error") is None, "no native error")
        check(d.get("initialModelCount") == d.get("reopenedModelCount") == 214 and d.get("sourceCount") == 213, "only same one template dimension")
        for k in ("saveStatus", "reopenStatus", "closeInputStatus"):
            check(d.get(k) == 0, k + " succeeded")
        for k in ("controlDWG", "outputDWG"):
            f = Path(d.get(k, ""))
            check(f.resolve().parent == path.resolve().parent and f.is_file(), k + ": isolated output")
        source, after, saved, mapping = (d.get(k) for k in ("sourceGeometry", "afterOriginalGeometry", "savedOriginalGeometry", "sourceMap"))
        original = {}
        if check(all(isinstance(v, list) and len(v) == 213 for v in (source, after, saved, mapping)), "complete original records"):
            original = {e["handle"]: e for e in source}
            a, s = ({e["handle"]: e for e in entries} for entries in (after, saved))
            check(len(original) == len(a) == len(s) == 213, "unique original objects")
            for m in mapping:
                sh, th = m["sourceHandle"], m["targetHandle"]
                if check(sh in original and th in a and th in s, "mapped original exists"):
                    same(original[sh], a[th], sh + "/after")
                    same(original[sh], s[th], sh + "/saved")
        wall, opening = d.get("wallControl", {}), d.get("openingControl", {})
        check(wall.get("sourceHandle") == "CD83" and opening.get("sourceHandle") == "CD84", "actual scheme wall and window")
        curve = (original.get("CD83", {}).get("curve") or {})
        check(near(wall.get("start"), curve.get("start")) and near(wall.get("end"), curve.get("end")), "actual wall endpoint basis")
        raw, projected = opening.get("rawPoints"), opening.get("measurementPoints")
        if check(isinstance(raw, list) and len(raw) == 2 and isinstance(projected, list) and len(projected) == 2, "two actual opening side points"):
            original_grips = original.get("CD84", {}).get("grip", {}).get("points", [])
            check(all(any(near(p, g) for g in original_grips) for p in raw), "raw points from real native opening grips")
            check(all(point(p) for p in projected) and all(abs(p[2]) <= 1e-6 for p in projected), "explicit projection to drawing plane")
            check(all(abs(p[0]-q[0]) <= 1e-6 and abs(p[1]-q[1]) <= 1e-6 for p, q in zip(raw, projected)), "projection preserves plan coordinates")
            check(abs(math.dist(*projected)-2400) <= 1e-6, "actual window width 2400")
        states = [d.get(k) for k in ("before", "after", "saved")]
        if check(all(isinstance(v, dict) for v in states), "dimension states"):
            before, after, saved = states
            check(all(v.get("type") == "TDbDimension2" and v.get("handle") == "D0D3" and v.get("proxy") is False for v in states), "same native dimension entity")
            same(after, saved, "persisted dimension")
            check(abs(before["curve"]["length"]-3600) <= 1e-6 and abs(saved["curve"]["length"]-3600) <= 1e-6, "total measured length 3600")
            check(len(saved["grip"]["points"]) > len(before["grip"]["points"]), "native dimension point count actually increased")
            if isinstance(projected, list):
                check(all(any(near(p, q) for q in saved["grip"]["points"]) for p in projected), "both opening measurements persist")
        segments = d.get("expectedSegments")
        check(isinstance(segments, list) and len(segments) == 3 and all(abs(a-b) <= 1e-6 for a, b in zip(segments, [600, 2400, 600])), "independent expected split")
    for f in failures:
        print("FAIL:", f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: dimension measurement-point structure and original preservation; rendered labels checked separately.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
