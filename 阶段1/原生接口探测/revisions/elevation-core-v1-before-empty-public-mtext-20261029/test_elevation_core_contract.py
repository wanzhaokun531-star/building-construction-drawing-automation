"""Precision/format guard tests; not native elevation round-trip evidence."""
import json
import tempfile
import unittest
from decimal import Decimal
from pathlib import Path
from verify_elevation_roundtrip_native import numeric_candidate,inspect


class ElevationCoreContractTests(unittest.TestCase):
    def test_three_decimal_actual_text_is_not_gui_request(self):
        r=numeric_candidate('+0.123')
        self.assertEqual(Decimal(r['decimalValue']),Decimal('0.123'))
        self.assertEqual(r['decimalPlaces'],3);self.assertEqual(r['actualText'],'+0.123')

    def test_two_decimal_format_is_not_promoted(self):
        self.assertEqual(numeric_candidate('0.12')['decimalPlaces'],2)

    def test_integer_zero_is_not_assumed_three_decimal(self):
        self.assertEqual(numeric_candidate('0')['decimalPlaces'],0)

    def test_original_signed_zero_format_is_preserved(self):
        r=numeric_candidate('-0.000')
        self.assertEqual(r['actualText'],'-0.000');self.assertEqual(Decimal(r['decimalValue']),0)

    def test_actual_plusminus_zero_formats_are_explicit(self):
        for text in ('±0.000','%%p0.000'):
            with self.subTest(text=text):
                r=numeric_candidate(text);self.assertEqual(r['actualText'],text);self.assertEqual(Decimal(r['decimalValue']),0)

    def test_plusminus_nonzero_is_ambiguous(self):
        self.assertIsNone(numeric_candidate('±0.123'));self.assertIsNone(numeric_candidate('%%p0.123'))

    def test_embedded_or_unit_decorated_number_is_not_guessed(self):
        for text in ('标高0.123','0.123m','0,123','{0.123}','0.123/0.000'):
            with self.subTest(text=text):self.assertIsNone(numeric_candidate(text))

    def test_precision_is_retained_without_rounding(self):
        r=numeric_candidate('−15.4334')
        self.assertEqual(r['decimalPlaces'],4);self.assertEqual(Decimal(r['decimalValue']),Decimal('-15.4334'))

    def test_missing_host_evidence_cannot_be_business_pass(self):
        r=inspect(Path('__missing_elevation_core_native__.json'))
        self.assertEqual(r['result'],'FAIL');self.assertEqual(r['businessRoundtrip'],'NOT_ATTEMPTED')

    def test_axis_or_success_flag_cannot_replace_elevation_core(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'fake.json';path.write_text(json.dumps({'contract':'stage1.axis-explode-readback-native.v1','completed':True}),encoding='utf8')
            r=inspect(path)
            self.assertEqual(r['result'],'FAIL');self.assertEqual(r['businessRoundtrip'],'NOT_ATTEMPTED')


if __name__=='__main__':unittest.main()
