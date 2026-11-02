"""Structural validation only; unreadable Tianzheng text requires separate rendering."""
import json
import sys
from pathlib import Path

from verify_scheme_native import near, point, points_equal


def main(path):
    count, failed = 0, []

    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failed.append(label)
        return bool(ok)

    def geometry(a, b, label, delta=(0, 0, 0), same_handle=True):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": geometry records"):
            return
        check(a.get("type") == b.get("type") and b.get("proxy") is False and a.get("layer") == b.get("layer"), label + ": native type/layer")
        if same_handle:
            check(a.get("handle") == b.get("handle"), label + ": identity")
        check(a.get("extentStatus") == b.get("extentStatus") == 0, label + ": actual extents")
        for key in ("min", "max"):
            p = a.get(key)
            check(point(p) and near([x+d for x, d in zip(p, delta)], b.get(key)), label + ": " + key)
        for key in ("grip", "stretch"):
            aa, bb = a.get(key, {}), b.get(key, {})
            pts = aa.get("points")
            check(aa.get("status") == bb.get("status") and isinstance(pts, list)
                  and all(point(p) for p in pts)
                  and points_equal([[x+d for x, d in zip(p, delta)] for p in pts], bb.get("points")), label + ": " + key)
        check(a.get("curve") == b.get("curve"), label + ": non-curve text state")

    def snapshot(a, b, label):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": snapshots"):
            return
        geometry(a.get("geometry"), b.get("geometry"), label)
        check(a.get("common") == b.get("common"), label + ": common style/ownership")
        check(a.get("nativeText") == b.get("nativeText"), label + ": actual native text getter observations")

    def preservation(a, b, label):
        if not check(isinstance(a, list) and isinstance(b, list) and len(a) == len(b) == 213, label + ": all 213 originals"):
            return
        aa, bb = ({v.get("geometry", {}).get("handle"): v for v in values} for values in (a, b))
        if check(len(aa) == len(bb) == 213 and set(aa) == set(bb), label + ": unique original identities"):
            for handle in aa:
                snapshot(aa[handle], bb[handle], label + "/" + str(handle))

    if not check(path.is_file(), "text bridge native evidence missing"):
        print("FAIL:", failed[0])
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (ValueError, OSError) as error:
        print("FAIL: cannot read evidence:", error)
        return 1
    if not check(isinstance(data, dict), "JSON object root"):
        print("FAIL:", failed[-1])
        print(f"assertions={count} passed={count-len(failed)} failed={len(failed)} result=FAIL")
        return 1
    check(data.get("contract") == "stage1.text-bridge-native.v1", "contract")
    check(data.get("api") == "ObjectARX native; no COM" and data.get("error") is None, "native API / no infrastructure error")
    if data.get("error") is not None:
        for message in failed:
            print("FAIL:", message)
        print("Native error:", data.get("error"))
        print(f"assertions={count} passed={count-len(failed)} failed={len(failed)} result=FAIL")
        return 1
    root = Path(__file__).resolve().parents[2]
    value = data.get("schemeInputPath")
    check(isinstance(value, str) and Path(value).resolve() == (root / "阶段0/输入快照/方案平面图.dwg").resolve(), "frozen scheme input identity")
    for key in ("controlDWG", "outputDWG"):
        value = data.get(key)
        if check(isinstance(value, str), key + ": path"):
            file = Path(value)
            check(file.resolve().parent == path.resolve().parent and file.is_file() and file.stat().st_size > 0, key + ": isolated file")
    if all(isinstance(data.get(k), str) for k in ("controlDWG", "outputDWG", "schemeInputPath")):
        check(len({Path(data[k]).resolve() for k in ("controlDWG", "outputDWG", "schemeInputPath")}) == 3, "source, control and output are separate files")
    check(data.get("initialModelCount") == 213, "complete initial scheme")
    for key in ("saveStatus", "reopenStatus", "closeInputStatus"):
        check(data.get(key) == 0, key)
    for key in ("zoomStatus", "regenStatus"):
        check(data.get(key) == 5100, key)
    check(data.get("businessTextVerification") in {"pending rendered verification", "native text getter verified", "native text getter mismatch"}, "honest business verification classification")
    check(data.get("renderedVerification") == "not performed by this probe", "rendering remains separate")
    source = data.get("sourceTarget", {})
    source_geometry = source.get("geometry", {})
    check(source_geometry.get("handle") == "CDC0" and source_geometry.get("type") == "TDbText"
          and source_geometry.get("proxy") is False, "actual native CDC0 source")
    original = data.get("originalBefore", [])
    preservation(original, data.get("originalAfter"), "originalAfter")
    preservation(original, data.get("originalSaved"), "originalSaved")
    original_handles = {v.get("geometry", {}).get("handle") for v in original}
    check("CDC0" in original_handles, "source belongs to protected originals")
    originals_by_handle = {v.get("geometry", {}).get("handle"): v for v in original}
    snapshot(originals_by_handle.get("CDC0"), source, "sourceTarget/protectedCDC0")
    cases = data.get("pairs")
    fixture_handles, getter_results = set(), []
    if check(isinstance(cases, list) and len(cases) == 2, "two independent donor/target pairs"):
        check({v.get("id") for v in cases} == {"code", "pending"}, "distinct text fixtures")
        for pair in cases:
            label = "pair/" + str(pair.get("id"))
            expected = "C09235" if pair.get("id") == "code" else "待确认"
            wanted = [40000, 20000, 0] if pair.get("id") == "code" else [44000, 20000, 0]
            check(pair.get("expectedText") == expected and near(pair.get("targetMinWcs"), wanted), label + ": fixed diagnostic contract")
            for key in ("cloneWasTransient", "cloneSameNativeType", "allObjectsClosedBeforeCommand"):
                check(pair.get(key) is True, label + ": " + key)
            for key in ("appendTargetStatus", "transformStatus", "appendDonorStatus"):
                check(pair.get(key) == 0, label + ": " + key)
            check(pair.get("activeTransactionsBeforeCommand") == pair.get("activeTransactionsAfterCommand") == 0, label + ": no transaction across command")
            before, after, saved = (pair.get(key, {}) for key in ("targetBefore", "targetAfter", "targetSaved"))
            before_geometry = before.get("geometry", {})
            delta = pair.get("translation")
            if check(point(delta), label + ": actual clone translation"):
                geometry(source_geometry, before_geometry, label + "/clone", delta, same_handle=False)
            check(near(before_geometry.get("min"), wanted), label + ": clone placed outside building")
            check(after.get("geometry", {}).get("type") == saved.get("geometry", {}).get("type") == "TDbText", label + ": Tianzheng target retained")
            check(after.get("geometry", {}).get("handle") == before_geometry.get("handle"), label + ": same target after command")
            snapshot(after, saved, label + "/persistedTarget")
            for stage in (before, after, saved):
                common = stage.get("common", {})
                check(common.get("sameDatabase") is True and common.get("ownerIsModel") is True, label + ": actual target Model ownership")
                raw = stage.get("publicDxf", {})
                check(raw.get("method") == "acdbEntGet" and isinstance(raw.get("available"), bool)
                      and raw.get("truncated") is False, label + ": read-only raw DXF audit")
            donor_before, donor_after, donor_saved = (pair.get(key, {}) for key in ("donorBefore", "donorAfter", "donorSaved"))
            snapshot(donor_before, donor_after, label + "/unchangedDonor")
            snapshot(donor_after, donor_saved, label + "/persistedDonor")
            for donor in (donor_before, donor_after, donor_saved):
                text = donor.get("nativeText", {})
                check(donor.get("geometry", {}).get("type") == "AcDbText" and text.get("readable") is True
                      and text.get("getter") == "AcDbText::textString" and text.get("status") == 0
                      and text.get("value") == expected, label + ": actual donor native content")
                check(near(text.get("position"), [wanted[0], wanted[1]-2000, wanted[2]])
                      and text.get("height") == 350, label + ": donor legend placement/height")
                common = donor.get("common", {})
                check(common.get("sameDatabase") is True and common.get("ownerIsModel") is True
                      and isinstance(common.get("textStyle"), dict), label + ": actual donor ownership/font reference")
            for handle in (before_geometry.get("handle"), donor_before.get("geometry", {}).get("handle")):
                check(isinstance(handle, str) and handle not in original_handles and handle not in fixture_handles, label + ": separate new fixture identity")
                fixture_handles.add(handle)
            check(pair.get("commandName") == "TTextReplace" and pair.get("commandAttempted") is True
                  and pair.get("commandStatus") == 5100 and pair.get("commandError") is None, label + ": actual command return without inferred text success")
            check(pair.get("tokens") == ["TTextReplace", "donor point", "", "target point", ""], label + ": one documented selection cycle")
            check(pair.get("coordinateTransformStatuses") == [5100, 5100], label + ": WCS to UCS conversions")
            for key in ("donorPickWcs", "targetPickWcs", "donorPickUcs", "targetPickUcs"):
                check(point(pair.get(key)), label + ": " + key)
            check(isinstance(pair.get("commandTrace"), list) and isinstance(pair.get("inputTrace"), list), label + ": real trace arrays")
            events = pair.get("commandTrace", [])
            relevant = [event for event in events if isinstance(event, dict)
                        and str(event.get("command", "")).lstrip("_.").upper() == "TTEXTREPLACE"]
            check(any(event.get("event") == "commandWillStart" for event in relevant)
                  and any(event.get("event") == "commandEnded" for event in relevant)
                  and not any(event.get("event") in {"commandCancelled", "commandFailed", "unknownCommand"}
                              for event in events if isinstance(event, dict)), label + ": observed command start/end without cancellation/failure")
            observed = []
            for event in pair.get("inputTrace", []):
                if not isinstance(event, dict):
                    continue
                if event.get("selectedHandle") is not None:
                    observed.append(event["selectedHandle"])
                if isinstance(event.get("selectedHandles"), list):
                    observed.extend(h for h in event["selectedHandles"] if h is not None)
            expected_handles = {before_geometry.get("handle"), donor_before.get("geometry", {}).get("handle")}
            check(all(handle in expected_handles for handle in observed), label + ": any observed selection belongs to this pair")
            trace = pair.get("trace", {})
            check(trace.get("editorAdded") is True and trace.get("editorRemoved") is True
                  and trace.get("inputAddReturned") is True and trace.get("inputAddStatus") == 0
                  and trace.get("inputRemoveReturned") is True and trace.get("inputRemoveStatus") in {0, 33}
                  and trace.get("retained") is False and trace.get("traceDropped") is False, label + ": trace safely detached")
            check(pair.get("geometryChanged") == (before_geometry != after.get("geometry")), label + ": honest observed geometry change only")
            native_after, native_saved = after.get("nativeText", {}), saved.get("nativeText", {})
            for native in (native_after, native_saved):
                check((native.get("readable") is True and native.get("status") == 0 and native.get("getter") == "AcDbText::textString")
                      or (native.get("readable") is False and native.get("status") is None and native.get("value") is None
                          and native.get("getter") is None), label + ": no fabricated business text getter")
            known_mismatch = any(native.get("readable") is True and native.get("value") != expected for native in (native_after, native_saved))
            check(not known_mismatch, label + ": any readable target value must match donor")
            getter_results.append((native_after.get("readable") is True and native_saved.get("readable") is True,
                                   native_after.get("value") == native_saved.get("value") == expected, known_mismatch))
    additions, persisted = data.get("added", []), data.get("savedAdded", [])
    if check(isinstance(additions, list) and isinstance(persisted, list), "actual additions inventories"):
        aa, bb = ({v.get("geometry", {}).get("handle"): v for v in values} for values in (additions, persisted))
        check(len(aa) == len(additions) == len(bb) == len(persisted) >= 4 and set(aa) == set(bb)
              and not (set(aa) & original_handles) and fixture_handles <= set(aa), "all unique fixture/dependency additions persisted")
        check(data.get("modelCountAfter") == data.get("reopenedModelCount") == 213+len(additions), "actual Model count includes all dependencies")
        check(data.get("extraModelEntityCount") == len(additions)-4, "extra dependencies explicitly counted")
        for handle in aa.keys() & bb.keys():
            snapshot(aa[handle], bb[handle], "addition/" + str(handle))
        for pair in cases if isinstance(cases, list) else []:
            for prefix in ("target", "donor"):
                pair_after, pair_saved = pair.get(prefix+"After", {}), pair.get(prefix+"Saved", {})
                handle = pair_after.get("geometry", {}).get("handle")
                snapshot(pair_after, aa.get(handle), "pair/inventory/" + prefix + "/after")
                snapshot(pair_saved, bb.get(handle), "pair/inventory/" + prefix + "/saved")
    if len(getter_results) == 2:
        readable = all(value[0] for value in getter_results)
        classification = "native text getter mismatch" if any(value[2] for value in getter_results) else "native text getter verified" if readable and all(value[1] for value in getter_results) else "pending rendered verification"
        check(data.get("businessTextVerification") == classification, "business result follows actual target getters")
        check(classification != "native text getter mismatch", "readable target text matches donors")
    for message in failed:
        print("FAIL:", message)
    print(f"assertions={count} passed={count-len(failed)} failed={len(failed)} result={'FAIL' if failed else 'PASS'}")
    print("Business text:", data.get("businessTextVerification"))
    print("Scope: STRUCTURE ONLY; command return, geometry changes and raw DXF are not proof of Tianzheng text replacement. Separate rendered verification is required when target getters are unavailable.")
    return int(bool(failed))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_text_bridge_native.py <text-bridge-native.json>")
    raise SystemExit(main(Path(sys.argv[1])))
