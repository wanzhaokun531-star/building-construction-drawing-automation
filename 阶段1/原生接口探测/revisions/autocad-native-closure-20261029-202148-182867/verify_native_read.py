"""Read-only acceptance checks for the stage 1 native ObjectARX read result."""

from __future__ import annotations

import json
import math
import re
import sys
from pathlib import Path


class Checks:
    def __init__(self) -> None:
        self.count = 0
        self.failures: list[str] = []

    def require(self, condition: bool, message: str) -> bool:
        self.count += 1
        if not condition:
            self.failures.append(message)
        return condition

    def finish(self) -> int:
        for message in self.failures[:30]:
            print(f"FAIL: {message}")
        if len(self.failures) > 30:
            print(f"... {len(self.failures) - 30} additional failures")
        failed = len(self.failures)
        print(
            f"assertions={self.count} passed={self.count - failed} "
            f"failed={failed} result={'PASS' if failed == 0 else 'FAIL'}"
        )
        return int(failed != 0)


def number(value: object) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
    )


def triplet(value: object) -> bool:
    return isinstance(value, list) and len(value) == 3 and all(map(number, value))


def has_property_evidence(entity: dict) -> bool:
    # Property names remain deliberately open until the native writer defines them.
    # A status code, a bound, or a class name alone is not property evidence.
    standard_fields = {"handle", "type", "layer", "proxy", "min", "max"}
    for key, value in entity.items():
        if key in standard_fields or re.search(r"status|error", key, re.IGNORECASE):
            continue
        if isinstance(value, dict):
            if any(item is not None for item in value.values()):
                return True
        elif value is not None and value != "" and value != []:
            return True
    return False


def read_json(path: Path, checks: Checks, role: str) -> object | None:
    if not checks.require(path.is_file(), f"{role} file missing: {path}"):
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        checks.require(False, f"{role} cannot be read as JSON: {error}")
        return None


def verify(path: Path) -> int:
    checks = Checks()
    payload = read_json(path, checks, "result")
    if payload is None:
        return checks.finish()
    if not checks.require(isinstance(payload, dict), "root must be a JSON object"):
        return checks.finish()

    for key, expected in (
        ("contract", "stage1.native.v1"),
        ("sdkMajor", 23),
        ("sdkMinor", 1),
        ("mscVer", 1916),
    ):
        checks.require(payload.get(key) == expected, f"{key} must equal {expected!r}")

    databases = payload.get("databases")
    if not checks.require(isinstance(databases, list), "databases must be an array"):
        return checks.finish()
    checks.require(len(databases) == 2, "databases must contain exactly two records")

    sample_path = (
        Path(__file__).resolve().parents[2]
        / "阶段0" / "数据" / "阶段1验证样本.json"
    )
    sample_data = read_json(sample_path, checks, "stage0 sample")
    try:
        wall = next(row for row in sample_data["记录"] if row["编号"] == "V01")
        wall_handle = wall["来源句柄"].upper()
        checks.require(wall["对象类型"] == "TDbWall", "V01 must specify TDbWall")
    except (TypeError, KeyError, StopIteration, AttributeError) as error:
        checks.require(False, f"V01 wall sample cannot be resolved: {error}")
        return checks.finish()

    expected_samples = {
        "scheme": {
            wall_handle: "TDbWall", "CD84": "TDbOpening",
            "CD89": "TDbOpening", "CDC0": "TDbText",
        },
        "reference": {
            "19A0": "TDbDimension2", "199F": "TDbAxisLabelSet",
            "19F3": "TDbDrawingName", "17DF": "AcDbBlockReference",
        },
    }
    expected_counts = {"scheme": 213, "reference": 618}
    seen_sources: list[str] = []
    for index, database in enumerate(databases):
        label = f"databases[{index}]"
        if not checks.require(isinstance(database, dict), f"{label} must be an object"):
            continue
        source = database.get("source")
        if not checks.require(isinstance(source, str) and source in expected_counts, f"{label}.source must be scheme/reference"):
            continue
        seen_sources.append(source)
        for key in ("readStatus", "closeInputStatus", "proxies"):
            checks.require(database.get(key) == 0, f"{source}.{key} must equal 0")
        checks.require(number(database.get("insunits")), f"{source}.insunits must be numeric")
        checks.require(
            database.get("modelCount") == expected_counts[source],
            f"{source}.modelCount must equal {expected_counts[source]}",
        )
        entities = database.get("entities")
        if not checks.require(isinstance(entities, list), f"{source}.entities must be an array"):
            continue
        checks.require(
            len(entities) == expected_counts[source],
            f"{source}.entities must cover all {expected_counts[source]} model-space entities",
        )
        indexed: dict[str, dict] = {}
        for entity_index, entity in enumerate(entities):
            entity_label = f"{source}.entities[{entity_index}]"
            if not checks.require(isinstance(entity, dict), f"{entity_label} must be an object"):
                continue
            handle = entity.get("handle")
            valid_handle = isinstance(handle, str) and bool(re.fullmatch(r"[0-9A-Fa-f]+", handle))
            checks.require(valid_handle, f"{entity_label}.handle must be a hexadecimal string")
            if valid_handle:
                handle = handle.upper()
                checks.require(handle not in indexed, f"{source} has duplicate handle {handle}")
                indexed[handle] = entity
            checks.require(
                isinstance(entity.get("type"), str) and bool(entity["type"]),
                f"{entity_label}.type must be a nonempty string",
            )
            checks.require(
                isinstance(entity.get("layer"), str) and bool(entity["layer"]),
                f"{entity_label}.layer must be a nonempty string",
            )
            checks.require(
                "proxy" in entity and entity["proxy"] in (False, 0),
                f"{entity_label}.proxy must be false/0",
            )
            min_ok = checks.require(triplet(entity.get("min")), f"{entity_label}.min must be three finite numbers")
            max_ok = checks.require(triplet(entity.get("max")), f"{entity_label}.max must be three finite numbers")
            if min_ok and max_ok:
                checks.require(
                    all(low <= high for low, high in zip(entity["min"], entity["max"])),
                    f"{entity_label} bounds must have min <= max",
                )
        for handle, entity_type in expected_samples[source].items():
            if not checks.require(handle in indexed, f"{source} missing sample handle {handle}"):
                continue
            entity = indexed[handle]
            checks.require(entity.get("type") == entity_type, f"{source}/{handle} must remain {entity_type}")
            checks.require(
                has_property_evidence(entity),
                f"{source}/{handle} needs property evidence beyond identity/bounds/status fields",
            )
            if (source, handle) in (("scheme", "CDC0"), ("reference", "19F3")) and triplet(entity.get("min")) and triplet(entity.get("max")):
                checks.require(entity["max"][0] > entity["min"][0] and entity["max"][1] > entity["min"][1], f"{source}/{handle} known text must have a nondegenerate XY bounding box")
    checks.require(sorted(seen_sources) == ["reference", "scheme"], "one scheme and one reference database required")
    return checks.finish()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python verify_native_read.py <native-read.json>", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(verify(Path(sys.argv[1])))
