"""In-memory rejection tests for the actual ordinary-AutoCAD host receipt."""
import copy,json
from pathlib import Path
import unittest
from verify_autocad_host_native import inspect

REPORT=Path(__file__).resolve().parents[1]/'runs/quiet-autocadhost-20261029-185205-511627/autocad-host-native.json'
def sample():return json.loads(REPORT.read_text(encoding='utf-8-sig'))
def passes(data):return all(ok for ok,_ in inspect(data))

class AutoCadHostAcceptance(unittest.TestCase):
 def supported(self):return json.loads((REPORT.parents[1]/'quiet-autocadsupportedhost-20261029-190733-509037/autocad-host-native.json').read_text(encoding='utf-8-sig'))
 def test_actual_supported_baseline(self):self.assertTrue(all(ok for ok,_ in inspect(self.supported(),supported=True)))
 def test_supported_mode_requires_complete_sample_set(self):
  data=self.supported()
  for source in data['sources']:source['samples']=source['samples'][:1]
  self.assertFalse(all(ok for ok,_ in inspect(data,supported=True)))
 def test_supported_mode_rejects_unknown_sample_handle(self):
  data=self.supported();sample=data['sources'][1]['samples'][-1];sample['handle']='UNKNOWN'
  self.assertFalse(all(ok for ok,_ in inspect(data,supported=True)))
 def test_supported_mode_rejects_duplicate_sample_handle(self):
  data=self.supported();data['sources'][1]['samples'].append(copy.deepcopy(data['sources'][1]['samples'][-1]))
  self.assertFalse(all(ok for ok,_ in inspect(data,supported=True)))
 def test_real_baseline(self):self.assertTrue(passes(sample()))
 def test_proxy_original_name_cannot_be_claimed_live_native_type(self):
  data=sample();data['sources'][0]['samples'][0]['proxy']=False
  self.assertFalse(passes(data))
 def test_loaded_vendor_module_invalidates_no_component_claim(self):
  data=sample();data['modulesAfter'].append(dict(name='tch_kernal.arx',path='C:\\Tangent\\TArchT20V8\\SYS23x64\\tch_kernal.arx'))
  self.assertFalse(passes(data))
 def test_security_reduction_is_rejected(self):
  data=sample();data['environment']['secureloadAfter']=0
  self.assertFalse(passes(data))
 def test_unrestored_demand_loading_is_rejected(self):
  data=sample();data['environment']['demandloadAfter']=0
  self.assertFalse(passes(data))
 def test_saved_native_geometry_must_be_read_back(self):
  data=sample();data['nativeControl']['saved']['end']=[1000,0,0]
  self.assertFalse(passes(data))

if __name__=='__main__':unittest.main(verbosity=2)
