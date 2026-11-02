"""Validate native access discovery evidence; unsupported APIs are recorded outcomes."""
import json
import math
import sys
from pathlib import Path


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def vector(value):
    return isinstance(value, list) and len(value) == 3 and all(finite(x) for x in value)


def main(path):
    checks = 0
    failures = []

    def require(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            failures.append(message)
        return condition

    if require(path.is_file(), f"native scan result missing: {path}"):
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        require(data.get("contract") == "stage1.native-access.v1", "incorrect scan contract")
        require(data.get("api") == "ObjectARX native; no COM", "object access API must be explicitly native")
        require([data.get(k) for k in ("sdkMajor", "sdkMinor", "mscVer")] == [23, 1, 1916], "unexpected SDK/compiler identity")
        control = data.get("nativeControl")
        if require(isinstance(control, dict), "native control sample missing"):
            require(control.get("type") == "AcDbLine", "native control must be a line")
            require(control.get("grip", {}).get("status") == 0 and len(control.get("grip", {}).get("points", [])) >= 2, "native line grip control must work")
            require(control.get("stretch", {}).get("status") == 0 and len(control.get("stretch", {}).get("points", [])) == 2, "native line stretch control must have endpoints")
        sources = data.get("sources")
        if require(isinstance(sources, list) and len(sources) == 2, "two sources required"):
            expected = {
                "scheme": {"CD83": "TDbWall", "CD84": "TDbOpening", "CD89": "TDbOpening", "CDC0": "TDbText"},
                "reference": {"19A0": "TDbDimension2", "199F": "TDbAxisLabelSet", "19F3": "TDbDrawingName", "17DF": "AcDbBlockReference"},
            }
            baseline_path = Path(__file__).resolve().parents[1] / "runs" / "native-20261028-164904-411" / "native-read.json"
            baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
            prior = {d["source"]: {e["handle"]: e for e in d["entities"]} for d in baseline["databases"]}
            names = []
            for source in sources:
                name = source.get("source")
                if not require(name in expected, "unexpected source"):
                    continue
                names.append(name)
                require(source.get("readStatus") == 0 and source.get("closeInputStatus") == 0, f"{name}: database read failed")
                require(source.get("modelCount") == (213 if name == "scheme" else 618), f"{name}: source object count changed")
                samples = source.get("samples")
                if not require(isinstance(samples, list) and len(samples) == 4, f"{name}: four samples required"):
                    continue
                handles = []
                for sample in samples:
                    handle = sample.get("handle", "").upper()
                    if not require(handle in expected[name], f"{name}: unexpected sample handle"):
                        continue
                    handles.append(handle)
                    label = f"{name}/{handle}"
                    require(sample.get("type") == expected[name][handle], f"{label}: incorrect native object type")
                    require(sample.get("proxy") is False, f"{label}: object became proxy")
                    require(sample.get("extentStatus") == 0, f"{label}: extents failed")
                    for key in ("min", "max"):
                        actual = sample.get(key)
                        if require(vector(actual), f"{label}: invalid {key}"):
                            require(all(abs(a-b) <= 1e-6 for a, b in zip(actual, prior[name][handle][key])), f"{label}: source geometry changed from validated baseline")
                    require(isinstance(sample.get("memberIteratorAvailable"), bool), f"{label}: missing iterator outcome")
                    members = sample.get("members")
                    if require(isinstance(members, list), f"{label}: members must be an array"):
                        for member in members:
                            require(isinstance(member.get("name"), str) and bool(member["name"]), f"{label}: member name missing")
                            require(isinstance(member.get("kind"), str) and bool(member["kind"]), f"{label}: member kind missing")
                            require(isinstance(member.get("valueType"), str), f"{label}: value type missing")
                            require(member.get("readOnly") is None or isinstance(member["readOnly"], bool), f"{label}: invalid readOnly outcome")
                            status = member.get("getStatus")
                            require(status is None or (isinstance(status, int) and not isinstance(status, bool)), f"{label}: invalid get status")
                            require(member.get("valueText") is None or isinstance(member["valueText"], str), f"{label}: invalid value text")
                            require(member.get("collectionCount") is None or (isinstance(member["collectionCount"], int) and member["collectionCount"] >= 0), f"{label}: invalid collection count")
                    for key in ("grip", "stretch"):
                        outcome = sample.get(key)
                        if not require(isinstance(outcome, dict), f"{label}: missing {key} outcome"):
                            continue
                        require(isinstance(outcome.get("status"), int) and not isinstance(outcome.get("status"), bool), f"{label}: missing {key} status")
                        points = outcome.get("points")
                        if require(isinstance(points, list), f"{label}: missing {key} points"):
                            require(all(vector(p) for p in points), f"{label}: nonfinite {key} point")
                        if key == "grip":
                            require(all(isinstance(outcome.get(k), list) and all(isinstance(v, int) for v in outcome[k]) for k in ("osnapModes", "geomIds")), f"{label}: missing native grip metadata")
                require(set(handles) == set(expected[name]) and len(set(handles)) == 4, f"{name}: incomplete sample set")
            require(sorted(names) == ["reference", "scheme"], "source set incomplete")
    for failure in failures:
        print("FAIL:", failure)
    print(f"assertions={checks} passed={checks-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Evidence validity is separate from native property/grip business support.")
    return 1 if failures else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_native_access.py <native-access.json>")
    raise SystemExit(main(Path(sys.argv[1])))
