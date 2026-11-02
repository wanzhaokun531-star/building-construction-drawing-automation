"""Native construction primitives and annotation mechanics, not Tianzheng replacement."""
import json
import math
import sys
from pathlib import Path
from verify_scheme_native import near, points_equal


def main(path):
    count, failed = 0, []
    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failed.append(label)
        return bool(ok)
    if not check(path.is_file(), "native geometry host evidence missing"):
        print("FAIL:", failed[0]);print("assertions=1 passed=0 failed=1 result=FAIL");return 1
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    check(d.get("contract") == "stage1.standard-geometry-native.v1", "contract")
    check(d.get("api") == "ObjectARX native; no COM" and d.get("error") is None, "native API and no operation error")
    expected_source = path.resolve().parents[3] / "阶段0" / "输入快照" / "方案平面图.dwg"
    check(Path(d.get("schemeInputPath", "")).resolve() == expected_source, "actual frozen scheme input")
    check(d.get("initialSchemeCount") == 213 and d.get("generatedModelCount") == d.get("reopenedModelCount") == 220, "exact full scheme and seven added model objects")
    for k in ("saveStatus", "reopenStatus", "closeInputStatus"):
        check(d.get(k) == 0, k)
    output = Path(d.get("outputDWG", ""))
    check(output.is_file() and output.resolve().parent == path.resolve().parent, "isolated DWG exists")
    for state in ("after", "saved"):
        s = d.get(state, {})
        r, outer, hatch = (s.get(k, {}) for k in ("rectangle", "outer", "hatch"))
        check(r.get("closed") is True and r.get("vertices") == 4 and abs(r.get("area", 0)-24000000) <= 1e-4, state+": explicit synthetic rectangle")
        check(outer.get("closed") is True and outer.get("vertices") == 4 and abs(outer.get("area", 0)-42560000) <= 1e-4, state+": outward offset geometry")
        for name, record, expected_points in (
            ("rectangle", r, [[50000,20000,0],[56000,20000,0],[56000,24000,0],[50000,24000,0]]),
            ("outer", outer, [[49200,19200,0],[56800,19200,0],[56800,24800,0],[49200,24800,0]]),
        ):
            points = record.get("points", [])
            check(len(points) == 4 and all(sum(near(p,q) for q in points) == 1 for p in expected_points), state+": exact "+name+" vertices")
        check(hatch.get("associative") is True and hatch.get("loops") == 2, state+": associative hatch with inner hole")
        check(abs(hatch.get("area",0)-18560000) <= 1e-4, state+": actual hatch area excludes inner hole")
        check(set(hatch.get("boundaries", [])) == {r.get("handle"), outer.get("handle")}, state+": actual persisted hatch boundary references")
        text = s.get("text", {})
        check(text.get("value") == "C09235" and abs(text.get("height", 0)-350) <= 1e-6 and abs(text.get("rotation", 0)-math.pi/2) <= 1e-12, state+": native rotated text and non-round code")
        check(near(text.get("position"),[59000,20000,0]), state+": text placed at explicit fixture point")
        mt = s.get("mtext", {})
        check("待确认" in mt.get("value", "") and "S1_SLOPE" in mt.get("value", ""), state+": unicode multi-line pending text")
        check(near(mt.get("position"),[61000,23000,0]) and near(s.get("block",{}).get("position"),[50600,28900,0]), state+": actual MText and moved INSERT positions")
        leader = s.get("leader", {})
        check(leader.get("annotation") == mt.get("handle") and leader.get("vertices") == 3, state+": leader references real annotation")
        attrs = s.get("attributes", [])
        check({a.get("tag"): a.get("value") for a in attrs} == {"SHEET_NO": "S1-A001", "ISSUE_CODE": "待确认"}, state+": editable frame attribute values")
        check(len(attrs) == 2 and all(a.get("owner") == s.get("block", {}).get("handle") for a in attrs), state+": attributes owned by actual INSERT")
        issue = s.get("issue", {})
        check(issue.get("id") == "S1_SLOPE" and issue.get("marker") == mt.get("handle") and issue.get("nativeXData") == "S1_SLOPE", state+": pending list to diagram link")
    before_attrs = {x.get("tag"):x for x in d.get("attributesBeforeMove", [])}
    after_attrs = {x.get("tag"):x for x in d.get("after", {}).get("attributes", [])}
    check(len(before_attrs) == len(after_attrs) == 2, "attributes recorded around block move")
    for tag, a in before_attrs.items():
        b = after_attrs.get(tag, {})
        expected = [v+delta for v,delta in zip(a.get("position", []), [600,900,0])]
        check(near(expected,b.get("position")), tag+": attributes move exactly once with block")
    check(d.get("after") == d.get("saved"), "native generated content preserved on new database reopen")
    baseline, after, saved = [d.get(k, []) for k in ("originalBefore", "originalAfter", "originalSaved")]
    if check(len(baseline) == len(after) == len(saved) == 213, "all original scheme objects recorded"):
        maps = [{e["handle"]:e for e in seq} for seq in (baseline, after, saved)]
        check(len(maps[0]) == 213 and set(maps[0]) == set(maps[1]) == set(maps[2]), "original handles preserved")
        for h,a in maps[0].items():
            for state, values in (("after",maps[1]),("saved",maps[2])):
                b = values.get(h,{})
                check(a.get("type") == b.get("type") and b.get("proxy") is False and a.get("layer") == b.get("layer"),h+"/"+state+": original type/layer")
                check(near(a.get("min"),b.get("min")) and near(a.get("max"),b.get("max")),h+"/"+state+": original geometry")
                for key in ("grip","stretch"):
                    check(a.get(key,{}).get("status") == b.get(key,{}).get("status") and points_equal(a.get(key,{}).get("points"),b.get(key,{}).get("points")),h+"/"+state+": original "+key)
                check(a.get("curve") == b.get("curve"),h+"/"+state+": original native curve")
    for f in failed[:80]:print("FAIL:",f)
    print(f"assertions={count} passed={count-len(failed)} failed={len(failed)} result={'FAIL' if failed else 'PASS'}")
    print("Scope: synthetic native geometry and persisted forward references; dynamic boundary/annotation following is not established. No Tianzheng replacement or final project dimensions/styles are claimed.")
    return int(bool(failed))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
