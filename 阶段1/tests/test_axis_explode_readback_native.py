"""Public explode readback evidence tests; no CAD or controller execution."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from verify_axis_explode_readback_native import inspect


class ExplodeReadbackTests(unittest.TestCase):
    def fixture(self):
        temp=tempfile.TemporaryDirectory(prefix='axis-explode-readback-');self.addCleanup(temp.cleanup)
        run=Path(temp.name);(run/'control.dwg').write_bytes(b'Synthetic_No_CAD')
        axis=lambda h:dict(handle=h,type='TDbAxisLabelSet',proxy=False,stretch={'status':0,'points':[[10,0,0],[10,20,0]]})
        originals=[dict(handle=str(i),type='SyntheticOriginal') for i in range(213)]
        axes=[]
        for h in ('A1','A2'):
            base=dict(transient=True,objectIdNull=True,ownerIdNull=True,databaseNull=True,parentNodeId=0,depth=1)
            nodes=[]
            for index,(kind,fields) in enumerate((('circle',dict(center=[10,0,0],radius=4,normal=[0,0,1])),('text',dict(text='1',plainText='1',position=[10,0,0],alignmentPoint=[10,0,0],height=2,rotation=0,bounds={'status':0,'min':[9,-1,0],'max':[11,1,0]})),('circle',dict(center=[10,20,0],radius=4,normal=[0,0,1])),('text',dict(text='1',plainText='1',position=[10,20,0],alignmentPoint=[10,20,0],height=2,rotation=0,bounds={'status':0,'min':[9,19,0],'max':[11,21,0]})),('line',dict(start=[10,0,0],end=[10,20,0]))),1):
                nodes.append(dict(base,nodeId=index,type={'circle':'AcDbCircle','text':'AcDbText','line':'AcDbLine'}[kind],kind=kind,**fields))
            axes.append(dict(axisHandle=h,before=axis(h),after=axis(h),rootExplodeStatus=0,explodedNodeCount=5,returnedTransientCount=5,deletedTransientCount=5,ownershipSafe=True,unresolvedOwnershipCount=0,rejectedBorrowedCount=0,duplicatePointerCount=0,allLeavesRead=True,hasText=True,hasCircle=True,explodeCalls=[dict(nodeId=0,depth=0,status=0,returnedCount=5)],nodes=nodes,completed=True,error=None))
        model=originals+[axis('A1'),axis('A2')]
        d=dict(contract='stage1.axis-explode-readback-native.v1',api='ObjectARX native public AcDbEntity::explode readback; no COM',controlDWG=str(run/'control.dwg'),initialModelCount=215,finalModelCount=215,axisCount=2,protectedNonAxisCount=213,allModelBefore=model,allModelAfter=copy.deepcopy(model),modelMembershipUnchanged=True,dbmodBefore=0,dbmodAfter=0,activeTransactionsBefore=0,activeTransactionsAfter=0,axes=axes,ownershipSafe=True,dwgSaved=False,persistentAppendAttempted=False,persistentEraseAttempted=False,privateFieldsAccessed=False,commandBridgeUsed=False,comUsed=False,completed=True,error=None)
        return run,d
    def result(self,run,d,**kwargs):
        path=run/'axis-explode-readback-native.json';path.write_text(json.dumps(d),encoding='utf-8');return inspect(path,**kwargs)
    def test_valid_readback_is_capability_only(self):
        run,d=self.fixture();r=self.result(run,d);self.assertEqual(r['result'],'PASS',r);self.assertEqual(r['numberingSemantics'],'READBACK_ONLY')
    def test_model_change_is_rejected(self):
        run,d=self.fixture();d['allModelAfter'][0]['type']='Changed';self.assertEqual(self.result(run,d)['result'],'FAIL')
    def test_unsupported_explode_is_rejected(self):
        run,d=self.fixture();d['axes'][0]['rootExplodeStatus']=1;self.assertEqual(self.result(run,d)['result'],'FAIL')
    def test_uncertain_ownership_leak_or_database_pointer_is_rejected(self):
        for f in ('unresolved','leak','borrowed','duplicate'):
            with self.subTest(f=f):
                run,d=self.fixture()
                if f=='unresolved':d['axes'][0]['unresolvedOwnershipCount']=1
                elif f=='leak':d['axes'][0]['deletedTransientCount']=4
                elif f=='borrowed':d['axes'][0]['nodes'][0]['databaseNull']=False
                else:d['axes'][0]['nodes'][1]['nodeId']=1
                self.assertEqual(self.result(run,d)['result'],'FAIL')
    def test_private_write_and_dbmod_changes_are_rejected(self):
        for key,value in (('privateFieldsAccessed',True),('persistentAppendAttempted',True),('dwgSaved',True),('dbmodAfter',1)):
            run,d=self.fixture();d[key]=value;self.assertEqual(self.result(run,d)['result'],'FAIL')
    def test_nonfinite_geometry_and_empty_text_are_rejected(self):
        for bad in ('nan','empty'):
            run,d=self.fixture()
            if bad=='nan':d['axes'][0]['nodes'][0]['radius']=float('nan')
            else:
                for n in d['axes'][0]['nodes']:
                    if n['kind']=='text':n['text']=n['plainText']=''
            self.assertEqual(self.result(run,d)['result'],'FAIL')
    def test_seed_is_not_full_four_side_semantics(self):
        run,d=self.fixture();self.assertEqual(self.result(run,d,expect_four_side=True)['result'],'FAIL')
    def test_missing_control_is_rejected(self):
        run,d=self.fixture();(run/'control.dwg').unlink();self.assertEqual(self.result(run,d)['result'],'FAIL')
    def test_wrong_parent_depth_is_rejected(self):
        run,d=self.fixture();d['axes'][0]['nodes'][0]['parentNodeId']=999;self.assertEqual(self.result(run,d)['result'],'FAIL')
    def full_fixture(self):
        run,d=self.fixture();prototype=d['axes'][0];axes=[]
        for orientation,labels in (('vertical',[str(i) for i in range(1,11)]),('horizontal',list('ABCDEFGHJKLM'))):
            for station,label in enumerate(labels):
                a=copy.deepcopy(prototype);h=orientation+str(station);a['axisHandle']=h
                centers=([station,-100,0],[station,100,0]) if orientation=='vertical' else ([-200,station,0],[200,station,0])
                a['before']=dict(handle=h,type='TDbAxisLabelSet',proxy=False,stretch={'status':0,'points':list(centers)})
                a['after']=copy.deepcopy(a['before'])
                for i,center in enumerate(centers):
                    a['nodes'][i*2]['center']=center;a['nodes'][i*2+1].update(text=label,plainText=label,position=center,alignmentPoint=center,bounds={'status':0,'min':[center[0]-1,center[1]-1,0],'max':[center[0]+1,center[1]+1,0]})
                a['nodes'][-1].update(start=centers[0],end=centers[1]);axes.append(a)
        d.update(initialModelCount=235,finalModelCount=235,axisCount=22,axes=axes)
        d['allModelBefore']=d['allModelBefore'][:213]+[copy.deepcopy(a['before']) for a in axes]
        d['allModelAfter']=copy.deepcopy(d['allModelBefore']);return run,d
    def test_44_actual_public_text_circle_pairs_verify_four_side(self):
        run,d=self.full_fixture();r=self.result(run,d,expect_four_side=True)
        self.assertEqual(r['result'],'PASS',r);self.assertEqual(r['numberingSemantics'],'PASS_ACTUAL_PUBLIC_TEXT_44_CIRCLES')
    def test_wrong_actual_letter_cannot_be_replaced_by_expected(self):
        run,d=self.full_fixture();d['axes'][18]['nodes'][1].update(text='I',plainText='I')
        self.assertEqual(self.result(run,d,expect_four_side=True)['result'],'FAIL')
        self.assertEqual(self.result(run,d)['result'],'PASS')

if __name__=='__main__':unittest.main()
