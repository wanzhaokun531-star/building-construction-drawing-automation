"""Validate three named saved-fixture previews; visible content needs rendered review."""
import hashlib
import json
import sys
from pathlib import Path

from verify_native_plot import near, ENGINE_STAGES, CONFIG_STAGES


FIXTURES = {
    "nativeTextPreview": {
        "run": "coverage-text-20261029-003350-351", "dwg": "text-bridge-generated.dwg",
        "report": "text-bridge-native.json", "count": 217, "window": [39000, 17000, 47000, 22000],
        "dwgHash": "D18096CD2294124B428F2CB52A82B3F4F699CAE96CE68E6B881110CBB816C008",
        "reportHash": "D466CF9AC44F78392D5A0D394F8B7E165E8F71733B54E887EB4F4D65BAEC7A31",
        "objects": {"D0DA": "TDbText", "D0DB": "AcDbText", "D0DC": "TDbText", "D0DD": "AcDbText"}},
    "nativeCombinePreview": {
        "run": "coverage-combine-20261028-234100-432", "dwg": "dimension-combine-generated.dwg",
        "report": "dimension-bridge-native.json", "count": 214, "window": [12000, 12000, 36000, 35000],
        "dwgHash": "56861B0A037FB28201EC09440C7CF6F0A163C44CEA0369405C512280FDC53C27",
        "reportHash": "5B3C6A50D4B39A2999F7471ECCF272B229B910FB8D425A53659FC32A342C5E1D",
        "objects": {"D0D3": "TDbDimension2"}},
    "nativeGeometryPreview": {
        "run": "coverage-geometry-20261028-232845-683", "dwg": "standard-geometry-generated.dwg",
        "report": "standard-geometry-native.json", "count": 220, "window": [48000, 18000, 68000, 32000],
        "dwgHash": "03A36511806ACFF2FC5416B21FBFE1A57EC42D4D26B18CAA26FE7579E9E1F62F",
        "reportHash": "E5AB4381E510D2DE138558D08A26DBAD99387383E758958507D2C1D92A3096C0",
        "objects": {"D0D8": "AcDbPolyline", "D0D9": "AcDbPolyline", "D0DA": "AcDbHatch",
                    "D0DB": "AcDbText", "D0DC": "AcDbMText", "D0DE": "AcDbLeader", "D0E5": "AcDbBlockReference"}},
}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest().upper()


