"""Synthetic visibility/cleanup rejection guards; never PDF or host evidence."""
import copy
import unittest
from verify_axis_overlap_preview_native import check_visibility_page,visibility_map


def refresh():
    return {'commandName':'_.REGEN','attempted':True,'completed':True,'commandStatus':5100,'activeTransactionsBefore':0,'activeTransactionsAfter':0}


def fixture():
    geometry=[{'handle':str(i),'geometry':[i,0,0]} for i in range(235)]
    visibility=[{'handle':str(i),'visibility':0} for i in range(235)]
    hidden=copy.deepcopy(visibility);hidden[0]['visibility']=1
    record={'error':None,'completed':True,'safeToContinue':True,'hiddenAxisHandle':'0','visibleCompanionHandle':'1','activeTransactionsBefore':0,'activeTransactionsAfter':0,
            'refreshBefore':refresh(),'geometryBefore':copy.deepcopy(geometry),'geometryHidden':copy.deepcopy(geometry),'geometryRestored':copy.deepcopy(geometry),
            'globalsBefore':{'BACKGROUNDPLOT':0},'globalsAfter':{'BACKGROUNDPLOT':0},'layoutBefore':{'paper':'existing'},'layoutAfter':{'paper':'existing'},
            'visibilityBefore':copy.deepcopy(visibility),'visibilityHidden':hidden,'visibilityRestored':copy.deepcopy(visibility),
            'visibilityOverride':{'api':'AcDbEntity::setVisibility','doSubents':False,'axisHandle':'0','originalVisibility':0,'setAttempted':True,'setStatus':0,'setTransactionStatus':0,'hiddenReadback':1,
                'restoreAttempted':True,'restoreStatus':0,'restoreTransactionStatus':0,'restoredReadback':0,'restored':True,'restoreError':None,'hiddenRefresh':refresh(),'restoredRefresh':refresh()}}
    return record,geometry,visibility


class OverlapVisibilityContractTests(unittest.TestCase):
    def failures(self,record,geometry,visibility):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_visibility_page(record,geometry,visibility,'0','1',check,'page')
        return failures

    def test_exact_one_target_hide_and_complete_restore_accepted(self):
        self.assertEqual(self.failures(*fixture()),[])

    def test_hiding_both_circles_is_rejected(self):
        values=fixture();values[0]['visibilityHidden'][1]['visibility']=1
        self.assertTrue(any('exactly one' in f for f in self.failures(*values)))

    def test_other_entity_visibility_change_is_rejected(self):
        values=fixture();values[0]['visibilityHidden'][200]['visibility']=1
        self.assertTrue(any('exactly one' in f for f in self.failures(*values)))

    def test_unrestored_actual_visibility_rejected_despite_success_flag(self):
        values=fixture();values[0]['visibilityRestored'][0]['visibility']=1
        self.assertTrue(any('states restored' in f for f in self.failures(*values)))

    def test_restore_api_failure_rejected_despite_success_flag(self):
        values=fixture();values[0]['visibilityOverride']['restoreStatus']=33
        self.assertTrue(any('restored by native setter' in f for f in self.failures(*values)))

    def test_geometry_mutation_during_hide_rejected(self):
        values=fixture();values[0]['geometryHidden'][50]['geometry'][0]+=1
        self.assertTrue(any('complete235 public geometry geometryHidden' in f for f in self.failures(*values)))

    def test_missing_entity_visibility_record_rejected(self):
        values=fixture();values[0]['visibilityRestored'].pop()
        self.assertTrue(any('states restored' in f for f in self.failures(*values)))

    def test_open_transaction_across_regen_is_rejected(self):
        values=fixture();values[0]['visibilityOverride']['hiddenRefresh']['activeTransactionsBefore']=1
        self.assertTrue(any('zero transactions' in f for f in self.failures(*values)))

    def test_no_native_refresh_is_rejected(self):
        values=fixture();values[0]['visibilityOverride']['restoredRefresh']['attempted']=False
        self.assertTrue(any('actual public REGEN' in f for f in self.failures(*values)))

    def test_duplicate_visibility_handles_rejected(self):
        self.assertIsNone(visibility_map([{'handle':'A','visibility':0},{'handle':'A','visibility':1}]))

    def test_boolean_is_not_native_visibility_enum(self):
        self.assertIsNone(visibility_map([{'handle':'A','visibility':False}]))

    def test_page_layout_change_rejected_even_if_visibility_restored(self):
        values=fixture();values[0]['layoutAfter']['paper']='changed'
        self.assertTrue(any('page saved layout' in f for f in self.failures(*values)))


if __name__=='__main__':unittest.main()
