"""No CAD is launched: exercise quiet-host acceptance against corrupt receipts."""
import copy
import contextlib
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from verify_isolated_host import validate, main


class IsolationEvidenceTests(unittest.TestCase):
    def receipt(self):
        nonce = "1234567890abcdef1234567890abcdef"
        return dict(contract="stage1.isolated-host-final.v1", workerCreated=True,
                    workerExited=True, workerExitCode=0, nativeQuitConfirmed=True, forcedTermination=False,
                    timedOut=False, monitorStartedBeforeWorker=True, monitorStarted=True,
                    monitorStopped=True, monitorError=None, droppedEvents=0,
                    eventFileWritten=True, sampleCount=3, workerForegroundEvents=0, workerForegroundSamples=0,
                    workerUserDesktopShowEvents=0, inputDesktopChanged=False,
                    inputDesktopReadErrors=0, nonce=nonce, desktopName="CodexS1_"+nonce,
                    inputDesktopBefore="Default", desktopClosed=True,
                    processHandlesClosed=True, switchDesktopCalled=False,
                    userCadTouched=False, readyIdentityConfirmed=True, supervisorErrors=[])

    def test_complete_synthetic_contract_is_accepted(self):
        self.assertEqual(validate(self.receipt())[1], [])

    def test_missing_monitor_is_rejected(self):
        data = self.receipt(); data["monitorStarted"] = False
        self.assertTrue(validate(data)[1])

    def test_brief_show_or_foreground_is_rejected(self):
        for key in ("workerForegroundEvents", "workerUserDesktopShowEvents"):
            data = self.receipt(); data[key] = 1
            self.assertTrue(validate(data)[1], key)

    def test_hidden_modal_timeout_is_rejected(self):
        data = self.receipt(); data["timedOut"] = True
        self.assertTrue(validate(data)[1])

    def test_wrong_or_current_desktop_is_rejected(self):
        for value in ("Default", "CodexS1_wrong"):
            data = self.receipt(); data["desktopName"] = value
            self.assertTrue(validate(data)[1])

    def test_process_exit_without_native_marker_is_rejected(self):
        data = self.receipt(); data["nativeQuitConfirmed"] = False
        self.assertTrue(validate(data)[1])

    def test_forced_stop_is_never_normal_success(self):
        data = self.receipt(); data["forcedTermination"] = True
        self.assertTrue(validate(data)[1])

    def raw_fixture(self):
        temp = tempfile.TemporaryDirectory(prefix="codex-stage1-isolation-test-")
        run = Path(temp.name).resolve()
        self.assertEqual(run.parent, Path(tempfile.gettempdir()).resolve())
        def cleanup():
            self.assertEqual(run.parent, Path(tempfile.gettempdir()).resolve())
            self.assertTrue(run.name.startswith("codex-stage1-isolation-test-"))
            temp.cleanup()
        self.addCleanup(cleanup)
        data=self.receipt();data.update(processId=424242, sampleCount=1,
               relatedCadEvents=0, relatedCadForegroundEvents=0,
               relatedCadShowEvents=0, relatedCadEventsNeedReview=False,
               closeOwnStartupTipsEnabled=False,startupTipActions=0,startupTipPosts=0)
        controller=Path(__file__).resolve().parents[1]/"原生接口探测/启动隔离桌面宿主.ps1"
        helper=controller.with_name("IsolatedCadHostNative.cs")
        launch=dict(contract="stage1.isolated-host-launch.v1",origin="CreateProcessW",
                    processId=424242,nonce=data["nonce"],session=str(run),desktopName=data["desktopName"],
                    monitorDesktop="Default",monitorStartedUtc="2026-10-29T00:00:00Z",
                    creationUtc="2026-10-29T00:00:01Z",controllerPath=str(controller),
                    controllerSHA256=hashlib.sha256(controller.read_bytes()).hexdigest().upper(),
                    nativeHelperPath=str(helper),nativeHelperSHA256=hashlib.sha256(helper.read_bytes()).hexdigest().upper(),
                    readyMarkerName="ready.json",quitMarkerName="quit.json",relatedProcessIdsAtStart=[22448],
                    closeOwnStartupTipsEnabled=False)
        marker=dict(nonce=data["nonce"],session=str(run),processId=424242)
        sample=dict(processId=424242,privateDesktop=data["desktopName"],inputDesktop="Default",
                    foreground={"Pid":100},workerWindows=[dict(Pid=424242,ThreadDesktop=None,
                    ThreadDesktopKnown=False,ThreadDesktopError=5,EnumerationDesktop=data["desktopName"],
                    PidAtEnumeration=424242,ThreadAtEnumeration=10001)])
        for name,value in [("isolation-final.json",data),("isolation-host-receipt.json",launch),("ready.json",marker),("quit.json",marker)]:
            (run/name).write_text(json.dumps(value),encoding="utf-8")
        (run/"isolation-user-events.jsonl").write_text("",encoding="utf-8")
        (run/"isolation-startup-tip-actions.jsonl").write_text("",encoding="utf-8")
        (run/"isolation-window-samples.jsonl").write_text(json.dumps(sample)+"\n",encoding="utf-8")
        return run

    def test_raw_record_fixture_is_accepted_without_starting_cad(self):
        run=self.raw_fixture()
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(run/"isolation-final.json"),0)

    def test_false_zero_summary_is_rejected_by_actual_event(self):
        run=self.raw_fixture()
        event=dict(Sequence=1,Pid=424242,Event=0x8002,MonitorDesktop="Default")
        (run/"isolation-user-events.jsonl").write_text(json.dumps(event)+"\n",encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(run/"isolation-final.json"),1)

    def test_existing_cad_activity_requires_review_not_blame(self):
        run=self.raw_fixture();path=run/"isolation-final.json";data=json.loads(path.read_text())
        data.update(relatedCadEvents=1,relatedCadShowEvents=1,relatedCadEventsNeedReview=True)
        path.write_text(json.dumps(data),encoding="utf-8")
        event=dict(Sequence=1,Pid=22448,Event=0x8002,MonitorDesktop="Default")
        (run/"isolation-user-events.jsonl").write_text(json.dumps(event)+"\n",encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(path),2)

    def test_marker_identity_is_really_read_and_rejected(self):
        run=self.raw_fixture();marker=run/"ready.json";data=json.loads(marker.read_text());data["processId"]=100
        marker.write_text(json.dumps(data),encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(run/"isolation-final.json"),1)

    def test_wrong_enumeration_desktop_is_rejected_even_when_thread_desktop_unknown(self):
        run=self.raw_fixture();path=run/"isolation-window-samples.jsonl";sample=json.loads(path.read_text())
        sample["workerWindows"][0]["EnumerationDesktop"]="Default"
        path.write_text(json.dumps(sample)+"\n",encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(run/"isolation-final.json"),1)

    def test_known_contradictory_thread_desktop_is_rejected(self):
        run=self.raw_fixture();path=run/"isolation-window-samples.jsonl";sample=json.loads(path.read_text())
        sample["workerWindows"][0].update(ThreadDesktopKnown=True,ThreadDesktop="Default",ThreadDesktopError=0)
        path.write_text(json.dumps(sample)+"\n",encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(run/"isolation-final.json"),1)

    def test_unapproved_startup_tip_action_is_rejected(self):
        run=self.raw_fixture();path=run/"isolation-startup-tip-actions.jsonl"
        path.write_text(json.dumps(dict(action=dict(Hwnd=123,Posted=True)))+"\n",encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(main(run/"isolation-final.json"),1)


if __name__ == "__main__":
    unittest.main()
