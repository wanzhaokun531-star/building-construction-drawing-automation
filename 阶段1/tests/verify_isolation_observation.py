"""Audit completed window isolation separately from clean exit and drawing correctness.

Reads only saved evidence. It never imports a dispatcher, contacts CAD, inspects
live processes, controls windows, or converts forced teardown into normal exit.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import ntpath
from pathlib import Path
import re


STOP_SCOPE = "Explicit StopOwnedWorker only; exe/creation time/OpenDesktop+EnumDesktopWindows PID and attested GUI thread verified immediately before termination"
SCOPE = ("Saved-evidence observation of the dedicated worker's window isolation only. "
         "Explicit owned teardown is permitted for this observation contract; it is not normal-exit success. "
         "Existing related-CAD activity requires attribution review. Drawing/business validation is separate.")


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def lines(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line]


def timestamp(value):
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("evidence timestamp lacks timezone")
    return result.astimezone(timezone.utc)


def same_path(value, expected):
    return bool(value) and Path(value).resolve() == expected.resolve()


def marker_identity(marker, nonce, pid, run, desktop):
    location = marker.get("session", marker.get("run"))
    ids = [marker[k] for k in ("pid", "process", "processId") if k in marker]
    return (marker.get("nonce") == nonce and same_path(location, run) and bool(ids)
            and all(value == pid for value in ids) and marker.get("desktop") == desktop
            and bool(marker.get("inputDesktop")) and marker.get("inputDesktop") != desktop
            and marker.get("error") is None)


def pinned_source(launch, path_key, hash_key, expected, run):
    if not same_path(launch.get(path_key), expected):
        return False
    wanted = launch.get(hash_key)
    if not isinstance(wanted, str) or not re.fullmatch(r"[A-F0-9]{64}", wanted):
        return False
    candidates = [expected, run / "candidate" / expected.name]
    return any(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest().upper() == wanted
               for path in candidates)


def inspect(path):
    result = dict(contract="stage1.window-isolation-observation.v1", evidence=str(path.resolve()),
                  windowIsolation="FAIL", normalExitProven=False, exitClassification="UNPROVEN",
                  businessValidation="NOT_ASSESSED", strictLifecycleValidation="NOT_REPLACED",
                  scope=SCOPE, assertions=0, passed=0, failed=0, failures=[], observations={})
    try:
        _inspect(path, result)
    except (OSError, ValueError, KeyError, TypeError, AttributeError, OverflowError) as error:
        result["assertions"] += 1
        result["failures"].append("incomplete or malformed evidence: " + str(error))
    result["failed"] = len(result["failures"])
    result["passed"] = result["assertions"] - result["failed"]
    if result["failures"]:
        result["windowIsolation"] = "FAIL"
    else:
        activity = (result["observations"].get("relatedShowEvents", 0)
                    + result["observations"].get("relatedForegroundEvents", 0))
        result["windowIsolation"] = "REVIEW_REQUIRED" if activity else "PASS"
    return result


def _inspect(path, result):
    def check(ok, label):
        result["assertions"] += 1
        if not ok:
            result["failures"].append(label)
        return bool(ok)

    run = path.resolve().parent
    final = read(path)
    launch = read(run / "isolation-host-receipt.json")
    pid, nonce = launch.get("processId"), launch.get("nonce", "")
    desktop = "CodexS1_" + nonce
    user_desktop = final.get("inputDesktopBefore")
    result.update(nonce=nonce, processId=pid, session=str(run), desktopName=desktop)
    check(final.get("contract") == "stage1.isolated-host-final.v1", "final supervisor contract")
    check(launch.get("contract") == "stage1.isolated-host-launch.v1"
          and launch.get("origin") == "CreateProcessW", "actual CreateProcess launch receipt")
    check(isinstance(pid, int) and not isinstance(pid, bool) and pid > 0
          and re.fullmatch(r"[0-9a-f]{32}", nonce) is not None, "positive PID and 32-hex nonce")
    check(final.get("processId") == pid and final.get("nonce") == nonce
          and same_path(final.get("session"), run) and same_path(launch.get("session"), run),
          "final and launch bound to the same PID/nonce/session")
    check(final.get("desktopName") == launch.get("desktopName") == desktop
          and launch.get("desktopPath") == "WinSta0\\" + desktop and user_desktop != desktop,
          "nonce-bound private desktop differs from user input desktop")
    check(bool(user_desktop) and launch.get("monitorDesktop") == user_desktop
          and launch.get("inputDesktopBefore") == user_desktop, "hook thread observes the actual initial input desktop")
    for field in ("workerCreated", "workerExited", "monitorStartedBeforeWorker", "monitorStarted",
                  "monitorStopped", "eventFileWritten", "desktopClosed", "processHandlesClosed"):
        check(final.get(field) is True, "completed observation lifecycle: " + field)
    check(final.get("stillRunningOwnedWorker") is False, "no worker remains beyond the observed interval")
    check(final.get("monitorError") in (None, "") and final.get("droppedEvents") == 0
          and final.get("supervisorErrors") == [], "monitor and supervisor had no errors or lost events")
    check(final.get("inputDesktopChanged") is False and final.get("inputDesktopReadErrors") == 0,
          "input desktop remained unchanged and continuously readable")
    check(final.get("switchDesktopCalled") is False and final.get("userCadTouched") is False
          and launch.get("switchDesktopCalled") is False and launch.get("userCadTouched") is False,
          "pinned supervisor declares no desktop switch or user-CAD control")
    check(launch.get("metadataState") == "complete" and launch.get("metadataErrors") == [],
          "launch identity and source metadata completed successfully")
    created, monitored, finished = (timestamp(launch["creationUtc"]),
                                   timestamp(launch["monitorStartedUtc"]), timestamp(final["recorded"]))
    check(monitored <= created <= finished and launch.get("monitorStartedBeforeWorker") is True,
          "actual hook timestamp precedes process creation and the final observation")
    check(str(launch.get("creationFileTime", "")).isdigit()
          and launch.get("initialThreadId", 0) > 0 and bool(launch.get("executable")),
          "launch records immutable process creation identity and initial thread")
    controller = Path(__file__).resolve().parents[1] / "原生接口探测/启动隔离桌面宿主.ps1"
    helper = controller.with_name("IsolatedCadHostNative.cs")
    check(pinned_source(launch, "controllerPath", "controllerSHA256", controller, run),
          "executed supervisor bytes match launch fingerprint in live source or candidate archive")
    check(pinned_source(launch, "nativeHelperPath", "nativeHelperSHA256", helper, run),
          "native launcher/monitor bytes match launch fingerprint in live source or candidate archive")
    boot = Path(launch["bootScript"])
    check(boot.resolve().parent == run and boot.is_file()
          and hashlib.sha256(boot.read_bytes()).hexdigest().upper() == launch.get("bootSHA256"),
          "boot script remains within the session and matches launch fingerprint")

    def marker(name_key):
        name = launch[name_key]
        if not check(Path(name).name == name and (run / name).resolve().parent == run,
                     name_key + " remains a simple session-local filename"):
            return {}
        return read(run / name)

    ready = marker("readyMarkerName")
    check(final.get("readyIdentityConfirmed") is True
          and ready.get("contract") == "stage1.quiet-worker-ready.v1"
          and ready.get("timerStarted") is True and ready.get("comUsed") is False
          and marker_identity(ready, nonce, pid, run, desktop), "native ready independently binds the isolated worker")
    quit_path = run / launch["quitMarkerName"]
    if final.get("nativeQuitConfirmed") is True:
        quit_marker = marker("quitMarkerName")
        check(marker_identity(quit_marker, nonce, pid, run, desktop), "native quit-preparation marker identity")
        check(quit_marker.get("contract") in ("stage1.quiet-worker-exit.v1", "stage1.quiet-worker-exit.v2")
              and (quit_marker.get("contract") != "stage1.quiet-worker-exit.v2"
                   or quit_marker.get("quitQueueStatus") == 0), "quit marker only establishes an accepted preparation/queue")
    else:
        check(not quit_path.exists(), "quit summary does not hide an unvalidated marker")

    stops = sorted(run.glob("isolation-stop-*.json"))
    if final.get("forcedTermination") is True:
        result["exitClassification"] = "EXPLICIT_OWNED_TERMINATION_70"
        check(final.get("workerExitCode") == 70 and len(stops) == 1,
              "forced observation completion requires exactly one explicit owned stop receipt and actual exit 70")
        for stop_path in stops:
            stop = read(stop_path)
            check(stop_path.resolve().parent == run
                  and stop.get("contract") == "stage1.isolated-host-explicit-stop.v1"
                  and stop.get("nonce") == nonce and stop.get("processId") == pid
                  and stop.get("creationFileTime") == launch.get("creationFileTime")
                  and stop.get("desktopName") == desktop and stop.get("exitCode") == 70
                  and stop.get("forcedTermination") is True and stop.get("userCadTouched") is False
                  and stop.get("scope") == STOP_SCOPE
                  and created <= timestamp(stop["recorded"]) <= finished,
                  "explicit stop fields, declared exe/time/desktop guard and timestamp match launch identity")
            check(("session" not in stop or same_path(stop["session"], run))
                  and ("executable" not in stop or stop["executable"] == launch.get("executable")),
                  "optional stop session/exe fields, if supplied, agree with launch")
            result["stopEvidence"] = dict(path=str(stop_path), receiptContainsExecutable="executable" in stop,
                receiptContainsSession="session" in stop, executableFromLaunch=launch.get("executable"),
                ownershipClaim=stop.get("scope"), normalExit=False)
    else:
        check(final.get("forcedTermination") is False and final.get("workerExitCode") == 0
              and not stops and final.get("nativeQuitConfirmed") is True,
              "normal completion requires process exit 0, quit preparation and no forced-stop receipt")
        result["normalExitProven"] = final.get("workerExited") is True and final.get("workerExitCode") == 0 and not stops
        result["exitClassification"] = "NORMAL_EXIT_0" if result["normalExitProven"] else "UNPROVEN"

    events = lines(run / "isolation-user-events.jsonl")
    samples = lines(run / "isolation-window-samples.jsonl")
    related_file = lines(run / "isolation-related-cad-events.jsonl")
    check([event.get("Sequence") for event in events] == list(range(1, len(events) + 1)),
          "raw WinEvent sequence is contiguous, unique and ordered")
    check(all(event.get("MonitorDesktop") == user_desktop
              and event.get("Event") in (3, 32768, 32769, 32770, 32771)
              and monitored <= timestamp(event["Utc"]) <= finished for event in events),
          "all raw events belong to the monitored interval and hook desktop")
    check(len(samples) == final.get("sampleCount") and bool(samples)
          and [sample.get("sequence") for sample in samples] == list(range(1, len(samples) + 1)),
          "every periodic sample is retained in order")
    sample_times = [timestamp(sample["utc"]) for sample in samples]
    gaps = [(right - left).total_seconds() for left, right in zip(sample_times, sample_times[1:])]
    check(bool(sample_times) and created <= sample_times[0] <= sample_times[-1] <= finished
          and (sample_times[0] - created).total_seconds() <= 5
          and (finished - sample_times[-1]).total_seconds() <= 5
          and all(0 <= gap <= 5 for gap in gaps), "samples cover launch-to-exit observation without gaps over 5 seconds")
    check(all(sample.get("processId") == pid and sample.get("privateDesktop") == desktop
              and sample.get("inputDesktop") == user_desktop for sample in samples),
          "every raw sample binds the worker and unchanged input/private desktops")
    windows = [window for sample in samples for window in sample.get("workerWindows", [])]
    check(bool(windows) and all(window.get("EnumerationDesktop") == desktop
          and window.get("PidAtEnumeration") == pid and window.get("ThreadAtEnumeration", 0) > 0
          and window.get("Pid") in (0, pid)
          and (window.get("ThreadDesktop") == desktop if window.get("ThreadDesktopKnown") is True else
               window.get("ThreadDesktopKnown") is False and window.get("ThreadDesktop") is None
               and window.get("ThreadDesktopError", 0) != 0) for window in windows),
          "OpenDesktop+EnumDesktopWindows proves own windows; foreign-thread access-denied is preserved as unknown")
    worker_fg = sum(event.get("Pid") == pid and event.get("Event") == 3 for event in events)
    worker_show = sum(event.get("Pid") == pid and event.get("Event") == 32770 for event in events)
    foreground_samples = sum(sample.get("foreground", {}).get("Pid") == pid for sample in samples)
    check(worker_fg == final.get("workerForegroundEvents") == 0, "raw events show no worker foreground on user desktop")
    check(worker_show == final.get("workerUserDesktopShowEvents") == 0, "raw events show no worker SHOW on user desktop")
    check(foreground_samples == final.get("workerForegroundSamples") == 0, "all foreground samples exclude the owned worker")
    related_ids = set(launch.get("relatedProcessIdsAtStart", []))
    def related(event):
        stem = ntpath.splitext(ntpath.basename(event.get("ProcessImage") or ""))[0]
        return event.get("Pid") != pid and (event.get("Pid") in related_ids or re.match(r"^(acad|tarch|tch|t20)", stem, re.I))
    related_rows = [event for event in events if related(event)]
    related_fg = sum(event.get("Event") == 3 for event in related_rows)
    related_show = sum(event.get("Event") == 32770 for event in related_rows)
    check(related_file == related_rows, "dedicated related-CAD log equals independently filtered raw events")
    check(final.get("relatedCadEvents") == len(related_rows)
          and final.get("relatedCadForegroundEvents") == related_fg
          and final.get("relatedCadShowEvents") == related_show
          and final.get("relatedCadEventsNeedReview") is bool(related_fg or related_show),
          "related-CAD counters and review flag agree with actual events")
    tips = lines(run / "isolation-startup-tip-actions.jsonl")
    enabled = launch.get("closeOwnStartupTipsEnabled") is True
    check(final.get("closeOwnStartupTipsEnabled") is enabled
          and final.get("startupTipActions") == len(tips)
          and final.get("startupTipPosts") == sum(row.get("action", {}).get("Posted") is True for row in tips)
          and (enabled or not tips), "startup-tip actions and opt-in match raw log")
    handles = [row.get("action", {}).get("Hwnd") for row in tips]
    check(len(set(handles)) == len(handles) and all(row.get("nonce") == nonce
          and same_path(row.get("session"), run) and row.get("sequence") == index + 1
          and row.get("action", {}).get("ProcessId") == pid and row["action"].get("DesktopName") == desktop
          and row["action"].get("ClassName") == "#32770" and row["action"].get("Title") == "日积月累"
          and row["action"].get("Matched") is True and row["action"].get("Posted") is True
          and row["action"].get("Error") == 0
          and row["action"].get("Status") == "posted_WM_CLOSE_exact_owned_startup_tip"
          and created <= timestamp(row["action"]["Utc"]) <= finished for index, row in enumerate(tips)),
          "only exact owned benign startup tips got a single logged WM_CLOSE per handle")
    result["observations"] = dict(sampleCount=len(samples), rawEventCount=len(events),
        privateWindowObservations=len(windows), firstSample=samples[0]["utc"] if samples else None,
        lastSample=samples[-1]["utc"] if samples else None, maximumSampleGapSeconds=max(gaps, default=0),
        workerShowEvents=worker_show, workerForegroundEvents=worker_fg, workerForegroundSamples=foreground_samples,
        relatedEvents=len(related_rows), relatedShowEvents=related_show, relatedForegroundEvents=related_fg,
        relatedActivity=[{key:event.get(key) for key in ("Sequence", "Utc", "Pid", "Event", "Name", "Hwnd", "Owner", "ClassName")}
                         for event in related_rows if event.get("Event") in (3, 32770, 32771)],
        unattributedShowOrForegroundEvents=sum(event.get("Pid") in (None, 0)
             and event.get("Event") in (3, 32770) for event in events),
        startupTipPosts=final.get("startupTipPosts"), timedOut=final.get("timedOut"),
        limitation="No claim about causes of existing-CAD activity or PID-unattributed transient events; no drawing/business or normal-exit substitution.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path, help="completed isolation-final.json")
    parser.add_argument("--output", type=Path, help="write a new observation report; existing files are refused")
    args = parser.parse_args()
    result = inspect(args.evidence)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(payload + "\n")
    print(payload)
    return {"PASS":0, "REVIEW_REQUIRED":2, "FAIL":1}[result["windowIsolation"]]


if __name__ == "__main__":
    raise SystemExit(main())
