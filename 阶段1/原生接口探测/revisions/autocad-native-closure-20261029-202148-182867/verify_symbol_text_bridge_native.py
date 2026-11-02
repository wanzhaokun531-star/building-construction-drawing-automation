"""Require separate native roundtrips for drawing-name and elevation classes."""
import json
import math
import sys
from pathlib import Path

FIXTURES = {
    "drawing-name": ("CFCE", "TDbDrawingName", "S1_TEST_NAME", "S1_SYMBOL_UNREAD_NAME", [40000, 28000, 0]),
    "elevation": ("CE8F", "TDbSymbElevation", "+0.123", "S1_SYMBOL_UNREAD_ELEV", [56000, 28000, 0]),
}


def near(a, b):
    if isinstance(a, dict):
        return isinstance(b, dict) and a.keys() == b.keys() and all(near(v, b[k]) for k, v in a.items())
    if isinstance(a, list):
        return isinstance(b, list) and len(a) == len(b) and all(near(x, y) for x, y in zip(a, b))
    if isinstance(a, (int, float)) and not isinstance(a, bool):
        return isinstance(b, (int, float)) and math.isfinite(a) and math.isfinite(b) and abs(a-b) <= 1e-6
    return a == b


def main(path):
    assertions, failures = 0, []

    def check(ok, label):
        nonlocal assertions
        assertions += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def snapshot(a, b, label):
        if not check(isinstance(a, dict) and isinstance(b, dict), label+": snapshots"):
            return
        check(near(a.get("geometry"), b.get("geometry")), label+": native geometry/type/handle")
        check(near(a.get("common"), b.get("common")), label+": common ownership/style")
        check(near(a.get("nativeText"), b.get("nativeText")), label+": native getter observation")

    def command(pair, reverse, source, target, label):
        prefix = "reverse" if reverse else ""
        key = lambda name: prefix+name[0].upper()+name[1:] if reverse else name
        good = check(pair.get(key("commandName")) == "TTextReplace" and pair.get(key("commandAttempted")) is True
                     and pair.get(key("commandStatus")) == 5100 and pair.get(key("commandError")) is None, label+": actual command call")
        events, inputs = pair.get(key("commandTrace"), []), pair.get(key("inputTrace"), [])
        check(isinstance(events, list) and isinstance(inputs, list), label+": actual trace arrays")
        trace = pair.get(key("trace"), {})
        detached = (trace.get("editorAdded") is True and trace.get("editorRemoved") is True
                    and trace.get("inputAddReturned") is True and trace.get("inputAddStatus") == 0
                    and trace.get("inputRemoveReturned") is True and trace.get("inputRemoveStatus") in (0, 33)
                    and trace.get("traceDropped") is False and trace.get("retained") is False)
        check(detached, label+": observers safely detached")
        balance, saw_start, ordered = 0, False, True
        global_balance, global_ordered = 0, True
        bad = False
        for event in events:
            name, action = str(event.get("command") or "").lstrip("_.").upper(), event.get("event")
            if action == "commandWillStart":
                global_balance += 1
            elif action in ("commandEnded", "commandCancelled", "commandFailed"):
                global_balance -= 1
                global_ordered = global_ordered and global_balance >= 0
            bad = bad or action in ("commandCancelled", "commandFailed", "unknownCommand")
            if name == "TTEXTREPLACE":
                if action == "commandWillStart":
                    saw_start, balance = True, balance+1
                elif action == "commandEnded":
                    balance -= 1
                    ordered = ordered and balance >= 0
        completed = saw_start and balance == global_balance == 0 and ordered and global_ordered and not bad
        check(completed, label+": observed complete noncancelled lifecycle")
        handles = []
        for event in inputs:
            selected = [event["selectedHandle"]] if event.get("selectedHandle") is not None else []
            selected += event.get("selectedHandles", [])
            for handle in selected:
                if handle is not None and (not handles or handles[-1] != handle):
                    handles.append(handle)
        selection_ok = all(h in (source, target) for h in handles) and (len(handles) < 2 or (handles[0] == source and handles[-1] == target))
        check(selection_ok, label+": observed selections limited to correct pair/order")
        actual = good and detached and completed and selection_ok
        check(pair.get("reverseCommandCompleted" if reverse else "forwardCommandCompleted") is actual, label+": honest command classification")
        return actual

    if not check(path.is_file(), "native compound-symbol text bridge evidence missing"):
        print("FAIL:", failures[0])
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    check(d.get("contract") == "stage1.symbol-text-bridge-native.v1" and d.get("api") == "ObjectARX native; no COM", "contract and native API")
    check(d.get("error") is None, "no infrastructure error")
    check(d.get("initialModelCount") == 213 and d.get("modelCountAfter") == d.get("reopenedModelCount") == 217, "full scheme213 plus exactly two native clones and two donors")
    check(d.get("extraModelEntityCount") == 0, "no hidden extra Model dependencies deleted or omitted")
    for key in ("saveStatus", "reopenStatus", "closeInputStatus"):
        check(d.get(key) == 0, key)
    for key in ("zoomStatus", "regenStatus"):
        check(d.get(key) == 5100, key)
    for key in ("controlDWG", "outputDWG"):
        p = Path(d.get(key, ""))
        check(p.is_file() and p.resolve().parent == path.resolve().parent, key+": isolated file")
    original = d.get("originalBefore", [])
    aa = {x.get("geometry", {}).get("handle"): x for x in original}
    check(len(original) == len(aa) == 213, "all original identities recorded")
    for stage in ("originalAfter", "originalSaved"):
        rows = d.get(stage, [])
        bb = {x.get("geometry", {}).get("handle"): x for x in rows}
        check(len(rows) == 213 and set(aa) == set(bb), stage+": complete original set")
        for h in aa.keys() & bb.keys():
            snapshot(aa[h], bb[h], stage+"/"+h)
    pairs = d.get("pairs", [])
    check(len(pairs) == 2 and {p.get("id") for p in pairs} == set(FIXTURES), "two distinct composite classes")
    new_handles, matches = set(), []
    for p in pairs:
        label = str(p.get("id"))
        if not check(label in FIXTURES, label+": known fixture"):
            continue
        sh, kind, expected, sentinel, wanted = FIXTURES[label]
        check((p.get("sourceHandle"), p.get("expectedNativeType"), p.get("expectedText"), p.get("sentinelText")) == (sh, kind, expected, sentinel), label+": fixed class/test values")
        check(p.get("caseError") is None, label+": no per-class infrastructure error")
        source = p.get("sourceOriginal", {})
        snapshot(aa.get(sh), source, label+"/protectedSource")
        check(source.get("geometry", {}).get("type") == kind, label+": actual source composite class")
        for key in ("cloneWasTransient", "cloneSameNativeType", "allObjectsClosedBeforeCommand", "reverseAllObjectsClosedBeforeCommand"):
            check(p.get(key) is True, label+": "+key)
        for key in ("appendTargetStatus", "transformStatus", "appendDonorStatus", "sentinelSetStatus"):
            check(p.get(key) == 0, label+": "+key)
        for key in ("activeTransactionsBeforeCommand", "activeTransactionsAfterCommand", "reverseActiveTransactionsBeforeCommand", "reverseActiveTransactionsAfterCommand"):
            check(p.get(key) == 0, label+": "+key)
        check(p.get("coordinateTransformStatuses") == p.get("reverseCoordinateTransformStatuses") == [5100, 5100], label+": real coordinate conversions")
        check(p.get("reverseRegenStatus") == 5100, label+": regenerated after sentinel change")
        stages = [p.get(k, {}) for k in ("targetBefore", "targetForwardAfter", "targetAfter", "targetSaved")]
        target_handles = {s.get("geometry", {}).get("handle") for s in stages}
        target_same = (len(target_handles) == 1 and all(s.get("geometry", {}).get("type") == kind
                       and s.get("geometry", {}).get("proxy") is False
                       and s.get("common", {}).get("sameDatabase") is True
                       and s.get("common", {}).get("ownerIsModel") is True for s in stages))
        check(target_same, label+": same genuine composite class/Model owner retained; no TDbText substitute")
        target_handle = stages[0].get("geometry", {}).get("handle")
        check(near(stages[0].get("geometry", {}).get("min"), wanted), label+": diagnostic position")
        delta = p.get("translation", [])
        if check(len(delta) == 3, label+": clone translation"):
            for key in ("min", "max"):
                source_point = source.get("geometry", {}).get(key, [])
                check(near([x+y for x, y in zip(source_point, delta)], stages[0].get("geometry", {}).get(key)), label+": actual cloned "+key)
            for key in ("grip", "stretch"):
                old = source.get("geometry", {}).get(key, {})
                new = stages[0].get("geometry", {}).get(key, {})
                check(old.get("status") == new.get("status") and near([[x+y for x, y in zip(point, delta)] for point in old.get("points", [])], new.get("points")), label+": actual cloned "+key)
        check(near(stages[0].get("common"), stages[2].get("common")), label+": target common style/ownership retained")
        snapshot(stages[1], stages[2], label+"/reverseSourceUnchanged")
        snapshot(stages[2], stages[3], label+"/persistedTarget")
        # Meaning comes from the independent live sentinel bridge. These are
        # observations of this fixed class/fixture, never a private-field writer.
        dxf_name = "TCH_DRAWINGNAME" if label == "drawing-name" else "TCH_ELEVATION"
        original_text = "一层平面图" if label == "drawing-name" else "0"
        raw_semantic_ok = True
        for stage_name, state, expected_text in (("source", source, original_text), ("before", stages[0], original_text),
                ("forward", stages[1], expected), ("after", stages[2], expected), ("saved", stages[3], expected)):
            raw = state.get("publicDxf") or {}
            rows = raw.get("stringGroups", [])
            identity = (raw.get("method") == "acdbEntGet" and raw.get("adsNameStatus") == 0
                        and raw.get("available") is True and raw.get("truncated") is False and isinstance(rows, list)
                        and [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 0] == [dxf_name]
                        and kind in [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 100])
            check(identity, label+"/"+stage_name+": complete native public DXF class identity")
            values = [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 1]
            text_matches = values == [expected_text]
            check(text_matches, label+"/"+stage_name+": unique public group1 agrees with independently bridged content")
            raw_semantic_ok = raw_semantic_ok and identity and text_matches
            if label == "drawing-name":
                scale_unchanged = [r.get("value") for r in rows if isinstance(r, dict) and r.get("code") == 2] == [""]
                check(scale_unchanged, label+"/"+stage_name+": original empty scale component stays empty; no scale-edit claim")
                raw_semantic_ok = raw_semantic_ok and scale_unchanged
        donor_stages = [p.get(k, {}) for k in ("donorBefore", "donorForwardAfter", "donorSentinel", "donorAfter", "donorSaved")]
        donor_handle = donor_stages[0].get("geometry", {}).get("handle")
        donor_same = (isinstance(donor_handle, str) and bool(donor_handle)
                      and all(s.get("geometry", {}).get("handle") == donor_handle and s.get("geometry", {}).get("type") == "AcDbText"
                              and s.get("geometry", {}).get("proxy") is False and s.get("common", {}).get("sameDatabase") is True
                              and s.get("common", {}).get("ownerIsModel") is True for s in donor_stages))
        check(donor_same, label+": same independent native donor through every sentinel/readback stage")
        for s, expected_text in zip(donor_stages, (expected, expected, sentinel, expected, expected)):
            text = s.get("nativeText", {})
            check(text.get("readable") is True and text.get("getter") == "AcDbText::textString" and text.get("status") == 0 and text.get("value") == expected_text, label+": actual donor/sentinel/readback content")
        snapshot(donor_stages[0], donor_stages[1], label+"/forwardSourceUnchanged")
        snapshot(donor_stages[3], donor_stages[4], label+"/persistedDonor")
        check(p.get("sentinelConfirmed") is True and sentinel != expected, label+": independent nonexpected sentinel confirmed")
        check(p.get("reverseReadback") == donor_stages[3].get("nativeText"), label+": exact readback bound to actual donor")
        identities_ok = isinstance(target_handle, str) and bool(target_handle) and target_handle != donor_handle
        for h in (target_handle, donor_handle):
            fresh = isinstance(h, str) and bool(h) and h not in aa and h not in new_handles
            check(fresh, label+": separate new identities")
            identities_ok = identities_ok and fresh
            new_handles.add(h)
        forward = command(p, False, donor_handle, target_handle, label+"/forward")
        reverse = command(p, True, target_handle, donor_handle, label+"/reverse")
        readback = p.get("reverseReadback", {})
        saved_text = donor_stages[4].get("nativeText", {})
        sentinel_native = donor_stages[2].get("nativeText", {})
        sentinel_ok = (p.get("sentinelConfirmed") is True and p.get("sentinelSetStatus") == 0
                       and sentinel_native.get("readable") is True and sentinel_native.get("getter") == "AcDbText::textString"
                       and sentinel_native.get("status") == 0 and sentinel_native.get("value") == sentinel and sentinel != expected)
        exact_readback = (readback == donor_stages[3].get("nativeText") and readback.get("readable") is True
                          and readback.get("getter") == "AcDbText::textString" and readback.get("status") == 0 and readback.get("value") == expected
                          and saved_text.get("readable") is True and saved_text.get("getter") == "AcDbText::textString"
                          and saved_text.get("status") == 0 and saved_text.get("value") == expected)
        direct_mismatch = any(s.get("nativeText", {}).get("readable") is True and s.get("nativeText", {}).get("value") != expected for s in stages[2:])
        actual_match = (forward and reverse and sentinel_ok and exact_readback and p.get("caseError") is None and not direct_mismatch
                        and target_same and donor_same and identities_ok and raw_semantic_ok)
        check(p.get("roundtripMatched") is actual_match, label+": honest semantic result")
        check(p.get("contentVerification") == ("command-bridge roundtrip verified" if actual_match else "not verified"), label+": per-class content label follows all actual evidence")
        check(actual_match, label+": this composite class actually supports exact public-command roundtrip")
        print("class="+label+" content="+("PASS" if actual_match else "FAIL")+" savedPublicText="+("PASS" if raw_semantic_ok else "FAIL"))
        matches.append(actual_match)
    for key in ("added", "savedAdded"):
        rows = d.get(key, [])
        inv = {s.get("geometry", {}).get("handle"): s for s in rows}
        check(len(rows) == len(inv) == 4 and set(inv) == new_handles, key+": complete four additions")
        for pair in pairs:
            for name in ("target", "donor"):
                state = pair.get(name+("Saved" if key == "savedAdded" else "After"), {})
                handle = state.get("geometry", {}).get("handle")
                snapshot(state, inv.get(handle), key+"/"+str(pair.get("id"))+"/"+name)
    check(d.get("businessTextVerification") == ("command-bridge roundtrip verified" if len(matches) == 2 and all(matches) else "not verified"), "overall classification follows both actual independent results")
    for failure in failures[:100]:
        print("FAIL:", failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: exact text roundtrip for two native composite classes only; elevation formatting/units and independent drawing-name scale fields remain unverified.")
    return int(bool(failures))


if __name__ == "__main__":
    try:
        raise SystemExit(main(Path(sys.argv[1])))
    except (OSError, ValueError, TypeError, KeyError, AttributeError, IndexError) as failure:
        print("FAIL: incomplete native symbol text bridge evidence:", failure)
        raise SystemExit(1)
