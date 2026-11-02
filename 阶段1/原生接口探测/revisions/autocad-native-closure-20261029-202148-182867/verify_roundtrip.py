"""Read-only checks for a stage 1 native clone/edit/save/reopen run."""

from __future__ import annotations

import math
import re
import sys
from pathlib import Path

from verify_native_read import Checks, number, read_json, triplet


GEOMETRY_TOLERANCE = 1.0e-6  # Numerical comparison in model units, not a design tolerance.


def handle(value: object) -> str | None:
    if isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]+", value):
        return value.upper()
    return None


def near(actual: object, expected: object) -> bool:
    return number(actual) and number(expected) and math.isclose(
        actual, expected, rel_tol=0.0, abs_tol=GEOMETRY_TOLERANCE
    )


def index_entities(entities: object, checks: Checks, label: str) -> dict[str, dict]:
    indexed: dict[str, dict] = {}
    if not checks.require(isinstance(entities, list), f"{label} must be an array"):
        return indexed
    for index, entity in enumerate(entities):
        item_label = f"{label}[{index}]"
        if not checks.require(isinstance(entity, dict), f"{item_label} must be an object"):
            continue
        entity_handle = handle(entity.get("handle"))
        if checks.require(entity_handle is not None, f"{item_label} needs a hexadecimal handle"):
            checks.require(entity_handle not in indexed, f"{label} duplicate handle {entity_handle}")
            indexed[entity_handle] = entity
        for key in ("type", "layer"):
            checks.require(
                isinstance(entity.get(key), str) and bool(entity[key]),
                f"{item_label}.{key} must be a nonempty string",
            )
        checks.require(
            "proxy" in entity and entity["proxy"] in (False, 0),
            f"{item_label}.proxy must be false/0",
        )
        min_ok = checks.require(triplet(entity.get("min")), f"{item_label}.min needs three finite numbers")
        max_ok = checks.require(triplet(entity.get("max")), f"{item_label}.max needs three finite numbers")
        if min_ok and max_ok:
            checks.require(
                all(low <= high for low, high in zip(entity["min"], entity["max"])),
                f"{item_label} bounds must have min <= max",
            )
    return indexed


