import copy,importlib.util,unittest
from pathlib import Path
P=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('product_gate',P/'tools/verify_native.py');gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
class SavedGeometryTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.integration=gate.read(P/'evidence/native-product-v88.json');record=Path(cls.integration['result']['records']);cls.a=gate.read(record/'过程/foundation/annotations.json');cls.w=gate.read(record/'过程/join/whole-dimension-chains.json');cls.c=gate.read(record/'过程/caption/door-caption-clearance.json');p=gate.read(P/'evidence/preview-product-v88.json');cls.p=gate.read(Path(p['job'])/'stage2-preview-native.json')
 def test_actual_saved_native_result(self):gate.verify_geometry(self.a,self.w,self.c,self.p)
 def test_dimension_mutation_rejected(self):
  p=copy.deepcopy(self.p);e=next(x for x in p['originalBefore'] if x['handle']==self.w['saved'][0]['handle']);e['grip']['points'][2][0]+=10;p['originalAfter']=copy.deepcopy(p['originalBefore'])
  with self.assertRaises(ValueError):gate.verify_geometry(self.a,self.w,self.c,p)
 def test_caption_on_dimension_text_rejected(self):
  p=copy.deepcopy(self.p);c=copy.deepcopy(self.c);e=next(x for x in p['originalBefore'] if x['handle']=='D225')
  for field in ('min','max'):e[field][0]+=475
  for field in ('grip','stretch'):
   for point in e[field]['points']:point[0]+=475
  p['originalAfter']=copy.deepcopy(p['originalBefore']);next(x for x in c['targets'] if x['handle']=='D225')['after'][0][0]+=475
  with self.assertRaisesRegex(ValueError,'dimension text or baseline'):gate.verify_geometry(self.a,self.w,c,p)
 def test_wrong_route_rejected(self):
  c=copy.deepcopy(self.c);c['COMUsed']=True
  with self.assertRaises(ValueError):gate.verify_geometry(self.a,self.w,c,self.p)
if __name__=='__main__':unittest.main()
