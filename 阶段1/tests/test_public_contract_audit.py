import unittest,copy
from verify_public_contract_audit import inspect
def fixture():
 source=dict(modelCount=213,originalBefore=[{'handle':str(i)} for i in range(213)],databaseInventory=dict(complete=True,openFailures=0,openedCount=700),samples=[dict(handle='199F',type='TDbAxisLabelSet',readError=None,publicDxf=dict(available=True,truncated=False))]);source['originalAfter']=copy.deepcopy(source['originalBefore']);return dict(contract='stage1.public-native-contract-audit.v1',error=None,commandTrace=dict(lost=False,commands=[]),COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,sourceDrawingWritten=False,sources=[source,copy.deepcopy(source)])
class PublicContractAudit(unittest.TestCase):
 def test_complete_read_only_fixture(self):self.assertTrue(all(ok for ok,_ in inspect(fixture())))
 def test_unread_object_prevents_complete_inventory(self):
  r=fixture();r['sources'][0]['databaseInventory']['openFailures']=1;self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_bag_missing_prevents_field_discovery(self):
  r=fixture();r['sources'][0]['samples'][0]['publicDxf']['available']=False;self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_source_modification_rejected(self):
  r=fixture();r['sources'][0]['originalAfter']=[];self.assertFalse(all(ok for ok,_ in inspect(r)))
if __name__=='__main__':unittest.main()
