"""Adversarial checker contracts only; synthetic records are never host evidence."""
import copy
import unittest
from verify_axis_continuous_number_native import check_number_request,check_merge_operation,single_auxiliary_policy,subtract_point_bag

def entity(handle,x):
    return {'handle':handle,'type':'TDbAxisLabelSet','proxy':False,'layer':'AXIS','curve':None,'extentStatus':0,'min':[x-4,-14,0],'max':[x+4,34,0],
      'grip':{'status':0,'points':[[x,20,0],[x,30,0],[x,26,0],[x,0,0],[x,-10,0],[x,-6,0],[x-4,26,0],[x-4,-6,0]]},
      'stretch':{'status':0,'points':[[x,20,0],[x,26,0],[x,0,0],[x,-6,0]]}}
def trace():return {'editorAdded':True,'editorRemoved':True,'inputAddStatus':0,'inputRemoveStatus':0,'retained':False,'traceDropped':False}
def command(name):return [{'event':'commandWillStart','command':name},{'event':'commandEnded','command':name}]
def number():
    a=entity('A1',10)
    return {'contract':'stage1.axis-continuous-number-operation.v1','api':'ObjectARX native + public Tianzheng command; no COM','error':None,
      'commandName':'TReAxisNo','commandStatus':5100,'commandAttempted':True,'activeTransactionsBefore':0,'activeTransactionsAfter':0,'coordinateTransformStatus':5100,
      'pickMatchesActualPublicGrip':True,'axisHandle':'A1','before':a,'after':copy.deepcopy(a),'requestedNumber':'1','pickWcs':[10,-6,0],'pickUcs':[10,-6,0],
      'tokens':['TReAxisNo','actual grip WCS converted to UCS','1',''],'trace':trace(),'commandTrace':command('TReAxisNo'),
      'inputTrace':[{'event':'endSSGet','selectedHandles':['A1']}],'commandCompleted':True,'safeToContinue':True,
      'numberSemantics':'pending rendered circles or independently confirmed public text evidence'}
def merge():
    a,b=entity('A1',10),entity('A2',25);c=copy.deepcopy(a)
    c['grip']['points']=copy.deepcopy(a['grip']['points'][:6]+b['grip']['points'][:6]+a['grip']['points'][6:])
    c['stretch']['points']=copy.deepcopy(a['stretch']['points']+b['stretch']['points'])
    c['max']=[29,34,0]
    protected=[{'handle':'B1','type':'AcDbLine'}]
    return {'contract':'stage1.axis-merge-operation.v2','api':'ObjectARX native + public Tianzheng command; no COM','error':None,'commandName':'TLabelMerge','commandStatus':5100,'commandAttempted':True,
      'beforeSets':[a,b],'afterSets':[c],'orientation':'vertical','requestedCollectionCount':2,'targetHandles':['A1','A2'],
      'pickWcs':[[10,22,0],[25,22,0]],'pointTransformStatuses':[5100,5100],'protectedBefore':protected,'protectedAfter':copy.deepcopy(protected),
      'beforeModelCount':3,'afterModelCount':2,'activeTransactionsBefore':0,'activeTransactionsAfter':0,'trace':trace(),'commandTrace':command('TLabelMerge'),
      'inputTrace':[{'event':'endSSGet','selectedHandles':['A1']},{'event':'endSSGet','selectedHandles':['A2']}],
      'pointUnionMatched':True,'stationGripsMatched':True,'auxiliaryMatched':True,'boundsMatched':True,'commandCompleted':True,'safeToContinue':True,'mergedAxisHandle':'A1',
      'auxiliaryControlPolicy':single_auxiliary_policy(a,True),'actualAuxiliaryPoints':copy.deepcopy(a['grip']['points'][6:]),'auxiliaryAfterSign':-1,
      'expectedMergedBounds':{'min':[6,-14,0],'max':[29,34,0]},'numberSemantics':'not checked by merge; renumber and render both sides separately'}

