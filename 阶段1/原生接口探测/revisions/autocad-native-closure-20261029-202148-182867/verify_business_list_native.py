"""Verify native LIST observation and log restoration, never infer business values."""
import json
import math
import sys
from pathlib import Path
from verify_scheme_native import points_equal


def near(a, b):
    if isinstance(a, (int, float)) and isinstance(b, (int, float)) and not isinstance(a, bool) and not isinstance(b, bool):
        return math.isfinite(a) and math.isfinite(b) and abs(a-b) <= 1e-6
    return isinstance(a, list) and isinstance(b, list) and len(a) == len(b) and all(near(x,y) for x,y in zip(a,b))


SAMPLES = {
    "CD83": "TDbWall", "CD84": "TDbOpening", "CD89": "TDbOpening",
    "CD94": "TDbGroupOpening", "CF5F": "TDbRectCornerStair",
    "CE8F": "TDbSymbElevation", "CDC0": "TDbText", "CFCE": "TDbDrawingName",
}


def main(path):
    assertions, failures = 0, []

    def check(ok, label):
        nonlocal assertions
        assertions += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    if not check(path.is_file(), "native business-LIST host evidence missing"):
        print("FAIL:", failures[0])
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    run = path.resolve().parent
    check(d.get("contract") == "stage1.business-list-native.v1", "contract")
    check(d.get("api") == "ObjectARX native; no COM", "native entity API")
    check(d.get("businessReadStatus") == "observation only", "no unverified business-read success claim")
    check(d.get("error") is None, "no observation operation error")
    for key in ("controlDWG", "currentDWG", "mdiActiveDWG", "workingDWG"):
        check(Path(d.get(key, "")).resolve() == run / "control.dwg", key + ": owned control")
    check(d.get("initialModelCount") == d.get("finalModelCount") == 213 and d.get("dwgSaved") is False, "scheme213 and no DWG save")
    logging = d.get("logging", {})
    for key in ("beforeMode", "beforePath", "beforeName", "pathReadback", "nameBeforeEnable", "modeAfterEnable", "nameAfterEnable", "modeAfterClose", "pathRestored", "modeRestored"):
        check(logging.get(key, {}).get("getStatus") == 5100, "actual variable read: " + key)
    check(logging.get("beforeMode", {}).get("restype") == 5003 and logging.get("beforeMode", {}).get("value") == 0, "existing user logging was off; never interrupted")
    for key in ("pathSetStatus", "enableStatus", "closeCommandStatus", "pathRestoreStatus", "modeRestoreStatus"):
        check(logging.get(key) == 5100, "logging operation: " + key)
    check(logging.get("preEnablePathSafe") is True and logging.get("preEnableFileAbsent") is True, "before enabling: isolated new log target confirmed")
    check(logging.get("preEnableFileCheckError") == 2, "target absence specifically returned ERROR_FILE_NOT_FOUND")
    check(logging.get("pathReadback", {}).get("restype") == 5005 and Path(logging.get("pathReadback", {}).get("value", "")).resolve() == run, "run directory read back")
    for key in ("nameBeforeEnable", "nameAfterEnable"):
        name = logging.get(key, {})
        check(name.get("restype") == 5005 and Path(name.get("value", "")).resolve().parent == run, key + ": log location protected")
    check(logging.get("modeAfterEnable", {}).get("value") == 1 and logging.get("modeAfterClose", {}).get("value") == 0, "logging opened and closed")
    before_path = logging.get("beforePath", {}).get("value", "")
    restored_path = logging.get("pathRestored", {}).get("value", "")
    check(isinstance(before_path, str) and bool(before_path) and before_path.casefold().rstrip("\\/") == restored_path.casefold().rstrip("\\/"), "original path restored")
    check(logging.get("modeRestored", {}).get("value") == 0 and logging.get("restored") is True and logging.get("restoreThrew") is False, "original mode/path restoration verified")
    originals = [d.get(k, []) for k in ("originalBefore", "originalAfter")]
    if check(all(len(x) == 213 for x in originals), "all original compact records"):
        aa, bb = [{e.get("handle"): e for e in seq} for seq in originals]
        check(len(aa) == 213 and set(aa) == set(bb), "original handle identities")
        for h in aa.keys() & bb.keys():
            a, b = aa[h], bb[h]
            for key in ("type", "layer", "proxy", "extentStatus"):
                check(a.get(key) == b.get(key), h + ": " + key)
            for key in ("min", "max"):
                check(near(a.get(key), b.get(key)), h + ": " + key)
            for key in ("grip", "stretch"):
                av, bv = a.get(key, {}), b.get(key, {})
                check(av.get("status") == bv.get("status") and points_equal(av.get("points"), bv.get("points")), h + ": " + key)
            ca, cb = a.get("curve"), b.get("curve")
            if ca is None:
                check(cb is None, h + ": noncurve preserved")
            elif check(isinstance(cb, dict), h + ": curve preserved"):
                for key in ("start", "end", "length"):
                    check(near(ca.get(key), cb.get(key)), h + ": curve " + key)
    info = d.get("logFile", {})
    file = Path(info.get("path", ""))
    check(info.get("observed") is True and info.get("readStatus") == 0 and file.is_file() and file.resolve().parent == run, "raw log observed in isolated run")
    check(file.resolve() == Path(logging.get("nameAfterEnable", {}).get("value", "")).resolve(), "actual log filename preserved")
    text = ""
    if file.is_file():
        data = file.read_bytes()
        check(len(data) == info.get("bytes") and len(data) > 0, "actual raw-byte size")
        bom = info.get("bom")
        if data.startswith(b"\xef\xbb\xbf"):
            encoding, expected_bom = "utf-8-sig", "UTF-8"
        elif data.startswith(b"\xff\xfe"):
            encoding, expected_bom = "utf-16", "UTF-16LE"
        elif data.startswith(b"\xfe\xff"):
            encoding, expected_bom = "utf-16", "UTF-16BE"
        else:
            encoding, expected_bom = "cp" + str(info.get("ansiCodePage")), "none"
        check(bom == expected_bom and isinstance(info.get("ansiCodePage"), int), "observed BOM and native ANSI code-page metadata")
        text = data.decode(encoding, errors="replace")
    samples = d.get("samples", [])
    check(len(samples) == 8 and {x.get("handle") for x in samples} == set(SAMPLES), "eight fixed native samples")
    previous = -1
    for sample in samples:
        h = sample.get("handle")
        if not check(h in SAMPLES, "known sample " + str(h)):
            continue
        kind = SAMPLES[h]
        check(sample.get("expectedType") == sample.get("type") == kind, h + ": native type")
        check(sample.get("lookupStatus") == sample.get("openStatus") == 0, h + ": real object lookup/open")
        check(sample.get("listInvoked") is True and sample.get("listReturned") is True and sample.get("error") is None, h + ": void LIST invocation returned")
        check(sample.get("businessReadStatus") == "observation only", h + ": no inferred field-success claim")
        begin = "[S1BUSINESSLIST_BEGIN handle=" + h + " type=" + kind + "]"
        end = "[S1BUSINESSLIST_END handle=" + h + " type=" + kind + "]"
        check(sample.get("beginMarker") == begin and sample.get("endMarker") == end, h + ": markers contain identity only")
        check(text.count(begin) == text.count(end) == 1, h + ": complete unique marker pair in raw log")
        left, right = text.find(begin), text.find(end)
        check(left > previous and right > left, h + ": synchronous ordered capture")
        previous = right
    for failure in failures[:100]:
        print("FAIL:", failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: native list invocation, log capture/restoration and scheme protection. Width/height/kind availability needs analysis of the observed object text.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_business_list_native.py <run/business-list-native.json>")
    try:
        result = main(Path(sys.argv[1]))
    except (OSError, ValueError, LookupError, TypeError, AttributeError, KeyError, IndexError) as failure:
        print("FAIL: unreadable or incomplete native business-LIST evidence:", failure)
        result = 1
    raise SystemExit(result)
