import unittest,copy
from verify_native_mtext_capability import inspect
def fixture():
 original=[dict(handle=str(i)) for i in range(213)];states=[]
 for text,rotation in [('第一行\\P待确认',0),('C09235\\P参数',1.5707963267948966),('第一行\\P待确认',0)]:
  live=dict(type='AcDbMText',contents=text,plainText=text.replace('\\P','\n'),rotation=rotation,styleHandle='AA',height=300,width=5000);states.append(dict(expectedContents=text,expectedRotation=rotation,passed=True,live=live,saved=copy.deepcopy(live),savedModelCount=214,savedProtected=copy.deepcopy(original)))
 return dict(contract='stage1.native-mtext-capability.v1',passed=True,error=None,commandTrace=dict(lost=False,commands=[]),COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,sourceDrawingWritten=False,phase2Started=False,originalBefore=original,originalAfter=copy.deepcopy(original),sourceText=dict(styleHandle='AA',height=300),states=states)
class MTextCapability(unittest.TestCase):
 def test_valid(self):self.assertTrue(all(ok for ok,_ in inspect(fixture())))
 def test_rotation_not_status(self):
  r=fixture();r['states'][1]['live']['rotation']=0;self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_saved_chinese_text_checked(self):
  r=fixture();r['states'][0]['saved']['plainText']='old';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_originals_protected(self):
  r=fixture();r['states'][2]['savedProtected']=[];self.assertFalse(all(ok for ok,_ in inspect(r)))
if __name__=='__main__':unittest.main()
