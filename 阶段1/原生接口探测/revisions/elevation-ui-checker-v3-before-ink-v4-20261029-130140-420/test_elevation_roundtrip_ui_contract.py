"""Synthetic adversarial checker tests only; never native business evidence."""
import copy
import json
import tempfile
import unittest
from decimal import Decimal
from pathlib import Path
from verify_elevation_roundtrip_ui_native import check_actual_value,check_elevation_ui_bridge_operation,desired,inspect


def geometry(h='NEW'):
    return {'handle':h,'type':'TDbSymbElevation','proxy':False,'layer':'ELEV','grip':{'status':0,'points':[[10,20,0]]},'stretch':{'status':0,'points':[[10,20,0]]}}


def readback(text='0.123'):
    g=geometry();t={'nodeId':1,'parentNodeId':0,'depth':1,'type':'AcDbText','kind':'text','transient':True,'objectIdNull':True,'ownerIdNull':True,'databaseNull':True,
      'plainText':text,'rawText':text,'position':[10,20,0],'height':300}
    return {'contract':'stage1.elevation-public-visible-readback.v2','rootHandle':'NEW','before':g,'after':copy.deepcopy(g),'rootExplodeStatus':0,'completed':True,'error':None,
      'nodes':[copy.deepcopy(t)],'texts':[t],'explodeCalls':[{'nodeId':0,'depth':0,'status':0,'returnedCount':1}],'emptyPublicRepresentations':[],
      'returnedTransientCount':1,'deletedTransientCount':1,'ownershipSafe':True,'unresolvedOwnershipCount':0,'rejectedBorrowedCount':0,'duplicatePointerCount':0,
      'allLeavesRead':True,'allPublicLeavesAccountedFor':True,'numericValueInferred':False,'privateContentInferred':False,'readbackScope':'actual visible public values; no full private field equivalence'}


def value(text='0.123'):
    return {'valid':True,'actualText':text,'metresDecimal':text,'displayPrecision':3,'unit':'m','comparisonUnit':'m','valueSource':'returned public Text/MText only','derivedMillimetres':123}


def defaults(raw='0.123',increase=True):
    return {'rawValue':raw,'increaseChecked':increase,'decreaseChecked':not increase,'valid':True}


def controls(d):
    common={'processId':700,'threadId':701,'visible':True,'enabled':True,'parent':100}
    edit=dict(common,hwnd=101,className='Edit',controlId=57,text=d['rawValue'],style=0)
    plus=dict(common,hwnd=102,className='Button',controlId=58,text='增加',style=9,checkRead=True,checkState=int(d['increaseChecked']))
    minus=dict(common,hwnd=103,className='Button',controlId=59,text='减少',style=9,checkRead=True,checkState=int(d['decreaseChecked']))
    unit=dict(common,hwnd=104,className='Static',controlId=60,text='(M)',style=0)
    return {'hwnd':100,'className':'#32770','recognized':True,'semanticRolesComplete':True,'enumerationComplete':True,'isMainFrame':False,
      'processId':700,'threadId':701,'desktop':'CodexS1_fixture','roles':{'changeEdit':101,'increaseRadio':102,'decreaseRadio':103,'okButton':0,'cancelButton':0},'controls':[edit,plus,minus,unit]}


def attempt(mode='SetupOnly',before=None,after=None):
    before=copy.deepcopy(before or defaults('3.000'));after=copy.deepcopy(after or (before if mode=='ObserveOnly' else defaults()))
    protected=[{'handle':'OLD'+str(i)} for i in range(213 if mode=='Apply' else 214)]
    messages=[]
    if mode=='SetupOnly':messages=[{'message':12,'wParam':0,'target':101,'panel':100,'processId':700,'threadId':701,'returned':True,'reply':1}]
    trace={'editorAdded':True,'editorRemoved':True,'inputAddStatus':0,'inputRemoveStatus':0,'retained':False,'traceDropped':False}
    return {'contract':'stage1.elevation-ui-attempt.v1','implementationVersion':3,'syntheticEditNotifications':False,'progressFailed':False,'defaultsReceiptPersisted':True,'mode':mode,'error':None,'completed':True,'processId':700,'threadId':701,'desktop':'CodexS1_fixture','inputDesktop':'Default',
      'contextGuardPassed':True,'callbackCommandIssued':False,'timerUsed':False,'originalDefaults':before,'beforeDefaults':copy.deepcopy(before),'afterDefaults':after,
      'beforeControls':controls(before),'afterControls':controls(after),'unitLabels':['(M)'],'requestedRawValue':'0.123','requestedIncrease':True,
      'configurationAttempted':mode=='SetupOnly','configurationVerified':True,'messages':messages,'observerAdded':True,'observerRemoved':True,'observerRetained':False,
      'observerAddStatus':0,'observerRemoveStatus':0,'trace':trace,'commandTrace':[{'event':'commandWillStart','command':'TChangeValElev'},{'event':'commandEnded','command':'TChangeValElev'}],
      'commandName':'TChangeValElev','commandStatus':5100,'activeTransactionsBefore':0,'activeTransactionsAfter':0,'panelClosed':True,'safeToContinue':True,'lifecycleSafe':True,
      'modelProtectionVerified':True,'protectedBefore':protected,'protectedAfter':copy.deepcopy(protected),'initialModelCount':214,'finalModelCount':214,'modelMembershipUnchanged':True,'protectedCount':len(protected),
      'cmdaBefore':1,'cmdaAfter':1,'cmdnamesBefore':'S1ELEV','cmdnamesAfter':'S1ELEV','promptEvents':[{'event':'beginSSGet' if mode=='Apply' else 'beginGetPoint','prompt':'actual prompt','flags':0},{'event':'beginSSGet' if mode=='Apply' else 'beginGetPoint','configurationVerified':True,'callbackCommandIssued':False}],
      'selectionVerified':True,'actualSelectedHandles':['NEW'] if mode=='Apply' else [],'inputTrace':[{'event':'endSSGet','selectedHandles':['NEW']}] if mode=='Apply' else [],
      'expectedPickedHandle':'NEW' if mode=='Apply' else None,'targetBefore':geometry() if mode=='Apply' else None,'targetAfter':geometry() if mode=='Apply' else None,
      'preconfiguredAtPrompt':mode=='Apply','coordinateTransformStatus':5100 if mode=='Apply' else -9999,'pickMatchesActualPublicGrip':mode=='Apply','targetClass':'TDbSymbElevation' if mode=='Apply' else None,
      'pickWcs':[10,20,0],'pickUcs':[10,20,0],
      'selectionInput':{'protocol':'RTPICKS exact single target plus one empty Enter; no point token','createStatus':5100,'addStatus':5100,'lengthStatus':5100,'length':1,'nameStatus':5100,'objectIdStatus':0,'sameTarget':True,'actualHandle':'NEW','releaseStatus':5100,'released':True}}


