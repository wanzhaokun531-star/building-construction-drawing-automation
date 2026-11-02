"""Window-isolation evidence tests. Never imports a controller or launches CAD."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from verify_isolation_observation import inspect


STOP_SCOPE = "Explicit StopOwnedWorker only; exe/creation time/OpenDesktop+EnumDesktopWindows PID and attested GUI thread verified immediately before termination"


class ObservationTests(unittest.TestCase):
    def fixture(self, forced=True):
        temp = tempfile.TemporaryDirectory(prefix="codex-isolation-observation-")
        self.addCleanup(temp.cleanup)
        run = Path(temp.name).resolve()
        nonce = "1234567890abcdef1234567890abcdef"
        desktop = "CodexS1_" + nonce
        controller = Path(__file__).resolve().parents[1] / "原生接口探测/启动隔离桌面宿主.ps1"
        helper = controller.with_name("IsolatedCadHostNative.cs")
        (run / "boot.scr").write_text("synthetic test only", encoding="ascii")
        launch = dict(contract="stage1.isolated-host-launch.v1", origin="CreateProcessW",
                      nonce=nonce, session=str(run), processId=424242, initialThreadId=1234,
                      creationFileTime="134377135887281943", creationUtc="2026-10-29T02:13:08Z",
                      desktopName=desktop, desktopPath="WinSta0\\" + desktop,
                      executable="D:\\CAD2020\\AutoCAD 2020\\acad.exe", metadataState="complete", metadataErrors=[],
                      monitorStartedBeforeWorker=True, monitorStartedUtc="2026-10-29T02:13:07Z",
                      monitorDesktop="Default", inputDesktopBefore="Default", relatedProcessIdsAtStart=[32448],
                      switchDesktopCalled=False, userCadTouched=False,
                      controllerPath=str(controller), nativeHelperPath=str(helper),
                      controllerSHA256=hashlib.sha256(controller.read_bytes()).hexdigest().upper(),
                      nativeHelperSHA256=hashlib.sha256(helper.read_bytes()).hexdigest().upper(),
                      bootScript=str(run / "boot.scr"),
                      bootSHA256=hashlib.sha256((run / "boot.scr").read_bytes()).hexdigest().upper(),
                      readyMarkerName="ready.json", quitMarkerName="quit.json", closeOwnStartupTipsEnabled=False)
        final = dict(contract="stage1.isolated-host-final.v1", recorded="2026-10-29T02:13:11Z",
                     nonce=nonce, session=str(run), processId=424242, workerCreated=True,
                     workerExited=True, workerExitCode=70 if forced else 0, forcedTermination=forced,
                     stillRunningOwnedWorker=False, readyIdentityConfirmed=True, nativeQuitConfirmed=True,
                     desktopName=desktop, inputDesktopBefore="Default", inputDesktopChanged=False,
                     inputDesktopReadErrors=0, monitorStartedBeforeWorker=True, monitorStarted=True,
                     monitorStopped=True, monitorError=None, droppedEvents=0, eventFileWritten=True,
                     sampleCount=3, workerForegroundEvents=0, workerUserDesktopShowEvents=0,
                     workerForegroundSamples=0, relatedCadEvents=0, relatedCadShowEvents=0,
                     relatedCadForegroundEvents=0, relatedCadEventsNeedReview=False,
                     desktopClosed=True, processHandlesClosed=True, switchDesktopCalled=False,
                     userCadTouched=False, supervisorErrors=[], closeOwnStartupTipsEnabled=False,
                     startupTipActions=0, startupTipPosts=0, timedOut=False)
        marker = dict(contract="stage1.quiet-worker-ready.v1", nonce=nonce, session=str(run),
                      processId=424242, desktop=desktop, inputDesktop="Default", timerStarted=True,
                      comUsed=False, error=None)
        quit_marker = dict(contract="stage1.quiet-worker-exit.v2", nonce=nonce, session=str(run),
                           processId=424242, desktop=desktop, inputDesktop="Default", error=None,
                           quitQueueStatus=0, timerStopped=True, reactorRemoved=True)
        for name, row in (("isolation-host-receipt.json", launch), ("isolation-final.json", final),
                          ("ready.json", marker), ("quit.json", quit_marker)):
            self.write(run / name, row)
        if forced:
            self.write(run / "isolation-stop-test.json", dict(
                contract="stage1.isolated-host-explicit-stop.v1", recorded="2026-10-29T02:13:10.5Z",
                nonce=nonce, processId=424242, creationFileTime=launch["creationFileTime"],
                desktopName=desktop, exitCode=70, forcedTermination=True,
                userCadTouched=False, scope=STOP_SCOPE))
        windows = [dict(Hwnd=555, Pid=424242, Thread=1234, EnumerationDesktop=desktop,
                        PidAtEnumeration=424242, ThreadAtEnumeration=1234, ThreadDesktop=None,
                        ThreadDesktopKnown=False, ThreadDesktopError=5, ClassName="SyntheticPrivateTest")]
        samples = [dict(utc=f"2026-10-29T02:13:{second:02d}Z", sequence=index + 1,
                        processId=424242, inputDesktop="Default", privateDesktop=desktop,
                        foreground={"Pid":99}, workerWindows=windows) for index, second in enumerate((8, 9, 10))]
        self.lines(run / "isolation-window-samples.jsonl", samples)
        for name in ("isolation-user-events.jsonl", "isolation-related-cad-events.jsonl",
                     "isolation-startup-tip-actions.jsonl"):
            self.lines(run / name, [])
        return run

    @staticmethod
    def write(path, row):
        path.write_text(json.dumps(row), encoding="utf-8")

    @staticmethod
    def lines(path, rows):
        path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")

    def change(self, run, name, **changes):
        path = run / name
        row = json.loads(path.read_text(encoding="utf-8"))
        row.update(changes)
        self.write(path, row)

    def result(self, run):
        return inspect(run / "isolation-final.json")

    def test_owned_explicit_70_can_pass_observation_but_not_normal_exit(self):
        result = self.result(self.fixture())
        self.assertEqual(result["windowIsolation"], "PASS", result)
        self.assertFalse(result["normalExitProven"])
        self.assertEqual(result["exitClassification"], "EXPLICIT_OWNED_TERMINATION_70")
        self.assertEqual(result["businessValidation"], "NOT_ASSESSED")

    def test_normal_exit_can_pass(self):
        result = self.result(self.fixture(False))
        self.assertEqual(result["windowIsolation"], "PASS", result)
        self.assertTrue(result["normalExitProven"])

    def test_missing_forced_stop_receipt_fails(self):
        run = self.fixture(); (run / "isolation-stop-test.json").unlink()
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_wrong_stop_nonce_pid_time_desktop_or_scope_fails(self):
        for key, bad in (("nonce", "0" * 32), ("processId", 99), ("creationFileTime", "0"),
                         ("desktopName", "Default"), ("scope", "unverified")):
            with self.subTest(key=key):
                run = self.fixture(); self.change(run, "isolation-stop-test.json", **{key: bad})
                self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_false_zero_summary_cannot_hide_worker_show(self):
        run = self.fixture()
        self.lines(run / "isolation-user-events.jsonl", [dict(
            Sequence=1, Pid=424242, Event=32770, Utc="2026-10-29T02:13:09Z", MonitorDesktop="Default")])
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_related_show_requires_review_without_blame(self):
        run = self.fixture(); event = dict(Sequence=1, Pid=32448, Event=32770,
            Utc="2026-10-29T02:13:09Z", MonitorDesktop="Default", Hwnd=55, Owner=0, ClassName="SyntheticNotification")
        self.lines(run / "isolation-user-events.jsonl", [event])
        self.lines(run / "isolation-related-cad-events.jsonl", [event])
        self.change(run, "isolation-final.json", relatedCadEvents=1, relatedCadShowEvents=1,
                    relatedCadEventsNeedReview=True)
        result = self.result(run)
        self.assertEqual(result["windowIsolation"], "REVIEW_REQUIRED", result)
        self.assertEqual(result["observations"]["relatedShowEvents"], 1)

    def test_related_summary_zero_is_rejected(self):
        run = self.fixture(); event = dict(Sequence=1, Pid=32448, Event=32770,
            Utc="2026-10-29T02:13:09Z", MonitorDesktop="Default")
        self.lines(run / "isolation-user-events.jsonl", [event])
        self.lines(run / "isolation-related-cad-events.jsonl", [event])
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_wrong_private_enum_desktop_is_rejected(self):
        run = self.fixture(); path = run / "isolation-window-samples.jsonl"
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        rows[1]["workerWindows"][0]["EnumerationDesktop"] = "Default"
        self.lines(path, rows)
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_known_contradictory_thread_desktop_is_rejected(self):
        run = self.fixture(); path = run / "isolation-window-samples.jsonl"
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        rows[1]["workerWindows"][0].update(ThreadDesktopKnown=True, ThreadDesktop="Default", ThreadDesktopError=0)
        self.lines(path, rows)
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_missing_samples_and_missing_monitor_are_rejected(self):
        for change in ({"sampleCount":4}, {"monitorStopped":False}, {"droppedEvents":1},
                       {"workerExited":False}, {"inputDesktopChanged":True}, {"supervisorErrors":["metadata failure"]}):
            with self.subTest(change=change):
                run = self.fixture(); self.change(run, "isolation-final.json", **change)
                self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_source_fingerprint_mismatch_is_rejected(self):
        run = self.fixture(); self.change(run, "isolation-host-receipt.json", controllerSHA256="0" * 64)
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_missing_ready_or_raw_event_file_is_rejected(self):
        for filename in ("ready.json", "isolation-user-events.jsonl"):
            run = self.fixture(); (run / filename).unlink()
            self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_noncontiguous_event_sequence_is_rejected(self):
        run = self.fixture()
        self.lines(run / "isolation-user-events.jsonl", [dict(
            Sequence=2, Pid=99, Event=32770, Utc="2026-10-29T02:13:09Z", MonitorDesktop="Default")])
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")

    def test_unapproved_startup_tip_action_is_rejected(self):
        run = self.fixture(); self.lines(run / "isolation-startup-tip-actions.jsonl", [dict(action={"Posted":True})])
        self.assertEqual(self.result(run)["windowIsolation"], "FAIL")


if __name__ == "__main__":
    unittest.main()
