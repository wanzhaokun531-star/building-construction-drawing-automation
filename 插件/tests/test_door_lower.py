import copy,importlib.util,math,unittest
from pathlib import Path
P=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('door_gate',P/'tools/verify_native.py');gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
class DoorStrokeTests(unittest.TestCase):
 def arc(self):return dict(arc=True,a=[0,0],b=[0,0],center=[0,0],radius=900,angle=0,sweep=math.pi/2)
 def test_empty_inside_quarter_arc_bbox_is_available(self):self.assertFalse(gate.stroke_hits_expanded_box(self.arc(),[250,50,500,150]))
 def test_arc_contact_rejected(self):self.assertTrue(gate.stroke_hits_expanded_box(self.arc(),[600,600,700,700]))
 def test_opposite_quadrant_available(self):self.assertFalse(gate.stroke_hits_expanded_box(self.arc(),[-910,-20,-880,20]))
 def test_clockwise_arc_contact(self):
  a=self.arc();a.update(angle=math.pi/2,sweep=-math.pi/2);self.assertTrue(gate.stroke_hits_expanded_box(a,[600,600,700,700]))
 def test_furniture_full_circle_contact(self):
  a=self.arc();a['sweep']=2*math.pi;self.assertTrue(gate.stroke_hits_expanded_box(a,[-910,-20,-880,20],60))
 def test_frame_contact(self):self.assertTrue(gate.stroke_hits_expanded_box(dict(arc=False,a=[0,0],b=[0,900]),[-10,400,10,500]))
 def test_forged_geometry_witness_rejected(self):
  e=dict(type='AcDbArc',boundsStatus=0,bounds=[[0,0,0],[900,900,0]],center=[[0,0,0]],radius=900,normal=[[0,0,1]])
  gate.verify_stroke_witness([self.arc()],[e]);a=self.arc();a['angle']=math.pi
  with self.assertRaisesRegex(ValueError,'witness bounds'):gate.verify_stroke_witness([a],[e])
 def test_unexpected_diagonal_rejected(self):
  s=dict(arc=False,a=[0,0],b=[900,900]);e=dict(type='AcDbLine',boundsStatus=0,bounds=[[0,0,0],[900,900,0]])
  with self.assertRaisesRegex(ValueError,'diagonal'):gate.verify_stroke_witness([s],[e])
 def test_native_source_88_primitives(self):
  result=gate.read(P/'evidence/native-product-v93.json')['result'];data=gate.read(Path(result['records'])/'过程/caption/door-graphic-probe.json');count=0
  for opening in data:
   strokes=[]
   for e in opening['entities']:
    if e['type']=='AcDbLine':
     a,b=e['bounds'];strokes.append(dict(arc=False,a=a[:2],b=b[:2]));count+=1
    elif e['type']=='AcDbArc':
     lo,hi=e['bounds'];cx,cy,_=e['center'][0];r=e['radius'];sign=e['normal'][0][2];ends=[math.atan2(y-cy,x-cx) for x in (lo[0],hi[0]) for y in (lo[1],hi[1]) if abs(math.hypot(x-cx,y-cy)-r)<1e-6]
     self.assertEqual(len(ends),2)
     for start,end in (ends,ends[::-1]):
      sweep=sign*((sign*(end-start))%(2*math.pi))
      if 0<abs(sweep)<=math.pi/2+1e-6:break
     strokes.append(dict(arc=True,a=[0,0],b=[0,0],center=[cx,cy],radius=r,angle=start,sweep=sweep));count+=1
   gate.verify_stroke_witness(strokes,opening['entities'])
  self.assertEqual(count,88)
class ActualLowerLayoutTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  r=gate.read(P/'evidence/native-product-v98.json');record=Path(r['result']['records']);cls.a=gate.read(record/'过程/foundation/annotations.json');cls.w=gate.read(record/'过程/join/whole-dimension-chains.json');cls.c=gate.read(record/'过程/caption/door-caption-clearance.json');v=gate.read(P/'evidence/preview-product-v98.json');cls.p=gate.read(Path(v['job'])/'stage2-preview-native.json');cls.probe=gate.read(record/'过程/caption/door-graphic-probe.json');cls.analysis=gate.read(record/'过程/foundation/analysis.json')
 def test_actual_native_lower_and_review_scope(self):
  result=gate.verify_geometry(self.a,self.w,self.c,self.p,self.probe,self.analysis);self.assertEqual(result['interiorLowerApplied'],6);self.assertEqual(result['interiorPlacementReview'],5);self.assertEqual(result['reviewedWallOverlaps'],1)
 def test_unreported_retained_wall_conflict_rejected(self):
  c=copy.deepcopy(self.c);next(x for x in c['interiorLowerPlacement']['targets'] if x['opening']=='D007')['wallOverlap']=False
  with self.assertRaisesRegex(ValueError,'wall without review'):gate.verify_geometry(self.a,self.w,c,self.p,self.probe,self.analysis)
 def test_fallback_cannot_authorize_arbitrary_location(self):
  c=copy.deepcopy(self.c);p=copy.deepcopy(self.p);meta=next(x for x in c['interiorLowerPlacement']['targets'] if x['opening']=='CD96');h=meta['handle'];row=next(x for x in c['targets'] if x['handle']==h);row['after'][0][0]+=10;e=next(x for x in p['originalBefore'] if x['handle']==h)
  for k in ('min','max'):e[k][0]+=10
  for k in ('grip','stretch'):
   for point in e[k]['points']:point[0]+=10
  p['originalAfter']=copy.deepcopy(p['originalBefore'])
  with self.assertRaisesRegex(ValueError,'established position'):gate.verify_geometry(self.a,self.w,c,p,self.probe,self.analysis)
 def test_actual_furniture_polyline_segment_omission_rejected(self):
  c=copy.deepcopy(self.c);f=next(x for x in c['interiorLowerPlacement']['furnitureAudit'] if x['root']=='CFB5');f['parts'].pop()
  with self.assertRaisesRegex(ValueError,'furniture polyline segment'):gate.verify_geometry(self.a,self.w,c,self.p,self.probe,self.analysis)
 def test_reintroduced_actual_furniture_collision_rejected(self):
  old=gate.read(P/'evidence/native-product-v97.json');oldc=gate.read(Path(old['result']['records'])/'过程/caption/door-caption-clearance.json');bad=next(x for x in oldc['targets'] if x['opening']=='CDBD');c=copy.deepcopy(self.c);p=copy.deepcopy(self.p);row=next(x for x in c['targets'] if x['opening']=='CDBD');h=row['handle'];delta=[bad['after'][0][i]-row['after'][0][i] for i in range(3)];row['after']=bad['after'];e=next(x for x in p['originalBefore'] if x['handle']==h)
  for k in ('min','max'):e[k]=[v+delta[i] for i,v in enumerate(e[k])]
  for k in ('grip','stretch'):
   for point in e[k]['points']:
    for i in range(3):point[i]+=delta[i]
  p['originalAfter']=copy.deepcopy(p['originalBefore'])
  with self.assertRaisesRegex(ValueError,'furniture overlap'):gate.verify_geometry(self.a,self.w,c,p,self.probe,self.analysis)
class FurnitureRegressionTests(unittest.TestCase):
 def test_v97_furniture_collision_rejected(self):
  r=gate.read(P/'evidence/native-product-v97.json');record=Path(r['result']['records']);a=gate.read(record/'过程/foundation/annotations.json');w=gate.read(record/'过程/join/whole-dimension-chains.json');c=gate.read(record/'过程/caption/door-caption-clearance.json');v=gate.read(P/'evidence/preview-product-v97.json');p=gate.read(Path(v['job'])/'stage2-preview-native.json');probe=gate.read(record/'过程/caption/door-graphic-probe.json');analysis=gate.read(record/'过程/foundation/analysis.json')
  with self.assertRaisesRegex(ValueError,'furniture'):gate.verify_geometry(a,w,c,p,probe,analysis)
if __name__=='__main__':unittest.main()

