"""Verify v2 predeclared group pruning and scheme X positioning, not an axisCount.

Only the fixed close40 pair may form one group, and only when both are unwanted.
The expected group plan is derived from the frozen fixture before any step's
actual result is read. V1 protective failures remain a different contract.
"""
import json
import math
import sys
from pathlib import Path


TOL = 1e-6
SOURCE_XS = [35081.381329981, 38881.381329634, 40281.381329981,
             41881.381329981, 46081.381329981, 47481.381329981,
             47521.381329981, 49621.381329981, 53081.381329982]


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
        isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x) for x in p)


def bag(a, b):
    if len(a) != len(b):
        return False
    remaining = list(b)
    for p in a:
        i = next((i for i, q in enumerate(remaining) if near(p, q)), None)
        if i is None:
            return False
        remaining.pop(i)
    return not remaining


def unique(values):
    result = []
    for x in sorted(values):
        if not result or abs(x-result[-1]) > TOL:
            result.append(x)
    return result


def raw(e, mode):
    v = e.get(mode, {})
    values = v.get("points")
    if v.get("status") != 0 or not isinstance(values, list) or not all(point(p) for p in values):
        raise ValueError("missing actual " + mode + " point array")
    return values


def xs(e):
    return unique(p[0] for p in raw(e, "stretch"))


def circles(e, side):
    points = raw(e, "grip")
    if not points:
        return []
    y = (max if side == "top" else min)(p[1] for p in points)
    return [p for p in points if abs(p[1]-y) <= TOL]


def real_wall(e):
    c = e.get("curve")
    return (e.get("type") == "TDbWall" and e.get("proxy") is False and e.get("extentStatus") == 0 and isinstance(c, dict)
            and c.get("startStatus") == c.get("endStatus") == 0
            and point(c.get("start")) and point(c.get("end")) and point(e.get("min")) and point(e.get("max"))
            and abs(c["start"][0]-c["end"][0]) <= TOL and abs(c["start"][1]-c["end"][1]) > TOL
            and abs(e["max"][0]-e["min"][0]-200) <= TOL)


def window_seen(inputs, corners):
    returned = [v.get("pointOut") for v in inputs if v.get("event") == "endGetPoint" and v.get("returnStatus") == 5100]
    if near(returned[:2], corners):
        return True
    a, b = corners
    polygon = [[a[0], a[1], a[2]], [b[0], a[1], a[2]], [b[0], b[1], b[2]], [a[0], b[1], b[2]]]
    return any(v.get("event") == "beginSSGet" and isinstance(v.get("points"), list)
               and (bag(v["points"], corners) or bag(v["points"], polygon)) for v in inputs)


