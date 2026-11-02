"""Check ObjectARX application-context opening of a task-owned scheme copy."""
import json
import sys
from pathlib import Path


def main(path):
    failures = []
    if not path.is_file():
        failures.append("native-open report missing")
    else:
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        if d.get("contract") != "stage1.native-control-open.v1":
            failures.append("incorrect native-open contract")
        if d.get("openStatus") != 0 or d.get("error") is not None:
            failures.append("native application-context open failed")
        if d.get("applicationContextObserved") is not True:
            failures.append("application context was not observed")
        expected = path.parent / "control.dwg"
        # curDocument may differ from mdiActiveDocument in application context
        # (ObjectARX 2020 acdocman.h:267-276). Modal business guards check both later.
        for key in ("controlDWG", "mdiActiveDWG"):
            if Path(d.get(key, "")).resolve() != expected.resolve():
                failures.append(key + " is not this run control")
        done = path.parent / "native-open.done"
        if not done.is_file():
            failures.append("native-open completion missing")
        else:
            c = json.loads(done.read_text(encoding="utf-8-sig"))
            if Path(c.get("run", "")).resolve() != path.parent.resolve() or c.get("safeToUnload") is not True:
                failures.append("native-open cleanup marker invalid")
    for f in failures:
        print("FAIL:", f)
    print("Native control open: " + ("FAIL" if failures else "PASS"))
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
