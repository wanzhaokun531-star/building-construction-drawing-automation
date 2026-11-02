import copy,unittest
from verify_plain_text_single import inspect
def fixture():
 info=dict(type='AcDbText',text='庭院',position=[1,2,0],alignmentPoint=[0,0,0],height=262.8,rotation=0,normal=[0,0,1],widthFactor=.8,oblique=0,thickness=0,horizontalMode=0,verticalMode=0,mirroredX=False,mirroredY=False,styleHandle='CA9E')
 original=[{'handle':str(i)} for i in range(212)]
 r=dict(contract='stage1.native-plain-text-replacement.v1',error='later batch refuses multiple components',COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,sourceDrawingWritten=False,phase2Started=False,commandTrace={'lost':False,'commands':[]},conversion=dict(sourceType='TDbText',handle='CDC0',handOverStatus=36,handleRetained=True,sourceVisibleGlyph=copy.deepcopy(info),expectedNative=copy.deepcopy(info),actualNative=copy.deepcopy(info)),baseline=copy.deepcopy(info),protectedBefore=original,protectedAfter=copy.deepcopy(original),states=[],singleReplacementPassed=True)
 for value in ('庭院','C09235','待确认','庭院'):
  current=copy.deepcopy(info);current['text']=value;r['states'].append(dict(expected=value,live=current,saved=copy.deepcopy(current),saveStatus=0,savedModelCount=213,savedProtected=copy.deepcopy(original)))
 return r
class NativeSingleTextAcceptance(unittest.TestCase):
 def good(self,r):return all(ok for ok,_ in inspect(r))
 def test_single_positive_does_not_erase_later_batch_failure(self):
  r=fixture();self.assertTrue(self.good(r));self.assertIsNotNone(r['error'])
 def test_saved_type_not_native_rejected(self):
  r=fixture();r['states'][1]['saved']['type']='TDbText';self.assertFalse(self.good(r))
 def test_saved_chinese_literal_changed_rejected(self):
  r=fixture();r['states'][2]['saved']['text']='wrong';self.assertFalse(self.good(r))
 def test_source_style_missing_rejected(self):
  r=fixture();del r['conversion']['sourceVisibleGlyph']['styleHandle'];self.assertFalse(self.good(r))
 def test_native_expected_and_actual_style_both_wrong_rejected(self):
  r=fixture()
  for key in ('expectedNative','actualNative'):r['conversion'][key]['styleHandle']='BAD'
  r['baseline']['styleHandle']='BAD';self.assertFalse(self.good(r))
 def test_other_object_changed_rejected(self):
  r=fixture();r['states'][0]['savedProtected'][0]['handle']='bad';self.assertFalse(self.good(r))
 def test_extra_model_entity_rejected(self):
  r=fixture();r['states'][0]['savedModelCount']=214;self.assertFalse(self.good(r))
 def test_missing_reverse_restore_rejected(self):
  r=fixture();r['states'].pop();self.assertFalse(self.good(r))
 def test_business_command_observed_rejected(self):
  r=fixture();r['commandTrace']['commands']=['TTextReplace'];self.assertFalse(self.good(r))
 def test_missing_route_assertion_rejected(self):
  r=fixture();del r['COMUsed'];self.assertFalse(self.good(r))
if __name__=='__main__':unittest.main()
