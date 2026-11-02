"""Synthetic panel evidence only: never opens or controls CAD/windows."""
import copy
import unittest
from verify_elevation_dialog_native import validate


class ElevationDiscoveryTests(unittest.TestCase):
    def fixture(self):
        controls=[]
        def control(hwnd,cls,text,style=0,check=-1):
            return dict(hwnd=hwnd,parent=100,processId=123,threadId=456,controlId=hwnd+900,
                        className=cls,text=text,style=style,exStyle=0,visible=True,enabled=True,
                        rectRead=True,rect=[0,0,100,20],checkRead=check!=-1,checkState=check)
        controls=[control(1,'Static','标高改值'),control(2,'Button','增加',9,1),
                  control(3,'Button','减少',9,0),control(4,'Button','高度变化值<'),
                  control(5,'Edit','3.000')]
        snapshot=dict(hwnd=100,processId=123,threadId=456,desktop='CodexS1_'+'1'*32,title='',
                      className='#32770',recognized=True,enumerationComplete=True,rect=[0,0,200,100],
                      roles=dict(changeEdit=5,increaseRadio=2,decreaseRadio=3,okButton=0,cancelButton=0),controls=controls)
        model=[dict(handle=str(i),type='SyntheticOriginal') for i in range(213)]
        return dict(contract='stage1.elevation-dialog-discover-native.v1',api='ObjectARX native + read-only standard Win32 control discovery; no COM',initialModelCount=213,finalModelCount=213,allModelBefore=model,allModelAfter=copy.deepcopy(model),dbmodBefore=0,dbmodAfter=0,commandAttempted=True,commandName='TChangeValElev',commandStatus=5100,cmdaBefore=1,cmdaAfter=1,cmdnamesBefore='S1ELEVATIONDIALOGDISCOVER',cmdnamesAfter='S1ELEVATIONDIALOGDISCOVER',discovery=dict(processId=123,threadId=456,desktop=snapshot['desktop'],recognized=True,captureSource='read-only sweep after zero-selection command returned',snapshot=snapshot,latestSnapshots=[snapshot],cancelAttempted=True,cancelDelivered=True,cancelMethod='WM_CLOSE exact semantically identified own parameter panel; no OK/Cancel assumed',cancelWin32Error=0,closeObserved=True,timedOut=False,timerStopReturned=True,timerStopped=True,error=None),commandTrace=[dict(event='commandWillStart',command='TChangeValElev'),dict(event='commandEnded',command='TChangeValElev')],inputTrace=[],trace=dict(editorAdded=True,editorRemoved=True,inputAddStatus=0,inputRemoveStatus=0,retained=False,traceDropped=False),callbackRetained=False,dwgSaved=False,fieldsChanged=False,radioChanged=False,objectsSelectedByProbe=False,comUsed=False,computerUseUsed=False,completed=True,error=None)
    def test_modeless_no_ok_cancel_and_descendant_caption_is_valid(self):self.assertEqual(validate(self.fixture())['result'],'PASS')
    def test_unknown_class_or_desktop_is_rejected(self):
        for key,value in (('className','Unknown'),('desktop','Default')):
            d=self.fixture();d['discovery']['snapshot'][key]=value;self.assertEqual(validate(d)['result'],'FAIL')
    def test_hidden_old_tab_radio_cannot_identify_active_panel(self):
        d=self.fixture();d['discovery']['snapshot']['controls'][1]['visible']=False;self.assertEqual(validate(d)['result'],'FAIL')
    def test_wrong_roles_or_two_selected_radios_is_rejected(self):
        for bad in ('edit','checks'):
            d=self.fixture()
            if bad=='edit':d['discovery']['snapshot']['roles']['changeEdit']=4
            else:d['discovery']['snapshot']['controls'][2]['checkState']=1
            self.assertEqual(validate(d)['result'],'FAIL')
    def test_model_or_parameter_change_is_rejected(self):
        for key in ('dbmodAfter','fieldsChanged','radioChanged','objectsSelectedByProbe'):
            d=self.fixture();d[key]=1 if key=='dbmodAfter' else True;self.assertEqual(validate(d)['result'],'FAIL')
    def test_timer_callback_or_command_imbalance_is_rejected(self):
        d=self.fixture();d['callbackRetained']=True;self.assertEqual(validate(d)['result'],'FAIL')
        d=self.fixture();d['commandTrace'].pop();self.assertEqual(validate(d)['result'],'FAIL')
    def test_selected_entity_in_actual_input_trace_is_rejected(self):
        d=self.fixture();d['inputTrace']=[dict(selectedHandle='CE8F')];self.assertEqual(validate(d)['result'],'FAIL')
    def test_unknown_cancel_or_unclosed_window_is_rejected(self):
        for key,value in (('cancelMethod','click guessed button'),('closeObserved',False),('timerStopped',False)):
            d=self.fixture();d['discovery'][key]=value;self.assertEqual(validate(d)['result'],'FAIL')
    def test_designed_cancel_trace_is_valid(self):
        d=self.fixture();d['commandTrace'][-1]['event']='commandCancelled';d['commandStatus']=-5002
        self.assertEqual(validate(d)['result'],'PASS')
    def prompt_fixture(self):
        d=self.fixture();d['contract']='stage1.elevation-dialog-discover-native.v2';s=d['discovery'];snap=s['snapshot']
        snap.update(className='ActualCustomTchParameterWindow',recognized=False,semanticRolesComplete=True,isMainFrame=False)
        s.update(recognized=False,snapshotCaptured=True,promptCaptured=True,autoClosed=True,cancelAttempted=False,
                 cancelDelivered=False,cancelMethod=None,promptObserverAdded=True,promptObserverRemoved=True,
                 promptObserverRetained=False,promptAddStatus=0,promptRemoveStatus=0,captureSource='beginSSGet')
        s['promptCaptureEvents']=[dict(event='beginSSGet',prompt='请选择要改值的标高标注<退出>:',processId=123,threadId=456,
            desktop=s['desktop'],snapshotCaptured=True,windowSnapshots=[copy.deepcopy(snap)],controlsWritten=False,closeAttempted=False)]
        d['inputTrace']=[dict(event='beginSSGet',prompt='请选择要改值的标高标注<退出>:')];return d
    def test_v2_prompt_custom_panel_auto_close_is_not_faked_cancel(self):
        result=validate(self.prompt_fixture());self.assertEqual(result['result'],'PASS',result)
        self.assertEqual(result['closureMode'],'ZERO_SELECTION_AUTO_CLOSE_OBSERVED')
    def test_v2_callback_must_never_write_or_close(self):
        for key in ('controlsWritten','closeAttempted'):
            d=self.prompt_fixture();d['discovery']['promptCaptureEvents'][0][key]=True
            self.assertEqual(validate(d)['result'],'FAIL')
    def test_v2_auto_close_requires_actual_prompt_evidence_and_no_cancel_delivery(self):
        for change in ({'cancelDelivered':True},{'promptCaptureEvents':[]},{'promptObserverRemoved':False}):
            d=self.prompt_fixture();d['discovery'].update(change);self.assertEqual(validate(d)['result'],'FAIL')
    def test_v2_mainframe_never_qualifies_as_parameter_panel(self):
        d=self.prompt_fixture();d['discovery']['snapshot']['isMainFrame']=True
        self.assertEqual(validate(d)['result'],'FAIL')

if __name__=='__main__':unittest.main()
