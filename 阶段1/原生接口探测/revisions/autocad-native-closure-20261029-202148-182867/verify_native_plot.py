"""Verify native plot completion/settings; PDF appearance needs a separate review.

The checks deliberately reject a PDF that exists after a failed pipeline, Fit scale,
wrong paper dimensions, changed layout/globals, or any changed scheme geometry.
"""
import json
import math
import sys
from pathlib import Path


PAPERS = {"A3-1-100": [420, 297], "A2-1-100": [594, 420], "A1-1-100": [841, 594]}
WINDOW = [12000, 12000, 36000, 35000]
ENGINE_STAGES = ("create", "beginPlot", "beginDocument", "beginPage", "beginGraphics",
                 "endGraphics", "endPage", "endDocument", "endPlot")
CONFIG_STAGES = ("setDevice", "setMedia", "setPaperUnits", "setRotation", "setPlotType",
                 "setWindow", "setStandardScale", "setScale", "setCentered")


def near(a, b, tolerance=1e-6):
    if isinstance(a, dict):
        return isinstance(b, dict) and a.keys() == b.keys() and all(near(a[k], b[k], tolerance) for k in a)
    if isinstance(a, (list, tuple)):
        return isinstance(b, (list, tuple)) and len(a) == len(b) and all(near(x, y, tolerance) for x, y in zip(a, b))
    if isinstance(a, (int, float)) and not isinstance(a, bool):
        return isinstance(b, (int, float)) and math.isfinite(a) and math.isfinite(b) and abs(a-b) <= tolerance
    return a == b


