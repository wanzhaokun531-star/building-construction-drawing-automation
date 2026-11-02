"""Geometry counterexamples for the SHA-bound native v25 gate."""
from pathlib import Path
import copy,importlib.util,json,sys,unittest
S=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('perimeter_gate',S/'verify_perimeter_axes.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
class GateTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  n=g.read(S/'evidence/native-run-v25.json');v=g.read(S/'evidence/native-preview-run-v25.json');p=Path(n['output']);cache=Path(g.read(Path(n['binding']['inputManifest']))['cache']['path'])
  cls.fixture=[g.read(p/'analysis.json'),g.read(p/'native-annotations.json'),g.read(p/'native-foundation.json'),g.read(Path(v['job'])/'stage2-preview-native.json'),{r[1]:(float(r[6])+float(r[7]))/2 for line in cache.read_text(encoding='utf-8-sig').splitlines() if (r:=line.split()) and r[0]=='W'}]
 def setUp(self):self.args=copy.deepcopy(self.fixture)
 def reject(self,text):
  with self.assertRaisesRegex(RuntimeError,text):g.verify_scene(*self.args)
 def test_actual_native_passes(self):self.assertTrue(g.verify_scene(*self.args)['passed'])
 def test_missing_lower_B_segment(self):
  a=self.args[1];first=next(d for d in a['dimensions'] if d['side']=='right' and d['row']=='axis');a['dimensions'].remove(first);self.reject('does not reach exterior faces: right')
 def test_B_swallowed_into_face(self):
  a=self.args[1];ds=[d for d in a['dimensions'] if d['side']=='right' and d['row']=='axis'];ds[1]['axisFrom']=ds[0]['axisFrom'];ds[1]['expected']+=ds[0]['expected'];a['dimensions'].remove(ds[0]);self.reject('omitted a face or retained axis: right')
 def test_bottom_3_never_restored(self):
  next(x for x in self.args[1]['axes'] if x['vertical'] and x['label']=='3')['showNegative']=True;self.reject('unsupported outward axis label: bottom 3')
 def test_wall_face_role_not_axis(self):
  next(d for d in self.args[1]['dimensions'] if d['side']=='right' and d['row']=='axis')['fromRole']='axis';self.reject('endpoint roles')
 def test_actual_saved_measurement_point(self):
  self.args[3]['dimensions'][0]['geometry'][0]['grip']['points'][2][0]+=100;self.reject('saved measurement point')
 def test_dimension_TCH_type_required(self):
  self.args[3]['dimensions'][0]['geometry'][0]['type']='AcDbAlignedDimension';self.reject('lost TCH type')
 def test_full_C_axis_cannot_be_omitted(self):
  a=self.args[1];a['fullAxisLines']=[x for x in a['fullAxisLines'] if x['label']!='C'];self.reject('not all logical axes')
 def test_full_H_axis_must_span_interior(self):
  a=self.args[1];h=next(x['handle'] for x in a['fullAxisLines'] if x['label']=='H');next(x for x in self.args[3]['originalBefore'] if x['handle']==h)['curve']['end'][0]-=3000;self.reject('actual saved full axis endpoints')
 def test_ring_near_edge_stays_600(self):
  self.args[1]['axes'][0]['circles'][0]['radius']+=1;self.reject('axis circle spacing')
 def test_witness_start_stays_1000(self):
  self.args[3]['dimensions'][0]['gapClip']['max'][1]+=100;self.reject('witness no longer starts at 1000')
if __name__=='__main__':unittest.main()
