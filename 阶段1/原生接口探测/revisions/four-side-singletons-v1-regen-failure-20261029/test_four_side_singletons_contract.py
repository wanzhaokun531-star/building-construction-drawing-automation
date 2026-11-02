"""Reject unrefreshed/stale evidence and never reclassify prior native failures."""
import copy
import unittest
from pathlib import Path
from verify_four_side_singletons_native import check_refresh,validate

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


if __name__=='__main__':unittest.main()