def verify(path):
    assertions, failures = 0, []

    def check(ok, label):
        nonlocal assertions
        assertions += 1
        if not ok:
            failures.append(label)
        return bool(ok)

    def settings(s, paper, label):
        if not check(isinstance(s, dict), label + ": actual settings available"):
            return
        for key in ("deviceStatus", "mediaStatus", "paperSizeStatus", "marginsStatus", "windowStatus", "scaleStatus"):
            check(s.get(key) == 0, label + ": " + key)
        check(s.get("device") == "DWG To PDF.pc3", label + ": enumerated PDF device")
        check(bool(s.get("canonicalMedia")), label + ": canonical media recorded")
        dims = s.get("paperMM", [])
        check(isinstance(dims, list) and near(sorted(dims), sorted(paper), 0.1), label + ": exact ISO paper size")
        check(s.get("paperUnits") == 1 and s.get("plotType") == 4, label + ": millimetres and window")
        check(s.get("useStandardScale") is False and near(s.get("scale"), [1, 100]), label + ": strict custom 1:100, no Fit")
        check(s.get("rotation") in (0, 1), label + ": explicit landscape rotation")
        if len(dims) == 2:
            logical = dims if s.get("rotation") == 0 else list(reversed(dims))
            check(logical[0] >= logical[1], label + ": landscape paper axes")
            margins = s.get("marginsMM", [])
            if check(len(margins) == 4, label + ": printable margins recorded"):
                printable = [dims[0]-margins[0]-margins[2], dims[1]-margins[1]-margins[3]]
                if s.get("rotation") == 1:
                    printable.reverse()
                check(printable[0] >= 240-0.1 and printable[1] >= 240-0.1, label + ": window fits at exact scale")
        check(near(s.get("windowDCS"), d.get("windowDCS")), label + ": actual DCS plot window")

    if not check(path.is_file(), "native PDF host evidence missing"):
        return assertions, failures
    d = json.loads(path.read_text(encoding="utf-8-sig"))
    run = path.resolve().parent
    check(d.get("contract") == "stage1.native-plot.v1", "contract")
    check(d.get("api") == "ObjectARX native plot; no COM", "native plot API")
    check(d.get("error") is None, "no top-level operation error")
    for key in ("controlDWG", "currentDWG", "mdiActiveDWG", "workingDWG"):
        check(Path(d.get(key, "")).resolve() == run / "control.dwg", "owned current/MDI/working control: " + key)
    check(d.get("isApplicationContext") is False and d.get("modelLayoutCurrent") is True, "document context and current Model")
    check(d.get("initialModelCount") == d.get("finalModelCount") == 213, "full scheme213 retained")
    check(d.get("globalsBefore") == d.get("globalsAfter") and bool(d.get("globalsBefore")), "global variables unchanged")
    for name in ("BACKGROUNDPLOT", "SECURELOAD", "PICKFIRST", "FILEDIA", "TILEMODE"):
        check(d.get("globalsBefore", {}).get(name, {}).get("getStatus") == 5100, "global read: " + name)
    background = d.get("backgroundOverride", {})
    check(background.get("processPlotStateBefore") == 0, "temporary foreground override only starts while plot subsystem idle")
    check(background.get("getStatus") == 5100 and background.get("oldRestype") == 5003
          and background.get("oldValue") == d.get("globalsBefore", {}).get("BACKGROUNDPLOT", {}).get("value"), "foreground override captured actual original value")
    check(background.get("setAttempted") is True and background.get("setStatus") == 5100
          and background.get("requestedValue") == 0, "foreground override setter succeeded")
    check(background.get("readbackStatus") == 5100 and background.get("newRestype") == 5003
          and background.get("newValue") == 0, "foreground mode independently read back")
    check(background.get("restoreAttempted") is True and background.get("restoreStatus") == 5100, "original background mode restore succeeded")
    check(background.get("restoreReadbackStatus") == 5100 and background.get("restoredRestype") == 5003
          and background.get("restoredValue") == background.get("oldValue") and background.get("restored") is True
          and background.get("restoreThrew") is False, "restored value independently verified; no permanent override")
    check(d.get("layoutBefore") == d.get("layoutAfter") and isinstance(d.get("layoutBefore"), dict), "layout settings unchanged; overrides only")
    check(d.get("windowWCS") == WINDOW and d.get("fullBuildingCoverage") is False, "explicit partial window fixture")
    check(d.get("windowTransformStatuses") == [5100]*4, "all four WCS corners transformed to DCS")
    window = d.get("windowDCS", [])
    if check(len(window) == 4, "actual DCS window"):
        check(near([window[2]-window[0], window[3]-window[1]], [24000, 23000]), "unrotated local fixture window dimensions")
    originals = [d.get(k, []) for k in ("originalBefore", "originalAfter")]
    if check(all(len(x) == 213 for x in originals), "all original geometry captured"):
        aa, bb = [{x.get("handle"): x for x in seq} for seq in originals]
        check(len(aa) == 213 and set(aa) == set(bb), "original handle set retained")
        for h in aa.keys() & bb.keys():
            for key in ("type", "layer", "proxy", "extentStatus", "min", "max", "grip", "stretch", "curve"):
                check(near(aa[h].get(key), bb[h].get(key)), "original " + h + ": " + key)
    cases = d.get("cases", [])
    check(len(cases) == 3 and {x.get("id") for x in cases} == set(PAPERS), "three distinct A3/A2/A1 pages")
    for c in cases:
        cid = c.get("id", "unknown")
        if not check(cid in PAPERS, cid + ": known paper fixture"):
            continue
        check(c.get("requestedPaperMM") == PAPERS[cid], cid + ": paper request")
        check(c.get("error") is None, cid + ": no operation error")
        check(c.get("deviceListStatus") == 0 and "DWG To PDF.pc3" in c.get("devices", []), cid + ": real PDF device enumeration")
        check(c.get("copySettingsStatus") == 0 and c.get("validateStatus") == 0 and c.get("isValidated") is True, cid + ": actual override validation")
        check(c.get("matchingPolicy") == 2, cid + ": existing-media matching only; no Custom/TmpCustom policy")
        for key in CONFIG_STAGES:
            check(c.get("configurationStatuses", {}).get(key) == 0, cid + ": " + key)
        check(c.get("processPlotStateBefore") == c.get("processPlotStateAfter") == 0, cid + ": no competing or retained plot")
        for key in ENGINE_STAGES:
            check(c.get("engineStatuses", {}).get(key) == 0, cid + ": native engine " + key)
        check(c.get("engineDestroyed") is True and c.get("pipelineComplete") is True, cid + ": full pipeline completed and destroyed")
        settings(c.get("settings"), PAPERS[cid], cid + "/override")
        settings(c.get("validatedSettings"), PAPERS[cid], cid + "/validated")
        pdf = Path(c.get("outputPDF", ""))
        check(pdf.is_absolute() and pdf.resolve().parent == run and pdf.name == cid + ".pdf", cid + ": isolated unique PDF path")
        if check(pdf.is_file() and c.get("fileObserved") is True, cid + ": output observed after engine completion"):
            check(pdf.stat().st_size == c.get("fileBytes") and pdf.stat().st_size > 5, cid + ": real output size")
            with pdf.open("rb") as stream:
                check(stream.read(5) == b"%PDF-", cid + ": PDF signature")
    check(d.get("multiPage", {}).get("status") == "not attempted", "multi-page job has no untested success claim")
    return assertions, failures


def main(path):
    try:
        assertions, failures = verify(path)
    except (OSError, ValueError, TypeError, KeyError) as exc:
        assertions, failures = 1, ["unreadable/invalid native evidence: " + str(exc)]
    for failure in failures[:100]:
        print("FAIL:", failure)
    print(f"assertions={assertions} passed={assertions-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Scope: native plot completion, true media/1:100 settings and input protection. PDF pages/fonts/lineweights/content require separate inspection.")
    return int(bool(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_native_plot.py <run/native-plot.json>")
    raise SystemExit(main(Path(sys.argv[1])))
