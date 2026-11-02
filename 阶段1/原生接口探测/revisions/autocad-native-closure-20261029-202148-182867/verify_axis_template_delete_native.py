"""Verify one documented TDelLabel trial; visible removal and stored X stations differ.

Removing only the visible top circle must remain Partial/FAIL, even when the
command returns RTNORM and the result survives reopening. Counts below describe
public grip/stretch observations, never a private Tianzheng axisCount field.
"""
import json
import math
import sys
from pathlib import Path


TOL = 1e-6
SOURCE_XS = [35081.381329981, 38881.381329634, 40281.381329981,
             41881.381329981, 46081.381329981, 47481.381329981,
             47521.381329981, 49621.381329981, 53081.381329982]
TARGET_X = SOURCE_XS[1]
TOP_Y = 36057.750468548


def near(a, b):
    if isinstance(a, bool) or isinstance(b, bool):
        return type(a) is type(b) and a == b
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return math.isfinite(a) and math.isfinite(b) and abs(a-b) <= TOL
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(near(x, y) for x, y in zip(a, b))
    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(near(a[k], b[k]) for k in a)
    return type(a) is type(b) and a == b


def point(p):
    return isinstance(p, list) and len(p) == 3 and all(
        isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in p)


def unique_xs(points):
    result = []
    for p in sorted(points, key=lambda p: p[0]):
        if not result or abs(p[0]-result[-1]) > TOL:
            result.append(p[0])
    return result


def same_point_multiset(a, b):
    if len(a) != len(b):
        return False
    remaining = list(b)
    for p in a:
        match = next((i for i, q in enumerate(remaining) if near(p, q)), None)
        if match is None:
            return False
        remaining.pop(match)
    return not remaining


def window_consumed(inputs, selection):
    returned = [v.get("pointOut") for v in inputs if v.get("event") == "endGetPoint" and v.get("returnStatus") == 5100]
    user = selection.get("userPoints")
    if isinstance(user, list) and len(user) == 2 and near(returned[:2], user):
        return True
    # AcEdInputContextReactor also has GetCorner callbacks, which the existing
    # shared owner does not override. Its actual SSGet input polygon can still
    # independently show both window corners; do not invent a GetCorner event.
    for corners in (user, selection.get("worldPoints")):
        if not isinstance(corners, list) or len(corners) != 2 or not all(point(p) for p in corners):
            continue
        a, b = corners
        rectangle = [[a[0], a[1], a[2]], [b[0], a[1], a[2]], [b[0], b[1], b[2]], [a[0], b[1], b[2]]]
        for event in inputs:
            actual = event.get("points")
            if event.get("event") != "beginSSGet" or not isinstance(actual, list) or not all(point(p) for p in actual):
                continue
            if same_point_multiset(actual, corners) or same_point_multiset(actual, rectangle):
                return True
    return False


def public_observation(entity):
    """Independent derivation from actual raw point arrays, not reported counts."""
    if not isinstance(entity, dict):
        return None
    grip, stretch = entity.get("grip", {}), entity.get("stretch", {})
    g, s = grip.get("points"), stretch.get("points")
    if (grip.get("status") != 0 or stretch.get("status") != 0
            or not isinstance(g, list) or not g or not all(point(p) for p in g)
            or not isinstance(s, list) or not s or not all(point(p) for p in s)):
        return None
    top, bottom = max(p[1] for p in g), min(p[1] for p in g)
    return {"unionXs": unique_xs(s), "gripXs": unique_xs(g),
            "gripPointCount": len(g), "stretchPointCount": len(s),
            "topGripY": top, "bottomGripY": bottom,
            "visibleTopCircleCandidates": [p for p in g if abs(p[1]-top) <= TOL],
            "visibleBottomCircleCandidates": [p for p in g if abs(p[1]-bottom) <= TOL]}


