"""Same full family checks, exactly one other-orientation reference fixture."""
import sys
from pathlib import Path
import verify_object_family_native as family
family.FIXTURES={"H01":("reference","19A3","TDbAxisLabelSet")}
if __name__=="__main__":
    p=Path(sys.argv[1])
    if not p.is_file():
        print("FAIL: horizontal axis native host evidence missing")
        print("assertions=1 passed=0 failed=1 result=FAIL")
        raise SystemExit(1)
    raise SystemExit(family.main(p))
