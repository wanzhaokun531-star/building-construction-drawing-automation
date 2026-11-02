"""Validate limited native relationship/resource/rollback evidence, not business coverage."""
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

    def geometry(a, b, label, offset=(0, 0, 0)):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": geometry records"):
            return
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": native type")
        check(a.get("extentStatus") == b.get("extentStatus") == 0, label + ": actual extents")
        check(a.get("handle") == b.get("handle") and a.get("layer") == b.get("layer"), label + ": identity/layer")
        check(a.get("colorIndex") == b.get("colorIndex"), label + ": color unchanged")
        for key in ("min", "max"):
            p = a.get(key)
            check(point(p) and near([x+d for x, d in zip(p, offset)], b.get(key)), label + ": " + key)
        for key in ("grip", "stretch"):
            aa, bb = a.get(key, {}), b.get(key, {})
            pts = aa.get("points")
            check(aa.get("status") == bb.get("status"), label + ": " + key + " query")
            check(isinstance(pts, list) and all(point(p) for p in pts)
                  and points_equal([[x+d for x, d in zip(p, offset)] for p in pts], bb.get("points")), label + ": " + key)
        ca, cb = a.get("curve"), b.get("curve")
        check((ca is None) == (cb is None), label + ": curve availability")
        if ca is not None:
            if check(isinstance(ca, dict) and isinstance(cb, dict), label + ": curve"):
                check(ca.get("distanceStatus") == cb.get("distanceStatus") == 0
                      and isinstance(ca.get("length"), (int, float))
                      and isinstance(cb.get("length"), (int, float))
                      and abs(ca["length"]-cb["length"]) <= 1e-6, label + ": length")
                for key in ("start", "end"):
                    p = ca.get(key)
                    check(point(p) and near([x+d for x, d in zip(p, offset)], cb.get(key)), label + ": curve " + key)

    def preservation(a, b, label):
        if not check(isinstance(a, list) and isinstance(b, list) and len(a) == len(b) == 213, label + ": 213 originals"):
            return
        aa, bb = ({v.get("handle"): v for v in seq} for seq in (a, b))
        if check(len(aa) == len(bb) == 213 and set(aa) == set(bb), label + ": complete unique handles"):
            for h in aa:
                geometry(aa[h], bb[h], label + "/" + str(h))

    def output(value, label):
        if not check(isinstance(value, str) and bool(value), label + ": output path"):
            return
        f = Path(value)
        check(f.resolve().parent == path.resolve().parent and f.is_file() and f.stat().st_size > 0, label + ": isolated nonempty file")

    def text_definition(record):
        if not isinstance(record, dict):
            return None
        keys = ("fileNameStatus", "fileName", "bigFontFileNameStatus", "bigFontFileName", "font",
                "textSize", "xScale", "obliquingAngle", "flagBits", "isVertical", "isShapeFile")
        return {k: record.get(k) for k in keys}

    def resources(a, b, label):
        if not check(isinstance(a, list) and isinstance(b, list) and len(a) == len(b) == 114, label + ": original resource inventory"):
            return
        aa, bb = ({r.get("handle"): r for r in seq} for seq in (a, b))
        if check(len(aa) == len(bb) == 114 and set(aa) == set(bb), label + ": original resource identities"):
            # The native reader includes runtime observations which are not definitions.
            ignored = {"isInUse", "isModifiedForRecompute", "fontFiles"}
            for h in aa:
                x = {k: v for k, v in aa[h].items() if k not in ignored}
                y = {k: v for k, v in bb[h].items() if k not in ignored}
                check(x == y, label + "/" + str(h) + ": actual resource definition preserved")

    def rollback_object(record, originals, handle, legacy_active, label):
        if not check(isinstance(record, dict), label + ": native post-transaction observation"):
            return
        check(record.get("idWellBehaved") is True, label + ": safe tracked ID")
        check(all(isinstance(record.get(k), bool) for k in ("idNull", "idValid", "idValidNotErased")), label + ": raw ID state")
        valid = record.get("idValid")
        if valid is True:
            check(record.get("idNull") is False and record.get("idSameDatabase") is True
                  and all(isinstance(record.get(k), bool) for k in ("idErased", "idEffectivelyErased", "idResident")), label + ": valid ID observations")
        else:
            check(all(record.get(k) is None for k in ("idErased", "idEffectivelyErased", "idResident", "idSameDatabase")), label + ": no dereference of invalid ID")
        predicate = valid is True and record.get("idErased") is False
        check(record.get("idValidNotErased") is predicate and legacy_active is predicate,
              label + ": legacy Active field is only the ID predicate")
        # SDK 2020 acadstrc.h: null ID, erased, permanently erased, not in DB,
        # invalid ID. A lock/type/generic open failure does not prove absence.
        absent_open_statuses = {16, 80, 81, 137, 600}
        status = record.get("openStatus")
        check(isinstance(status, int) and not isinstance(status, bool) and status in absent_open_statuses,
              label + ": ordinary read-open establishes uncreated/erased object")
        check(status != 16 or record.get("idNull") is True, label + ": eNullObjectId agrees with raw ID")
        check(record.get("openReturnedObject") is False
              and all(record.get(k) is None for k in ("openClass", "openOwner", "openOwnerIsModel", "openObjectIdMatches", "closeStatus")),
              label + ": no read-openable object after rollback")
        check(record.get("iteratorStatus") == 0 and record.get("iteratorSkipErased") is True
              and record.get("modelRecordClass") == "AcDbBlockTableRecord"
              and isinstance(record.get("modelRecordHandle"), str) and bool(record.get("modelRecordHandle")), label + ": actual Model iterator")
        values = record.get("modelHandles")
        expected = {v.get("handle") for v in originals} if isinstance(originals, list) else set()
        if check(isinstance(values, list) and len(values) == 213 and all(isinstance(v, str) for v in values), label + ": actual 213 Model handles"):
            check(len(set(values)) == 213 and set(values) == expected and handle not in values, label + ": only original Model members")
        check(record.get("modelIdMatches") == record.get("modelHandleMatches") == 0
              and record.get("modelMember") is False, label + ": new ID and handle absent from Model")

    if not check(path.is_file(), "native safety/resources evidence missing"):
        print("FAIL:", failures[0])
        print(f"assertions={count} passed=0 failed=1 result=FAIL")
        return 1
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (ValueError, OSError) as error:
        print("FAIL: evidence cannot be read:", error)
        return 1
    check(data.get("contract") == "stage1.native-safety-resources.v1", "contract")
    check(data.get("api") == "ObjectARX native; no COM", "native API")
    frozen_root = Path(__file__).resolve().parents[2] / "阶段0" / "输入快照"
    for key, name in (("schemeInputPath", "方案平面图.dwg"), ("referenceInputPath", "施工图平面参考图.dwg")):
        value = data.get(key)
        check(isinstance(value, str) and Path(value).resolve() == (frozen_root / name).resolve(), "frozen input identity: " + key)
    check(data.get("businessAssociationStatus") == "Unknown", "business associations remain unverified")
    sections = {}
    for key in ("relation", "styleConflict", "geometryRollback", "creationRollback"):
        s = data.get(key)
        if check(isinstance(s, dict), key + ": section"):
            sections[key] = s
            check(s.get("error") is None, key + ": native error")

    r = sections.get("relation", {})
    check(r.get("readStatus") == r.get("closeInputStatus") == 0 and r.get("sourceCount") == 213, "relation: frozen scheme read")
    check(r.get("createNetworkIfAbsent") is False and r.get("businessAssociationStatus") == "Unknown", "relation: read-only classification")
    records = r.get("records")
    if check(isinstance(records, list) and len(records) == 213, "relation: all 213 structural records"):
        check(len({v.get("handle") for v in records}) == 213, "relation: unique records")
        for v in records:
            label = "relation/" + str(v.get("handle"))
            check(v.get("observation") in {"PublicStructureObserved", "NoPublicStructureObserved", "QueryUnavailable"}, label + ": honest classification")
            check(isinstance(v.get("dependencyQueryStatus"), int), label + ": actual dependency status")
            check(isinstance(v.get("reactors"), list) and isinstance(v.get("dependencies"), list), label + ": returned arrays")
            owner = v.get("owner")
            check(isinstance(owner, dict) and owner.get("valid") is True and owner.get("sameDatabase") is True
                  and owner.get("openStatus") == 0, label + ": real source owner")
            for ref_key in ("extensionDictionary", "objectNetwork"):
                ref = v.get(ref_key)
                check(ref is None or (isinstance(ref, dict) and ref.get("valid") is True
                                     and ref.get("sameDatabase") is True), label + ": " + ref_key)
            if v.get("observation") == "NoPublicStructureObserved":
                check(v.get("dependencyQueryStatus") == 0 and not v.get("dependencies")
                      and not any(x.get("persistent") for x in v.get("reactors", []))
                      and v.get("objectNetwork") is None, label + ": absence only after successful query")
        check(sum(v.get("type") == "TDbWall" for v in records) == 63, "relation: all walls")
        check(sum(v.get("type") == "TDbOpening" for v in records) == 29, "relation: all ordinary openings")
        check(sum(v.get("type") == "TDbGroupOpening" for v in records) == 1, "relation: group opening included")

    b = sections.get("styleConflict", {})
    for key in ("cloneStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
        check(b.get(key) == 0, "styleConflict: " + key)
    ss, rr, target, saved = (b.get(k) for k in ("schemeStyle", "referenceStyle", "clonedStyle", "savedStyle"))
    if check(all(isinstance(v, dict) for v in (ss, rr, target, saved)), "styleConflict: four actual styles"):
        check(ss.get("name") == rr.get("name") == "_TCH_WINDOW", "styleConflict: real conflicting source names")
        check(ss.get("xScale") == 1 and rr.get("xScale") == 0.7, "styleConflict: independent known conflict")
        check(all(v.get("fileNameStatus") == v.get("bigFontFileNameStatus") == v.get("font", {}).get("status") == 0
                  for v in (ss, rr, target, saved)), "styleConflict: successful native definition getters")
        check(target.get("name") != "_TCH_WINDOW" and target.get("handle") == saved.get("handle"), "styleConflict: mapped isolated target")
        check(text_definition(rr) == text_definition(target) == text_definition(saved), "styleConflict: source/target/persisted getter definitions")
        mapping = b.get("mapping", {})
        check(mapping.get("sourceHandle") == rr.get("handle") and mapping.get("targetHandle") == target.get("handle")
              and mapping.get("isCloned") is True, "styleConflict: actual clone mapping")
    check(b.get("modelCountBefore") == b.get("modelCountAfter") == b.get("reopenedModelCount") == 213, "styleConflict: no business entity clone")
    check(b.get("businessPrimaryBindingVerified") is False, "styleConflict: style-only clone scope")
    resources(b.get("originalResourcesBefore"), b.get("originalResourcesAfter"), "styleConflict/after")
    resources(b.get("originalResourcesBefore"), b.get("originalResourcesSaved"), "styleConflict/saved")
    output(b.get("outputDWG"), "styleConflict")

    c = sections.get("geometryRollback", {})
    trials = c.get("trials")
    if check(isinstance(trials, list) and len(trials) == 2, "geometryRollback: dimension and line control"):
        check({t.get("id") for t in trials} == {"dimension", "native-line"}, "geometryRollback: distinct cases")
        for t in trials:
            label = "geometryRollback/" + str(t.get("id"))
            for key in ("moveStatus", "abortStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
                check(t.get(key) == 0, label + ": " + key)
            before, during, after, saved = (t.get(k) for k in ("before", "during", "after", "saved"))
            check(point(t.get("offset")) and any(abs(v) > 1e-6 for v in t.get("offset", [])), label + ": nonzero fixture")
            geometry(before, during, label + "/during", t.get("offset", (0, 0, 0)))
            geometry(before, after, label + "/after")
            geometry(before, saved, label + "/saved")
            if isinstance(before, dict) and isinstance(during, dict):
                check(not points_equal(before.get("grip", {}).get("points"), during.get("grip", {}).get("points")), label + ": real observed change before abort")
                check(before.get("type") == ("TDbDimension2" if t.get("id") == "dimension" else "AcDbLine"), label + ": control type")
            output(t.get("outputDWG"), label)
    preservation(c.get("originalBefore"), c.get("originalAfter"), "geometryRollback/after")
    preservation(c.get("originalBefore"), c.get("originalSaved"), "geometryRollback/saved")

    d = sections.get("creationRollback", {})
    for key in ("moveStatus", "appendStatus", "registerStatus", "abortStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
        check(d.get(key) == 0, "creationRollback: " + key)
    check(d.get("modelCountBefore") == d.get("modelCountAfter") == d.get("reopenedModelCount") == 213
          and d.get("modelCountDuring") == 214, "creationRollback: actual added entity removed by abort")
    check(d.get("legacyActiveDefinition") == "id.isValid && !id.isErased; not Model membership", "creationRollback: legacy ID predicate explicitly distinguished")
    rollback_object(d.get("postAbortObject"), d.get("originalBefore"), d.get("newEntityHandle"),
                    d.get("newEntityActiveAfter"), "creationRollback/postAbort")
    rollback_object(d.get("postReopenObject"), d.get("originalBefore"), d.get("newEntityHandle"),
                    d.get("newEntityActiveSaved"), "creationRollback/postReopen")
    lookup = d.get("savedHandleLookupStatus")
    check(isinstance(lookup, int) and not isinstance(lookup, bool) and lookup in {0, 13}, "creationRollback: real persisted handle lookup")
    post_abort, post_reopen = d.get("postAbortObject"), d.get("postReopenObject")
    if isinstance(post_abort, dict) and isinstance(post_reopen, dict):
        check(post_abort.get("idNull") is False, "creationRollback: query original tracked new-object ID after abort")
        check(post_abort.get("modelRecordHandle") == post_reopen.get("modelRecordHandle"), "creationRollback: persisted Model owner identity")
        check((d.get("savedHandleLookupStatus") == 13) == (post_reopen.get("idNull") is True), "creationRollback: missing saved handle is a null ID")
    before_object, new_object = d.get("existingBefore", {}), d.get("newEntityDuring", {})
    check(isinstance(before_object, dict) and before_object.get("handle") == "CDE6" and before_object.get("type") == "TDbDimension2", "creationRollback: real existing dimension fixture")
    check(isinstance(new_object, dict) and new_object.get("type") == "AcDbLine"
          and new_object.get("handle") == d.get("newEntityHandle") and new_object.get("proxy") is False, "creationRollback: actual newly registered LINE")
    geometry(d.get("existingBefore"), d.get("existingDuring"), "creationRollback/during", d.get("offset", (0, 0, 0)))
    geometry(d.get("existingBefore"), d.get("existingAfter"), "creationRollback/after")
    if isinstance(d.get("existingBefore"), dict) and isinstance(d.get("existingDuring"), dict):
        check(not points_equal(d["existingBefore"].get("grip", {}).get("points"), d["existingDuring"].get("grip", {}).get("points")), "creationRollback: existing geometry genuinely changed")
    preservation(d.get("originalBefore"), d.get("originalAfter"), "creationRollback/originalAfter")
    preservation(d.get("originalBefore"), d.get("originalSaved"), "creationRollback/originalSaved")
    status, failure_path = d.get("saveFailureStatus"), d.get("saveFailureDWG")
    check(isinstance(status, int) and not isinstance(status, bool) and status != 0, "saveFailure: actual non-eOk status")
    if check(isinstance(failure_path, str), "saveFailure: path"):
        f = Path(failure_path)
        check(f.resolve().parent.parent == path.resolve().parent and not f.exists() and not f.parent.exists(), "saveFailure: absent isolated parent/no published output")
    check(d.get("failedOutputPublished") is False, "saveFailure: no publication")
    sentinel = d.get("sentinel", {})
    check(sentinel.get("guardRejected") is True and sentinel.get("before") == sentinel.get("after") == "stage1-native-safety-sentinel-v1\n", "sentinel: real preflight refusal and unchanged content")
    if check(isinstance(sentinel.get("path"), str), "sentinel: path"):
        f = Path(sentinel["path"])
        check(f.resolve().parent == path.resolve().parent and f.is_file()
              and f.read_bytes() == b"stage1-native-safety-sentinel-v1\n", "sentinel: independent file read")
    output(d.get("outputDWG"), "creationRollback")
    for failure in failures:
        print("FAIL:", failure)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: structural observations, isolated style transfer and limited native rollback; business associations/fonts remain unverified.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_native_safety_resources.py <native-safety-resources.json>")
    raise SystemExit(main(Path(sys.argv[1])))
