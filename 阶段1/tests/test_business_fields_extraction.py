"""Regression checks for observed LIST fields; no CAD or source-file writes."""
import copy
import importlib
import json
import tempfile
import unittest
from pathlib import Path

try:
    extractor = importlib.import_module("extract_business_fields_native")
except ModuleNotFoundError:
    extractor = None

RUN = Path(__file__).resolve().parents[1] / "runs" / "coverage-list-20261029-004034-482"
RAW = (RUN / "control_1aabfcb2c.log").read_bytes().decode("cp936")
HOST = json.loads((RUN / "business-list-native.json").read_text(encoding="utf-8-sig"))


class BusinessFieldExtractionTests(unittest.TestCase):
    def extract(self, text=RAW, geometry=None):
        if extractor is None:
            self.fail("LIST business-field extractor implementation is missing")
        return extractor.extract_text(text, source="fixed raw log", geometry=geometry)

    @staticmethod
    def entity(report, handle):
        return next(x for x in report["entities"] if x["handle"] == handle)

    def changed_section(self, handle, transform):
        begin = RAW.index("[S1BUSINESSLIST_BEGIN handle=" + handle + " ")
        end = RAW.index("[S1BUSINESSLIST_END handle=" + handle + " ", begin)
        return RAW[:begin] + transform(RAW[begin:end]) + RAW[end:]

    def test_actual_window_door_and_group_dimensions_are_observed(self):
        result = self.extract()
        self.assertEqual(result["status"], "PASS")
        for handle, height, kind in (("CD84", 1500, "窗"), ("CD89", 2400, "门"), ("CD94", 2400, "组合门")):
            fields = self.entity(result, handle)["fields"]
            self.assertEqual(fields["宽度"]["value"], 2400)
            self.assertEqual(fields["高度"]["value"], height)
            self.assertEqual(fields["门窗类型"]["value"], kind)
        group = self.entity(result, "CD94")
        self.assertNotIn("doorWindowSubtype", group)
        self.assertNotIn("numberingPrefix", group)
        self.assertFalse(result["automationReady"])

    def test_duplicate_identical_fields_retain_every_source(self):
        result = self.extract()
        field = self.entity(result, "CF5F")["fields"]["三跑梯段宽"]
        self.assertEqual(field["value"], 1300)
        self.assertEqual(field["status"], "repeated identical")
        self.assertEqual(len(field["sources"]), 2)
        self.assertNotEqual(field["sources"][0]["line"], field["sources"][1]["line"])
        self.assertEqual(len(self.entity(result, "CD84")["fields"]["对象句柄"]["sources"]), 2)

    def test_paper_suffix_and_empty_text_are_preserved(self):
        result = self.extract()
        elevation = self.entity(result, "CE8F")["fields"]
        self.assertEqual(elevation["文字内容"]["value"], "0")
        self.assertEqual(elevation["文字高度"]["value"], 3.5)
        self.assertEqual(elevation["文字高度"]["unit"], "P")
        self.assertEqual(elevation["文字高度"]["sources"][0]["rawValue"], "3.5P")
        drawing = self.entity(result, "CFCE")["fields"]
        self.assertEqual(drawing["比例内容"]["value"], "")
        self.assertEqual(drawing["图名高度"]["value"], 7)
        self.assertEqual(drawing["图名高度"]["unit"], "P")
        self.assertEqual(self.entity(result, "CD84")["fields"]["门窗编号"]["value"], "")

    def test_conflicting_width_does_not_resolve_to_first_value(self):
        result = self.extract(self.changed_section("CD84", lambda s: s + "\n宽度: 2500\n"))
        self.assertEqual(result["status"], "FAIL")
        self.assertFalse(result["usableAsReadEvidence"])
        field = self.entity(result, "CD84")["fields"]["宽度"]
        self.assertIsNone(field["value"])
        self.assertEqual(field["status"], "conflict")
        self.assertEqual(len(field["sources"]), 2)
        self.assertTrue(any(e["code"] == "FIELD_CONFLICT" for e in result["errors"]))

    def test_identical_numeric_duplicates_keep_raw_spellings(self):
        result = self.extract(self.changed_section("CD84", lambda s: s + "\n宽度: 2400.0\n"))
        field = self.entity(result, "CD84")["fields"]["宽度"]
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(field["value"], 2400)
        self.assertEqual([x["rawValue"] for x in field["sources"]], ["2400", "2400.0"])

    def test_paper_and_unsuffixed_values_are_never_equated_by_scale(self):
        result = self.extract(self.changed_section("CDC0", lambda s: s + "\n文字高度: 350\n"))
        self.assertEqual(result["status"], "FAIL")
        self.assertIsNone(self.entity(result, "CDC0")["fields"]["文字高度"]["value"])

    def test_missing_end_and_missing_entity_fail_closed(self):
        for text in (RAW.replace("[S1BUSINESSLIST_END handle=CD84 type=TDbOpening]", ""),
                     self.changed_section("CD94", lambda s: "").replace("[S1BUSINESSLIST_END handle=CD94 type=TDbGroupOpening]", "")):
            with self.subTest(text=text[:20]):
                result = self.extract(text)
                self.assertEqual(result["status"], "FAIL")
                self.assertFalse(result["usableAsReadEvidence"])

    def test_nonfinite_and_double_overflow_numbers_fail_closed(self):
        for token in ("NaN", "Infinity", "-inf", "1e999"):
            with self.subTest(token=token):
                text = self.changed_section("CD84", lambda s: s.replace("高度: 1500", "高度: " + token))
                result = self.extract(text)
                self.assertEqual(result["status"], "FAIL")
                self.assertTrue(any(e["code"] == "NON_FINITE_NUMBER" for e in result["errors"]))
                self.assertIsNone(self.entity(result, "CD84")["fields"]["高度"]["value"])

    def test_fractional_step_count_is_rejected(self):
        result = self.extract(self.changed_section("CF5F", lambda s: s.replace("一跑梯段步数: 11", "一跑梯段步数: 11.5")))
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any(e["code"] == "INVALID_INTEGER" for e in result["errors"]))

    def test_actual_native_geometry_checks_pass_without_inventing_height_geometry(self):
        result = self.extract(geometry=HOST)
        self.assertEqual(result["status"], "PASS")
        checks = {x["id"]: x for x in result["checks"]}
        self.assertTrue(checks["CD84.width.gripXSpan"]["passed"])
        self.assertEqual(checks["CD84.width.gripXSpan"]["observed"], 2400)
        self.assertFalse(any(x["id"].startswith("CD84.height") for x in result["checks"]))
        self.assertTrue(checks["CF5F.stepSumHeight"]["passed"])

    def test_geometry_disagreement_is_not_silently_accepted(self):
        host = copy.deepcopy(HOST)
        wall = next(x for x in host["originalBefore"] if x["handle"] == "CD83")
        wall["curve"]["length"] = 3500
        result = self.extract(geometry=host)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any(e["code"] == "CHECK_MISMATCH" for e in result["errors"]))

    def test_unstructured_or_wrong_identity_lines_fail_closed(self):
        for text in (self.changed_section("CD84", lambda s: s + "\n未识别的续行内容\n"),
                     self.changed_section("CD84", lambda s: s.replace("对象句柄: CD84", "对象句柄: CD85"))):
            with self.subTest(text=text[:20]):
                result = self.extract(text)
                self.assertEqual(result["status"], "FAIL")

    def test_review_uses_actual_parsed_values_instead_of_fixture_constants(self):
        result = self.extract(self.changed_section("CD84", lambda s: s.replace("高度: 1500", "高度: 1600")))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.json"
            path.write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")
            review = extractor.review_text(result, path)
        self.assertIn("CD84窗2400×1600", review)
        self.assertNotIn("CD84窗2400×1500", review)


if __name__ == "__main__":
    unittest.main()
