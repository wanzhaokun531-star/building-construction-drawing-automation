"""Validate completeness of read-only native resource-table evidence."""
import json
import re
import sys
from pathlib import Path


def main(path):
    failures, count = [], 0

    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    if check(path.is_file(), "native resource evidence missing"):
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        check(d.get("contract") == "stage1.native-styles.v1", "contract")
        check(d.get("scope") == "resourceTablesOnly" and d.get("entityOverridesVerified") is False, "limited scope explicit")
        sources = d.get("sources")
        if check(isinstance(sources, list) and len(sources) == 2, "two input resource audits"):
            check({s.get("role") for s in sources} == {"scheme", "reference"}, "source roles")
            for s in sources:
                role = s.get("role")
                check(s.get("readStatus") == 0 and s.get("closeInputStatus") == 0 and s.get("error") is None, role + ": native read")
                for kind in ("textStyles", "linetypes", "dimStyles", "layers"):
                    records = s.get(kind)
                    if not check(isinstance(records, list) and len(records) > 0, role + "/" + kind + ": records"):
                        continue
                    check(len({r.get("name") for r in records}) == len(records), role + "/" + kind + ": unique names")
                    check(all(isinstance(r.get("handle"), str) and bool(r["handle"]) and isinstance(r.get("type"), str) for r in records), role + "/" + kind + ": native identities")
                    frozen = Path(__file__).resolve().parents[2] / "阶段0" / "输入快照" / ("方案图对象属性.json" if role == "scheme" else "参考图对象属性.json")
                    oracle = json.loads(frozen.read_text(encoding="utf-8-sig"))
                    original_key = {"textStyles": "TextStyles", "linetypes": "LineTypes", "dimStyles": "DimStyles", "layers": "Layers"}[kind]
                    known = {(r["Name"] if isinstance(r, dict) else r).casefold() for r in oracle[original_key]}
                    check(known <= {r["name"].casefold() for r in records}, role + "/" + kind + ": all independent baseline names covered")
                check(isinstance(s.get("databaseSettings"), dict) and bool(s["databaseSettings"]), role + ": database scale settings")
                sdk = Path(r"C:\Autodesk\ObjectARX_for_AutoCAD_2020_Win_64_bit\inc\dbdimvar.h").read_text()
                getters = set(re.findall(r"\b(dim\w+)\s*\(\s*\)\s*const\s*;", sdk))
                for style in s.get("dimStyles", []):
                    values = style.get("values", {})
                    check(isinstance(values, dict) and len(values) >= 78, role + "/" + style.get("name", "?") + ": complete SDK DIM getter set")
                    check(getters <= set(values), role + "/" + style.get("name", "?") + ": every actual SDK getter present")
    for f in failures:
        print("FAIL:", f)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: native resource-table audit; not all custom entity overrides or final font/print acceptance.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main(Path(sys.argv[1])))