def verify(run_directory: Path) -> int:
    checks = Checks()
    result = read_json(run_directory / "roundtrip.json", checks, "roundtrip result")
    if result is None:
        return checks.finish()
    if not checks.require(isinstance(result, dict), "roundtrip root must be an object"):
        return checks.finish()
    checks.require(result.get("contract") == "stage1.roundtrip.v1", "roundtrip contract must be stage1.roundtrip.v1")

    native = read_json(run_directory / "native-read.json", checks, "native baseline")
    if not checks.require(isinstance(native, dict), "native baseline root must be an object"):
        return checks.finish()
    checks.require(native.get("contract") == "stage1.native.v1", "native baseline contract must be stage1.native.v1")
    databases = native.get("databases")
    if not checks.require(isinstance(databases, list), "native baseline databases must be an array"):
        return checks.finish()
    native_sources: dict[str, dict[str, dict]] = {}
    for index, database in enumerate(databases):
        if not checks.require(isinstance(database, dict), f"native database[{index}] must be an object"):
            continue
        source = database.get("source")
        if not checks.require(source in ("scheme", "reference"), f"native database[{index}] has invalid source"):
            continue
        checks.require(source not in native_sources, f"native baseline duplicate source {source}")
        native_sources[source] = index_entities(database.get("entities"), checks, f"native.{source}.entities")
    checks.require(set(native_sources) == {"scheme", "reference"}, "native baseline needs both sources")

    sample_path = Path(__file__).resolve().parents[2] / "阶段0" / "数据" / "阶段1验证样本.json"
    samples = read_json(sample_path, checks, "stage0 sample")
    try:
        wall_handle = next(row["来源句柄"] for row in samples["记录"] if row["编号"] == "V01").upper()
    except (TypeError, KeyError, StopIteration, AttributeError) as error:
        checks.require(False, f"cannot resolve V01 wall sample: {error}")
        return checks.finish()
    expected_samples = {
        "scheme": {wall_handle: "TDbWall", "CD84": "TDbOpening", "CD89": "TDbOpening", "CDC0": "TDbText"},
        "reference": {"19A0": "TDbDimension2", "199F": "TDbAxisLabelSet", "19F3": "TDbDrawingName", "17DF": "AcDbBlockReference"},
    }
    expected_counts = {"scheme": 213, "reference": 618}
    clone_entries = result.get("cloneEntries")
    if not checks.require(isinstance(clone_entries, list), "cloneEntries must be an array"):
        return checks.finish()
    checks.require(len(clone_entries) == 2, "cloneEntries must have exactly two sources")
    mappings: dict[str, dict[str, tuple[str, str]]] = {}
    target_handles: set[str] = set()
    for index, entry in enumerate(clone_entries):
        if not checks.require(isinstance(entry, dict), f"cloneEntries[{index}] must be an object"):
            continue
        source = entry.get("source")
        if not checks.require(isinstance(source, str) and source in expected_counts, f"cloneEntries[{index}] has invalid source"):
            continue
        checks.require(source not in mappings, f"duplicate clone source {source}")
        mappings[source] = {}
        checks.require(entry.get("status") == 0, f"{source} clone status must be 0")
        for key in ("sourceCount", "mappedCount"):
            checks.require(entry.get(key) == expected_counts[source], f"{source}.{key} must equal {expected_counts[source]}")
        sample_map = entry.get("sampleMap")
        if not checks.require(isinstance(sample_map, list), f"{source}.sampleMap must be an array"):
            continue
        checks.require(len(sample_map) == 4, f"{source}.sampleMap must contain four samples")
        for sample_index, sample in enumerate(sample_map):
            label = f"{source}.sampleMap[{sample_index}]"
            if not checks.require(isinstance(sample, dict), f"{label} must be an object"):
                continue
            source_handle = handle(sample.get("sourceHandle"))
            target_handle = handle(sample.get("targetHandle"))
            if not checks.require(source_handle in expected_samples[source], f"{label} has unexpected sourceHandle"):
                continue
            if not checks.require(target_handle is not None, f"{label} needs hexadecimal targetHandle"):
                continue
            checks.require(source_handle not in mappings[source], f"{source} duplicate source sample {source_handle}")
            checks.require(target_handle not in target_handles, f"duplicate cloned target handle {target_handle}")
            target_handles.add(target_handle)
            expected_type = expected_samples[source][source_handle]
            checks.require(sample.get("type") == expected_type, f"{label}.type must equal {expected_type}")
            mappings[source][source_handle] = (target_handle, expected_type)
        checks.require(set(mappings[source]) == set(expected_samples[source]), f"{source} sample mapping must cover all four expected handles")
    checks.require(set(mappings) == set(expected_counts), "cloneEntries must cover scheme and reference")

    transform = result.get("transform")
    if checks.require(isinstance(transform, dict), "transform must be an object"):
        for key in ("attempted", "passed"):
            checks.require(transform.get(key) == 213, f"transform.{key} must equal 213")
        checks.require(transform.get("offset") == [100000, 0, 0], "transform.offset must equal [100000,0,0]")
        checks.require(transform.get("applied") is True and transform.get("transactionStatus") == 0, "transform must be committed successfully")
    rollback = result.get("rollback")
    if checks.require(isinstance(rollback, dict), "rollback must be an object"):
        checks.require("before" in rollback and "after" in rollback, "rollback needs before and after observations")
        checks.require(rollback.get("before") is not None and rollback.get("before") == rollback.get("after"), "rollback before/after observations must agree")
        checks.require(rollback.get("abortStatus") == 0, "rollback.abortStatus must equal 0")
        checks.require(rollback.get("writeStatus") == 0 and rollback.get("during") == rollback.get("trial") and rollback.get("trial") != rollback.get("before"), "rollback must undo an observed successful change")
        control = rollback.get("nativeControl")
        if checks.require(isinstance(control, dict), "rollback must include native entity control"):
            checks.require(control.get("abortStatus") == 0 and control.get("writeStatus") == 0 and control.get("during") == control.get("trial") and control.get("before") == control.get("after"), "native control rollback must restore its observed change")
    for key in ("saveStatus", "reopenStatus", "closeInputStatus"):
        checks.require(result.get(key) == 0, f"roundtrip.{key} must equal 0")
    dwg_path = run_directory / "clone-cohort.dwg"
    checks.require(dwg_path.is_file() and dwg_path.stat().st_size > 0, "clone-cohort.dwg must exist and be nonempty")

    reopened = result.get("reopened")
    if not checks.require(isinstance(reopened, dict), "reopened must be an object"):
        return checks.finish()
    checks.require(reopened.get("modelCount") == 831, "reopened.modelCount must equal 831")
    checks.require(reopened.get("proxies") == 0, "reopened.proxies must equal 0")
    reopened_entities = reopened.get("entities")
    targets = index_entities(reopened_entities, checks, "reopened.entities")
    checks.require(isinstance(reopened_entities, list) and len(reopened_entities) == 831, "reopened.entities must cover all 831 objects")
    for source, sample_mappings in mappings.items():
        for source_handle, (target_handle, entity_type) in sample_mappings.items():
            label = f"{source}/{source_handle}->{target_handle}"
            original = native_sources.get(source, {}).get(source_handle)
            target = targets.get(target_handle)
            if not checks.require(original is not None, f"{label} missing native baseline entity"):
                continue
            if not checks.require(target is not None, f"{label} missing reopened target entity"):
                continue
            checks.require(original.get("type") == entity_type and target.get("type") == entity_type, f"{label} must preserve {entity_type}")
            source_layer, target_layer = original.get("layer"), target.get("layer")
            layer_matches = isinstance(source_layer, str) and isinstance(target_layer, str) and (
                source_layer == target_layer or re.fullmatch(r"(?:\$\d+\$)+" + re.escape(source_layer), target_layer) is not None
            )
            checks.require(layer_matches, f"{label} must preserve source layer or use the specified kDrcMangleName prefix")
            # These public property edits intentionally change the sample bounds.
            # Source-file integrity is checked separately using the frozen hashes.
            if source == "scheme" and source_handle == "CD84":
                properties = target.get("properties")
                checks.require(isinstance(properties, dict) and near(properties.get("Width"), 2500), f"{label} reopened Width must equal 2500")
                for boundary in ("min", "max"):
                    first = original.get(boundary)
                    second = target.get(boundary)
                    checks.require(
                        triplet(first) and triplet(second) and all(near(second[axis], first[axis]) for axis in (1, 2)),
                        f"{label}.{boundary} must preserve y and z after width edit",
                    )
                continue
            if source == "scheme" and source_handle == "CDC0":
                properties = target.get("properties")
                checks.require(
                    isinstance(properties, dict) and properties.get("Text") == "庭院 [S1]",
                    f"{label} reopened Text must equal the requested edit",
                )
                continue
            offset = [100000, 0, 0] if source == "scheme" else [0, 0, 0]
            for boundary in ("min", "max"):
                first = original.get(boundary)
                second = target.get(boundary)
                checks.require(
                    triplet(first) and triplet(second) and all(near(actual, prior + delta) for actual, prior, delta in zip(second, first, offset)),
                    f"{label}.{boundary} must match baseline plus {offset} within {GEOMETRY_TOLERANCE} model units",
                )

    edits = result.get("propertyEdits")
    if checks.require(isinstance(edits, list), "propertyEdits must be an array"):
        checks.require(len(edits) == 2, "propertyEdits must have exactly two records")
        expected_edits = {"CD84": ("Width", 2500), "CDC0": ("Text", "庭院 [S1]")}
        seen_edits: set[str] = set()
        for index, edit in enumerate(edits):
            label = f"propertyEdits[{index}]"
            if not checks.require(isinstance(edit, dict), f"{label} must be an object"):
                continue
            source_handle = handle(edit.get("sourceHandle"))
            if not checks.require(source_handle in expected_edits, f"{label} has unexpected sourceHandle"):
                continue
            checks.require(source_handle not in seen_edits, f"duplicate edit for {source_handle}")
            seen_edits.add(source_handle)
            property_name, expected = expected_edits[source_handle]
            checks.require(edit.get("property") == property_name, f"{label}.property must equal {property_name}")
            checks.require(edit.get("expected") == expected, f"{label}.expected must equal {expected!r}")
            checks.require(edit.get("status") == 0, f"{label}.status HRESULT must equal 0")
            checks.require(edit.get("actual") == expected, f"{label}.actual must equal {expected!r}")
            mapped = mappings.get("scheme", {}).get(source_handle)
            target = targets.get(mapped[0]) if mapped else None
            properties = target.get("properties") if isinstance(target, dict) else None
            checks.require(
                isinstance(properties, dict) and properties.get(property_name) == expected,
                f"{source_handle} reopened {property_name} must persist as {expected!r}",
            )
        checks.require(seen_edits == set(expected_edits), "propertyEdits must cover both expected edits")
    return checks.finish()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python verify_roundtrip.py <run-directory>", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(verify(Path(sys.argv[1])))