def main(path):
    count, failures = 0, []

    def check(ok, label):
        nonlocal count
        count += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def geometry(a, b, label):
        for key in ("handle", "type", "layer", "proxy", "extentStatus", "min", "max", "curve", "grip", "stretch"):
            check(key in a and key in b and near(a[key], b[key]), label + ": " + key)

    try:
        if not path.is_file():
            print("FAIL: native preview host evidence missing")
            print("assertions=1 passed=0 failed=1 result=FAIL")
            return 1
        d = json.loads(path.read_text(encoding="utf-8-sig"))
        check(d.get("contract") == "stage1.native-preview.v1" and d.get("api") == "ObjectARX native plot; no COM", "native preview contract")
        check(d.get("error") is None, "no native operation error")
        cid = d.get("fixtureId")
        if not check(cid in FIXTURES, "strict named fixture"):
            raise ValueError("unknown fixture")
        fixture, run = FIXTURES[cid], path.resolve().parent
        root = Path(__file__).resolve().parents[1]
        source_run = root / "runs" / fixture["run"]
        source, report = source_run / fixture["dwg"], source_run / fixture["report"]
        for key, actual in (("sourceDWG", source), ("sourceReport", report)):
            check(Path(d.get(key, "")).resolve() == actual.resolve(), "fixed source identity: " + key)
        check(d.get("expectedSourceSHA256") == fixture["dwgHash"] and d.get("expectedSourceReportSHA256") == fixture["reportHash"], "fixed source hash contract")
        check(source.is_file() and digest(source) == fixture["dwgHash"], "saved fixture DWG SHA256")
        check(report.is_file() and digest(report) == fixture["reportHash"], "original native report SHA256")
        control = run / "control.dwg"
        for key in ("controlDWG", "currentDWG", "mdiActiveDWG", "workingDWG"):
            check(Path(d.get(key, "")).resolve() == control, "owned copy: " + key)
        check(control.is_file() and digest(control) == fixture["dwgHash"], "actual owned control equals the saved fixture bytes")
        check(d.get("dwgSaved") is False and d.get("isApplicationContext") is False and d.get("modelLayoutCurrent") is True, "document Model context and no DWG save")
        check(d.get("initialModelCount") == d.get("finalModelCount") == d.get("expectedModelCount") == fixture["count"], "complete fixed fixture count")
        check(d.get("windowWCS") == fixture["window"] and d.get("fullBuildingCoverage") is False, "strict partial preview window")
        check(d.get("windowTransformStatuses") == [5100]*4, "all WCS corners converted to DCS")
        w = d.get("windowDCS", [])
        check(len(w) == 4 and near([w[2]-w[0], w[3]-w[1]], [fixture["window"][2]-fixture["window"][0], fixture["window"][3]-fixture["window"][1]]), "DCS dimensions preserve requested window")
        objects = d.get("fixtureObjects", [])
        check(len(objects) == len(fixture["objects"]) and {o.get("handle"): o.get("expectedType") for o in objects} == fixture["objects"], "exact fixture handle/type manifest")
        for obj in objects:
            h = obj.get("handle")
            check(obj.get("lookupStatus") == obj.get("openStatus") == obj.get("extentStatus") == 0
                  and obj.get("type") == fixture["objects"].get(h) and obj.get("proxy") is False
                  and obj.get("sameDatabase") is True and obj.get("ownerIsModel") is True
                  and obj.get("enumeratedInModel") is True and obj.get("insideWindow") is True, "real native fixture object: " + str(h))
            lo, hi = obj.get("min", []), obj.get("max", [])
            check(len(lo) == len(hi) == 3 and lo[0] >= fixture["window"][0]-1e-6 and lo[1] >= fixture["window"][1]-1e-6
                  and hi[0] <= fixture["window"][2]+1e-6 and hi[1] <= fixture["window"][3]+1e-6, "entire native fixture object inside window: " + str(h))
        check(d.get("fullFixtureContained") is True, "native containment guard passed")
        check(d.get("globalsBefore") == d.get("globalsAfter") and isinstance(d.get("globalsBefore"), dict), "globals restored exactly")
        for name in ("BACKGROUNDPLOT", "SECURELOAD", "PICKFIRST", "FILEDIA", "TILEMODE"):
            check(d.get("globalsBefore", {}).get(name, {}).get("getStatus") == 5100, "actual global getter: " + name)
        bg = d.get("backgroundOverride", {})
        check(bg.get("processPlotStateBefore") == 0 and bg.get("getStatus") == 5100 and bg.get("oldRestype") == 5003
              and bg.get("oldValue") == d.get("globalsBefore", {}).get("BACKGROUNDPLOT", {}).get("value"), "capture original background mode while idle")
        check(bg.get("setAttempted") is True and bg.get("setStatus") == bg.get("readbackStatus") == 5100
              and bg.get("newRestype") == 5003 and bg.get("requestedValue") == bg.get("newValue") == 0, "foreground override and readback")
        check(bg.get("restoreAttempted") is True and bg.get("restoreStatus") == bg.get("restoreReadbackStatus") == 5100
              and bg.get("restoredRestype") == 5003 and bg.get("restoredValue") == bg.get("oldValue")
              and bg.get("restored") is True and bg.get("restoreThrew") is False, "actual background restoration")
        check(isinstance(d.get("layoutBefore"), dict) and d.get("layoutBefore") == d.get("layoutAfter"), "layout unchanged")
        before, after = d.get("originalBefore", []), d.get("originalAfter", [])
        if check(len(before) == len(after) == fixture["count"], "all model geometry captured"):
            aa, bb = ({o.get("handle"): o for o in values} for values in (before, after))
            check(len(aa) == len(bb) == fixture["count"] and set(aa) == set(bb), "all model identities retained")
            for h in aa.keys() & bb.keys():
                geometry(aa[h], bb[h], "unchanged model/" + h)
            source_data = json.loads(report.read_text(encoding="utf-8-sig"))
            if cid == "nativeTextPreview":
                source_geometry = [o["geometry"] for o in source_data["originalSaved"]+source_data["savedAdded"]]
            elif cid == "nativeCombinePreview":
                source_geometry = source_data["savedOriginalGeometry"]+[source_data["saved"]]
            else:
                source_geometry = source_data["originalSaved"]
            for obj in source_geometry:
                h = obj["handle"]
                if check(h in aa, "saved source member present/" + h):
                    geometry(obj, aa[h], "saved source geometry/" + h)
        c = d.get("page", {})
        check(c.get("id") == cid and c.get("requestedPaperMM") == [420, 297], "one A3 page with a distinct fixture ID")
        check(c.get("error") is None and c.get("copySettingsStatus") == c.get("validateStatus") == c.get("deviceListStatus") == 0
              and c.get("isValidated") is True and "DWG To PDF.pc3" in c.get("devices", []), "real PDF device and validated override")
        check(c.get("matchingPolicy") == 2 and c.get("processPlotStateBefore") == c.get("processPlotStateAfter") == 0, "existing media and plot idle")
        for key in CONFIG_STAGES:
            check(c.get("configurationStatuses", {}).get(key) == 0, "configuration/" + key)
        for key in ENGINE_STAGES:
            check(c.get("engineStatuses", {}).get(key) == 0, "engine/" + key)
        check(c.get("engineDestroyed") is True and c.get("cleanupThrew") is False and c.get("pipelineComplete") is True, "completed and destroyed native pipeline")
        for key in ("settings", "validatedSettings"):
            s = c.get(key, {})
            check(all(s.get(k) == 0 for k in ("deviceStatus", "mediaStatus", "paperSizeStatus", "marginsStatus", "windowStatus", "scaleStatus")), key + ": actual getters")
            check(s.get("device") == "DWG To PDF.pc3" and bool(s.get("canonicalMedia"))
                  and near(sorted(s.get("paperMM", [])), [297, 420], 0.1), key + ": A3 media")
            check(s.get("paperUnits") == 1 and s.get("plotType") == 4 and s.get("useStandardScale") is False
                  and near(s.get("scale"), [1, 100]) and near(s.get("windowDCS"), w), key + ": exact 1:100 window")
            dims, margins, rotation = s.get("paperMM", []), s.get("marginsMM", []), s.get("rotation")
            if check(len(dims) == 2 and len(margins) == 4 and rotation in (0, 1), key + ": explicit axes/margins"):
                printable = [dims[0]-margins[0]-margins[2], dims[1]-margins[1]-margins[3]]
                logical = dims if rotation == 0 else list(reversed(dims))
                if rotation == 1:
                    printable.reverse()
                check(logical[0] >= logical[1] and printable[0] >= (fixture["window"][2]-fixture["window"][0])/100-0.1
                      and printable[1] >= (fixture["window"][3]-fixture["window"][1])/100-0.1, key + ": entire window fits at 1:100")
        pdf = run / (cid+".pdf")
        check(Path(c.get("outputPDF", "")).resolve() == pdf and pdf.is_file() and c.get("fileObserved") is True, "isolated observed PDF")
        check(pdf.stat().st_size == c.get("fileBytes") and pdf.stat().st_size > 5 and pdf.read_bytes()[:5] == b"%PDF-", "PDF size and signature")
        from pypdf import PdfReader
        pages = PdfReader(str(pdf)).pages
        check(len(pages) == 1, "actual one-page PDF")
        if pages:
            check(near(sorted([float(pages[0].mediabox.width)*25.4/72, float(pages[0].mediabox.height)*25.4/72]), [297, 420], 0.5*25.4/72+1e-6), "actual PDF MediaBox is A3 within half-point quantization")
        check(d.get("visualReview") == "pending independent rendering and inspection", "no unperformed visual/font claim")
    except (OSError, ValueError, TypeError, KeyError, IndexError, AttributeError, ImportError) as error:
        check(False, "unreadable/incomplete preview evidence: " + str(error))
    for failure in failures:
        print("FAIL:", failure)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: saved-fixture identity, native local A3 pipeline and unchanged model/layout/globals. Visible text, clipping and fonts require rendered review.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify_native_preview.py <native*Preview.json>")
    raise SystemExit(main(Path(sys.argv[1])))
