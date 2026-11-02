"""Verify read-only standard Win32 discovery/cancellation evidence; never control CAD."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def validate(d):
    result=dict(contract='stage1.elevation-dialog-discovery-check.v1',result='FAIL',assertions=0,failures=[])
    def check(ok,label):
        result['assertions']+=1
        if not ok:result['failures'].append(label)
        return bool(ok)
    v2=d.get('contract')=='stage1.elevation-dialog-discover-native.v2'
    check(d.get('contract') in ('stage1.elevation-dialog-discover-native.v1','stage1.elevation-dialog-discover-native.v2'),'versioned native discovery contract')
    check(d.get('api')=='ObjectARX native + read-only standard Win32 control discovery; no COM','authorized native/standard Win32 route')
    check(d.get('completed') is True and d.get('error') is None,'native discovery completed')
    before=d.get('allModelBefore') or []
    check(d.get('initialModelCount')==d.get('finalModelCount')==len(before)==213
          and before==d.get('allModelAfter') and len({e.get('handle') for e in before})==213,'all213 model identities and public snapshots unchanged')
    check(isinstance(d.get('dbmodBefore'),int) and d['dbmodBefore']>=0
          and d['dbmodBefore']==d.get('dbmodAfter'),'DBMOD unchanged')
    check(d.get('cmdaBefore')==d.get('cmdaAfter') and isinstance(d.get('cmdaBefore'),int)
          and d['cmdaBefore']>=0 and d.get('cmdnamesBefore')==d.get('cmdnamesAfter'),'CMDACTIVE and CMDNAMES balanced against own command baseline')
    for key in ('callbackRetained','dwgSaved','fieldsChanged','radioChanged','objectsSelectedByProbe','comUsed','computerUseUsed'):
        check(d.get(key) is False,'no '+key)
    check(d.get('commandAttempted') is True and d.get('commandName')=='TChangeValElev'
          and d.get('commandStatus') in (5100,-5002),'actual TChangeValElev returned normally or designed cancellation')
    trace=d.get('trace') or {}
    check(trace.get('editorAdded') is True and trace.get('editorRemoved') is True
          and trace.get('inputAddStatus')==0 and trace.get('inputRemoveStatus') in (0,33)
          and trace.get('retained') is False and trace.get('traceDropped') is False,'actual trace observers removed without loss')
    events=d.get('commandTrace') or []
    target=[e for e in events if str(e.get('command','')).lstrip('_.').upper()=='TCHANGEVALELEV']
    check(len(target)==2 and target[0].get('event')=='commandWillStart'
          and target[1].get('event') in ('commandEnded','commandCancelled')
          and not any(e.get('event') in ('commandFailed','unknownCommand') for e in events),'observed target command start and end/designed cancel')
    inputs=d.get('inputTrace') or []
    check(all(not e.get('selectedHandle') and not e.get('selectedHandles') for e in inputs),'actual input trace contains no selected objects')
    discovery=d.get('discovery') or {};snapshot=discovery.get('snapshot') or {};controls=snapshot.get('controls') or []
    check(discovery.get('snapshotCaptured') is True if v2 else discovery.get('recognized') is True,
          'actual parameter panel snapshot captured')
    check(discovery.get('error') is None and discovery.get('timedOut') is False
          and discovery.get('timerStopReturned') is True and discovery.get('timerStopped') is True,'bounded discovery timer stopped before state destruction')
    source=discovery.get('captureSource');prompt_sources=('beginEntsel','beginSSGet','beginGetPoint')
    check(source in (prompt_sources+('UI-thread timer read-only snapshot','read-only sweep and fresh known-panel close after zero-selection returned') if v2 else ('UI-thread timer during command/modal message pump','read-only sweep after zero-selection command returned')),'actual public prompt/modal/modeless capture path')
    auto=v2 and discovery.get('autoClosed') is True
    if auto:
        check(discovery.get('promptCaptured') is True and discovery.get('cancelAttempted') is False
              and discovery.get('cancelDelivered') is False and discovery.get('cancelMethod') is None
              and discovery.get('closeObserved') is True,'zero-selection auto-close is observed without claiming delivered cancellation')
        result['closureMode']='ZERO_SELECTION_AUTO_CLOSE_OBSERVED'
    else:
        check(discovery.get('recognized') is True and discovery.get('cancelAttempted') is True
              and discovery.get('cancelDelivered') is True and discovery.get('cancelWin32Error')==0
              and discovery.get('closeObserved') is True,'only fresh known-panel cancellation delivered and closed/hidden')
        result['closureMode']='EXACT_KNOWN_PANEL_CANCELLATION'
    if v2:
        check(discovery.get('promptObserverAdded') is True and discovery.get('promptObserverRemoved') is True
              and discovery.get('promptObserverRetained') is False and discovery.get('promptAddStatus')==0
              and discovery.get('promptRemoveStatus') in (0,33),'public input-context reactor registered and safely removed')
        captures=discovery.get('promptCaptureEvents') or []
        check(len(captures)<=8 and (not auto or bool(captures)) and all(c.get('event') in prompt_sources
              and c.get('controlsWritten') is False and c.get('closeAttempted') is False
              and c.get('processId')==discovery.get('processId') and c.get('threadId')==discovery.get('threadId')
              and c.get('desktop')==discovery.get('desktop') and isinstance(c.get('windowSnapshots'),list)
              and len(c['windowSnapshots'])<=16 and any(i.get('event')==c.get('event') for i in inputs)
              for c in captures),'prompt callbacks actually observed selection input and only read bounded window snapshots')
        check(snapshot.get('semanticRolesComplete') is True and snapshot.get('isMainFrame') is False,
              'actual semantic role combination excludes AutoCAD main frame')
        if auto:check(any(c.get('snapshotCaptured') is True and any(w.get('hwnd')==snapshot.get('hwnd')
                  and w.get('semanticRolesComplete') is True for w in c.get('windowSnapshots',[])) for c in captures),
                  'auto-closed custom panel was really captured before the selection prompt consumed Enter')
    check((auto or snapshot.get('recognized') is True and snapshot.get('className')=='#32770')
          and snapshot.get('enumerationComplete') is True and snapshot.get('processId')==discovery.get('processId')
          and isinstance(snapshot.get('processId'),int) and snapshot['processId']>0
          and isinstance(snapshot.get('threadId'),int) and snapshot['threadId']>0
          and snapshot.get('desktop')==discovery.get('desktop')
          and re.fullmatch(r'CodexS1_[0-9a-f]{32}',str(snapshot.get('desktop',''))) is not None,'own-process private panel identity; custom class is read-only and must auto-close')
    by_hwnd={c.get('hwnd'):c for c in controls}
    check(bool(controls) and len(controls)<=128 and len(by_hwnd)==len(controls)
          and all(isinstance(c.get('hwnd'),int) and c['hwnd']>0
                  and c.get('processId')==snapshot.get('processId')
                  and isinstance(c.get('controlId'),int) and isinstance(c.get('className'),str)
                  and isinstance(c.get('text'),str) and isinstance(c.get('style'),int)
                  and isinstance(c.get('exStyle'),int) and isinstance(c.get('rect'),list) and len(c['rect'])==4
                  for c in controls),'all actual child HWNDs/classes/IDs/raw text/styles/rectangles recorded')
    semantic=str(snapshot.get('title',''))+'\n'+'\n'.join(c.get('text','') for c in controls if c.get('visible') is True)
    check('标高' in semantic and '改值' in semantic and '变化值' in semantic,'semantic identity comes from actual visible window/control text')
    roles=snapshot.get('roles') or {};edit=by_hwnd.get(roles.get('changeEdit'));inc=by_hwnd.get(roles.get('increaseRadio'));dec=by_hwnd.get(roles.get('decreaseRadio'))
    visible=lambda c:c and c.get('visible') is True and c.get('enabled') is True
    edits=[c for c in controls if c.get('className','').lower()=='edit' and visible(c)]
    check(len(edits)==1 and edit==edits[0] and edit.get('rectRead') is True,'unique actual enabled visible Edit; control ID is observed, not assumed')
    def radio(c,caption):
        return visible(c) and c.get('className','').lower()=='button' and c.get('style',-1)&15 in (4,9)
    check(radio(inc,'增加') and radio(dec,'减少') and inc.get('hwnd')!=dec.get('hwnd')
          and '增加' in inc.get('text','') and '减少' in dec.get('text','')
          and inc.get('checkRead') is True and dec.get('checkRead') is True
          and sorted((inc.get('checkState'),dec.get('checkState')))==[0,1],'actual visible increase/decrease radio styles and original BM_GETCHECK states')
    def push(c):return visible(c) and c.get('className','').lower()=='button' and c.get('style',-1)&15 in (0,1)
    ok=by_hwnd.get(roles.get('okButton'));cancel=by_hwnd.get(roles.get('cancelButton'))
    check(not roles.get('okButton') or push(ok) and ('确定' in ok.get('text','') or 'ok' in ok.get('text','').lower()),'OK is optional and never inferred from IDOK')
    check(not roles.get('cancelButton') or push(cancel) and ('取消' in cancel.get('text','') or 'cancel' in cancel.get('text','').lower()),'Cancel is optional and never inferred from IDCANCEL')
    expected_method='WM_COMMAND actual observed unique Cancel button' if roles.get('cancelButton') else 'WM_CLOSE exact semantically identified own parameter panel; no OK/Cancel assumed'
    check(discovery.get('cancelMethod') is None if auto else discovery.get('cancelMethod')==expected_method,'closure method agrees with actual controls or zero-selection auto-close')
    result['actualRoles']={name:by_hwnd.get(hwnd) for name,hwnd in roles.items()}
    result['dialogIdentity']={k:snapshot.get(k) for k in ('hwnd','processId','threadId','desktop','title','className')}
    result['failed']=len(result['failures']);result['passed']=result['assertions']-result['failed'];result['result']='FAIL' if result['failures'] else 'PASS'
    result['scope']='Read-only public-prompt/window discovery and actual original control state, followed by observed zero-selection auto-close or fresh known-panel cancellation. No parameter/elevation editing, default-state restoration or business round-trip is established.'
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('report',type=Path);args=parser.parse_args()
    try:
        d=json.loads(args.report.read_text('utf-8-sig'));result=validate(d)
        control=Path(d.get('controlDWG',''));good=control.is_file() and control.resolve()==args.report.resolve().parent/'control.dwg'
        result['assertions']+=1
        if not good:result['failures'].append('actual owned run/control.dwg missing or escaped')
        else:result['controlSHA256']=hashlib.sha256(control.read_bytes()).hexdigest().upper()
        result['failed']=len(result['failures']);result['passed']=result['assertions']-result['failed'];result['result']='FAIL' if result['failures'] else 'PASS'
        print(json.dumps(result,ensure_ascii=False,indent=2));return int(result['result']!='PASS')
    except (OSError,ValueError,KeyError,TypeError,AttributeError) as error:print('FAIL: incomplete elevation discovery evidence:',error);return 1


if __name__=='__main__':raise SystemExit(main())
