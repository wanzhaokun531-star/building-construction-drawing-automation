"""Verify native annotation trials on the frozen scheme, with building preservation."""
import json
import math
import sys
from pathlib import Path


def point(p):
    return isinstance(p, list) and len(p) == 3 and all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in p)


def near(a, b):
    return point(a) and point(b) and all(abs(x-y) <= 1e-6 for x, y in zip(a, b))


def points_equal(a, b):
    return isinstance(a, list) and isinstance(b, list) and len(a) == len(b) and all(near(x, y) for x, y in zip(a, b))


def main(path):
    count = 0
    failures = []

    def check(ok, message):
        nonlocal count
        count += 1
        if not ok:
            failures.append(message)
        return ok

    def geometry_equal(a, b, label):
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": native type changed")
        check(a.get("layer") == b.get("layer"), label + ": original scheme layer changed")
        check(near(a.get("min"), b.get("min")) and near(a.get("max"), b.get("max")), label + ": extents changed")
        for kind in ("grip", "stretch"):
            aa, bb = a.get(kind, {}), b.get(kind, {})
            check(aa.get("status") == bb.get("status") and points_equal(aa.get("points"), bb.get("points")), label + ": " + kind + " geometry changed")

    if check(path.is_file(), "scheme-native result missing: " + str(path)):
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        check(data.get("contract") == "stage1.scheme-native.v1", "incorrect scheme contract")
        check(data.get("api") == "ObjectARX native; no COM", "native object operations required")
        check(data.get("schemeInputPath", "").endswith("方案平面图.dwg"), "scheme must be the building input")
        cases = data.get("cases")
        expected = {"scheme-existing-dimension": 213, "scheme-added-dimension": 214}
        if check(isinstance(cases, list) and len(cases) == 2, "two scheme trials required"):
            seen = []
            for c in cases:
                name = c.get("id")
                if not check(name in expected, "unexpected scheme trial"):
                    continue
                seen.append(name)
                for key in ("loadCopyStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
                    check(c.get(key) == 0, name + ": " + key + " must succeed")
                check(c.get("copyMethod") == "readDwgFile separate working database", name + ": work copy load method must be explicit")
                check(c.get("error") is None, name + ": trial reported an error")
                check(c.get("sourceCount") == 213 and c.get("mappedCount") == 213, name + ": original 213 scheme entities must be mapped to the independent work copy")
                check(c.get("reopenedModelCount") == expected[name], name + ": unexpected output model count")
                source, saved, mapping = c.get("sourceGeometry"), c.get("savedOriginalGeometry"), c.get("sourceMap")
                if not check(all(isinstance(x, list) and len(x) == 213 for x in (source, saved, mapping)), name + ": complete preservation evidence required"):
                    continue
                original = {e["handle"].upper(): e for e in source}
                result = {e["handle"].upper(): e for e in saved}
                check(len(original) == len(result) == 213, name + ": duplicate geometry records")
                check({m.get("sourceHandle", "").upper() for m in mapping} == set(original), name + ": incomplete source mapping")
                check({m.get("targetHandle", "").upper() for m in mapping} == set(result), name + ": incomplete target mapping")
                allowed = ["CDE6"] if name == "scheme-existing-dimension" else []
                check(c.get("allowedSourceHandles") == allowed, name + ": incorrect change allowance")
                for m in mapping:
                    sh, th = m["sourceHandle"].upper(), m["targetHandle"].upper()
                    if sh in original and th in result and sh not in allowed:
                        geometry_equal(original[sh], result[th], name + "/" + sh)
                before, after, reopened = c.get("before"), c.get("after"), c.get("saved")
                if not check(all(isinstance(x, dict) for x in (before, after, reopened)), name + ": selected dimension states missing"):
                    continue
                check(all(x.get("type") == "TDbDimension2" and x.get("proxy") is False for x in (before, after, reopened)), name + ": preserve Tianzheng dimension type")
                geometry_equal(after, reopened, name + ": persisted dimension")
                if name == "scheme-existing-dimension":
                    check(c.get("sourceHandle") == "CDE6" and c.get("moveStatus") == 0, name + ": wrong existing-dimension edit")
                    first = before["grip"]["points"][0]
                    destination = [first[0], first[1]-200, first[2]]
                    check(any(near(p, destination) for p in reopened["grip"]["points"]), name + ": dimension-line shift must persist")
                    check(abs(before["curve"]["length"] - reopened["curve"]["length"]) <= 1e-6, name + ": shift must preserve dimension curve length")
                else:
                    check(c.get("templateCloneStatus") == 0 and c.get("addedObjectCount") == 1 and c.get("templateHandle") == "19A0", name + ": only one reference dimension template may be added")
                    wall = c.get("wallControl", {})
                    check(wall.get("sourceHandle") == "CD83" and point(wall.get("start")) and point(wall.get("end")), name + ": real scheme wall endpoints required")
                    targets = wall.get("sortedPoints")
                    if check(isinstance(targets, list) and len(targets) == 2 and all(point(p) for p in targets), name + ": wall measurement targets missing"):
                        wall_length = math.dist(targets[0], targets[1])
                        check(abs(wall_length-3600) <= 1e-6, name + ": known scheme wall length must be 3600")
                        check(all(any(near(p, target) for p in reopened["grip"]["points"]) for target in targets), name + ": actual wall endpoints must persist as native dimension grips")
                        check(abs(reopened["curve"]["length"]-wall_length) <= 1e-6, name + ": new dimension must use the actual scheme wall length")
                    steps = c.get("steps")
                    if check(isinstance(steps, list) and len(steps) == 3, name + ": three native point edits required"):
                        check([s.get("gripIndex") for s in steps] == [2, 3, 0], name + ": wrong native editing sequence")
                        check(all(s.get("queryStatus") == 0 and s.get("moveStatus") == 0 and s.get("transactionStatus") == 0 for s in steps), name + ": every native point edit must succeed")
                output = c.get("outputDWG")
                if check(isinstance(output, str) and bool(output), name + ": output file missing"):
                    file = Path(output)
                    check(file.resolve().parent == path.resolve().parent and file.is_file() and file.stat().st_size > 0, name + ": isolated nonempty output required")
            check(sorted(seen) == sorted(expected), "scheme trial set incomplete")
    for f in failures:
        print("FAIL:", f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("This verifies scheme-input trials; it is not full construction drawing generation.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_scheme_native.py <scheme-native.json>")
    raise SystemExit(main(Path(sys.argv[1])))
