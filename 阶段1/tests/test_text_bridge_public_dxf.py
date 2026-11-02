"""Reject corrupted persisted TCH text evidence without touching CAD or DWG."""
import copy
import contextlib
import io
import json
from pathlib import Path
from unittest.mock import patch
import unittest
import verify_text_bridge_native as verifier

EVIDENCE = Path(__file__).resolve().parents[1] / "runs/coverage-text-20261029-003350-351/text-bridge-native.json"
ORIGINAL_READ = Path.read_text

class PublicDxfEvidenceTests(unittest.TestCase):
    def run_data(self, data):
        def read(path, *args, **kwargs):
            return json.dumps(data, ensure_ascii=False) if path.resolve() == EVIDENCE.resolve() else ORIGINAL_READ(path, *args, **kwargs)
        output = io.StringIO()
        with patch.object(Path, "read_text", read), contextlib.redirect_stdout(output):
            status = verifier.main(EVIDENCE)
        return status, output.getvalue()

    def test_actual_native_evidence(self):
        data = json.loads(ORIGINAL_READ(EVIDENCE, encoding="utf-8-sig"))
        self.assertEqual(self.run_data(data)[0], 0)

    def test_saved_target_old_text_is_rejected(self):
        data = json.loads(ORIGINAL_READ(EVIDENCE, encoding="utf-8-sig"))
        rows = data["pairs"][0]["targetSaved"]["publicDxf"]["stringGroups"]
        next(r for r in rows if r["code"] == 1)["value"] = "庭院"
        status, output = self.run_data(data)
        self.assertEqual(status, 1)
        self.assertIn("pair/code/saved: exact unique public group1", output)

    def test_duplicate_public_text_group_is_rejected(self):
        data = json.loads(ORIGINAL_READ(EVIDENCE, encoding="utf-8-sig"))
        rows = data["pairs"][1]["targetSaved"]["publicDxf"]["stringGroups"]
        rows.append(copy.deepcopy(next(r for r in rows if r["code"] == 1)))
        status, output = self.run_data(data)
        self.assertEqual(status, 1)
        self.assertIn("pair/pending/saved: exact unique public group1", output)

    def test_unobserved_public_read_is_rejected(self):
        data = json.loads(ORIGINAL_READ(EVIDENCE, encoding="utf-8-sig"))
        data["pairs"][0]["targetSaved"]["publicDxf"]["available"] = False
        status, output = self.run_data(data)
        self.assertEqual(status, 1)
        self.assertIn("pair/code/saved: complete actual public DXF read", output)

if __name__ == "__main__":
    unittest.main()