def classify(before, after, target_x, circle):
    a, b = public_observation(before), public_observation(after)
    if a is None or b is None:
        return False, False, "Fail"
    visible = (any(near(p, circle) for p in before["grip"]["points"])
               and not any(near(p, circle) for p in after["grip"]["points"])
               and not near(before["grip"]["points"], after["grip"]["points"]))
    wanted = [x for x in a["unionXs"] if abs(x-target_x) > TOL]
    stored = (len(a["unionXs"]) == 9 and len(b["unionXs"]) == 8
              and near(b["unionXs"], wanted)
              and not any(abs(p[0]-target_x) <= TOL for p in after["stretch"]["points"]))
    return visible, stored, "Pass" if visible and stored else "Partial" if visible else "Fail"


def main(path):
    checks, failures = 0, []

    def check(ok, label):
        nonlocal checks
        checks += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def compact(a, b, label, same_handle=True):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": compact records"):
            return
        check(a.get("type") == b.get("type") and b.get("proxy") is False, label + ": native type")
        if same_handle:
            check(a.get("handle") == b.get("handle"), label + ": handle")
        for key in ("layer", "extentStatus", "min", "max", "curve", "grip", "stretch"):
            check(key in a and key in b and near(a[key], b[key]), label + ": " + key)

    if not check(path.is_file(), "native axis-template-delete host evidence missing"):
        print("FAIL:", failures[0])
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    run = path.resolve().parent
    check(d.get("contract") == "stage1.axis-template-delete-native.v1", "versioned contract")
    check(d.get("api") == "ObjectARX native; no COM" and d.get("commandApi") == "acedCommandS", "native API")
    check(d.get("entrypoint") == "S1AXISTEMPLATEDELETE" and d.get("commandName") == "TDelLabel", "documented public command")
    check(d.get("operation") == "delete-one-visible-top-interior-label-no-renumber", "only one deletion operation")
    check(d.get("error") is None, "no native operation error")
    for key in ("cloneStatus", "sourceReadStatus", "sourceCloseInputStatus", "saveStatus",
                "firstReaderStatus", "firstReaderCloseInputStatus", "reopenStatus", "closeInputStatus"):
        check(d.get(key) == 0, "actual status: " + key)
    check(d.get("commandStatus") == d.get("zoomStatus") == 5100, "command returns recorded; effects checked separately")
    check(d.get("readerDestroyedBeforeFinalRead") is True, "first reader destroyed before final new reader")
    check(d.get("hostDatabaseDestroyed") is False, "host retains ownership of document database")
    check(d.get("activeTransactionsBeforeCommand") == d.get("activeTransactionsAfterCommand") == 0, "no transaction across command")
    for key, expected in (("controlDWG", run / "control.dwg"), ("outputDWG", run / "axis-template-delete-generated.dwg")):
        value = d.get(key)
        check(isinstance(value, str) and Path(value).resolve() == expected, key + ": isolated exact path")
    output = Path(d.get("outputDWG", ""))
    check(output.is_file() and output.stat().st_size > 0, "actual nonempty isolated output")
    frozen = Path(__file__).resolve().parents[2] / "阶段0" / "输入快照"
    for key, name in (("schemeInputPath", "方案平面图.dwg"), ("referenceInputPath", "施工图平面参考图.dwg")):
        check(Path(d.get(key, "")).resolve() == frozen / name, "frozen input: " + key)
    check(d.get("initialModelCount") == 213, "complete scheme213 before cloning")
    check(d.get("postCloneModelCount") == d.get("finalModelCount") == d.get("firstReaderModelCount")
          == d.get("reopenedModelCount") == 214, "same one added native collection and original213")
    primary = d.get("primaryHandle")
    mapping = d.get("cloneMapping", {})
    check(isinstance(primary, str) and bool(primary) and mapping.get("sourceHandle") == "199F"
          and mapping.get("targetHandle") == primary and mapping.get("isCloned") is True
          and mapping.get("type") == "TDbAxisLabelSet", "real source-to-primary clone mapping")
    source, source_after = d.get("sourceTemplate"), d.get("sourceTemplateAfterClone")
    compact(source, source_after, "source template remains unchanged")
    before, after, saved = [d.get(k) for k in ("before", "after", "saved")]
    for key, e in (("before", before), ("after", after), ("saved", saved)):
        check(isinstance(e, dict) and e.get("handle") == primary and e.get("type") == "TDbAxisLabelSet"
              and e.get("proxy") is False, key + ": same actual native collection")
        check(d.get(key + "ReadStatus") == 0, key + ": actual successful read")
        actual = public_observation(e)
        check(actual is not None and near(actual, d.get("observed" + key.title())), key + ": actual public observations independently derived")
    if isinstance(source, dict) and isinstance(before, dict):
        check(source.get("handle") == "199F" and source.get("type") == "TDbAxisLabelSet", "actual reference source identity")
        for key in ("type", "proxy", "extentStatus", "min", "max", "curve", "grip", "stretch"):
            check(key in source and near(source[key], before.get(key)), "clone actual source geometry: " + key)
        check(any(v.get("type") == "AcDbLayerTableRecord" and v.get("sourceName") == source.get("layer")
                  and v.get("targetName") == before.get("layer") for v in d.get("resourceMapping", [])),
              "actual primary layer follows the source-to-target resource mapping")
    check(isinstance(before, dict) and isinstance(after, dict) and isinstance(saved, dict)
          and before.get("layer") == after.get("layer") == saved.get("layer"), "deletion keeps the actual target layer")
    compact(after, saved, "changed collection persists")
    for key in ("addedAfterClone", "addedAfterCommand", "savedAdded"):
        values = d.get(key)
        expected = before if key == "addedAfterClone" else after if key == "addedAfterCommand" else saved
        check(isinstance(values, list) and len(values) == 1 and near(values[0], expected), key + ": full actual additions")
    original_sets = [d.get(k) for k in ("originalBefore", "originalAfter", "originalSaved")]
    if check(all(isinstance(v, list) and len(v) == 213 for v in original_sets), "all213 compact records in three states"):
        maps = [{v.get("handle"): v for v in seq} for seq in original_sets]
        if check(all(len(v) == 213 for v in maps) and maps[0].keys() == maps[1].keys() == maps[2].keys()
                 and primary not in maps[0], "all original identities protected"):
            for handle, record in maps[0].items():
                compact(record, maps[1][handle], "original-after/" + str(handle))
                compact(record, maps[2][handle], "original-saved/" + str(handle))
    selection = d.get("selectionWindow", {})
    circle = selection.get("centerWCS")
    check(point(circle) and near(circle, [TARGET_X, TOP_Y, 0]), "actual second-station visible top circle fixture")
    observed = public_observation(before)
    if observed is not None and point(circle):
        check(near(observed["unionXs"], SOURCE_XS) and observed["gripPointCount"] == 41
              and observed["stretchPointCount"] == 36, "fixed observed template public baseline")
        check(any(near(circle, p) for p in observed["visibleTopCircleCandidates"]), "selected center really occurs in top grip candidates")
        check(near(selection.get("targetStationX"), observed["unionXs"][1]), "second interior station derived from actual stretch union")
        check(near(selection.get("observedRadius"), 400) and near(before.get("max", [0, 0, 0])[1]-circle[1], 400), "400 radius supported by actual bounds and point pattern")
        check(any(near(p, [circle[0], circle[1]-400, circle[2]]) for p in before["grip"]["points"]), "actual lower circumference grip")
        check(near(selection.get("halfSize"), 450), "exact450 window around one circle")
        world = [[circle[0]-450, circle[1]-450, circle[2]], [circle[0]+450, circle[1]+450, circle[2]]]
        check(near(selection.get("worldPoints"), world), "two world corners match the one-circle window")
        check(min(abs(x-circle[0]) for x in observed["unionXs"] if abs(x-circle[0]) > TOL) > 900, "window excludes neighbouring stored X stations")
    check(selection.get("transformStatuses") == [5100, 5100]
          and isinstance(selection.get("userPoints"), list) and len(selection["userPoints"]) == 2
          and all(point(p) for p in selection["userPoints"]), "both WCS corners converted to UCS")
    check(d.get("commandAttempted") is True and d.get("requestedReorderAnswer") == d.get("suppliedReorderAnswer") == "N" and d.get("batchDeletion") is False
          and d.get("otherAxisMovementAttempted") is False, "no renumber command/batch/movement requested")
    trace = d.get("trace", {})
    check(trace.get("editorAdded") is True and trace.get("editorRemoved") is True
          and trace.get("inputAddStatus") == trace.get("inputRemoveStatus") == 0,
          "native observers actually installed and removed")
    check(trace.get("traceDropped") is False and trace.get("retained") is False, "complete trace with no retained callbacks")
    commands, inputs = d.get("commandTrace", []), d.get("inputTrace", [])
    for event in ("commandWillStart", "commandEnded"):
        check(any(v.get("event") == event and (v.get("command") or "").upper() == "TDELLABEL" for v in commands), "actual TDelLabel " + event)
    check(not any(v.get("event") in ("commandCancelled", "commandFailed", "unknownCommand") for v in commands), "no observed command cancellation/failure")
    check(not any((v.get("command") or "").upper() in ("TREAXISNO", "TADDLABEL", "TSHOWLABEL") for v in commands), "no separate renumber/add/hide command")
    check(any("框选轴号" in (v.get("prompt") or "") for v in inputs), "real deletion window prompt observed")
    check(any("重排轴号" in (v.get("prompt") or "") for v in inputs), "real reorder question observed")
    reorder_begin = next((v for v in inputs if "重排轴号" in (v.get("prompt") or "")), None)
    answers = [v for v in inputs if reorder_begin and v.get("sequence", -1) > reorder_begin.get("sequence", -1)
               and v.get("event") in ("endGetKeyword", "endGetString")]
    check(bool(answers) and answers[0].get("returnStatus") in (5100, -5005), "reorder question actually completed with a response")
    if answers:
        answer = answers[0].get("keyword") if answers[0].get("event") == "endGetKeyword" else answers[0].get("value")
        # The shared recorder omits normal-status keyword text. Preserve this
        # limit; never turn a supplied N into a fabricated native readback.
        check(answer is None or answer.upper() == "N", "any observed reorder response agrees with supplied N")
    selections = [v for v in inputs if v.get("event") == "endSSGet" and v.get("returnStatus") == 5100]
    check(bool(selections) and all(v.get("selectedHandles") == [primary] for v in selections), "actual selected collection only; no original selected")
    check(window_consumed(inputs, selection), "actual point returns or SSGet input geometry proves the exact supplied window")
    visible, stored, outcome = classify(before, after, TARGET_X, circle)
    saved_visible, saved_stored, saved_outcome = classify(before, saved, TARGET_X, circle)
    result = d.get("result", {})
    check(result.get("visibleMarkerRemoved") is visible and result.get("storedStationXsReduced") is stored
          and result.get("status") == outcome, "reported two-dimensional outcome agrees with raw observations")
    check((visible, stored, outcome) == (saved_visible, saved_stored, saved_outcome), "both result dimensions persist")
    check(visible, "visible marker removed: actual selected circle grip disappears")
    check(stored, "stored public stretch-X stations9->8 with only selected X removed")
    if isinstance(before, dict) and isinstance(after, dict):
        for mode in ("grip", "stretch"):
            a = [p for p in before.get(mode, {}).get("points", []) if abs(p[0]-TARGET_X) > TOL]
            b = [p for p in after.get(mode, {}).get("points", []) if abs(p[0]-TARGET_X) > TOL]
            check(same_point_multiset(a, b), mode + ": every other public point and multiplicity unchanged")
    for label in failures[:80]:
        print("FAIL:", label)
    print(f"visible_marker_result={'PASS' if visible else 'FAIL'} stored_station_result={'PASS' if stored else 'FAIL'} outcome={outcome.upper()}")
    print(f"assertions={checks} passed={checks-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: one template deletion only; public derived stations are not an internal axisCount; no2/3-axis generation or rendered-label approval.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_axis_template_delete_native.py <run/axis-template-delete-native.json>")
    try:
        result = main(Path(sys.argv[1]))
    except (OSError, ValueError, TypeError, AttributeError, KeyError, IndexError) as failure:
        print("FAIL: unreadable or incomplete native axis-template-delete evidence:", failure)
        result = 1
    raise SystemExit(result)
