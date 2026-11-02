"""Adversarial evidence contracts only; synthetic data are not native proof."""
import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from verify_four_side_axis_native import check_number_axis_binding,check_number_observation,digest


def binding_fixture():
    axis={"handle":"V1","type":"TDbAxisLabelSet","grip":{"status":0,"points":[[10,20,0]]}}
    return ({"axisHandle":"V1","before":copy.deepcopy(axis),"after":copy.deepcopy(axis)},
            {"mergedAxisHandle":"V1","afterSets":[copy.deepcopy(axis)]},
            copy.deepcopy(axis),copy.deepcopy(axis))


class NumberAxisBindingTests(unittest.TestCase):
    def failures(self,values):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_number_axis_binding(*values,check,"number")
        return failures

    def test_exact_same_direction_chain_is_accepted(self):
        self.assertEqual(self.failures(binding_fixture()),[])

    def test_valid_command_for_other_direction_is_rejected(self):
        number,merge,current,saved=binding_fixture()
        number["axisHandle"]="H1"
        for key in ("before","after"):number[key]["handle"]="H1"
        self.assertTrue(self.failures((number,merge,current,saved)))

    def test_matching_handle_does_not_hide_changed_merge_input(self):
        values=binding_fixture();values[0]["before"]["grip"]["points"][0][0]+=1
        self.assertTrue(any("input equals" in x for x in self.failures(values)))

    def test_matching_handle_does_not_hide_changed_final_output(self):
        values=binding_fixture();values[2]["grip"]["points"][0][0]+=1
        self.assertTrue(any("persisted same-direction" in x for x in self.failures(values)))

    def test_missing_same_direction_merge_output_is_rejected(self):
        values=binding_fixture();values[1]["afterSets"]=[]
        self.assertTrue(self.failures(values))


class NumberPreviewBindingTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();preview_dir=self.root/"preview";preview_dir.mkdir()
        self.source=self.root/"full-generated.dwg";self.source.write_bytes(b"synthetic-dwg-binding-bytes")
        self.control=preview_dir/"control.dwg";self.control.write_bytes(self.source.read_bytes())
        self.pdf=preview_dir/"axis-preview.pdf";self.pdf.write_bytes(b"synthetic-not-a-real-native-pdf")
        self.report=preview_dir/"axis-preview-native.json";self.source_report=self.root/"four-side-axis-native.json"
        self.source_report.write_text("{}",encoding="utf-8")
        self.preview={"contract":"stage1.four-axis-preview-native.v1","sourceKind":"full-four-side-215",
                      "inputDWG":str(self.control),"controlDWG":str(self.control),
                      "outputPDF":str(self.pdf),"page":{"outputPDF":str(self.pdf)}}
        self.report.write_text(json.dumps(self.preview),encoding="utf-8")
        self.d={"outputDWG":str(self.source)}
        self.digits=[str(i) for i in range(1,11)];self.letters=list("ABCDEFGHJKLM")
        self.o={"contract":"stage1.four-side-axis-numbering-observation.v2","sourceDWG":str(self.source),"sourceDWGSHA256":digest(self.source),
                "previewReport":str(self.report),"previewReportSHA256":digest(self.report),"pdfPath":str(self.pdf),"pdfSHA256":digest(self.pdf),
                "method":"manual inspection of native PDF rendering","topDigits":self.digits,"bottomDigits":self.digits,
                "leftLetters":self.letters,"rightLetters":self.letters}

    def check_observation(self,preview_result=(100,())):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        # The preview verifier is deliberately mocked: these tiny byte files
        # exercise binding and rejection only and cannot become host proof.
        with patch("verify_four_axis_preview_native.validate",return_value=preview_result) as validate_preview:
            result=check_number_observation(self.o,self.d,self.source_report,self.digits,self.letters,check)
            calls=validate_preview.call_args_list
        return result,failures,calls

    def test_complete_binding_requires_actual_preview_verifier(self):
        valid,failures,calls=self.check_observation()
        self.assertTrue(valid);self.assertEqual(failures,[])
        self.assertEqual(len(calls),1)
        self.assertEqual(calls[0].args,(self.report,self.source_report))

    def test_preview_failure_prevents_semantic_pass(self):
        valid,failures,_=self.check_observation((1,["native plot failed"]))
        self.assertFalse(valid);self.assertTrue(any("independent Preview" in x for x in failures))

    def test_unrelated_pdf_with_matching_self_hash_is_rejected(self):
        other=self.root/"unrelated.pdf";other.write_bytes(b"different PDF")
        self.o["pdfPath"]=str(other);self.o["pdfSHA256"]=digest(other)
        valid,failures,_=self.check_observation()
        self.assertFalse(valid);self.assertTrue(any("preview's actual native output" in x for x in failures))

    def test_wrong_preview_report_hash_prevents_semantic_pass(self):
        self.o["previewReportSHA256"]="0"*64
        valid,failures,_=self.check_observation()
        self.assertFalse(valid);self.assertTrue(any("report path and SHA256" in x for x in failures))

    def test_different_preview_control_bytes_prevent_semantic_pass(self):
        self.control.write_bytes(b"different-source-dwg")
        valid,failures,_=self.check_observation()
        self.assertFalse(valid);self.assertTrue(any("control bytes" in x for x in failures))

    def test_wrong_source_hash_prevents_semantic_pass_despite_correct_lists(self):
        self.o["sourceDWGSHA256"]="0"*64
        valid,_,_=self.check_observation()
        self.assertFalse(valid)

    def test_missing_preview_report_cannot_be_semantic_proof(self):
        self.o.pop("previewReport")
        valid,_,calls=self.check_observation()
        self.assertFalse(valid);self.assertEqual(calls,[])

    def test_old_unbound_contract_is_rejected(self):
        self.o["contract"]="stage1.four-side-axis-numbering-observation.v1"
        valid,_,_=self.check_observation();self.assertFalse(valid)

    def test_wrong_circle_strings_are_rejected_with_valid_binding(self):
        self.o["rightLetters"]=list("ABCDEFGHIJKL")
        valid,failures,_=self.check_observation()
        self.assertFalse(valid);self.assertTrue(any("all44 circles" in x for x in failures))

    def test_limited_template_preview_cannot_replace_full_preview(self):
        self.preview["sourceKind"]="single-template-number-214"
        self.report.write_text(json.dumps(self.preview),encoding="utf-8")
        self.o["previewReportSHA256"]=digest(self.report)
        valid,_,_=self.check_observation();self.assertFalse(valid)


if __name__=="__main__":unittest.main()
