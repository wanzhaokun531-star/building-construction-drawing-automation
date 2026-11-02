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
if __name__=='__main__':unittest.main()
