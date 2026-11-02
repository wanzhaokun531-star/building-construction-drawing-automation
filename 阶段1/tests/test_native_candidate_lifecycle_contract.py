"""Reject false publication and native-dimension claims using actual receipts."""
import json
from pathlib import Path
import unittest
from verify_native_candidate_lifecycle import inspect
ROOT=Path(__file__).resolve().parents[1]/'runs'
FILES={'discard':ROOT/'quiet-candidatediscardnative-20261029-193229-065075/native-candidate-discard.json','dimension':ROOT/'quiet-reopendimensionnative-20261029-193231-567304/native-reopen-dimension.json'}
def read(kind):return json.loads(FILES[kind].read_text(encoding='utf-8-sig'))
def passes(d):return all(ok for ok,_ in inspect(d))
class CandidateLifecycleContract(unittest.TestCase):
 def test_actual_discard_and_reopen_cases(self):
  for kind in FILES:
   with self.subTest(kind=kind):self.assertTrue(passes(read(kind)))
 def test_failed_validation_cannot_publish(self):
  d=read('discard');d['cases'][0]['published']=True;self.assertFalse(passes(d))
 def test_collision_must_preserve_target(self):
  d=read('discard');d['cases'][1]['collisionSentinelUnchanged']=False;self.assertFalse(passes(d))
 def test_private_candidate_must_be_removed(self):
  d=read('discard');d['cases'][0]['candidateExistsAfter']=True;self.assertFalse(passes(d))
 def test_published_originals_must_be_preserved(self):
  d=read('discard');d['cases'][2]['publishedOriginal'].pop();self.assertFalse(passes(d))
 def test_native_measurement_must_be3600(self):
  d=read('dimension');d['states']['savedForward']['dimensionRepresentation']['representations'][0]['measurement']=3601;self.assertFalse(passes(d))
 def test_native_formatter_must_be3600(self):
  d=read('dimension');d['states']['savedRestored']['dimensionRepresentation']['representations'][0]['plainFormattedMeasurement']='18500';self.assertFalse(passes(d))
 def test_standard_dimension_cannot_replace_tch_root(self):
  d=read('dimension');d['states']['before']['geometry']['type']='AcDbAlignedDimension';self.assertFalse(passes(d))
 def test_returned_representation_must_not_be_added_to_database(self):
  d=read('dimension');d['states']['forward']['dimensionRepresentation']['representations'][0]['databaseNull']=False;self.assertFalse(passes(d))
 def test_saved_line_must_be_restored(self):
  d=read('dimension');d['states']['savedRestored']['geometry']['grip']['points'][0][1]-=123;self.assertFalse(passes(d))
 def test_native_formatter_is_not_rendered_glyph_evidence(self):
  d=read('dimension');d['states']['before']['dimensionRepresentation']['renderedTextRead']=True;self.assertFalse(passes(d))
if __name__=='__main__':unittest.main(verbosity=2)
