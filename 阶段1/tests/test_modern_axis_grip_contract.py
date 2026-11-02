"""Synthetic verifier tests; these are not native modern-grip success evidence."""
import copy
import unittest
from pathlib import Path
from verify_modern_axis_grip_native import geometry,expected_forward,matches,check_operation,validate


class ModernGripContractTests(unittest.TestCase):
    def entity(self):
        return {'handle':'E1','type':'TDbAxisLabelSet','layer':'AXIS','proxy':False,'extentStatus':0,'min':[600,1600,0],'max':[6800,2400,0],
                'grip':{'status':0,'points':[[2400,2000,0],[1000,2000,0],[1400,2000,0],[1400,2400,0],[5000,2000,0],[6400,2000,0],[6000,2000,0],[6000,2400,0]]},
                'stretch':{'status':0,'points':[[2400,2000,0],[1400,2000,0],[5000,2000,0],[6000,2000,0]]}}

    def operation(self):
        before=self.entity();expected=expected_forward(before);after=copy.deepcopy(before)
        after['grip']['points'],after['stretch']['points'],after['min'],after['max']=expected
        protected=[{'handle':str(i)} for i in range(214)]
        return {'before':before,'after':after,'semanticRightInnerBefore':[5000,2000,0],'completed':True,'safeToContinue':True,'error':None,'getFlags':0,'moveFlags':0,'getStatus':0,'moveStatus':0,'openStatus':0,'transactionStatus':0,'moveAttempted':True,
                'modernGrips':[{'index':0,'gripPoint':[5000,2000,0],'appDataNonNull':True}],'selectedModernGripIndex':0,'opaqueAppDataPassedUnchanged':True,'appDataDecoded':False,
                'lifecycle':{'getCalled':True,'returnedGripDataCount':1,'deletedUniqueGripDataCount':1,'gripsToBeDeletedCalled':True,'gripsToBeDeletedReturned':True,'gripsDoneCalled':True,'gripsDoneReturned':True,'allGripWrappersReleased':True,'safeToContinue':True,'cleanupThrew':False,'appDataDecoded':False,'appDataDeleted':False},
                'regenerate':{'commandName':'_.REGEN','commandStatus':5100,'activeTransactionsBefore':0,'activeTransactionsAfter':0},'expectedGeometryMatched':True,'expectedGripPoints':expected[0],'expectedStretchPoints':expected[1],'expectedMin':expected[2],'expectedMax':expected[3],'protectedBefore':protected,'protectedAfter':protected}

    def failures(self,record):
        result=[]
        def check(ok,label):
            if not ok:result.append(label)
            return bool(ok)
        check_operation(record,expected_forward(self.entity()),check,'operation')
        return result

    def test_synthetic_complete_contract(self):
        self.assertEqual(self.failures(self.operation()),[])

    def test_success_codes_without_real_movement_rejected(self):
        r=self.operation();r['after']=copy.deepcopy(r['before'])
        self.assertTrue(any('complete8GP/4SP' in f for f in self.failures(r)))

    def test_only_moving_stretch_endpoint_does_not_preserve_complete_geometry(self):
        e=self.entity();expected=expected_forward(e);e['stretch']['points'][2][0]+=100
        self.assertFalse(matches(e,expected))

    def test_opaque_application_data_must_be_nonnull(self):
        r=self.operation();r['modernGrips'][0]['appDataNonNull']=False
        self.assertTrue(any('nonnull appData' in f for f in self.failures(r)))

    def test_modern_grip_must_match_semantic_public_point(self):
        r=self.operation();r['modernGrips'][0]['gripPoint']=[4999,2000,0]
        self.assertTrue(self.failures(r))

    def test_skipped_public_lifecycle_rejected(self):
        r=self.operation();r['lifecycle']['gripsDoneReturned']=False
        self.assertTrue(self.failures(r))

    def test_deleting_opaque_application_data_is_rejected(self):
        r=self.operation();r['lifecycle']['appDataDeleted']=True
        self.assertTrue(self.failures(r))

    def test_refresh_cannot_cross_open_transaction(self):
        r=self.operation();r['regenerate']['activeTransactionsBefore']=1
        self.assertTrue(self.failures(r))

    def test_absent_native_evidence_remains_failure(self):
        c,f=validate(Path('__missing_modern_axis_grip_native__.json'))
        self.assertEqual(c,1);self.assertTrue(f)


if __name__=='__main__':unittest.main()
