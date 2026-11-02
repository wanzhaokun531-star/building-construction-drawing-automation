"""Verify native template capability in full scheme copies; never infer business creation."""
import json
import math
import sys
from pathlib import Path
from verify_scheme_native import near, points_equal


FIXTURES = {
    "V01": ("scheme", "CD83", "TDbWall"),
    "V02": ("scheme", "CD84", "TDbOpening"),
    "V03": ("scheme", "CD89", "TDbOpening"),
    "V04": ("scheme", "CDC0", "TDbText"),
    "V05": ("reference", "19A0", "TDbDimension2"),
    "V06": ("reference", "199F", "TDbAxisLabelSet"),
    "V07": ("reference", "19F3", "TDbDrawingName"),
    "V08": ("reference", "17DF", "AcDbBlockReference"),
    "V09": ("scheme", "CD94", "TDbGroupOpening"),
    "V10": ("scheme", "CF5F", "TDbRectCornerStair"),
    "V11": ("reference", "199E", "TDbSymbElevation"),
    "V12": ("reference", "19E9", "TDbSymbMultiLeader"),
    "V13": ("reference", "1A15", "TDbSymbArrow"),
    "V14": ("reference", "1A05", "TDbSymbSection"),
    "V15": ("reference", "1A26", "TDbPipe"),
    "V16": ("reference", "1A29", "TDbPipeFitting"),
    "V17": ("reference", "19AE", "TDbColumn"),
    "V18": ("reference", "1A1E", "TDbSymbComposing"),
}


def main(path):
    assertions, failures = 0, []

    def check(ok, label):
        nonlocal assertions
        assertions += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def same(a, b, label, delta=(0, 0, 0), layer=True):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": records"):
            return
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": native type")
        if layer:
            check(a.get("layer") == b.get("layer"), label + ": layer")
        for key in ("min", "max"):
            v = a.get(key)
            expected = [x + y for x, y in zip(v, delta)] if isinstance(v, list) else v
            check(a.get("extentStatus") == b.get("extentStatus") and near(expected, b.get(key)), label + ": " + key)
        for key in ("grip", "stretch"):
            aa, bb = a.get(key, {}), b.get(key, {})
            pp = aa.get("points")
            expected = [[x + y for x, y in zip(p, delta)] for p in pp] if isinstance(pp, list) else pp
            check(aa.get("status") == bb.get("status") and points_equal(expected, bb.get("points")), label + ": " + key)
        ca, cb = a.get("curve"), b.get("curve")
        if isinstance(ca, dict):
            if check(isinstance(cb, dict), label + ": curve retained"):
                for key in ("start", "end"):
                    p = ca.get(key)
                    expected = [x + y for x, y in zip(p, delta)] if isinstance(p, list) else p
                    check(near(expected, cb.get(key)), label + ": curve " + key)
                va, vb = ca.get("length"), cb.get("length")
                length_equal = va is None and vb is None
                if isinstance(va, (int, float)) and isinstance(vb, (int, float)):
                    length_equal = math.isfinite(va) and math.isfinite(vb) and abs(va - vb) <= 1e-6
                check(ca.get("distanceStatus") == cb.get("distanceStatus") and length_equal, label + ": curve length/status")

    if not check(path.is_file(), "native object-family host evidence missing"):
        print("FAIL:", failures[0])
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    check(d.get("contract") == "stage1.object-family-native.v1", "contract")
    check(d.get("api") == "ObjectARX native; no COM", "native object API")
    cases = d.get("fixtures", [])
    check(len(cases) == len(FIXTURES) and {x.get("id") for x in cases} == set(FIXTURES), "all eighteen distinct representative fixtures")
    for c in cases:
        cid = c.get("id", "unknown")
        if not check(cid in FIXTURES, cid + ": known fixture"):
            continue
        source, handle, kind = FIXTURES[cid]
        check((c.get("source"), c.get("sourceHandle"), c.get("expectedType")) == (source, handle, kind), cid + ": frozen sample identity")
        check(c.get("initialSchemeCount") == 213, cid + ": full frozen scheme input")
        check(c.get("error") is None, cid + ": no operation error")
        for key in ("cloneStatus", "transformStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
            check(c.get(key) == 0, cid + ": " + key)
        out = Path(c.get("outputDWG", ""))
        check(out.is_file() and out.resolve().parent == path.resolve().parent, cid + ": isolated persisted DWG")
        class_info = c.get("classInfo", {})
        chain = class_info.get("parentChain", [])
        check(bool(chain) and chain[0] == kind and "AcDbEntity" in chain, cid + ": actual native inheritance")
        check(isinstance(class_info.get("casts"), dict), cid + ": actual public base-class casts")
        src = c.get("sourceEntity", {})
        check(src.get("type") == kind and src.get("handle") == handle and src.get("proxy") is False, cid + ": source native entity")
        check(isinstance(src.get("members"), list) and src.get("memberIteratorAvailable") is True, cid + ": public RX capability evidence")
        before, after, saved = (c.get(k, {}) for k in ("before", "after", "saved"))
        check(bool(c.get("primaryHandle")) and all(e.get("handle") == c.get("primaryHandle") for e in (before, after, saved)), cid + ": primary handle bound through every state")
        layer_map = [m for m in c.get("resourceMapping", []) if m.get("type") == "AcDbLayerTableRecord" and m.get("sourceName") == src.get("layer")]
        check(bool(layer_map) and before.get("layer") in {m.get("targetName") for m in layer_map}, cid + ": clone layer follows actual resource mapping")
        same(src, before, cid + "/clone", layer=False)
        check(c.get("offset") == [40000, 0, 0], cid + ": explicit diagnostic translation")
        same(before, after, cid + "/translate", (40000, 0, 0))
        same(after, saved, cid + "/reopen")
        originals = [c.get(k, []) for k in ("originalBefore", "originalAfter", "originalSaved")]
        if check(all(len(seq) == 213 for seq in originals), cid + ": all original scheme entities recorded"):
            a, b, z = [{x.get("handle"): x for x in seq} for seq in originals]
            check(len(a) == 213 and set(a) == set(b) == set(z), cid + ": original handle identities")
            for h in a:
                if h in b and h in z:
                    same(a[h], b[h], cid + "/original-after/" + h)
                    same(a[h], z[h], cid + "/original-saved/" + h)
            check(before.get("handle") not in a, cid + ": primary clone is additional")
        added, persisted = c.get("added", []), c.get("savedAdded", [])
        check(len(added) == len(persisted) == c.get("addedCount") and len(added) >= 1, cid + ": complete addition cohort")
        check(c.get("reopenedModelCount") == 213 + len(added), cid + ": model count includes every dependency")
        aa, bb = [{x.get("handle"): x for x in seq} for seq in (added, persisted)]
        check(c.get("primaryHandle") in aa and c.get("primaryHandle") in bb, cid + ": primary belongs to both complete addition records")
        check(len(aa) == len(added) and set(aa) == set(bb), cid + ": addition handles persisted")
        for h in aa:
            if h in bb:
                same(aa[h], bb[h], cid + "/added/" + h)
        check(c.get("associationStatus") == "not validated", cid + ": no untested relationship claim")
    for f in failures[:100]:
        print("FAIL:", f)
    if len(failures) > 100:
        print("Additional failures:", len(failures) - 100)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: native class/RX inventory, template clone, diagnostic translation, scheme preservation and persistence. Business parameters, relationships and visual styles need separate evidence.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
