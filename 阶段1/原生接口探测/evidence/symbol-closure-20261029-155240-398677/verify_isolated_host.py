"""Validate isolation evidence; a missing/failed CAD host is never a quiet-host PASS."""
import argparse
import json
import hashlib
import re
from datetime import datetime
from pathlib import Path


def validate(data):
    failures = []
    checks = 0
    def check(ok, text):
        nonlocal checks
        checks += 1
        if not ok:
            failures.append(text)
    check(data.get("contract") == "stage1.isolated-host-final.v1", "final supervisor contract")
    check(data.get("workerCreated") is True, "a real owned worker was created")
    check(data.get("workerExited") is True, "owned worker process exited")
    check(data.get("workerExitCode") == 0, "owned worker exited normally")
    check(data.get("nativeQuitConfirmed") is True, "native quit marker identity confirmed")
    check(data.get("forcedTermination") is False, "normal completion was not termination")
    check(data.get("timedOut") is False, "no hidden timeout counted as success")
    check(data.get("monitorStartedBeforeWorker") is True, "monitor started before CreateProcess")
    check(data.get("monitorStarted") is True and data.get("monitorStopped") is True,
          "hook/message-pump lifecycle completed")
    check(data.get("monitorError") in (None, ""), "no hook/message-pump error")
    check(data.get("droppedEvents") == 0, "no event overflow")
    check(data.get("eventFileWritten") is True and data.get("sampleCount", 0) > 0,
          "continuous events and samples were actually recorded")
    check(data.get("workerForegroundEvents") == 0, "owned worker did not take user-desktop foreground")
    check(data.get("workerForegroundSamples") == 0, "foreground samples never point to owned worker")
    check(data.get("workerUserDesktopShowEvents") == 0, "owned worker did not show on user desktop")
    check(data.get("inputDesktopChanged") is False and data.get("inputDesktopReadErrors") == 0,
          "user input desktop stayed observable and unchanged")
    nonce = data.get("nonce", "")
    check(len(nonce) == 32 and all(c in "0123456789abcdef" for c in nonce), "32-hex launch nonce")
    check(data.get("desktopName") == "CodexS1_" + nonce, "private desktop bound to nonce")
    check(data.get("desktopName") != data.get("inputDesktopBefore"), "worker desktop differs from user input desktop")
    check(data.get("desktopClosed") is True and data.get("processHandlesClosed") is True,
          "owned desktop and process handles released")
    check(data.get("switchDesktopCalled") is False and data.get("userCadTouched") is False,
          "no desktop switch or user-CAD control")
    check(data.get("readyIdentityConfirmed") is True, "native ready confirms nonce/session/PID")
    check(data.get("supervisorErrors") == [], "supervisor had no unreported failure")
    return checks, failures


