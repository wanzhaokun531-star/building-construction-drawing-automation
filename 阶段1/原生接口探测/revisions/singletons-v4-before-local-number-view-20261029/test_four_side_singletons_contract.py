"""Reject unrefreshed/stale evidence and never reclassify prior native failures."""
import copy
import json
import unittest
from pathlib import Path
from verify_four_side_singletons_native import check_refresh,check_rigid_translation,check_endpoint_index_binding,validate

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
        rows=[{'index':i,'gripPoint':list(p),'appDataNonNull':True} for i,p in enumerate(gp)]
        modern={'contract':'stage1.singleton-modern-move.v1','semanticStretchPoint':list(sp[2]),'offset':[1,0,0],'getFlags':0,'moveFlags':0,'openStatus':0,'getStatus':0,'moveStatus':0,'transactionStatus':0,'moveAttempted':True,'modernGrips':rows,'selectedModernGripIndex':4,'selectedGripPoint':list(gp[4]),'opaqueAppDataPassedUnchanged':True,'appDataDecoded':False,'completed':True,'safeToContinue':True,'error':None,
                'lifecycle':{'getCalled':True,'returnedGripDataCount':8,'deletedUniqueGripDataCount':8,'gripsToBeDeletedCalled':True,'gripsToBeDeletedReturned':True,'gripsDoneCalled':True,'gripsDoneReturned':True,'allGripWrappersReleased':True,'safeToContinue':True,'cleanupThrew':False,'appDataDecoded':False,'appDataDeleted':False},
                'view':{'screenStatus':5100,'screenRestype':5002,'screenPixels':[1200,800,0],'viewSizeStatus':5100,'viewSize':40000,'gripSizeStatus':5100,'gripSizePixels':5,'viewDirStatus':5100,'viewDirection':[0,0,1],'pixelsPerDrawingUnit':0.02}}
        return {'before':{'stretch':{'points':sp},'grip':{'points':gp}},'indices':[2],'selectedStretchPoint':sp[2],'operationIndices':[4],'selectedGripIndex':4,'selectedGripPoint':gp[4],'operationMethod':'AcDbEntity::moveGripPointsAt(AcDbVoidPtrArray,offset,flags)','pointIndexSource':'actual modern getGripPoints coordinate matched to semantic stretch endpoint','modern':modern,'moveAttempted':True,'currentAcross':30,'targetAcross':31,'openStatus':0,'moveStatus':0,'transactionStatus':0}

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
        r=self.grip_record();r['modern']['modernGrips'].append({'index':8,'gripPoint':list(r['selectedGripPoint']),'appDataNonNull':True})
        self.assertTrue(self.binding_failures(r))

    def test_false_grip_coordinate_cannot_be_claimed(self):
        r=self.grip_record();r['selectedGripPoint']=[31,20,0]
        self.assertTrue(self.binding_failures(r))

    def test_modern_order_is_independent_of_legacy_grip_indices(self):
        r=self.grip_record();rows=list(reversed(r['modern']['modernGrips']))
        for i,row in enumerate(rows):row['index']=i
        r['modern']['modernGrips']=rows;r['modern']['selectedModernGripIndex']=3;r['operationIndices']=[3];r['selectedGripIndex']=3
        self.assertEqual(self.binding_failures(r),[])

    def test_legacy_public_grip_protocol_cannot_replace_modern_call(self):
        r=self.grip_record();r['operationMethod']='AcDbEntity::moveGripPointsAt'
        self.assertTrue(self.binding_failures(r))

    def test_nonnull_opaque_identity_and_complete_lifecycle_are_required(self):
        for mutation in ('appData','delete','decode'):
            with self.subTest(mutation=mutation):
                r=self.grip_record()
                if mutation=='appData':r['modern']['modernGrips'][4]['appDataNonNull']=False
                elif mutation=='delete':r['modern']['lifecycle']['deletedUniqueGripDataCount']=7
                else:r['modern']['appDataDecoded']=True
                self.assertTrue(self.binding_failures(r))

    def rigid_record(self):
        source=ROOT/'阶段1/runs/quiet-moderngrip-20261029-105252-668492/modern-axis-grip-native.json'
        before=json.loads(source.read_text('utf-8-sig'))['axisBefore'];sp=before['stretch']['points'];anchor=sorted(sp,key=lambda p:p[0])[1]
        left,station=15111.890259172244,16541.263551952812;delta=[left-anchor[0],station-anchor[1],0];after=copy.deepcopy(before)
        def shifted(points):return [[p[i]+delta[i] for i in range(3)] for p in points]
        for key in ('grip','stretch'):after[key]['points']=shifted(before[key]['points'])
        after['min']=shifted([before['min']])[0];after['max']=shifted([before['max']])[0]
        return {'before':before,'translated':after,'translationPolicy':'H-left-inner-to-scheme-left-wall-and-station','translationSourceAnchor':anchor,'translationTargetAnchor':[left,station,anchor[2]],'translationOffset':delta},left,station

    def rigid_failures(self,record,left,station):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_rigid_translation(record,False,station,left,check,'rigid')
        return failures

    def test_H_rigid_placement_translates_all_real_source_controls(self):
        r,left,station=self.rigid_record()
        self.assertEqual(self.rigid_failures(r,left,station),[])

    def test_extra500_cannot_be_added_to_declared_rigid_offset(self):
        r,left,station=self.rigid_record();r['translationOffset'][0]-=500
        self.assertTrue(self.rigid_failures(r,left,station))

    def test_only_moving_anchor_cannot_replace_whole_entity_translation(self):
        r,left,station=self.rigid_record();r['translated']['grip']['points'][0][0]+=1
        self.assertTrue(self.rigid_failures(r,left,station))

    def test_native_bounds_must_follow_same_rigid_vector(self):
        r,left,station=self.rigid_record();r['translated']['max'][0]+=1
        self.assertTrue(self.rigid_failures(r,left,station))

    def test_wrong_source_anchor_cannot_be_declared_for_translation(self):
        r,left,station=self.rigid_record();r['translationSourceAnchor']=list(r['translationSourceAnchor']);r['translationSourceAnchor'][0]+=500
        self.assertTrue(self.rigid_failures(r,left,station))


if __name__=='__main__':unittest.main()
