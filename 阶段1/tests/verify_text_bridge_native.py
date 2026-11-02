"""Validate structure and sentinel-protected command roundtrip content separately."""
import json
import sys
from pathlib import Path

from verify_scheme_native import near, point, points_equal


def main(path):
    count, failed, semantic_count, semantic_failed = 0, [], 0, []

    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failed.append(label)
        return bool(ok)

    def semantic(ok, label):
        nonlocal semantic_count
        semantic_count += 1
        if not ok:
            semantic_failed.append(label)
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

    def command_evidence(pair, reverse, source_handle, destination_handle, label):
        def key(name):
            return "reverse" + name[0].upper() + name[1:] if reverse else name
        attempted = pair.get(key("commandAttempted")) is True
        returned = pair.get(key("commandStatus")) == 5100 and pair.get(key("commandError")) is None
        check(pair.get(key("commandName")) == "TTextReplace" and attempted and returned, label + ": actual command return")
        events, inputs = pair.get(key("commandTrace")), pair.get(key("inputTrace"))
        check(isinstance(events, list) and isinstance(inputs, list), label + ": real trace arrays")
        events = events if isinstance(events, list) else []
        inputs = inputs if isinstance(inputs, list) else []
        relevant = [event for event in events if isinstance(event, dict)
                    and str(event.get("command", "")).lstrip("_.").upper() == "TTEXTREPLACE"]
        balance, saw_start, ordered_lifecycle = 0, False, True
        for event in relevant:
            if event.get("event") == "commandWillStart":
                balance += 1
                saw_start = True
            elif event.get("event") == "commandEnded":
                balance -= 1
                ordered_lifecycle = ordered_lifecycle and balance >= 0
        global_balance, globally_ordered = 0, True
        for event in events:
            if not isinstance(event, dict):
                continue
            if event.get("event") == "commandWillStart":
                global_balance += 1
            elif event.get("event") in {"commandEnded", "commandCancelled", "commandFailed"}:
                global_balance -= 1
                globally_ordered = globally_ordered and global_balance >= 0
        completed = saw_start and balance == 0 and ordered_lifecycle and global_balance == 0 and globally_ordered and not any(
            event.get("event") in {"commandCancelled", "commandFailed", "unknownCommand"}
            for event in events if isinstance(event, dict))
        check(completed, label + ": observed start then end without failure")
        observed = []
        for event in inputs:
            if not isinstance(event, dict):
                continue
            handles = [event["selectedHandle"]] if event.get("selectedHandle") is not None else []
            if isinstance(event.get("selectedHandles"), list):
                handles += [h for h in event["selectedHandles"] if h is not None]
            for handle in handles:
                if not observed or observed[-1] != handle:
                    observed.append(handle)
        selected_consistent = all(h in {source_handle, destination_handle} for h in observed)
        check(selected_consistent, label + ": observed selections restricted to this pair")
        if len(observed) >= 2:
            ordered = observed[0] == source_handle and observed[-1] == destination_handle
            check(ordered, label + ": observed source/destination order")
            selected_consistent = selected_consistent and ordered
        trace = pair.get(key("trace"), {})
        detached = (trace.get("editorAdded") is True and trace.get("editorRemoved") is True
                    and trace.get("inputAddReturned") is True and trace.get("inputAddStatus") == 0
                    and trace.get("inputRemoveReturned") is True and trace.get("inputRemoveStatus") in {0, 33}
                    and trace.get("retained") is False and trace.get("traceDropped") is False)
        check(detached, label + ": trace safely detached")
        actual_complete = attempted and returned and completed and detached and selected_consistent
        check(pair.get("reverseCommandCompleted" if reverse else "forwardCommandCompleted") is actual_complete,
              label + ": completion classification matches actual evidence")
        return actual_complete

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
    check(data.get("businessTextVerification") in {"pending command-bridge roundtrip verification", "command-bridge roundtrip verified", "command-bridge roundtrip mismatch"}, "honest business verification classification")
    check(data.get("renderedVerification") == "not performed by this probe", "rendering remains separate")
    check(data.get("fontVerification") == "pending rendered verification", "font appearance remains separate")
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
    fixture_handles, roundtrip_results, known_mismatches = set(), [], []
    if check(isinstance(cases, list) and len(cases) == 2, "two independent donor/target pairs"):
        check({v.get("id") for v in cases} == {"code", "pending"}, "distinct text fixtures")
        for pair in cases:
            label = "pair/" + str(pair.get("id"))
            expected = "C09235" if pair.get("id") == "code" else "待确认"
            wanted = [40000, 20000, 0] if pair.get("id") == "code" else [44000, 20000, 0]
            if not check(all(isinstance(pair.get(k), dict) for k in ("targetBefore", "targetForwardAfter", "targetAfter", "targetSaved", "donorBefore", "donorForwardAfter", "donorSentinel", "donorAfter", "donorSaved", "reverseReadback")), label + ": forward, sentinel, reverse and readback snapshots present"):
                semantic(False, label + ": missing reverse/readback evidence")
                continue
            check(pair.get("expectedText") == expected and near(pair.get("targetMinWcs"), wanted), label + ": fixed diagnostic contract")
            for key in ("cloneWasTransient", "cloneSameNativeType", "allObjectsClosedBeforeCommand"):
                check(pair.get(key) is True, label + ": " + key)
            for key in ("appendTargetStatus", "transformStatus", "appendDonorStatus"):
                check(pair.get(key) == 0, label + ": " + key)
            check(pair.get("activeTransactionsBeforeCommand") == pair.get("activeTransactionsAfterCommand") == 0, label + ": no transaction across command")
            before, forward_after, after, saved = (pair[key] for key in ("targetBefore", "targetForwardAfter", "targetAfter", "targetSaved"))
            before_geometry = before.get("geometry", {})
            delta = pair.get("translation")
            if check(point(delta), label + ": actual clone translation"):
                geometry(source_geometry, before_geometry, label + "/clone", delta, same_handle=False)
            check(near(before_geometry.get("min"), wanted), label + ": clone placed outside building")
            check(after.get("geometry", {}).get("type") == saved.get("geometry", {}).get("type") == "TDbText", label + ": Tianzheng target retained")
            check(after.get("geometry", {}).get("handle") == before_geometry.get("handle"), label + ": same target after command")
            snapshot(after, saved, label + "/persistedTarget")
            snapshot(forward_after, after, label + "/reverseSourceUnchanged")
            check(before.get("common") == after.get("common"), label + ": target common style preserved")
            for stage in (before, after, saved):
                common = stage.get("common", {})
                check(common.get("sameDatabase") is True and common.get("ownerIsModel") is True, label + ": actual target Model ownership")
                raw = stage.get("publicDxf", {})
                check(raw.get("method") == "acdbEntGet" and isinstance(raw.get("available"), bool)
                      and raw.get("truncated") is False, label + ": read-only raw DXF audit")
            # The live bridge independently establishes the meaning of the raw
            # public group1 value. Read only: never entmod Tianzheng private data.
            for stage_name, stage, text_expected in (("before", before, "庭院"),
                    ("forward", forward_after, expected), ("after", after, expected),
                    ("saved", saved, expected)):
                raw = stage.get("publicDxf", {})
                rows = raw.get("stringGroups", [])
                check(raw.get("method") == "acdbEntGet" and raw.get("adsNameStatus") == 0
                      and raw.get("available") is True and raw.get("truncated") is False,
                      label + "/" + stage_name + ": complete actual public DXF read")
                check(isinstance(rows, list) and [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 0] == ["TCH_TEXT"]
                      and "TDbText" in [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 100],
                      label + "/" + stage_name + ": actual TCH_TEXT subclass identity")
                values = [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 1]
                semantic(values == [text_expected], label + "/" + stage_name
                         + ": exact unique public group1 agrees with independent bridge content")
            donor_before, donor_forward, donor_sentinel, donor_after, donor_saved = (pair[k] for k in ("donorBefore", "donorForwardAfter", "donorSentinel", "donorAfter", "donorSaved"))
            snapshot(donor_before, donor_forward, label + "/forwardSourceUnchanged")
            snapshot(donor_after, donor_saved, label + "/persistedDonor")
            for donor in (donor_before, donor_forward, donor_sentinel, donor_after, donor_saved):
                text = donor.get("nativeText", {})
                check(donor.get("geometry", {}).get("type") == "AcDbText" and text.get("readable") is True
                      and text.get("getter") == "AcDbText::textString" and text.get("status") == 0
                      and isinstance(text.get("value"), str), label + ": actual donor native getter")
                check(near(text.get("position"), [wanted[0], wanted[1]-2000, wanted[2]])
                      and text.get("height") == 350, label + ": donor legend placement/height")
                common = donor.get("common", {})
                check(common.get("sameDatabase") is True and common.get("ownerIsModel") is True
                      and isinstance(common.get("textStyle"), dict), label + ": actual donor ownership/font reference")
                check(common == donor_before.get("common"), label + ": donor common style preserved")
            check(donor_before.get("nativeText", {}).get("value") == donor_forward.get("nativeText", {}).get("value") == expected, label + ": actual forward donor content")
            for handle in (before_geometry.get("handle"), donor_before.get("geometry", {}).get("handle")):
                check(isinstance(handle, str) and handle not in original_handles and handle not in fixture_handles, label + ": separate new fixture identity")
                fixture_handles.add(handle)
            donor_handle = donor_before.get("geometry", {}).get("handle")
            target_handle = before_geometry.get("handle")
            same_donor = all(state.get("geometry", {}).get("handle") == donor_handle
                             and state.get("geometry", {}).get("type") == "AcDbText"
                             and state.get("geometry", {}).get("proxy") is False
                             for state in (donor_forward, donor_sentinel, donor_after, donor_saved))
            check(same_donor, label + ": same donor identity through sentinel and readback")
            forward_completed = command_evidence(pair, False, donor_handle, target_handle, label + "/forward")
            check(pair.get("tokens") == ["TTextReplace", "donor point", "", "target point", ""], label + ": one documented selection cycle")
            check(pair.get("coordinateTransformStatuses") == [5100, 5100], label + ": WCS to UCS conversions")
            for key in ("donorPickWcs", "targetPickWcs", "donorPickUcs", "targetPickUcs"):
                check(point(pair.get(key)), label + ": " + key)
            sentinel = "S1_UNREAD_1" if pair.get("id") == "code" else "S1_UNREAD_2"
            sentinel_native = donor_sentinel.get("nativeText", {})
            confirmed = (pair.get("sentinelText") == sentinel and sentinel != expected and pair.get("sentinelSetStatus") == 0
                         and sentinel_native.get("readable") is True and sentinel_native.get("status") == 0
                         and sentinel_native.get("getter") == "AcDbText::textString" and sentinel_native.get("value") == sentinel)
            check(pair.get("sentinelSetStatus") == 0 and confirmed and pair.get("sentinelConfirmed") is True, label + ": actual nonexpected sentinel confirmed before reverse")
            check(pair.get("reverseRegenStatus") == 5100 and pair.get("reverseCoordinateTransformStatuses") == [5100, 5100], label + ": regenerated reverse points converted to UCS")
            check(pair.get("reverseAllObjectsClosedBeforeCommand") is True
                  and pair.get("reverseActiveTransactionsBeforeCommand") == pair.get("reverseActiveTransactionsAfterCommand") == 0, label + ": no object/transaction across reverse command")
            check(pair.get("reverseTokens") == ["TTextReplace", "target point", "", "sentinel donor point", ""], label + ": documented reverse selection cycle")
            for key in ("reverseSourcePickWcs", "reverseDonorPickWcs", "reverseSourcePickUcs", "reverseDonorPickUcs"):
                check(point(pair.get(key)), label + ": " + key)
            for key, state in (("reverseSourcePickWcs", forward_after), ("reverseDonorPickWcs", donor_sentinel)):
                lo, hi = state.get("geometry", {}).get("min"), state.get("geometry", {}).get("max")
                check(point(lo) and point(hi) and near([(a+b)/2 for a, b in zip(lo, hi)], pair.get(key)), label + ": reverse pick recomputed from actual current bounds")
            reverse_completed = command_evidence(pair, True, target_handle, donor_handle, label + "/reverse")
            readback = pair["reverseReadback"]
            check(readback == donor_after.get("nativeText") and readback.get("readable") is True
                  and readback.get("getter") == "AcDbText::textString" and readback.get("status") == 0, label + ": actual reverse native readback bound to donor")
            actual_readback = (readback == donor_after.get("nativeText") and readback.get("readable") is True
                               and readback.get("getter") == "AcDbText::textString" and readback.get("status") == 0)
            saved_native = donor_saved.get("nativeText", {})
            exact_readback = (actual_readback and saved_native.get("readable") is True and saved_native.get("status") == 0
                              and saved_native.get("getter") == "AcDbText::textString"
                              and readback.get("value") == saved_native.get("value") == expected)
            semantic(exact_readback, label + ": sentinel replaced with exact expected content and persisted")
            check(pair.get("geometryChanged") == (before_geometry != after.get("geometry")), label + ": honest observed geometry change only")
            native_after, native_saved = after.get("nativeText", {}), saved.get("nativeText", {})
            for native in (native_after, native_saved):
                check((native.get("readable") is True and native.get("status") == 0 and native.get("getter") == "AcDbText::textString")
                      or (native.get("readable") is False and native.get("status") is None and native.get("value") is None
                          and native.get("getter") is None), label + ": no fabricated business text getter")
            known_mismatch = any(native.get("readable") is True and native.get("value") != expected for native in (native_after, native_saved))
            semantic(not known_mismatch, label + ": any readable target value must match donor")
            target_retained = after.get("geometry", {}).get("type") == saved.get("geometry", {}).get("type") == "TDbText"
            matched = forward_completed and reverse_completed and same_donor and confirmed and pair.get("sentinelConfirmed") is True and exact_readback and target_retained and not known_mismatch
            check(pair.get("roundtripMatched") is matched, label + ": roundtrip result follows measured evidence")
            roundtrip_results.append(matched)
            known_mismatches.append(known_mismatch or (readback.get("readable") is True and readback.get("value") != expected)
                                    or (saved_native.get("readable") is True and saved_native.get("value") != expected))
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
    classification = "command-bridge roundtrip mismatch" if any(known_mismatches) else "command-bridge roundtrip verified" if len(roundtrip_results) == 2 and all(roundtrip_results) else "pending command-bridge roundtrip verification"
    check(data.get("businessTextVerification") == classification, "business classification follows sentinel-protected roundtrip")
    semantic(classification == "command-bridge roundtrip verified", "both reverse bridges actually executed with exact readbacks")
    for message in failed:
        print("FAIL:", message)
    for message in semantic_failed:
        print("SEMANTIC FAIL:", message)
    total, failures = count+semantic_count, len(failed)+len(semantic_failed)
    print(f"structure={'FAIL' if failed else 'PASS'} semantic={'FAIL' if semantic_failed else 'PASS'}")
    print(f"assertions={total} passed={total-failures} failed={failures} result={'FAIL' if failures else 'PASS'}")
    print("Business text:", data.get("businessTextVerification"))
    print("Scope: sentinel-protected public command bridge content roundtrip only; font rendering, visual quality and arbitrary Tianzheng fields remain unverified.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_text_bridge_native.py <text-bridge-native.json>")
    raise SystemExit(main(Path(sys.argv[1])))
