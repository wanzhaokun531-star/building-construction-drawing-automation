"""Reject unrefreshed/stale evidence and never reclassify prior native failures."""
import copy
import unittest
from pathlib import Path
from verify_four_side_singletons_native import check_refresh,check_endpoint_index_binding,validate

ROOT=Path(__file__).resolve().parents[2]


class SingletonEvidenceTests(unittest.TestCase):
    def failures(self,record):
        result=[]
        def check(ok,label):
            if not ok:result.append(label)
            return bool(ok)
        check_refresh(record,check,'refresh')
        return result

    def valid(self):
        return {'attempted':True,'commandName':'_.REGEN','commandStatus':5100,'activeTransactionsBefore':0,'activeTransactionsAfter':0}

    def test_actual_native_refresh_protocol(self):
        self.assertEqual(self.failures(self.valid()),[])

    def test_omitted_refresh_is_rejected(self):
        self.assertTrue(self.failures({'attempted':False}))

    def test_failed_native_refresh_status_is_rejected(self):
        r=self.valid();r['commandStatus']=0
        self.assertTrue(self.failures(r))

    def test_active_transaction_across_refresh_is_rejected(self):
        for key in ('activeTransactionsBefore','activeTransactionsAfter'):
            with self.subTest(key=key):
                r=self.valid();r[key]=1
                self.assertTrue(self.failures(r))

    def test_missing_native_fixture_evidence_is_rejected(self):
        count,failures=validate(Path('__missing_native_singletons__.json'))
        self.assertEqual(count,1);self.assertTrue(failures)

    def test_failed_prior_native_full_run_remains_failed(self):
        count,failures=validate(ROOT/'阶段1/runs/quiet-fourside-20261029-102417-806449/four-side-axis-native.json')
        self.assertGreater(count,1)
        self.assertIn('explicit native singleton fixture contract',failures)
        self.assertIn('all native fixture operations completed safely',failures)

    def grip_record(self):
        sp=[[10,20,0],[8,20,0],[30,20,0],[32,20,0]]
        gp=[sp[0],[7,20,0],sp[1],[8,21,0],sp[2],[33,20,0],sp[3],[32,21,0]]
        return {'before':{'stretch':{'points':sp},'grip':{'points':gp}},'indices':[2],'selectedStretchPoint':sp[2],'operationIndices':[4],'selectedGripIndex':4,'selectedGripPoint':gp[4],'operationMethod':'AcDbEntity::moveGripPointsAt','pointIndexSource':'actual getGripPoints coordinate matched to semantic stretch endpoint'}

    def binding_failures(self,record):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_endpoint_index_binding(record,False,check,'binding')
        return failures

    def test_horizontal_public_grip_index_is_coordinate_derived(self):
        self.assertEqual(self.binding_failures(self.grip_record()),[])

    def test_semantic_stretch_index_cannot_be_used_as_grip_index(self):
        r=self.grip_record();r['operationIndices']=[2];r['selectedGripIndex']=2
        self.assertTrue(self.binding_failures(r))

    def test_duplicate_grip_coordinate_cannot_be_guessed(self):
        r=self.grip_record();r['before']['grip']['points'].append(list(r['selectedGripPoint']))
        self.assertTrue(self.binding_failures(r))

    def test_false_grip_coordinate_cannot_be_claimed(self):
        r=self.grip_record();r['selectedGripPoint']=[31,20,0]
        self.assertTrue(self.binding_failures(r))


if __name__=='__main__':unittest.main()
