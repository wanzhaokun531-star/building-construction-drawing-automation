import unittest,importlib.util,copy
from pathlib import Path
P=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('current_gate',P/'tools/verify_dynamic.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
class DynamicGateTests(unittest.TestCase):
 def test_actual_two_plan_fixture_reaches_geometry_checks(self):
  b=g.read(P.parent/'阶段2/evidence/native-host-binding-v111.json');r=g.read(Path(b['job'])/'floor-1/product-result.json');root=Path(r['records']);preview=g.read(P/'evidence/preview-dynamic-v111-floor1.json');p=g.read(Path(preview['job'])/'stage2-preview-native.json');a=g.read(root/'过程/foundation/annotations.json');w=g.read(root/'过程/join/whole-dimension-chains.json');c=g.read(root/'过程/caption/door-caption-clearance.json')
  # This old probe lacks actual caption-content witnesses and must be refused.
  with self.assertRaisesRegex(ValueError,'actual annotation witnesses'):
   g.verify_model((root/'过程/foundation/current-plan.tsv').read_text(encoding='utf-8'),a,w,c,p,preview['axisAudit'])
class CurrentNativeGateTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  b=g.read(P.parent/'阶段2/evidence/native-host-binding-v118.json');cls.fixtures=[]
  for i in [1,2]:
   r=g.read(Path(b['job'])/f'floor-{i}/product-result.json');root=Path(r['records']);preview=g.read(P/f'evidence/preview-dynamic-v118-floor{i}.json');cls.fixtures.append([(root/'过程/foundation/current-plan.tsv').read_text(encoding='utf-8'),g.read(root/'过程/foundation/annotations.json'),g.read(root/'过程/join/whole-dimension-chains.json'),g.read(root/'过程/caption/door-caption-clearance.json'),g.read(Path(preview['job'])/'stage2-preview-native.json'),g.read(Path(preview['job'])/'product-axis-document-audit.json'),g.read(root/'过程/foundation/analysis.json'),g.read(root/'过程/caption/door-graphic-probe.json')])
 def fixture(self):return copy.deepcopy(self.fixtures[0])
 def test_both_actual_floor_drawings(self):
  self.assertEqual([g.verify_model(*x)['dimensionSegments'] for x in self.fixtures],[93,88])
 def test_old_exterior_wall_collisions_are_rejected(self):
  b=g.read(P.parent/'阶段2/evidence/native-host-binding-v115.json')
  for floor in [1,2]:
   r=g.read(Path(b['job'])/f'floor-{floor}/product-result.json');root=Path(r['records']);preview=g.read(P/f'evidence/preview-dynamic-v115-floor{floor}.json');f=[(root/'过程/foundation/current-plan.tsv').read_text(encoding='utf-8'),g.read(root/'过程/foundation/annotations.json'),g.read(root/'过程/join/whole-dimension-chains.json'),g.read(root/'过程/caption/door-caption-clearance.json'),g.read(Path(preview['job'])/'stage2-preview-native.json'),g.read(Path(preview['job'])/'product-axis-document-audit.json'),g.read(root/'过程/foundation/analysis.json'),g.read(root/'过程/caption/door-graphic-probe.json')]
   with self.assertRaisesRegex(ValueError,'caption overlaps actual wall'):g.verify_model(*f)
 def test_source_wall_displacement_rejected(self):
  f=self.fixture();e=next(e for e in f[4]['originalBefore'] if e['type']=='TDbWall');e['curve']['start'][0]+=10;f[4]['originalAfter']=copy.deepcopy(f[4]['originalBefore'])
  with self.assertRaisesRegex(ValueError,'wall geometry'):g.verify_model(*f)
 def test_actual_caption_content_replacement_rejected(self):
  f=self.fixture();h=f[1]['captions'][0]['handle'];next(e for e in f[5]['nativeAnnotations'] if e['handle']==h)['text']='C0000'
  with self.assertRaisesRegex(ValueError,'caption content'):g.verify_model(*f)
 def test_whole_chain_model_object_deletion_rejected(self):
  f=self.fixture();h=f[2]['saved'][0]['handle'];f[4]['originalBefore']=[x for x in f[4]['originalBefore'] if x['handle']!=h];f[4]['originalAfter']=copy.deepcopy(f[4]['originalBefore']);f[4]['finalModelCount']-=1
  with self.assertRaisesRegex(ValueError,'chain absent'):g.verify_model(*f)
 def test_coordinated_door_caption_move_onto_wall_is_rejected(self):
  f=self.fixture();target=f[3]['targets'][0];h=target['handle'];wall=next(e for e in f[4]['originalBefore'] if e['type']=='TDbWall');a,b=wall['curve']['start'],wall['curve']['end'];new=[(a[i]+b[i])/2 for i in range(3)];old=target['after'][0];delta=[new[i]-old[i] for i in range(3)];target['after']=[new];target['textBounds']=[target['textBounds'][i]+delta[i%2] for i in range(4)]
  e=next(e for e in f[4]['originalBefore'] if e['handle']==h)
  for name in ['min','max']:e[name]=[e[name][i]+delta[i] for i in range(3)]
  for field in ['grip','stretch']:
   for p in e[field]['points']:
    for i in range(3):p[i]+=delta[i]
  next(e for e in f[5]['nativeAnnotations'] if e['handle']==h)['position']=[new];f[4]['originalAfter']=copy.deepcopy(f[4]['originalBefore'])
  lower=f[3]['interiorLowerPlacement'];meta=next((x for x in lower['targets'] if x['handle']==h),None)
  if meta:meta['after']=[new]
  with self.assertRaises(ValueError):g.verify_model(*f)
 def test_axis_visible_end_inversion_rejected(self):
  f=self.fixture();f[5]['axes'][0]['inverted']=not f[5]['axes'][0]['inverted']
  with self.assertRaisesRegex(ValueError,'endpoint visibility'):g.verify_model(*f)
 def test_axis_line_piece_deletion_rejected(self):
  f=self.fixture();f[5]['groups'][0]['lines'].pop()
  with self.assertRaisesRegex(ValueError,'pieces missing'):g.verify_model(*f)
if __name__=='__main__':unittest.main()