def operation(mode='Apply'):
    a=attempt(mode,defaults() if mode=='Apply' else defaults('3.000'))
    pre=attempt('ObserveOnly',defaults()) if mode=='Apply' else None
    return {'contract':'stage1.elevation-ui-bridge-operation.v1','id':'synthetic-test','error':None,'completed':True,'safeToContinue':True,'mode':mode,'commandDispatched':True,'attempt':a,'preflight':pre,
      'requestedRawValue':'0.123','requestedIncrease':True,'originalDefaults':copy.deepcopy(a['originalDefaults']),'selectionTokensMayHaveBeenConsumed':mode=='Apply','targetHandle':'NEW','expectedPickedHandle':'NEW' if mode=='Apply' else None}


class ElevationUiContractTests(unittest.TestCase):
    def failures(self,fn,*args,receipt_mutator=None):
        errors=[]
        def check(ok,label):
            if not ok:errors.append(label)
            return bool(ok)
        if fn is check_elevation_ui_bridge_operation:
            # These tiny immutable files test receipt binding only. They are
            # synthetic and cannot prove a CAD command, control, or value.
            with tempfile.TemporaryDirectory() as temp:
                root=Path(temp).resolve();r=args[0]
                for i,a in enumerate((r.get('preflight'),r.get('attempt')),1):
                    if not isinstance(a,dict):continue
                    a['attemptSerial']=i;a['controlDWG']=str(root/'control.dwg');receipt=root/('elevation-ui-defaults-attempt-'+str(i)+'.json');a['defaultsReceipt']=str(receipt)
                    body={'contract':'stage1.elevation-ui-defaults-before-setter.v2','capturedBeforeAnySetter':True,'defaultsRestorationProven':False,'attemptSerial':i,
                          'processId':a.get('processId'),'threadId':a.get('threadId'),'desktop':a.get('desktop'),'id':r.get('id'),'mode':a.get('mode'),'run':str(root),
                          'originalDefaults':copy.deepcopy(a.get('originalDefaults')),'beforeControls':copy.deepcopy(a.get('beforeControls'))}
                    receipt.write_text(json.dumps(body),encoding='utf-8')
                    if receipt_mutator:receipt_mutator(receipt)
                fn(*args,check,'test')
        else:fn(*args,check,'test')
        return errors

    def test_actual_public_metres_is_accepted(self):
        self.assertEqual(self.failures(check_actual_value,readback(),value(),'NEW',Decimal('.123')),[])

    def test_gui_claim_does_not_replace_unchanged_public_value(self):
        self.assertTrue(self.failures(check_actual_value,readback('0.000'),value(),'NEW',Decimal('.123')))

    def test_metadata_cannot_replace_actual_nodes(self):
        r=readback();r['texts'][0]['plainText']='0.000'
        self.assertTrue(self.failures(check_actual_value,r,value(),'NEW',Decimal('.123')))

    def test_millimetres_cannot_be_used_as_primary_metres(self):
        v=value();v['unit']=v['comparisonUnit']='mm'
        self.assertTrue(self.failures(check_actual_value,readback(),v,'NEW',Decimal('.123')))

    def test_two_decimals_cannot_prove_point123(self):
        self.assertTrue(self.failures(check_actual_value,readback('0.12'),value('0.12'),'NEW',Decimal('.123')))

    def test_inverse_preserves_original_zero_text(self):
        errors=[]
        def check(ok,label):
            if not ok:errors.append(label)
            return bool(ok)
        check_actual_value(readback('0.000'),dict(value('0.000'),derivedMillimetres=0),'NEW',Decimal('0'),check,'inverse',exact_text='%%P0.000')
        self.assertTrue(any('format restored exactly' in s for s in errors))

    def test_unsafe_transient_ownership_cannot_be_business_proof(self):
        r=readback();r['ownershipSafe']=False
        self.assertTrue(self.failures(check_actual_value,r,value(),'NEW',Decimal('.123')))

    def test_apply_requires_independent_observe_reopen(self):
        self.assertEqual(self.failures(check_elevation_ui_bridge_operation,operation()),[])
        r=operation();r['preflight']=None
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_old_delta_at_preflight_is_rejected(self):
        r=operation();r['preflight']=attempt('ObserveOnly',defaults('3.000'))
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_same_input_desktop_is_rejected(self):
        r=operation();r['attempt']['inputDesktop']='CodexS1_fixture'
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_retained_callback_is_rejected(self):
        r=operation();r['attempt']['observerRetained']=True
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_other_actual_selected_handle_rejected(self):
        r=operation();r['attempt']['inputTrace'][0]['selectedHandles']=['CE8F'];r['attempt']['actualSelectedHandles']=['CE8F']
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_fake_grip_pick_rejected(self):
        r=operation();r['attempt']['pickWcs']=[11,20,0]
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_wrong_panel_unit_rejected(self):
        r=operation();r['attempt']['beforeControls']['controls'][-1]['text']='(MM)'
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_false_unconsumed_point_claim_rejected(self):
        r=operation();r['selectionTokensMayHaveBeenConsumed']=False
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_observe_only_cannot_set_the_value(self):
        r=operation();r['preflight']['messages']=[{'message':12,'wParam':0,'target':101,'panel':100,'processId':700,'threadId':701,'returned':True,'reply':1}]
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_default_restore_requires_exact_radio_and_raw_value(self):
        self.assertTrue(desired(defaults('3.000'), '3.000',True))
        self.assertFalse(desired(defaults('3.000',False),'3.000',True))
        self.assertFalse(desired(defaults('0.123'),'3.000',True))

    def test_setup_accepts_actual_dynamic_ids_other_than_discovery_sample(self):
        self.assertEqual(self.failures(check_elevation_ui_bridge_operation,operation('SetupOnly')),[])

    def test_rtpicks_does_not_require_getpoint_but_requires_real_selection(self):
        r=operation();self.assertEqual(self.failures(check_elevation_ui_bridge_operation,r),[])
        r['attempt']['actualSelectedHandles']=[];r['attempt']['inputTrace']=[{'event':'endSSGet','selectedHandles':[],'returnStatus':-5001}]
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_rtpicks_two_objects_cannot_pass_as_exact_target(self):
        r=operation();r['attempt']['selectionInput']['length']=2
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_rtpicks_release_failure_is_rejected(self):
        r=operation();r['attempt']['selectionInput']['released']=False
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_apply_cannot_rewrite_delta(self):
        r=operation();r['attempt']['messages'].append({'message':12,'wParam':0,'target':101,'panel':100,'processId':700,'threadId':701,'returned':True,'reply':1})
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_observe_allows_actual_readonly_text_and_radio_messages(self):
        r=operation();r['preflight']['messages']=[{'message':13,'wParam':2048,'target':100,'panel':100,'processId':700,'threadId':701,'returned':True,'reply':4},
          {'message':240,'wParam':0,'target':102,'panel':100,'processId':700,'threadId':701,'returned':True,'reply':1}]
        self.assertEqual(self.failures(check_elevation_ui_bridge_operation,r),[])

    def test_v2_rejects_synthetic_change_notification(self):
        r=operation();r['attempt']['messages'].append({'message':273,'wParam':(0x300<<16)|57,'target':100,'panel':100,'processId':700,'threadId':701,'returned':True,'reply':0})
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_old_v1_implementation_is_not_reclassified_as_v2(self):
        r=operation();r['attempt']['implementationVersion']=1
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,r))

    def test_durable_default_capture_cannot_claim_restore(self):
        def mutate(path):
            r=json.loads(path.read_text('utf-8'));r['defaultsRestorationProven']=True;path.write_text(json.dumps(r),encoding='utf-8')
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,operation(),receipt_mutator=mutate))

    def test_missing_immutable_default_receipt_is_rejected(self):
        self.assertTrue(self.failures(check_elevation_ui_bridge_operation,operation(),receipt_mutator=lambda path:path.unlink()))

    def test_partial_progress_without_final_native_report_cannot_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'elevation-ui-bridge-progress.json').write_text('{"completed":true,"defaultsReceiptPersisted":true}',encoding='utf-8')
            r=inspect(root/'elevation-roundtrip-native.json')
            self.assertEqual(r['result'],'FAIL');self.assertEqual(r['businessRoundtrip'],'UNVERIFIED')


if __name__=='__main__':unittest.main()
