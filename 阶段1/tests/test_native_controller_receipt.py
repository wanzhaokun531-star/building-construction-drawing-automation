"""Controller preflight failures must leave evidence without ever reaching CAD."""
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


CONTROLLER = Path(__file__).resolve().parents[1] / "运行原生接口探测.ps1"
WINDOWS_POWERSHELL = Path("C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe")


class ControllerReceiptTest(unittest.TestCase):
    def test_missing_binary_preserves_preflight_failure_receipt(self):
        # Removing the early receipt or moving preflight outside try/finally
        # must fail this test. No ARX exists here, so COM cannot be reached.
        for mode in ("Scan", "AxisLabels2C"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="s1-receipt-") as temp:
                stage = Path(temp) / "stage"
                stage.mkdir()
                script = stage / CONTROLLER.name
                shutil.copyfile(CONTROLLER, script)
                environment = os.environ.copy()
                # pwsh's inherited module path hides Windows PowerShell's
                # Utility module; use its real bundled modules, not a mock.
                environment["PSModulePath"] = str(WINDOWS_POWERSHELL.parent / "Modules")
                result = subprocess.run(
                    [str(WINDOWS_POWERSHELL), "-NoProfile", "-NonInteractive", "-File", str(script), "-Mode", mode],
                    capture_output=True, timeout=15, env=environment,
                )
                self.assertEqual(result.returncode, 1, result.stderr.decode(errors="replace"))
                runs = list((stage / "runs").glob("native-access-*"))
                self.assertEqual(len(runs), 1)
                receipt = runs[0] / "host-receipt.json"
                self.assertTrue(receipt.is_file(), "preflight failure lost its host receipt")
                data = json.loads(receipt.read_text(encoding="utf-8-sig"))
                self.assertEqual(data["Mode"], mode)
                self.assertEqual(Path(data["Run"]), runs[0])
                self.assertEqual(data["Load"], "not executed")
                self.assertEqual(data["Unload"], "not executed")
                self.assertIsNone(data["VerificationExitCode"])
                self.assertIn("NativeAccessProbe.arx", data["Error"])
                self.assertTrue(data["ErrorDetails"]["FullyQualifiedErrorId"])
                self.assertEqual(data["FailureBoundary"], "binary-hash")
                self.assertTrue(data["Started"])
                self.assertTrue(data["Finished"])
                boundaries = [item["Boundary"] for item in data["BoundaryHistory"]]
                self.assertEqual(boundaries[:2], ["run-created", "binary-hash"])
                self.assertNotIn("host-connect", boundaries)
                self.assertFalse((runs[0] / "control.dwg").exists())


if __name__ == "__main__":
    unittest.main()