class NumberingVerifierTests(unittest.TestCase):
    def failures(self,fn,data):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        fn(data,check,'test');return failures
    def test_structure_has_no_implied_number_semantic_pass(self):
        self.assertEqual(self.failures(check_number_request,number()),[])
        d=number();d['numberSemantics']='verified'
        self.assertTrue(any('semantic proof not fabricated' in x for x in self.failures(check_number_request,d)))
    def test_success_return_without_real_selection_is_rejected(self):
        d=number();d['inputTrace']=[]
        self.assertTrue(any('actually' in x or 'selected only' in x for x in self.failures(check_number_request,d)))
    def test_preserved_standard_block_cannot_replace_tch_collection(self):
        d=number();d['before']['type']=d['after']['type']='AcDbBlockReference'
        self.assertTrue(any('native Tianzheng' in x for x in self.failures(check_number_request,d)))
    def test_unobserved_or_invented_public_grip_is_rejected(self):
        d=number();d['pickWcs']=[10,12,0]
        self.assertTrue(any('independently belongs' in x for x in self.failures(check_number_request,d)))
    def test_merge_allows_real_exterior_stub_midpoint(self):
        self.assertEqual(self.failures(check_merge_operation,merge()),[])
    def test_merge_rejects_internal_building_span_pick(self):
        d=merge();d['pickWcs'][0]=[10,12,0]
        self.assertTrue(any('source pick bound' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_flag_does_not_hide_changed_station_point(self):
        d=merge();d['afterSets'][0]['stretch']['points'][0][0]+=1
        self.assertTrue(any('complete stretch-point union' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_all_targets_must_be_observed(self):
        d=merge();d['inputTrace']=d['inputTrace'][:1]
        self.assertTrue(any('actual base and all other' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_joint_positive_mirror_is_valid(self):
        d=merge();aux=copy.deepcopy(d['auxiliaryControlPolicy']['positiveState'])
        d['afterSets'][0]['grip']['points'][-2:]=copy.deepcopy(aux)
        d['actualAuxiliaryPoints']=aux;d['auxiliaryAfterSign']=1
        self.assertEqual(self.failures(check_merge_operation,d),[])
    def test_merge_joint_mixed_signs_rejected(self):
        d=merge();d['afterSets'][0]['grip']['points'][-1][0]+=8
        self.assertTrue(any('one declared joint' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_auxiliary_intermediate_offset_rejected(self):
        d=merge()
        for p in d['afterSets'][0]['grip']['points'][-2:]:p[0]+=2
        self.assertTrue(any('one declared joint' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_auxiliary_wrong_near_edge_rejected(self):
        d=merge();d['afterSets'][0]['grip']['points'][-1][1]+=1
        self.assertTrue(any('one declared joint' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_auxiliary_wrong_z_rejected(self):
        d=merge();d['afterSets'][0]['grip']['points'][-1][2]=1
        self.assertTrue(any('one declared joint' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_real_circle_grip_movement_rejected(self):
        d=merge();d['afterSets'][0]['grip']['points'][1][1]+=1
        self.assertTrue(any('six station grips retained' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_missing_real_station_grip_rejected(self):
        d=merge();d['afterSets'][0]['grip']['points'].pop(4)
        self.assertTrue(any('six station grips retained' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_added_unexplained_grip_rejected(self):
        d=merge();d['afterSets'][0]['grip']['points'].append([0,0,0])
        self.assertTrue(any('one declared joint' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_changed_circle_bounds_rejected(self):
        d=merge();d['afterSets'][0]['max'][1]+=1
        self.assertTrue(any('complete circle bounds' in x for x in self.failures(check_merge_operation,d)))
    def test_merge_bad_source_radius_rejected_despite_claimed_policy(self):
        d=merge();d['beforeSets'][1]['max'][1]+=1
        self.assertTrue(any('source auxiliary/radius/bounds' in x for x in self.failures(check_merge_operation,d)))
    def test_declared_policy_cannot_be_recomputed_after_command(self):
        d=merge();d['auxiliaryControlPolicy']['declaredBeforeCommand']=False
        self.assertTrue(any('states declared from complete base' in x for x in self.failures(check_merge_operation,d)))
    def test_multiset_subtraction_retains_coincident_auxiliary_copy(self):
        # Another station may coincide with firstStation+R: coordinate filters
        # would drop the auxiliary. Multiplicity leaves exactly one copy.
        a=[[10,20,0],[14,26,0],[14,26,0]];required=[[10,20,0],[14,26,0]]
        self.assertEqual(subtract_point_bag(a,required),[[14,26,0]])
    def test_numbering_does_not_accept_auxiliary_mirror(self):
        d=number();d['after']['grip']['points'][-1][0]+=8
        self.assertTrue(any('no geometric mutation grip' in x for x in self.failures(check_number_request,d)))
    def test_horizontal_single_policy_has_same_joint_definition(self):
        a=entity('H1',10)
        for key in ('min','max'):a[key][0],a[key][1]=a[key][1],a[key][0]
        for kind in ('grip','stretch'):
            for p in a[kind]['points']:p[0],p[1]=p[1],p[0]
        policy=single_auxiliary_policy(a,False)
        self.assertEqual(policy['sourceSign'],-1)
        self.assertEqual(policy['positiveState'],[[-6,14,0],[26,14,0]])
if __name__=='__main__':unittest.main()
