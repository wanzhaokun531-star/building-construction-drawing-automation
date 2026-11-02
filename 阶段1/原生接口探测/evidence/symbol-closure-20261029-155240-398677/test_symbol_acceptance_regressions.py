"""Reject plausible false-positive symbol receipts using actual saved evidence.

These tests do not run CAD or alter any DWG. All corruptions are in-memory.
"""
import copy
import json
from pathlib import Path
import unittest
from verify_symbol_business_native import inspect as business
from verify_symbol_geometry_native import inspect as geometry

RUNS=Path(__file__).resolve().parents[1]/'runs'
NAMES={
 'slope':'quiet-slope-20261029-154346-195615',
 'section':'quiet-section-20261029-154435-453099',
 'pointer':'quiet-pointerindex-20261029-154402-285592',
 'index':'quiet-sectionindex-20261029-154431-408405',
}

def load(kind):return json.loads((RUNS/NAMES[kind]/'symbol-business-native.json').read_text(encoding='utf-8-sig'))
def good(checks):return all(ok for ok,_ in checks)

class SymbolEvidenceRegression(unittest.TestCase):
 def test_real_four_families(self):
  for kind in NAMES:
   with self.subTest(kind=kind):
    data=load(kind);self.assertTrue(good(business(data)));self.assertTrue(good(geometry(data)))
 def test_requested_ui_text_is_not_output(self):
  data=load('pointer');data['symbols'][0]['savedReadback']['texts'][0]['plainText']='WRONG'
  self.assertFalse(good(business(data)))
 def test_standard_geometry_cannot_replace_native_symbol(self):
  data=load('slope');data['symbols'][0]['dxfName']='LWPOLYLINE';data['symbols'][0]['rxName']='AcDbPolyline'
  self.assertFalse(good(business(data)))
 def test_snapped_input_is_rejected(self):
  data=load('slope');op=next(o for o in data['operations'] if o['mode']=='create')
  next(e for e in op['inputTrace'] if e['event']=='endGetPoint' and e['returnStatus']==5100)['pointOut']=[0,0,0]
  self.assertFalse(good(geometry(data)))
 def test_removed_arrowhead_is_rejected(self):
  data=load('slope')
  for n in data['symbols'][0]['savedReadback']['nodes']:
   for w in n.get('segmentWidths',[]):w['start']=w['end']=0
  self.assertFalse(good(geometry(data)))
 def test_reversed_cutting_stroke_is_rejected(self):
  data=load('index');root=data['symbols'][0]['savedReadback'];tip=[48000,45000,0]
  for n in root['nodes']:
   if n.get('kind')=='polyline':
    n['vertices']=[[2*tip[i]-p[i] for i in range(3)] for p in n['vertices']]
  self.assertFalse(good(geometry(data)))
 def test_unrestored_host_mode_is_rejected(self):
  data=load('slope');op=next(o for o in data['operations'] if o['mode']=='create')
  env=next(e for e in op['events'] if e['event']=='point-environment-restored');env['variables'][0]['after']=-100
  self.assertFalse(good(geometry(data)))
 def test_default_restore_claim_needs_actual_reopen(self):
  data=load('pointer');data['observedRestoredDefaults']['sheet']['text']='S1-01'
  self.assertFalse(good(business(data)))
 def test_own_temporary_clone_must_be_erased(self):
  data=load('index');data['temporaryDefaultRestore']['erased']=False
  self.assertFalse(good(business(data)))
 def test_original_scheme_change_is_rejected(self):
  data=load('section');data['originalAfter'].pop()
  self.assertFalse(good(business(data)))

if __name__=='__main__':unittest.main(verbosity=2)