def main(path):
    count, failed = 0, []

    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failed.append(label)
        return bool(ok)

    def same(a, b, label):
        if not check(isinstance(a, dict) and isinstance(b, dict), label + ": records"):
            return
        for k in ("handle", "type", "proxy", "layer", "extentStatus", "min", "max", "curve", "grip", "stretch"):
            check(k in a and k in b and near(a[k], b[k]), label + ": " + k)
        check(b.get("proxy") is False, label + ": native")

    if not check(path.is_file(), "native axis-template-prune host evidence missing"):
        print("FAIL:", failed[0]); print("assertions=1 passed=0 failed=1 result=FAIL"); return 1
    d = json.loads(path.read_text(encoding="utf-8-sig")); run = path.resolve().parent
    check(d.get("contract") == "stage1.axis-template-prune-native.v2", "v2 grouped-deletion contract")
    check(d.get("api") == "ObjectARX native; no COM", "native object API")
    n = d.get("requestedStationCount")
    if not check(isinstance(n, int) and not isinstance(n, bool) and n in (2, 3), "exact two/three public station fixture"):
        n = 2
    check(d.get("entrypoint") == ("S1AXISTEMPLATETWO" if n == 2 else "S1AXISTEMPLATETHREE"), "entrypoint")
    check(d.get("error") is None, "native operation error: " + str(d.get("error")))
    check(d.get("pruneCompleted") is True, "requested template pruning completed")
    check(d.get("positioningCompleted") is True, "requested scheme positioning completed")
    check(d.get("numberingAttempted") is False, "numbering deliberately outside this fixture")
    for key in ("cloneStatus", "sourceReadStatus", "sourceCloseInputStatus", "saveStatus", "firstReaderStatus", "firstReaderCloseInputStatus", "reopenStatus", "closeInputStatus"):
        check(d.get(key) == 0, key)
    check(d.get("readerDestroyedBeforeFinalRead") is True and d.get("hostDatabaseDestroyed") is False, "independent reader lifetime, host ownership")
    for key in ("initialModelCount",): check(d.get(key) == 213, key)
    for key in ("postCloneModelCount", "finalModelCount", "reopenedModelCount", "firstReaderModelCount"): check(d.get(key) == 214, key)
    check(Path(d.get("controlDWG", "")).resolve() == run / "control.dwg", "owned control path")
    output = Path(d.get("outputDWG", ""))
    check(output.resolve() == run / f"axis-template-prune-{n}-generated.dwg" and output.is_file() and output.stat().st_size > 0, "isolated actual output")
    frozen = Path(__file__).resolve().parents[2] / "阶段0" / "输入快照"
    for key, name in (("schemeInputPath", "方案平面图.dwg"), ("referenceInputPath", "施工图平面参考图.dwg")):
        check(Path(d.get(key, "")).resolve() == frozen / name, key)
    sets = [d.get(k, []) for k in ("originalBefore", "originalAfter", "originalSaved")]
    originals = {}
    if check(all(len(v) == 213 for v in sets), "213 originals in three states"):
        maps = [{e.get("handle"): e for e in v} for v in sets]
        if check(all(len(v) == 213 for v in maps) and maps[0].keys() == maps[1].keys() == maps[2].keys(), "original identities"):
            originals = maps[0]
            for h, e in originals.items():
                same(e, maps[1][h], "original-after/" + h); same(e, maps[2][h], "original-saved/" + h)
    primary = d.get("primaryHandle"); mapping = d.get("cloneMapping", {})
    check(isinstance(primary, str) and primary not in originals and mapping.get("sourceHandle") == "199F"
          and mapping.get("targetHandle") == primary and mapping.get("isCloned") is True, "real199F primary mapping")
    # Protective native stops legitimately leave later stages as JSON null.
    # Keep those stages failed/unexecuted, without aborting the whole report.
    before, pruned, translated, after, saved = [v if isinstance(v, dict) else {} for v in
        (d.get(k) for k in ("before", "afterPrune", "afterTranslation", "after", "saved"))]
    for key, e in (("before", before), ("pruned", pruned), ("translated", translated), ("after", after), ("saved", saved)):
        check(e.get("handle") == primary and e.get("type") == "TDbAxisLabelSet" and e.get("proxy") is False, key + ": native primary")
        check(e.get("layer") == before.get("layer") and e.get("curve") == before.get("curve"), key + ": layer and noncurve kind preserved")
    source = d.get("sourceTemplate", {})
    same(source, d.get("sourceTemplateAfterClone"), "source template unchanged")
    check(source.get("handle") == "199F" and source.get("type") == "TDbAxisLabelSet" and near(xs(source), SOURCE_XS) and near(xs(before), SOURCE_XS)
          and len(raw(source, "grip")) == 41 and len(raw(source, "stretch")) == 36, "actual bound nine-station source")
    for k in ("min", "max", "grip", "stretch"):
        check(near(source.get(k), before.get(k)), "clone actual source " + k)
    check(any(v.get("type") == "AcDbLayerTableRecord" and v.get("sourceName") == source.get("layer") and v.get("targetName") == before.get("layer")
              for v in d.get("resourceMapping", [])), "actual mapped primary layer")
    keep = [SOURCE_XS[0], SOURCE_XS[-1]] if n == 2 else [SOURCE_XS[0], SOURCE_XS[3], SOURCE_XS[-1]]
    check(near(d.get("sourceKeepXs"), keep), "approved template keep stations")
    unwanted = [x for x in SOURCE_XS if not any(abs(x-k) <= TOL for k in keep)]
    pair = [SOURCE_XS[5], SOURCE_XS[6]]
    grouped = all(any(abs(x-v) <= TOL for v in unwanted) for x in pair)
    expected_groups = []
    for x in unwanted:
        if grouped and abs(x-pair[1]) <= TOL:
            continue
        expected_groups.append(pair if grouped and abs(x-pair[0]) <= TOL else [x])
    check(near(d.get("declaredGroupTargets"), expected_groups), "fixed group plan declared before command outcomes")
    check(all(not any(abs(x-k) <= TOL for k in keep) for group in expected_groups for x in group), "no kept station included in any group")
    steps = d.get("deleteSteps", [])
    check(len(steps) == len(expected_groups), "every predeclared group has exactly one deletion step")
    previous = before
    for i, step in enumerate(steps):
        label = f"delete/{i}"
        a, b, x = step.get("before", {}), step.get("after", {}), step.get("targetX")
        group = expected_groups[i] if i < len(expected_groups) else []
        same(previous, a, label + "/continuity")
        check(bool(group) and near(x, group[0]) and near(step.get("expectedDeleteXs"), group), label + ": only the predeclared unwanted group")
        check(all(any(abs(v-g) <= TOL for v in xs(a)) for g in group), label + ": full expected group present before command")
        wanted = [v for v in xs(a) if not any(abs(v-g) <= TOL for g in group)]
        check(len(xs(b)) == len(xs(a))-len(group) and near(xs(b), wanted), label + ": exactly the predeclared public station group removed")
        for mode in ("grip", "stretch"):
            check(bag([p for p in raw(a, mode) if not any(abs(p[0]-g) <= TOL for g in group)], raw(b, mode)), label + ": only the entire predeclared group's points removed")
        attempts = step.get("attempts", [])
        check(1 <= len(attempts) <= 2, label + ": at most two visible-side windows")
        attempt_before = a
        seen_sides = set()
        for j, attempt in enumerate(attempts):
            tag = label + f"/attempt{j}"
            same(attempt_before, attempt.get("before"), tag + "/continuity")
            side, circle = attempt.get("side"), attempt.get("centerWCS")
            check(side in ("top", "bottom") and side not in seen_sides, tag + ": each visible side attempted at most once"); seen_sides.add(side)
            candidates = circles(attempt_before, side)
            check(point(circle) and any(near(circle, p) for p in candidates) and near(circle[0], x), tag + ": actual visible circle at requested X")
            check(near(attempt.get("expectedDeleteXs"), group), tag + ": attempt preserves the predeclared group")
            check(all(any(abs(p[0]-g) <= TOL for g in group) or abs(p[0]-x) > 900 for p in candidates), tag + ": no same-side circle outside the declared group in window")
            world = [[circle[0]-450, circle[1]-450, circle[2]], [circle[0]+450, circle[1]+450, circle[2]]]
            check(near(attempt.get("worldPoints"), world) and attempt.get("transformStatuses") == [5100, 5100], tag + ":450 WCS window converted")
            check(all(any(abs(p[0]-g) <= TOL and world[0][0]-TOL <= p[0] <= world[1][0]+TOL
                          and world[0][1]-TOL <= p[1] <= world[1][1]+TOL and abs(p[2]-circle[2]) <= TOL
                          for p in raw(attempt_before, "stretch")) for g in group), tag + ": window covers a public control for every declared X")
            check(attempt.get("commandAttempted") is True and attempt.get("commandStatus") == 5100 and attempt.get("suppliedReorderAnswer") == "N", tag + ": one TDelLabel/N command")
            check(attempt.get("activeTransactionsBeforeCommand") == attempt.get("activeTransactionsAfterCommand") == 0 and attempt.get("error") is None, tag + ": command lifecycle")
            commands, inputs, trace = attempt.get("commandTrace", []), attempt.get("inputTrace", []), attempt.get("trace", {})
            for event in ("commandWillStart", "commandEnded"):
                check(any(v.get("event") == event and (v.get("command") or "").upper() == "TDELLABEL" for v in commands), tag + ": " + event)
            for event in commands:
                if (event.get("command") or "").upper() == "TDELLABEL":
                    check(event.get("isApplicationContext") is False
                          and all(Path(event.get(k, "")).resolve() == run / "control.dwg" for k in ("currentDWG", "mdiActiveDWG")), tag + ": actual command document identity")
            check(not any(v.get("event") in ("commandFailed", "commandCancelled", "unknownCommand") for v in commands), tag + ": no failure event")
            selected = [v.get("selectedHandles") for v in inputs if v.get("event") == "endSSGet" and v.get("returnStatus") == 5100]
            check(bool(selected) and all(v == [primary] for v in selected), tag + ": actual primary-only selection")
            check(window_seen(inputs, attempt.get("userPoints", [])), tag + ": actual window consumed")
            check(any("重排轴号" in (v.get("prompt") or "") for v in inputs), tag + ": real reorder prompt")
            check(trace.get("editorRemoved") is True and trace.get("inputAddStatus") == trace.get("inputRemoveStatus") == 0
                  and trace.get("traceDropped") is False and trace.get("retained") is False, tag + ": observers safely removed")
            attempt_before = attempt.get("after", {})
        same(attempt_before, b, label + "/last-attempt-result"); previous = b
    same(previous, pruned, "last-deletion matches afterPrune")
    check(near(xs(pruned), keep), "exact retained template station set")
    for side in ("top", "bottom"):
        check(near(unique(p[0] for p in circles(pruned, side)), keep), "all retained circles visible on " + side)
    left, right = originals.get("CDB4", {}), originals.get("CDB0", {})
    check(real_wall(left) and real_wall(right), "two genuine vertical200 scheme walls")
    lo, hi = ((left.get("min", [0])[0]+left.get("max", [0])[0])/2, (right.get("min", [0])[0]+right.get("max", [0])[0])/2)
    check(lo < hi, "scheme control walls ordered")
    target = [lo, hi]
    if n == 3:
        candidate_xs = unique((e["min"][0]+e["max"][0])/2 for e in originals.values() if real_wall(e) and lo+TOL < (e["min"][0]+e["max"][0])/2 < hi-TOL)
        if check(bool(candidate_xs), "actual interior200 wall candidates"):
            middle = min(candidate_xs, key=lambda x: (abs(x-(lo+hi)/2), x)); target = [lo, middle, hi]
    check(near(d.get("targetStationXs"), target), "positions independently follow real scheme wall centers")
    controls = d.get("targetWallControls", [])
    check(len(controls) == n, "each station has actual scheme-wall provenance")
    for i, c in enumerate(controls):
        e = originals.get(c.get("handle"), {})
        check(real_wall(e) and near(c.get("centerX"), target[i]) and near(c.get("entity"), e), "target wall control bound to original geometry")
    translation = d.get("translation"); translation = translation if isinstance(translation, dict) else {}; dx = lo-keep[0]
    check(near(translation.get("offset"), [dx, 0, 0]) and translation.get("openStatus") == translation.get("moveStatus") == translation.get("transactionStatus") == 0, "actual initial rigid translation")
    if translated:
        for mode in ("grip", "stretch"):
            check(bag([[p[0]+dx, p[1], p[2]] for p in raw(pruned, mode)], raw(translated, mode)), "translation actual " + mode)
    else:
        check(False, "translation stage unexecuted after protective stop")
    moves = d.get("stretchMoves", []); check(len(moves) == n, "one current four-point group per retained station")
    previous = translated; handled = set()
    for move in moves:
        a, b, i = move.get("before", {}), move.get("after", {}), move.get("mappingIndex")
        same(previous, a, "stretch continuity")
        if not check(isinstance(i, int) and 0 <= i < n and i not in handled, "unique target mapping index"):
            continue
        handled.add(i); current_x = keep[i]+dx; offset = target[i]-current_x
        check(near(move.get("currentX"), current_x) and near(move.get("targetX"), target[i]) and near(move.get("offset"), [offset, 0, 0]), "actual group maps to selected scheme wall")
        indices = [k for k, p in enumerate(raw(a, "stretch")) if abs(p[0]-current_x) <= TOL]
        check(len(indices) == 4 and move.get("indices") == indices, "four actual current stretch indices")
        check(not any(abs(x-target[i]) <= TOL and abs(x-current_x) > TOL for x in xs(a)), "target station unoccupied before move")
        if abs(offset) > TOL:
            check(move.get("moveAttempted") is True and move.get("moveStatus") == move.get("openStatus") == move.get("transactionStatus") == 0, "native nonzero stretch succeeded")
        else:
            check(move.get("moveAttempted") is False and move.get("moveStatus") is None, "zero group not reported as an unexecuted eOk")
        for mode in ("grip", "stretch"):
            expected = [[p[0]+(offset if abs(p[0]-current_x) <= TOL else 0), p[1], p[2]] for p in raw(a, mode)]
            check(bag(expected, raw(b, mode)), "only selected group moved in " + mode)
        previous = b
    same(previous, after, "last stretch matches final state"); same(after, saved, "final state persists")
    check(near(xs(after), target) and near(xs(saved), target), "exact two/three real-scheme station X positions")
    for side in ("top", "bottom"):
        check(near(unique(p[0] for p in circles(after, side)), target), "both sides visible after positioning: " + side)
    check(len(raw(after, "stretch")) == 4*n and len(raw(after, "grip")) == 6*n+2, "complete public point pattern for retained visible stations")
    for mode in ("grip", "stretch"):
        expected = []
        for p in raw(pruned, mode):
            i = next((i for i, x in enumerate(keep) if abs(p[0]-x) <= TOL), None)
            expected.append([target[i] if i is not None else p[0]+dx, p[1], p[2]])
        check(bag(expected, raw(after, mode)), "final full point geometry independent of step results: " + mode)
    for k in ("min", "max"):
        v = pruned.get(k); expected_x = v[0]+dx if k == "min" else target[-1]+400
        check(near(after.get(k), [expected_x, v[1], v[2]]), "circle/vertical envelope preserved: " + k)
    for key, e in (("addedAfterClone", before), ("addedAfterPrune", pruned), ("added", after), ("savedAdded", saved)):
        actual = d.get(key); check(isinstance(actual, list) and len(actual) == 1 and near(actual[0], e), key + ": actual full Model additions")
    for failure in failed[:100]: print("FAIL:", failure)
    print(f"assertions={count} passed={count-len(failed)} failed={len(failed)} result={'FAIL' if failed else 'PASS'}")
    print("Scope: v2 fixed group plan and X positioning only; v1 failures remain unchanged; public stations are not an internal axisCount; numbering/Y range/full grid/relations/rendering unverified.")
    return int(bool(failed))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_axis_template_prune_native.py <run/axis-template-prune-native.json>")
    try:
        result = main(Path(sys.argv[1]))
    except (OSError, ValueError, TypeError, AttributeError, KeyError, IndexError) as error:
        print("FAIL: unreadable or incomplete axis-template-prune evidence:", error); result = 1
    raise SystemExit(result)