def main(path):
    if not path.is_file():
        print("FAIL: isolated host execution evidence missing")
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    checks, failures = validate(data)
    def check(ok, label):
        nonlocal checks
        checks += 1
        if not ok:
            failures.append(label)
        return ok
    run = path.resolve().parent
    launch = json.loads((run / "isolation-host-receipt.json").read_text(encoding="utf-8-sig"))
    pid, nonce = launch.get("processId"), launch.get("nonce")
    check(launch.get("contract") == "stage1.isolated-host-launch.v1"
          and launch.get("origin") == "CreateProcessW", "actual owned CreateProcess launch receipt")
    check(data.get("processId") == pid and data.get("nonce") == nonce,
          "final evidence bound to actual launch PID/nonce")
    check(Path(launch.get("session", "")).resolve() == run,
          "launch session bound to evidence directory")
    check(launch.get("desktopName") == data.get("desktopName")
          and launch.get("monitorDesktop") == data.get("inputDesktopBefore"),
          "launch desktop and actual hook-thread desktop bound")
    created = datetime.fromisoformat(launch["creationUtc"].replace("Z", "+00:00"))
    monitored = datetime.fromisoformat(launch["monitorStartedUtc"].replace("Z", "+00:00"))
    check(monitored <= created, "actual hook started before native process creation time")
    events = [json.loads(line) for line in (run / "isolation-user-events.jsonl").read_text(encoding="utf-8-sig").splitlines() if line]
    samples = [json.loads(line) for line in (run / "isolation-window-samples.jsonl").read_text(encoding="utf-8-sig").splitlines() if line]
    seq = [row["Sequence"] for row in events]
    check(len(seq) == len(set(seq)) and seq == sorted(seq), "recorded WinEvent sequence is unique and ordered")
    actual_fg = sum(row.get("Pid") == pid and row.get("Event") == 3 for row in events)
    actual_show = sum(row.get("Pid") == pid and row.get("Event") == 0x8002 for row in events)
    actual_fg_samples = sum(row.get("foreground", {}).get("Pid") == pid for row in samples)
    check(actual_fg == data.get("workerForegroundEvents") == 0,
          "raw user-desktop events show no worker foreground")
    check(actual_show == data.get("workerUserDesktopShowEvents") == 0,
          "raw user-desktop events show no worker SHOW, including brief SHOW")
    check(actual_fg_samples == data.get("workerForegroundSamples") == 0,
          "raw foreground samples do not point to worker")
    check(len(samples) == data.get("sampleCount") and len(samples) > 0,
          "all periodic private-window/user-foreground samples retained")
    check(all(row.get("inputDesktop") == data.get("inputDesktopBefore")
              for row in samples), "all actual input-desktop observations stayed unchanged")
    check(all(row.get("processId") == pid and row.get("privateDesktop") == data.get("desktopName")
              for row in samples), "all samples bound to owned PID and private desktop")
    check(all(row.get("MonitorDesktop") == data.get("inputDesktopBefore") for row in events),
          "all raw events belong to monitored user desktop")
    windows = [window for row in samples for window in row.get("workerWindows", [])]
    check(bool(windows), "real worker windows were observed on private desktop")
    check(all(window.get("EnumerationDesktop") == data.get("desktopName")
              and window.get("PidAtEnumeration") == pid
              and window.get("ThreadAtEnumeration", 0) > 0
              and window.get("Pid") in (0, pid)
              and (window.get("ThreadDesktop") == data.get("desktopName")
                   if window.get("ThreadDesktopKnown") else
                   window.get("ThreadDesktop") is None and window.get("ThreadDesktopError", 0) != 0)
              for window in windows),
          "OpenDesktop+EnumDesktopWindows proves owned PID/thread/private desktop; foreign-thread E5 remains unknown")
    expected_controller = Path(__file__).resolve().parents[1] / "原生接口探测/启动隔离桌面宿主.ps1"
    expected_helper = expected_controller.with_name("IsolatedCadHostNative.cs")
    check(Path(launch["controllerPath"]).resolve() == expected_controller.resolve()
          and launch.get("controllerSHA256") == hashlib.sha256(expected_controller.read_bytes()).hexdigest().upper(),
          "supervisor source fingerprint matches launch")
    check(Path(launch["nativeHelperPath"]).resolve() == expected_helper.resolve()
          and launch.get("nativeHelperSHA256") == hashlib.sha256(expected_helper.read_bytes()).hexdigest().upper(),
          "native monitor/launcher source fingerprint matches launch")
    for label, name in (("ready", launch["readyMarkerName"]), ("quit", launch["quitMarkerName"])):
        if not check(Path(name).name == name and (run/name).resolve().parent == run,
                     label + " marker stays within session directory"):
            continue
        marker = json.loads((run / name).read_text(encoding="utf-8-sig"))
        location = marker.get("session", marker.get("run", ""))
        ids = [marker[key] for key in ("pid", "process", "processId") if key in marker]
        check(marker.get("nonce") == nonce and Path(location).resolve() == run
              and bool(ids) and all(int(value) == pid for value in ids), label + " raw marker identity")
    tips_file = run / "isolation-startup-tip-actions.jsonl"
    tips = [json.loads(line) for line in tips_file.read_text(encoding="utf-8-sig").splitlines() if line]
    enabled = launch.get("closeOwnStartupTipsEnabled") is True
    check(data.get("closeOwnStartupTipsEnabled") is enabled
          and data.get("startupTipActions") == len(tips)
          and data.get("startupTipPosts") == sum(row.get("action", {}).get("Posted") is True for row in tips),
          "startup-tip opt-in and actual action counters bound to launch and raw log")
    check(enabled or not tips, "startup-tip action requires explicit launch opt-in")
    tip_handles = [row.get("action", {}).get("Hwnd") for row in tips]
    check(len(set(tip_handles)) == len(tip_handles) and all(
          row.get("nonce") == nonce and Path(row.get("session", "")).resolve() == run
          and row.get("sequence") == index + 1
          and row.get("action", {}).get("ProcessId") == pid
          and row["action"].get("DesktopName") == data.get("desktopName")
          and row["action"].get("ClassName") == "#32770"
          and row["action"].get("Title") == "日积月累"
          and row["action"].get("Matched") is True and row["action"].get("Posted") is True
          and row["action"].get("Error") == 0
          and row["action"].get("Status") == "posted_WM_CLOSE_exact_owned_startup_tip"
          for index, row in enumerate(tips)), "only exact owned benign startup tip got one WM_CLOSE per handle")
    related_ids = set(launch.get("relatedProcessIdsAtStart", []))
    def related(row):
        stem = Path(row.get("ProcessImage") or "").stem.lower()
        return row.get("Pid") != pid and (row.get("Pid") in related_ids
                or re.match(r"^(acad|tarch|tch|t20)", stem))
    related_rows = [row for row in events if related(row)]
    related_fg = sum(row.get("Event") == 3 for row in related_rows)
    related_show = sum(row.get("Event") == 0x8002 for row in related_rows)
    check(data.get("relatedCadEvents") == len(related_rows)
          and data.get("relatedCadForegroundEvents") == related_fg
          and data.get("relatedCadShowEvents") == related_show,
          "existing related-CAD counters independently derived from raw events")
    check(data.get("relatedCadEventsNeedReview") is bool(related_fg or related_show),
          "existing related-CAD activity is explicitly flagged for attribution review")
    for value in failures:
        print("FAIL:", value)
    review = not failures and bool(related_fg or related_show)
    result = "FAIL" if failures else "REVIEW_REQUIRED" if review else "PASS"
    print(f"assertions={checks} passed={checks-len(failures)} failed={len(failures)} result={result}")
    if review:
        print("REVIEW: existing AutoCAD/TCH SHOW or foreground events must be attributed; worker-zero counters do not prove the whole session was non-disruptive.")
    print("Scope: observed private-host isolation only; drawing/axis business results need their own native evidence.")
    return 1 if failures else 2 if review else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence", type=Path)
    args = parser.parse_args()
    try:
        raise SystemExit(main(args.evidence))
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        print("FAIL: incomplete isolated-host evidence:", error)
        raise SystemExit(1)
