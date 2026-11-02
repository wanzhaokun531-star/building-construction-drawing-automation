import unittest,copy
from verify_native_public_symbol_patch import inspect
def fixture():
 p=[{'handle':str(i)} for i in range(213)];visible=lambda v:dict(completed=True,ownershipSafe=True,texts=[dict(plainText=v)])
 states=[dict(expected=v,api=dict(entModStatus=5100,entUpdStatus=5100),live=visible(v),saved=visible(v),liveRootType='TDbSymbElevation',savedRootType='TDbSymbElevation',liveModelCount=214,savedModelCount=214,changed=True,reopened=True,saveStatus=0,savedProtected=copy.deepcopy(p),savedPublicBag=dict(available=True,stringGroups=[dict(code=1,value=v)])) for v in ['0.123','-0.030']]
 return dict(contract='stage1.native-public-symbol-patch.v1',type='TDbSymbElevation',target='D0DA',error=None,passed=True,sourceVisible=visible('-0.030'),originalBefore=p,originalAfter=copy.deepcopy(p),standardPositiveControl=dict(entModStatus=5100,entUpdStatus=5100),states=states,commandTrace=dict(lost=False,commands=[]),COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,sourceDrawingWritten=False,phase2Started=False)
class NativeSymbol(unittest.TestCase):
 def test_valid_case(self):self.assertTrue(all(ok for ok,_ in inspect(fixture())))
 def test_status_alone_is_not_value(self):
  r=fixture();r['states'][0]['saved']['texts'][0]['plainText']='-0.030';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_wrong_native_type_rejected(self):
  r=fixture();r['states'][0]['savedRootType']='AcDbText';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_original_changes_rejected(self):
  r=fixture();r['states'][1]['savedProtected']=[];self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_commands_rejected(self):
  r=fixture();r['commandTrace']['commands']=['TChangeValElev'];self.assertFalse(all(ok for ok,_ in inspect(r)))
if __name__=='__main__':unittest.main()
