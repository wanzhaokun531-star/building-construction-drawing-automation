"""Check preservation and persistence of a documented renumber command trial.

Actual label semantics must also be checked in raw DXF or rendered output.
"""
import json
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

    def same(a, b, label, bounds=True):
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": type")
        check(a.get("layer") == b.get("layer"), label + ": layer")
        if bounds:
            check(near(a.get("min"), b.get("min")) and near(a.get("max"), b.get("max")), label + ": bounds")
        for kind in ("grip", "stretch"):
            aa, bb = a.get(kind, {}), b.get(kind, {})
            check(aa.get("status") == bb.get("status") and points_equal(aa.get("points"), bb.get("points")), label + ": " + kind)

    if check(path.is_file(), "renumber evidence missing"):
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        check(d.get("contract") == "stage1.axis-renumber-native.v1", "contract")
        check(d.get("api") == "ObjectARX native; no COM", "native object operation")
        check(d.get("commandName") == "TReAxisNo" and d.get("requestedNumber") == "21", "documented command and trial number")
        check(d.get("commandStatus") == 5100 and d.get("zoomStatus") == 5100, "command and zoom returned RTNORM")
        check(d.get("error") is None, "native execution error")
        check(d.get("initialModelCount") == d.get("reopenedModelCount") == 214 and d.get("sourceCount") == 213, "one axis collection, original scheme")
        for k in ("saveStatus", "reopenStatus", "closeInputStatus"):
            check(d.get(k) == 0, k + " succeeded")
        for k in ("controlDWG", "outputDWG"):
            f = Path(d.get(k, ""))
            check(f.resolve().parent == path.resolve().parent and f.is_file(), k + ": isolated file")
        source, after, saved, mapping = (d.get(k) for k in ("sourceGeometry", "afterOriginalGeometry", "savedOriginalGeometry", "sourceMap"))
        if check(all(isinstance(v, list) and len(v) == 213 for v in (source, after, saved, mapping)), "complete original preservation records"):
            original = {e["handle"]: e for e in source}
            aa, ss = ({e["handle"]: e for e in states} for states in (after, saved))
            check(len(original) == len(aa) == len(ss) == 213, "unique original entities")
            for m in mapping:
                sh, th = m["sourceHandle"], m["targetHandle"]
                if check(sh in original and th in aa and th in ss, "original mapped entity"):
                    same(original[sh], aa[th], sh + "/after")
                    same(original[sh], ss[th], sh + "/saved")
        states = [d.get(k) for k in ("before", "after", "saved")]
        if check(all(isinstance(v, dict) for v in states), "axis collection observations"):
            before, after, saved = states
            check(all(v.get("type") == "TDbAxisLabelSet" and v.get("handle") == "D0D3" for v in states), "native axis collection identity")
            same(before, after, "axis positions stay", bounds=False)
            same(after, saved, "persisted axis")
        pick = d.get("pick", {})
        check(pick.get("gripQueryStatus") == 0 and pick.get("transformStatus") == 5100, "fresh native grip and UCS conversion")
        for k in ("dxfBefore", "dxfAfter", "dxfSaved"):
            observation = d.get(k)
            if check(isinstance(observation, dict), k + ": public DXF observation"):
                check(isinstance(observation.get("available"), bool), k + ": availability explicit")
                check(isinstance(observation.get("stringGroups"), list), k + ": string groups")
        if all(isinstance(d.get(k), dict) for k in ("dxfAfter", "dxfSaved")):
            check(d["dxfAfter"].get("stringGroups") == d["dxfSaved"].get("stringGroups"), "public strings persist")
    for f in failures:
        print("FAIL:", f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: command trial and original-geometry preservation; label numbering still needs explicit DXF semantics or rendered verification.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
