import copy, unittest
from pathlib import Path
from chain_contract import S,read,load_v67,build_contract
from verify_whole_chains import verify

class WholeChainGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        run=read(S/'evidence/native-whole-chain-run-v74.json');cls.payload=read(Path(run['job'])/'whole-dimension-chains.json');cls.contract=build_contract(*load_v67());p=read(S/'evidence/whole-chain-preview-v74.json');cls.preview=read(Path(p['job'])/'stage2-preview-native.json')
    def test_actual_candidate(self):verify(self.payload,self.contract,self.preview)
    def test_separate_old_drawing_is_rejected(self):
        with self.assertRaises(ValueError):verify(load_v67()[0],self.contract)
    def test_mutations_are_rejected(self):
        mutations=[
            lambda p:p['live'][0].update(handle=p['live'][1]['handle']),
            lambda p:p['live'][0].update(ownerModel=False),
            lambda p:p['live'][0].update(singleNativeEntity=False),
            lambda p:p['live'][0].update(originalStyleAndTextVerified=False),
            lambda p:p['live'][0].update(sharedNodeChecks=0),
            lambda p:p['live'][0].update(singleGripWholeBaseline=False),
            lambda p:p['live'][0]['values'].__setitem__(0,999),
            lambda p:p['saved'][0]['grips'][2].__setitem__(0,999),
            lambda p:p['live'][0]['grips'][10].__setitem__(0,999),
            lambda p:p['events'][0].update(status=-5001),
            lambda p:p['events'][0]['inputTrace'][1].update(handles=['CD83']),
            lambda p:p['protectedBefore'].pop(),
        ]
        for change in mutations:
            p=copy.deepcopy(self.payload);change(p)
            with self.subTest(change=change),self.assertRaises(ValueError):verify(p,self.contract,self.preview)
    def test_preview_wrapper_rejected(self):
        p=copy.deepcopy(self.preview);h=self.payload['live'][0]['handle'];next(x for x in p['originalBefore'] if x['handle']==h)['type']='AcDbBlockReference'
        p['originalAfter']=copy.deepcopy(p['originalBefore'])
        with self.assertRaises(ValueError):verify(self.payload,self.contract,p)

if __name__=='__main__':unittest.main()
