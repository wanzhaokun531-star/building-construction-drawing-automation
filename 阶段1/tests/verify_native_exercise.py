"""Check concrete native grip edits and their persisted geometry."""
import json
import math
import sys
from pathlib import Path


def vector(v):
    return isinstance(v, list) and len(v) == 3 and all(isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x) for x in v)


def near(a, b):
    return vector(a) and vector(b) and all(abs(x-y) <= 1e-6 for x, y in zip(a, b))


def same_points(a, b):
    return isinstance(a, list) and isinstance(b, list) and len(a) == len(b) and all(near(x, y) for x, y in zip(a, b))


def main(path, expected=None, contract="stage1.native-exercise.v1", modes=None):
    failures = []
    count = 0

    def require(ok, message):
        nonlocal count
        count += 1
        if not ok:
            failures.append(message)
        return ok

    if require(path.is_file(), f"native exercise result missing: {path}"):
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        require(data.get("contract") == contract, "incorrect native exercise contract")
        require(data.get("api") == "ObjectARX native; no COM", "object operations must use native API")
        expected = expected or {
            "dimension-grip-0": ("19A0", "TDbDimension2", 0, [0, -200, 0]),
            "dimension-measure-grip-3": ("19A0", "TDbDimension2", 3, [500, 0, 0]),
            "axis-grip-1": ("199F", "TDbAxisLabelSet", 1, [0, 200, 0]),
        }
        cases = data.get("cases")
        if require(isinstance(cases, list) and len(cases) == len(expected), "expected independent edit cases required"):
            seen = []
            for case in cases:
                name = case.get("id")
                if not require(name in expected, "unexpected case"):
                    continue
                seen.append(name)
                handle, entity_type, index, offset = expected[name]
                point_kind = "stretch" if modes and modes[name] == "stretch" else "grip"
                if modes:
                    require(case.get("mode") == modes[name], f"{name}: incorrect native editing method")
                require(case.get("sourceHandle", "").upper() == handle, f"{name}: incorrect source handle")
                require(case.get("index") == index and case.get("offset") == offset, f"{name}: incorrect operation")
                for key in ("cloneStatus", "selectionQueryStatus", "moveOpenStatus", "moveStatus", "moveTransactionStatus", "saveStatus", "reopenStatus", "closeInputStatus"):
                    require(case.get(key) == 0, f"{name}: {key} must be eOk")
                for key in ("sourceCount", "mappedCount", "reopenedModelCount"):
                    require(case.get(key) == 618, f"{name}: {key} must preserve 618 top-level entities")
                require(case.get("error") is None, f"{name}: reported operation error")
                states = [case.get(key) for key in ("before", "after", "saved")]
                if not require(all(isinstance(s, dict) for s in states), f"{name}: three observed states required"):
                    continue
                before, after, saved = states
                for state in states:
                    require(state.get("type") == entity_type and state.get("proxy") is False, f"{name}: native type must remain intact")
                    require(state.get("extentStatus") == 0 and vector(state.get("min")) and vector(state.get("max")), f"{name}: real geometric extents required")
                    require(state.get(point_kind, {}).get("status") == 0, f"{name}: point query must work")
                first = before.get(point_kind, {}).get("points")
                second = after.get(point_kind, {}).get("points")
                persisted = saved.get(point_kind, {}).get("points")
                if require(isinstance(first, list) and len(first) > index and vector(first[index]), f"{name}: selected original grip missing"):
                    require(near(case.get("selectionPoint"), first[index]), f"{name}: selection must use the observed point array index")
                    destination = [a+b for a, b in zip(first[index], offset)]
                    require(isinstance(second, list) and any(near(p, destination) for p in second), f"{name}: requested grip destination must be observed after edit")
                    require(isinstance(persisted, list) and any(near(p, destination) for p in persisted), f"{name}: requested destination must persist after reopen")
                require(not same_points(first, second), f"{name}: geometry must actually change")
                require(same_points(second, persisted), f"{name}: all post-edit grips must persist in their observed order")
                require(near(after.get("min"), saved.get("min")) and near(after.get("max"), saved.get("max")), f"{name}: post-edit extents must persist")
                output = case.get("outputDWG")
                if require(isinstance(output, str) and bool(output), f"{name}: output DWG required"):
                    file = Path(output)
                    require(file.resolve().parent == path.resolve().parent and file.is_file() and file.stat().st_size > 0, f"{name}: isolated nonempty output DWG required")
            require(sorted(seen) == sorted(expected), "each independent case must be executed once")
    for item in failures:
        print("FAIL:", item)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("This checks selected grip edits, not arbitrary axis numbering or dimension generation.")
    return 1 if failures else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_native_exercise.py <native-exercise.json>")
    raise SystemExit(main(Path(sys.argv[1])))
