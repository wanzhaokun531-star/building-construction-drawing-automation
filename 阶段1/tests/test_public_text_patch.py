import copy,unittest
from verify_public_text_patch import inspect
def fixture():
 original=[{'handle':str(i),'type':'TDbWall'} for i in range(213)]
 def read(text):return {'completed':True,'ownershipSafe':True,'error':None,'texts':[{'plainText':text,'position':[10,20,0],'alignmentPoint':[0,0,0],'height':300,'rotation':0}]}
 def bag(text):return {'available':True,'truncated':False,'stringGroups':[{'code':0,'value':'TCH_TEXT'},{'code':100,'value':'TDbText'},{'code':7,'value':'STYLE'},{'code':1,'value':text}]}
 r=dict(contract='stage1.native-public-text-patch.v1',error=None,COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,sourceDrawingWritten=False,commandTrace={'lost':False,'commands':[]},originalBefore=original,originalAfter=original,standardControl={'entModStatus':5100},sourceText=read('庭院'),businessContentWritePassed=True,states=[])
 prev='庭院'
 for new in ('C09235','待确认','庭院'):
  r['states'].append(dict(expected=new,api={'entModStatus':5100,'beforePublicBag':bag(prev),'afterPublicBag':bag(new)},before=read(prev),live=read(new),saved=read(new),liveRootType='TDbText',savedRootType='TDbText',liveModelCount=214,savedModelCount=214,saveStatus=0,savedOriginal=original,savedSource=read('庭院'),savedPublicBag=bag(new)))
  prev=new
 return r
class PublicTextPatchAcceptance(unittest.TestCase):
 def test_complete_positive(self):self.assertTrue(all(ok for ok,_ in inspect(fixture())))
 def test_rtNorm_without_literal_change_rejected(self):
  r=fixture();r['states'][0]['live']['texts'][0]['plainText']='庭院';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_standard_root_substitution_rejected(self):
  r=fixture();r['states'][1]['savedRootType']='AcDbText';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_saved_original_missing_rejected(self):
  r=fixture();r['states'][0]['savedOriginal']=[];self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_source_room_name_change_rejected(self):
  r=fixture();r['states'][2]['savedSource']['texts'][0]['plainText']='changed';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_moved_text_rejected(self):
  r=fixture();r['states'][0]['saved']['texts'][0]['position']=[0,0,0];self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_other_style_changed_rejected(self):
  r=fixture();r['states'][0]['savedPublicBag']['stringGroups'][2]['value']='WRONG';self.assertFalse(all(ok for ok,_ in inspect(r)))
 def test_duplicate_group1_rejected(self):
  r=fixture();r['states'][0]['savedPublicBag']['stringGroups'].append({'code':1,'value':'C09235'});self.assertFalse(all(ok for ok,_ in inspect(r)))
if __name__=='__main__':unittest.main()
