"""Independent saved-DWG checks for the one-off room-area/name sample.

Run after the isolated drawing-edit job has saved and reopened both floors:
    python verify_area_name_sample.py v135 [--no-publish]

PDFs are internal preview evidence; only the two DWGs are published.  Importing
this module does not contact CAD.  The native queue is loaded only by main().
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import importlib.util
import json
import math
import shutil


ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
EPS = 1e-6
EXPECTED_COUNTS = {1: 282, 2: 269}


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def require(condition, *detail):
    if not condition:
        raise AssertionError(detail)


def near(a, b):
    return (len(a) == len(b)
            and all(math.isfinite(float(x)) and math.isfinite(float(y))
                    and abs(x - y) < EPS for x, y in zip(a, b)))


def add(point, delta):
    require(len(point) == len(delta) == 3, "3D translation required")
    return [x + y for x, y in zip(point, delta)]


def bounds(root):
    return [*root["min"][:2], *root["max"][:2]]


def center(box):
    return [(box[0] + box[2]) / 2, (box[1] + box[3]) / 2]


def joint_bounds(roots):
    boxes = [bounds(root) for root in roots]
    return [min(b[0] for b in boxes), min(b[1] for b in boxes),
            max(b[2] for b in boxes), max(b[3] for b in boxes)]


def index(rows):
    result = {row["handle"]: row for row in rows}
    require(len(result) == len(rows), "duplicate native handles")
    return result


def check_translated_root(old, now, delta):
    """Permit a rigid translation, preserving every other exposed root field."""
    require(set(old) == set(now), "root fields changed", old["handle"])
    for key in old:
        if key in ("min", "max"):
            require(near(now[key], add(old[key], delta)),
                    "translated bounds differ", old["handle"], key)
        elif key in ("grip", "stretch"):
            require(set(old[key]) == set(now[key]), "point metadata fields changed")
            for subkey in old[key]:
                if subkey == "points":
                    before_points, after_points = old[key][subkey], now[key][subkey]
                    require(len(before_points) == len(after_points),
                            "point count changed", old["handle"], key)
                    require(all(near(a, add(b, delta))
                                for a, b in zip(after_points, before_points)),
                            "non-rigid point movement", old["handle"], key)
                else:
                    require(old[key][subkey] == now[key][subkey],
                            "point metadata changed", old["handle"], key, subkey)
        else:
            require(old[key] == now[key], "non-position root field changed",
                    old["handle"], key)


def reopened_groups(floor):
    saved = Path(floor["log"]).read_text(encoding="cp936").splitlines()
    require(f'SAVED|F{floor["floor"]}' in saved, "save marker missing")
    groups, current, counts, metadata = {}, None, [], {}
    for line in Path(floor["reopenedLog"]).read_text(encoding="cp936").splitlines():
        fields = line.split("|")
        if fields[0] == "GROUP":
            require(len(fields) == 3 and fields[1] not in groups,
                    "invalid/duplicate reopened group", fields)
            current = fields[1]
            groups[current] = {"name": fields[2], "members": []}
        elif fields[0] == "MEMBER":
            require(len(fields) == 3 and current is not None,
                    "member without group", fields)
            groups[current]["members"].append((fields[1], fields[2]))
        elif fields[0] == "COUNT":
            require(len(fields) == 2, "invalid COUNT", fields)
            counts.append(int(fields[1]))
        elif fields[0] == "AREA-TEXT":
            require(len(fields) == 8 and fields[1] not in metadata,
                    "invalid/duplicate area text metadata", fields)
            metadata[fields[1]] = dict(handle=fields[1], text=fields[2], style=fields[3],
                                       height=float(fields[4]), width=float(fields[5]),
                                       rotation=float(fields[6]), oblique=float(fields[7]))
    expected = {area["id"]: area for area in floor["areas"]}
    require(len(expected) == len(groups) == 10 and set(expected) == set(groups),
            "area group count/identity changed")
    require(counts == [floor["expectedModelCount"]], "reopened root count", counts)
    all_members = []
    for identity, group in groups.items():
        area = expected[identity]
        require(group["name"] == area["group"], "group name changed", identity)
        require(len(group["members"]) == 2
                and set(group["members"]) == {(h, "TEXT") for h in area["members"]},
                "group members changed", identity, group)
        all_members.extend(h for h, _ in group["members"])
    require(len(set(all_members)) == 20, "area groups share members")
    require(set(metadata) == set(all_members), "reopened area text metadata missing")
    for area in floor["areas"]:
        for original in area["memberDetails"]:
            actual = metadata[original["handle"]]
            require(actual["text"] == original["text"]
                    and actual["style"] == original["style"]
                    and abs(actual["height"] - original["height"]) < EPS,
                    "area text content/style/height changed", original["handle"])
    return {"groups": groups, "areaTextMetadata": metadata}


def check_plugin(manifest):
    require(manifest["pluginCodeChanged"] is False, "not a sample-only manifest")
    for path, digest in manifest["sourceFiles"].items():
        require(sha(ROOT / path) == digest.upper(), "plugin source changed", path)
    require(sha(ROOT / "插件/bin/BuildingPlanPlugin2020.arx")
            == manifest["pluginBinarySHA256"].upper(), "plugin ARX changed")


def check_floor_contract(floor, planned):
    number = floor["floor"]
    require(number in EXPECTED_COUNTS
            and floor["expectedModelCount"] == EXPECTED_COUNTS[number],
            "unexpected floor/model count", number)
    for key in ("source", "sourceSHA256", "sourcePreview", "areas", "nameMoves"):
        require(floor.get(key, []) == planned.get(key, []),
                "manifest differs from bound placement plan", number, key)
    require(sha(floor["source"]) == floor["sourceSHA256"].upper(),
            "source DWG fingerprint changed", number)
    preview_digest = floor.get("sourcePreviewSHA256", planned["sourcePreviewSHA256"])
    require(sha(floor["sourcePreview"]) == preview_digest.upper(),
            "source preview fingerprint changed", number)
    require(len(floor["areas"]) == 10, "ten area groups required", number)
    expected_moves = {}
    for area in floor["areas"]:
        require(len(area["members"]) == 2, "two area Text members required")
        for handle in area["members"]:
            require(handle not in expected_moves, "repeated area member", handle)
            expected_moves[handle] = area["delta"]
    name_handles = {a.get("nameHandle") for a in floor["areas"]} - {None, ""}
    for move in floor.get("nameMoves", []):
        require(move["handle"] in name_handles and move["handle"] not in expected_moves,
                "unapproved name move", move["handle"])
        expected_moves[move["handle"]] = move["delta"]
    require(floor["moves"] == expected_moves, "move whitelist differs from area/name plan")


def native_preview(floor, session, ready, queue):
    number = floor["floor"]
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    job = ROOT / "阶段1/runs" / f"stage2-preview-area-name-{stamp}-floor{number}"
    job.mkdir()
    shutil.copyfile(floor["control"], job / "control.dwg")
    x0 = 26000 if number == 1 else 72422
    window = [x0, 17500, x0 + 31000, 44000]
    (job / "annotation-index.tsv").write_text(
        f'S2DIMPREVIEW3 0 {floor["expectedModelCount"]} '
        f'{x0} 17500 {x0 + 31000} 44000 594 420\n', encoding="ascii")
    steps = [queue.dispatch(session, ready, "S1OPENPROBECONTROL", job,
                            "native-open.done", 90)]
    (job / "phase2-mask-audit.only").write_text(
        "Native opened DWG area/name review sample\n", encoding="ascii")
    steps.append(queue.dispatch(session, ready, "S1PHASE2EXECUTE", job,
                                "native-mask-document-audit.json", 60))
    masks = read(job / "native-mask-document-audit.json")
    audit = read(job / "product-axis-document-audit.json")
    require(masks["error"] is None and masks["readOnly"] is True
            and masks["masks"] == 4 and masks["frame70"] == 0
            and masks["WIPEOUTFRAME"] == 0 and masks["publicVisibleFrames"] == 0,
            "mask audit failed", masks)
    require(audit["error"] is None and audit["readOnly"] is True,
            "native annotation audit failed")
    steps.append(queue.dispatch(session, ready, "S1PHASE2PREVIEW", job,
                                "stage2-preview-native.json", 120))
    preview = read(job / "stage2-preview-native.json")
    require(preview["error"] is None
            and preview["originalBefore"] == preview["originalAfter"]
            and preview["initialModelCount"] == preview["finalModelCount"]
            == floor["expectedModelCount"], "preview changed model roots")
    require(preview["globalsBefore"] == preview["globalsAfter"]
            and preview["layoutBefore"] == preview["layoutAfter"],
            "preview did not restore global/layout settings")
    page = preview["page"]
    require(near(preview["windowWCS"], window), "preview window differs")
    require(page["error"] is None and page["pipelineComplete"]
            and page["fileObserved"] and page["fileBytes"] > 0,
            "internal PDF preview failed")
    for settings in (page["settings"], page["validatedSettings"]):
        require(near(settings["paperMM"], [594, 420])
                and near(settings["scale"], [1, 100])
                and near(settings["windowDCS"], window), "A2/1:100 settings differ")
    return job, preview, audit, steps


def verify_geometry(floor, planned, preview, audit):
    old_preview = read(floor["sourcePreview"])
    old_audit_path = Path(floor["sourcePreview"]).parent / "product-axis-document-audit.json"
    old_audit = read(old_audit_path)
    require(old_preview["error"] is None and old_audit["error"] is None,
            "source evidence has errors")
    before, after = index(old_preview["originalBefore"]), index(preview["originalBefore"])
    require(len(before) == len(after) == floor["expectedModelCount"]
            and set(before) == set(after), "model root set changed")
    for handle, old in before.items():
        if handle in floor["moves"]:
            require(old["type"] in ("AcDbText", "TDbText"),
                    "move whitelist contains non-text", handle)
            check_translated_root(old, after[handle], floor["moves"][handle])
        else:
            require(old == after[handle], "protected source root changed", handle)
    original_masks = [h for h, e in before.items() if e["type"] == "AcDbWipeout"]
    require(len(original_masks) == 4
            and all(before[h] == after[h] for h in original_masks),
            "original Wipeout roots changed")
    old_annotations, actual = index(old_audit["nativeAnnotations"]), index(audit["nativeAnnotations"])
    require(set(old_annotations) == set(actual), "native annotation set changed")
    for handle, old in old_annotations.items():
        now = actual[handle]
        if handle in floor["moves"]:
            # Native left-aligned AcDbText position[0] can remain [0,0,0].
            # Actual movement is proved by root bounds/grip/stretch above.
            require({k: v for k, v in old.items() if k != "position"}
                    == {k: v for k, v in now.items() if k != "position"},
                    "moved native text content/style fields changed", handle)
        else:
            require(old == now, "protected native annotation changed", handle)
    for key in old_audit:
        if key != "nativeAnnotations":
            require(old_audit[key] == audit.get(key), "axis/group audit changed", key)
    dimensions = [e for e in old_annotations.values()
                  if e["type"] == "TDbDimension2" and e["visible"]]
    require(len(dimensions) == 27, "27 whole visible TCH dimension chains required")
    reports = []
    for area in floor["areas"]:
        members = area["members"]
        require(all(before[h]["type"] == after[h]["type"] == "AcDbText"
                    for h in members), "area is not native Text", area["id"])
        body, exponent = members
        require(actual[body]["text"] == area["areaBodyText"]
                and actual[exponent]["text"] == "2", "area value/unit changed", area["id"])
        old_box, new_box = joint_bounds([before[h] for h in members]), joint_bounds([after[h] for h in members])
        require(near(old_box, area["currentBounds"])
                and near(new_box, area["desiredBounds"]),
                "combined area bounds differ", area["id"], new_box, area["desiredBounds"])
        require(near(center(new_box), area["desiredCenter"]),
                "final area center differs", area["id"])
        for handle in members:
            old_b, new_b = bounds(before[handle]), bounds(after[handle])
            require(near([old_b[2] - old_b[0], old_b[3] - old_b[1]],
                         [new_b[2] - new_b[0], new_b[3] - new_b[1]]),
                    "area glyph bbox dimensions changed", handle)
        require(near([after[exponent]["min"][i] - after[body]["min"][i] for i in range(3)],
                     [before[exponent]["min"][i] - before[body]["min"][i] for i in range(3)]),
                "superscript relative placement changed", area["id"])
        name_handle = area.get("nameHandle")
        report = dict(id=area["id"], group=area["group"], members=members,
                      actualBounds=new_box, center=center(new_box), nameHandle=name_handle)
        if name_handle:
            require(before[name_handle]["type"] == after[name_handle]["type"] == "TDbText",
                    "room name is not expected TDbText", name_handle)
            name_box = bounds(after[name_handle])
            require(abs(center(name_box)[0] - center(new_box)[0]) < EPS
                    and abs(name_box[1] - new_box[3] - area["gap"]) < EPS,
                    "name/area center or vertical gap differs", area["id"], name_box, new_box)
            require(near(name_box, area["nameBounds"]), "name target bounds differ", name_handle)
            report.update(nameBounds=name_box, actualGap=name_box[1] - new_box[3])
        else:
            report["placement"] = "approved room-middle target; no mathematical centroid requirement"
        reports.append(report)
    for name in floor.get("nameMoves", []):
        require(near(bounds(before[name["handle"]]), name["beforeBounds"])
                and near(bounds(after[name["handle"]]), name["desiredBounds"]),
                "name move bounds differ", name["handle"])
    total_members = planned["totalAreaMembersUnchanged"]
    require(len(total_members) == 2 and all(h not in floor["moves"]
            and before[h] == after[h] and old_annotations[h] == actual[h]
            for h in total_members), "total area members changed")
    return dict(areas=reports, areaGroups=10, areaTextMembers=20,
                movedNames=len(floor.get("nameMoves", [])), wholeTCHDimensions=27,
                allOriginalDimensionAnnotationsUnchanged=True, modelRoots=len(after),
                originalWipeoutRootsUnchanged=original_masks,
                allUnlistedRootsUnchanged=True, areaContentRotationAndGlyphBoundsPreserved=True,
                superscriptRelativePlacementPreserved=True, totalAreaMembersUnchanged=total_members,
                oldNativeAnnotations=str(old_audit_path), oldNativeAnnotationsSHA256=sha(old_audit_path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", nargs="?", default="v135")
    parser.add_argument("--no-publish", action="store_true")
    args = parser.parse_args()
    manifest_path = HERE / f"area-name-manifest-{args.version}.json"
    manifest = read(manifest_path)
    require(manifest["version"] == args.version, "version mismatch")
    require(sha(manifest["plan"]) == manifest["planSHA256"].upper(), "placement plan changed")
    layout = read(manifest["plan"])
    planned = {f["floor"]: f for f in layout["floors"]}
    require(len(planned) == len(manifest["floors"]) == 2
            and {f["floor"] for f in manifest["floors"]} == {1, 2}, "two floors required")
    check_plugin(manifest)
    # Complete file-only preflight for both floors before sending any native job.
    reopened = {}
    for floor in manifest["floors"]:
        check_floor_contract(floor, planned[floor["floor"]])
        reopened[floor["floor"]] = reopened_groups(floor)
    session = Path(manifest["session"])
    ready = read(session / "quiet-worker-ready.json")
    spec = importlib.util.spec_from_file_location(
        "native_queue", ROOT / "阶段1/原生接口探测/run_quiet_job.py")
    queue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(queue)
    output = dict(contract="bcp.area-under-name-native-qa.v1", version=args.version,
                  manifest=str(manifest_path), manifestSHA256=sha(manifest_path),
                  pluginCodeChanged=False, sampleOnly=True, PDFDelivery=False,
                  geometryPassed=False, floors=[], evidenceLimits=[
                      "Native annotation audit exposes AcDbText content/rotation; root bounds/grip/stretch prove rigid translation.",
                      "TDbText room-name content/font is not exposed by the native audit; its complete root is checked for rigid translation only.",
                      "AREA-TEXT reopen records prove AcDbText content/style/height against source memberDetails; width/oblique are recorded without an unavailable source-value comparison."])
    for floor in manifest["floors"]:
        control_sha = sha(floor["control"])
        job, preview, audit, steps = native_preview(floor, session, ready, queue)
        summary = verify_geometry(floor, planned[floor["floor"]], preview, audit)
        summary.update(reopened[floor["floor"]])
        for handle, metadata in summary["areaTextMetadata"].items():
            require(metadata["text"] == next(a for a in audit["nativeAnnotations"]
                    if a["handle"] == handle)["text"], "text log/SDK disagreement", handle)
        require(sha(floor["control"]) == control_sha, "review control file changed")
        summary.update(floor=floor["floor"], source=floor["source"],
                       sourceSHA256=floor["sourceSHA256"], control=floor["control"],
                       SHA256=control_sha, sourcePreview=floor["sourcePreview"],
                       sourcePreviewSHA256=sha(floor["sourcePreview"]),
                       previewJob=str(job), PDF=preview["page"]["outputPDF"], steps=steps)
        output["floors"].append(summary)
        print(json.dumps({k: summary[k] for k in
                          ("floor", "areaGroups", "areaTextMembers", "movedNames",
                           "modelRoots", "wholeTCHDimensions")}, ensure_ascii=False), flush=True)
    check_plugin(manifest)
    for floor in manifest["floors"]:
        check_floor_contract(floor, planned[floor["floor"]])
    output["geometryPassed"] = True
    if not args.no_publish:
        targets = [(floor, HERE / ("方案施工图_" + ("一层" if floor["floor"] == 1 else "二层")
                                  + "_房名下方面积确认样张_v6.dwg")) for floor in output["floors"]]
        require(all(not target.exists() for _, target in targets), "refusing to overwrite a delivered DWG")
        for floor, target in targets:
            with Path(floor["control"]).open("rb") as source, target.open("xb") as destination:
                shutil.copyfileobj(source, destination)
            require(sha(target) == floor["SHA256"], "delivered DWG copy hash mismatch")
            floor.update(DWG=str(target), DWGSHA256=sha(target))
    output["published"] = not args.no_publish
    destination = HERE / f"area-name-qa-{args.version}.json"
    destination.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    print(str(destination), flush=True)


if __name__ == "__main__":
    main()
