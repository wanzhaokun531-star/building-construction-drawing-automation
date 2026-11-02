"""Independent metre-value/UI-default/ownership checks; GUI strings are not values."""
import json
import math
import re
import sys
from collections import Counter
from decimal import Decimal,InvalidOperation
from pathlib import Path
from verify_four_axis_seed_native import near
from verify_elevation_roundtrip_native import inspect as inspect_core,numeric_candidate,check_empty_public

DELTA=Decimal('0.123')


def finite_point(p):
    return isinstance(p,list) and len(p)==3 and all(type(v) in (int,float) and math.isfinite(v) for v in p)


def check_public_readback(r,handle,check,label):
    check(r.get('contract')=='stage1.elevation-public-visible-readback.v2' and r.get('completed') is True and r.get('error') is None,label+': complete public readback contract')
    before=r.get('before') or {};after=r.get('after') or {}
    check(r.get('rootHandle')==handle==before.get('handle') and before.get('type')=='TDbSymbElevation' and before.get('proxy') is False and near(before,after),label+': same real TCH elevation root unchanged by public const explode')
    nodes=r.get('nodes') or [];texts=r.get('texts') or [];calls=r.get('explodeCalls') or [];index={n.get('nodeId'):n for n in nodes}
    check(bool(nodes) and [n.get('nodeId') for n in nodes]==list(range(1,len(nodes)+1)),label+': complete unique transient identities')
    check(r.get('rootExplodeStatus')==0 and all(r.get(k) is True for k in ('allLeavesRead','allPublicLeavesAccountedFor','ownershipSafe'))
          and all(r.get(k) is False for k in ('numericValueInferred','privateContentInferred')) and 'no full private field equivalence' in r.get('readbackScope',''),label+': actual public leaves, no guessed private value')
    check(r.get('returnedTransientCount')==r.get('deletedTransientCount')==len(nodes) and all(r.get(k)==0 for k in ('unresolvedOwnershipCount','rejectedBorrowedCount','duplicatePointerCount')),label+': every returned transient safely released once')
    for n in nodes:
        parent=n.get('parentNodeId');depth=n.get('depth')
        check(n.get('kind') in ('text','mtext','geometry','compound','empty-public-representation') and all(n.get(k) is True for k in ('transient','objectIdNull','ownerIdNull','databaseNull'))
              and type(depth) is int and 1<=depth<=8 and ((parent==0 and depth==1) or parent in index and index[parent].get('kind')=='compound' and depth==index[parent].get('depth',-9)+1),label+': actual generic transient tree node '+str(n.get('nodeId')))
    expected=[n for n in nodes if n.get('kind') in ('text','mtext')]
    check(bool(texts) and near(texts,expected) and all(isinstance(t.get('plainText'),str) and isinstance(t.get('rawText'),str) and finite_point(t.get('position')) and type(t.get('height')) in (int,float) and math.isfinite(t['height']) and t['height']>0 for t in texts),label+': actual finite Text/MText payloads')
    cm={c.get('nodeId'):c for c in calls};empty=[];keys=('type','rawExplodeStatus','rawReturnedCount','visibility','bounds','proxy','privateContentInferred')
    for n in nodes:
        if n.get('kind')=='empty-public-representation':
            check_empty_public(n,cm.get(n.get('nodeId')) or {},check,label+' empty'+str(n.get('nodeId')))
            empty.append({k:n.get(k) for k in keys})
    check(near(r.get('emptyPublicRepresentations'),empty),label+': all explicitly empty public structures retained')
    check(bool(calls) and calls[0].get('nodeId')==0 and all(c.get('status')==0 and (c.get('returnedCount',0)>0 or index.get(c.get('nodeId'),{}).get('kind')=='empty-public-representation') for c in calls),label+': actual successful explode calls, only proved empties admitted')
    return texts


def check_actual_value(r,metadata,handle,expected,check,label,exact_text=None):
    texts=check_public_readback(r,handle,check,label)
    candidates=[numeric_candidate(t.get('plainText')) for t in texts];candidates=[c for c in candidates if c is not None]
    if not check(len(texts)==len(candidates)==1 and candidates[0]['decimalPlaces']==3,label+': exactly one actual three-decimal visible metre value'):return None
    c=candidates[0];value=Decimal(c['decimalValue'])
    check(value==expected,label+': actual public Decimal metre value equals '+str(expected))
    if exact_text is not None:check(c['actualText']==exact_text,label+': actual original text and zero-sign format restored exactly')
    check(metadata.get('valid') is True and metadata.get('actualText')==c['actualText'] and metadata.get('unit')==metadata.get('comparisonUnit')=='m'
          and metadata.get('valueSource')=='returned public Text/MText only' and metadata.get('displayPrecision')==3,label+': native value metadata bound to actual public text in metres')
    try:match=Decimal(metadata.get('metresDecimal'))==value
    except (InvalidOperation,TypeError,ValueError):match=False
    check(match,label+': metre metadata independently agrees with actual text')
    if 'derivedMillimetres' in metadata:check(type(metadata['derivedMillimetres']) is int and Decimal(metadata['derivedMillimetres'])==value*1000,label+': optional123mm is only a derived explanation')
    return c['actualText']


def default_valid(d):
    try:value=Decimal(d.get('rawValue'))
    except (InvalidOperation,TypeError,ValueError):return False
    return d.get('valid') is True and value.is_finite() and type(d.get('increaseChecked')) is bool and type(d.get('decreaseChecked')) is bool and d['increaseChecked']!=d['decreaseChecked']


def desired(d,raw,increase):
    return default_valid(d) and d['rawValue']==raw and d['increaseChecked'] is increase and d['decreaseChecked'] is not increase


def check_controls(s,defaults,pid,thread,desktop,check,label):
    check(s.get('recognized') is True and s.get('semanticRolesComplete') is True and s.get('enumerationComplete') is True and s.get('isMainFrame') is False
          and s.get('className')=='#32770' and s.get('processId')==pid and s.get('threadId')==thread and s.get('desktop')==desktop,label+': fresh actual owned private standard panel')
    controls=s.get('controls') or [];ids=[c.get('hwnd') for c in controls];ci={c.get('hwnd'):c for c in controls};roles=s.get('roles') or {};panel=s.get('hwnd')
    check(len(ci)==len(ids) and all(type(h) is int and h>0 for h in ids),label+': complete unique real control HWNDs')
    selected=[roles.get(k) for k in ('changeEdit','increaseRadio','decreaseRadio')]
    check(len(set(selected))==3 and all(h in ci for h in selected),label+': unique observed semantic controls, no hardcoded ID')
    expected={}
    for key in ('changeEdit','increaseRadio','decreaseRadio'):
        c=ci.get(roles.get(key)) or {};expected[key]=c
        check(c.get('processId')==pid and c.get('threadId')==thread and c.get('visible') is True and c.get('enabled') is True
              and type(c.get('controlId')) is int and 0<c['controlId']<=65535,label+': actual enabled role identity '+key)
        if key=='changeEdit':check(c.get('className')=='Edit' and c.get('text')==defaults.get('rawValue'),label+': actual Edit raw value')
        else:
            value=defaults.get('increaseChecked') if key=='increaseRadio' else defaults.get('decreaseChecked')
            check(c.get('className')=='Button' and type(c.get('style')) is int and c['style']&15 in (4,9) and c.get('checkRead') is True
                  and type(value) is bool and type(c.get('checkState')) is int and c['checkState']==int(value) and ('增加' if key=='increaseRadio' else '减少') in c.get('text',''),label+': actual mutually exclusive standard radio '+key)
    check(any(re.fullmatch(r'\(M\)',str(c.get('text','')).strip().replace('（','(').replace('）',')'),re.I) for c in controls if c.get('visible') is True),label+': actual visible panel unit(M), not model-mm input')
    check(default_valid(defaults),label+': actual numeric raw value and one selected radio')
    return panel,expected


def selected_handles(events):
    result=[]
    for e in events:
        for h in (([e['selectedHandle']] if e.get('selectedHandle') is not None else [])+(e.get('selectedHandles') or [])):
            if not result or result[-1]!=h:result.append(h)
    return result


def check_selection_input(s,handle,check,label):
    check(s.get('protocol')=='RTPICKS exact single target plus one empty Enter; no point token',label+': explicit v3 RTPICKS protocol')
    for key in ('createStatus','addStatus','lengthStatus','nameStatus','releaseStatus'):
        check(s.get(key)==5100,label+': actual selection set '+key)
    check(type(s.get('length')) is int and s['length']==1 and s.get('objectIdStatus')==0 and s.get('sameTarget') is True
          and s.get('actualHandle')==handle and s.get('released') is True,label+': exact one actual target and released native selection ownership')


def check_public_ink_pick(p,geometry,pick,check,label):
    check(p.get('contract')=='stage1.elevation-public-ink-pick.v1' and p.get('completed') is True and p.get('error') is None,label+': completed public line-ink proof')
    check(p.get('rootHandle')==geometry.get('handle') and geometry.get('type')=='TDbSymbElevation' and geometry.get('proxy') is False
          and near(p.get('before'),geometry) and near(p.get('before'),p.get('after')),label+': read-only proof bound to the actual selected elevation geometry')
    check(all(p.get(k) is True for k in ('ownerIsModel','rootVisible','modelMembershipUnchanged','ownershipSafe'))
          and all(p.get(k) is False for k in ('rootLayerOff','rootLayerFrozen','rootLayerLocked','persistentAppendAttempted','persistentEraseAttempted','bboxUsedForPick','privateFieldsAccessed'))
          and p.get('modelCountBefore')==p.get('modelCountAfter')==214,label+': visible editable model root and complete214 membership unchanged')
    check(type(p.get('dbmodBefore')) is int and p['dbmodBefore']>=0 and p.get('dbmodBefore')==p.get('dbmodAfter'),label+': native database modification state unchanged')
    parts=p.get('returnedParts') or [];lines=p.get('lineCandidates') or [];count=p.get('rawReturnedCount')
    check(p.get('rootExplodeStatus')==0 and type(count) is int and 0<count<=4096 and count==len(parts)==p.get('returnedTransientCount')==p.get('deletedTransientCount')
          and all(p.get(k)==0 for k in ('unresolvedOwnershipCount','rejectedBorrowedCount','duplicatePointerCount')),label+': all actual transient returns released exactly once')
    check(type(count) is int and [v.get('returnIndex') for v in parts]==list(range(count)),label+': every direct public return has its actual unique ordinal')
    check(all(isinstance(v.get('type'),str) and bool(v['type']) and all(v.get(k) is True for k in ('transient','objectIdNull','ownerIdNull','databaseNull')) for v in parts),label+': actual nonresident public return identities')
    lineparts=[v for v in parts if v.get('type')=='AcDbLine']
    check(bool(lines) and [v.get('returnIndex') for v in lines]==[v.get('returnIndex') for v in lineparts],label+': all actual AcDbLine returns retained in order')
    check(p.get('epsilon')==1e-6 and p.get('selectionRule')=='longest eligible actual visible AcDbLine; strict interior midpoint; no bbox or grip substitution',label+': explicit finite interior-line selection contract')
    candidates=[]
    for line in lines:
        valid=finite_point(line.get('startWcs')) and finite_point(line.get('endWcs'))
        check(valid,label+': finite actual returned line endpoints')
        check(line.get('type')=='AcDbLine' and line.get('proxy') is False and all(line.get(k) is True for k in ('transient','objectIdNull','ownerIdNull','databaseNull'))
              and line.get('returnIndex') in [v.get('returnIndex') for v in lineparts],label+': candidate belongs to an owned actual public AcDbLine')
        if not valid:continue
        a,b=line['startWcs'],line['endWcs'];length=math.dist(a,b);mid=[(x+y)/2 for x,y in zip(a,b)]
        eligible=line.get('visibility')==0 and length>2e-6 and math.dist(mid,a)>1e-6 and math.dist(mid,b)>1e-6
        check(near(line.get('length'),length) and near(line.get('midpointWcs'),mid) and near(line.get('distanceFromStart'),math.dist(mid,a))
              and near(line.get('distanceFromEnd'),math.dist(mid,b)) and line.get('eligible') is eligible,label+': midpoint, distances and eligibility independently recomputed from actual endpoints')
        if eligible:candidates.append((length,line,mid))
    selected=max(candidates,key=lambda item:item[0]) if candidates else None
    check(selected is not None,label+': at least one actual visible nondegenerate interior line')
    if selected:
        _,line,mid=selected
        check(p.get('selectedReturnIndex')==line.get('returnIndex') and near(p.get('selectedLine'),line) and near(p.get('pickWcs'),mid)
              and near(pick,mid),label+': actual pick is the independently selected longest line interior midpoint')


def check_ink_selection_input(a,geometry,check,label):
    s=a.get('inkSelectionInput') or {};pick=a.get('actualInkPickWcs')
    check('selectionInput' not in a and s.get('protocol')=='public visible AcDbLine interior midpoint RT3DPOINT plus one empty Enter'
          and s.get('pointSource')=='returned transient public explode line endpoints; not public grip',label+': explicit v4 visible-line point protocol, no RTPICKS substitution')
    check(all(s.get(k) is True for k in ('attempted','completed','ownershipSafe','actualInkPointObserved')) and finite_point(pick)
          and near(s.get('actualInkPickWcs'),pick) and near(s.get('actualPickUcs'),a.get('pickUcs'))
          and near(s.get('inkPickProof'),a.get('inkPickProof')),label+': actual point and public ink proof bound across input facts')
    check_public_ink_pick(a.get('inkPickProof') or {},geometry,pick,check,label+'/public-ink')
    events=[e for e in a.get('promptEvents') or [] if e.get('event')=='endGetPoint']
    normal=[e for e in events if e.get('returnStatus')==5100]
    check(type(s.get('normalGetPointCount')) is int and s['normalGetPointCount']==len(normal) and bool(normal),label+': normal point count independently derived from raw endGetPoint events')
    matches=[]
    for e in events:
        normal_event=e.get('returnStatus')==5100
        match=normal_event and finite_point(e.get('pointOut')) and finite_point(pick) and math.dist(e['pointOut'],pick)<1e-6
        check(e.get('pointOutCoordinateSystem')=='WCS per ObjectARX2020 public callback contract' and near(e.get('actualInkPickWcs'),pick)
              and e.get('pointOutMatchesActualInkPickWcs') is match and (finite_point(e.get('pointOut')) if normal_event else e.get('pointOut') is None),label+': actual callback WCS output independently compared with submitted ink midpoint')
        matches.append(match)
    check(any(matches) and s.get('actualInkPointObserved') is any(matches),label+': a real normal WCS point output proves the ink input was consumed')


def check_ui_attempt(a,check,label):
    check(a.get('contract')=='stage1.elevation-ui-attempt.v1' and a.get('error') is None and a.get('completed') is True,label+': actual UI attempt contract')
    version=a.get('implementationVersion')
    check(type(version) is int and version in (3,4) and a.get('syntheticEditNotifications') is False and a.get('progressFailed') is False
          and a.get('defaultsReceiptPersisted') is True,label+': explicit v3/v4 implementation with durable defaults and no fabricated edit notifications')
    mode=a.get('mode');check(mode in ('SetupOnly','Apply','ObserveOnly'),label+': explicit known UI mode')
    pid,thread,desktop=a.get('processId'),a.get('threadId'),a.get('desktop')
    check(type(pid) is int and pid>0 and type(thread) is int and thread>0 and isinstance(desktop,str) and desktop.startswith('CodexS1_')
          and isinstance(a.get('inputDesktop'),str) and bool(a['inputDesktop']) and a['inputDesktop']!=desktop,label+': actual private desktop differs from input desktop')
    check(a.get('contextGuardPassed') is True and a.get('callbackCommandIssued') is False and a.get('timerUsed') is False,label+': actual guarded context and no nested callback command')
    before,after=a.get('originalDefaults') or {},a.get('afterDefaults') or {}
    check(near(a.get('beforeDefaults'),before),label+': duplicate defaults facts agree with actual initial panel')
    p,roles=check_controls(a.get('beforeControls') or {},before,pid,thread,desktop,check,label+'/before')
    q,after_roles=check_controls(a.get('afterControls') or {},after,pid,thread,desktop,check,label+'/after')
    check(p==q,label+': same actual panel across field configuration')
    visible_units=[c.get('text') for c in (a.get('beforeControls') or {}).get('controls',[]) if c.get('visible') is True and re.fullmatch(r'\(M\)',str(c.get('text','')).strip().replace('（','(').replace('）',')'),re.I)]
    check(bool(visible_units) and a.get('unitLabels')==visible_units,label+': unit audit consists only of actually visible panel(M) labels')
    if mode in ('ObserveOnly','Apply'):check(a.get('configurationAttempted') is False and near(before,after),label+': read-only observation/apply did not rewrite defaults')
    else:check(a.get('configurationAttempted') is True and desired(after,a.get('requestedRawValue'),a.get('requestedIncrease')),label+': actual configured radio/raw value readback')
    check(a.get('configurationVerified') is True,label+': control configuration verified separately from business value')
    for m in a.get('messages') or []:
        msg=m.get('message');target=m.get('target')
        readonly=msg in (0x000D,0x00F0) # WM_GETTEXT and BM_GETCHECK.
        check(m.get('returned') is True and m.get('processId')==pid and m.get('threadId')==thread
              and type(m.get('panel')) is int and m['panel']>0 and type(target) is int and target>0
              and (readonly or m.get('panel')==p),label+': standard message belongs to this own process/thread; mutations bind the actual panel')
        check(msg in (0x00F5,0x000C,0x0010,0x000D,0x00F0),label+': known standard read/write messages, no synthetic WM_COMMAND')
        if readonly:
            check((msg==0x000D and m.get('wParam')==2048) or (msg==0x00F0 and m.get('wParam')==0 and type(m.get('reply')) is int and m['reply'] in (0,1)),label+': actual read-only text/radio message')
            if m.get('panel')==p:
                members={c.get('hwnd') for c in (a.get('beforeControls') or {}).get('controls',[])}|{c.get('hwnd') for c in (a.get('afterControls') or {}).get('controls',[])}|{p}
                check(target in members,label+': recognized-panel read targets an actually captured control')
                if msg==0x00F0:check(target in (roles['increaseRadio'].get('hwnd'),roles['decreaseRadio'].get('hwnd')),label+': radio read is bound to observed standard Button')
        if msg==0x000C:check(mode=='SetupOnly' and target==roles['changeEdit'].get('hwnd') and m.get('reply') not in (None,0),label+': actual setup-only Edit WM_SETTEXT acknowledged')
        elif msg==0x00F5:check(mode=='SetupOnly' and target in (roles['increaseRadio'].get('hwnd'),roles['decreaseRadio'].get('hwnd')),label+': setup-only BM_CLICK uses actual discovered radio')
        elif msg==0x0010:check(target==p,label+': cleanup closes only actual parameter panel')
    if mode=='SetupOnly':check(any(m.get('message')==0x000C for m in a.get('messages') or []),label+': field write actually attempted through standard message')
    check(a.get('observerAdded') is True and a.get('observerRemoved') is True and a.get('observerRetained') is False and a.get('observerAddStatus')==0 and a.get('observerRemoveStatus') in (0,33),label+': UI callback observer detached before state release')
    t=a.get('trace') or {};check(t.get('editorAdded') is True and t.get('editorRemoved') is True and t.get('inputAddStatus')==0 and t.get('inputRemoveStatus') in (0,33)
                               and t.get('retained') is False and t.get('traceDropped') is False,label+': command/input trace lifecycle detached')
    events=a.get('commandTrace') or [];calls=[e for e in events if str(e.get('command','')).lstrip('_.').upper()=='TCHANGEVALELEV']
    check(len(calls)==2 and calls[0].get('event')=='commandWillStart' and (calls[1].get('event')=='commandEnded' if mode=='Apply' else calls[1].get('event') in ('commandEnded','commandCancelled'))
          and not any(e.get('event') in ('commandFailed','unknownCommand') for e in events),label+': actual balanced public command lifecycle')
    check(a.get('commandName')=='TChangeValElev' and a.get('commandStatus') in (5100,-5002) and a.get('activeTransactionsBefore')==a.get('activeTransactionsAfter')==0
          and a.get('panelClosed') is True and a.get('safeToContinue') is True and a.get('lifecycleSafe') is True,label+': safe ended native command/panel and zero transactions')
    check(a.get('modelProtectionVerified') is True and near(a.get('protectedBefore'),a.get('protectedAfter')),label+': all non-target public objects unchanged')
    check(a.get('initialModelCount')==a.get('finalModelCount')==214 and a.get('modelMembershipUnchanged') is True
          and a.get('protectedCount')==len(a.get('protectedBefore') or [])==(213 if mode=='Apply' else 214),label+': complete actual214 model membership and protected cohort')
    check(type(a.get('cmdaBefore')) is int and a.get('cmdaBefore')==a.get('cmdaAfter') and isinstance(a.get('cmdnamesBefore'),str) and a.get('cmdnamesBefore')==a.get('cmdnamesAfter'),label+': actual command context restored')
    prompts=a.get('promptEvents') or [];summaries=[e for e in prompts if e.get('configurationVerified') is True]
    allowed=('beginSSGet','beginGetPoint') if mode=='Apply' else ('beginGetPoint',)
    check(len(summaries)==1 and summaries[0].get('event') in allowed and summaries[0].get('callbackCommandIssued') is False
          and any(e.get('event')==summaries[0].get('event') and 'prompt' in e and type(e.get('flags')) is int for e in prompts),label+': one verified configuration/read at an actually recorded prompt; full event stream retained')
    check(all(e.get('configurationVerified') is not False or (mode=='Apply' and e.get('event')=='beginSSGet' and e.get('captureDeferred') is True) for e in prompts),label+': only explicit not-ready SSGet defer may precede successful capture')
    selected=selected_handles(a.get('inputTrace') or [])
    check(a.get('selectionVerified') is True and a.get('actualSelectedHandles')==selected,label+': selection audit equals actual input reactor handles')
    if mode=='Apply':
        h=a.get('expectedPickedHandle');g=a.get('targetBefore') or {};afterg=a.get('targetAfter') or {}
        check(isinstance(h,str) and bool(h) and selected==[h] and g.get('handle')==afterg.get('handle')==h
              and g.get('type')==afterg.get('type')=='TDbSymbElevation' and g.get('proxy') is False and afterg.get('proxy') is False,label+': actual selection only the native elevation target')
        pick=a.get('pickWcs');gp=(g.get('grip') or {}).get('points') or []
        check(a.get('preconfiguredAtPrompt') is True and desired(before,a.get('requestedRawValue'),a.get('requestedIncrease'))
              and a.get('coordinateTransformStatus')==5100 and a.get('geometryAnchorMatchesActualPublicGrip' if version==4 else 'pickMatchesActualPublicGrip') is True and a.get('targetClass')=='TDbSymbElevation'
              and finite_point(pick) and any(near(pick,v) for v in gp) and finite_point(a.get('pickUcs')),label+': persistent defaults and actual geometric reference; not a point selection token')
        if version==4:
            check(near(a.get('geometryAnchorWcs'),pick) and a.get('pickWcsRole')=='geometry anchor only; actual RT3DPOINT is actualInkPickWcs transformed to pickUcs'
                  and a.get('coordinateTransformSource')=='actualInkPickWcs',label+': public grip is only the geometry anchor, actual point source is the public ink midpoint')
            check_ink_selection_input(a,g,check,label+'/ink-selection-input')
        else:check_selection_input(a.get('selectionInput') or {},h,check,label+'/selection-input')
    else:check(not selected,label+': empty-selection mode did not edit any target')


def check_defaults_receipt(a,request_id,check,label):
    """An immutable pre-setter capture is evidence of the baseline, never restore."""
    control=Path(a.get('controlDWG',''));path=Path(a.get('defaultsReceipt',''));serial=a.get('attemptSerial')
    ready=type(serial) is int and serial>0 and path.is_file() and path.is_absolute() and path.resolve()==control.resolve().parent/('elevation-ui-defaults-attempt-'+str(serial)+'.json')
    check(ready,label+': real immutable receipt in this owned run')
    if not ready:return
    r=json.loads(path.read_text('utf-8-sig'))
    check(r.get('contract')=='stage1.elevation-ui-defaults-before-setter.v2' and r.get('capturedBeforeAnySetter') is True
          and r.get('defaultsRestorationProven') is False,label+': capture precedes setters but does not falsely prove restoration')
    check(r.get('attemptSerial')==serial and r.get('processId')==a.get('processId') and r.get('threadId')==a.get('threadId')
          and r.get('desktop')==a.get('desktop') and r.get('id')==request_id and r.get('mode')==a.get('mode')
          and Path(r.get('run','')).resolve()==control.resolve().parent,label+': receipt binds this exact attempt/context')
    check(near(r.get('originalDefaults'),a.get('originalDefaults')) and near(r.get('beforeControls'),a.get('beforeControls')),label+': actual first defaults/control snapshot persisted unchanged')


def check_elevation_ui_bridge_operation(r,check,label):
    check(r.get('contract')=='stage1.elevation-ui-bridge-operation.v1' and r.get('error') is None and r.get('completed') is True and r.get('safeToContinue') is True,label+': bridge wrapper completed safely')
    check(r.get('mode') in ('SetupOnly','Apply','ObserveOnly') and r.get('commandDispatched') is True,label+': explicit dispatched bridge mode')
    a=r.get('attempt') or {};check_ui_attempt(a,check,label+'/attempt');check_defaults_receipt(a,r.get('id'),check,label+'/attempt receipt')
    check(a.get('mode')==r.get('mode') and a.get('requestedRawValue')==r.get('requestedRawValue') and a.get('requestedIncrease') is r.get('requestedIncrease'),label+': attempt bound to requested wrapper')
    check(near(r.get('originalDefaults'),a.get('originalDefaults')),label+': wrapper originals are actual first panel defaults')
    if r.get('mode')=='Apply':
        pre=r.get('preflight') or {};check_ui_attempt(pre,check,label+'/preflight');check_defaults_receipt(pre,r.get('id'),check,label+'/preflight receipt')
        check(pre.get('mode')=='ObserveOnly' and desired(pre.get('originalDefaults') or {},r.get('requestedRawValue'),r.get('requestedIncrease'))
              and desired(pre.get('afterDefaults') or {},r.get('requestedRawValue'),r.get('requestedIncrease')),label+': independent empty reopen confirms desired persistent defaults before point tokens')
        check(r.get('selectionTokensMayHaveBeenConsumed') is True and r.get('expectedPickedHandle')==r.get('targetHandle')==a.get('expectedPickedHandle'),label+': actual selection-input attempt is not falsely claimed withheld')
    else:check(r.get('preflight') is None and r.get('selectionTokensMayHaveBeenConsumed') is False,label+': setup/observe do not feed point tokens')


def protected_signature(records,originals,check,label):
    check(len(records)==6 and {v.get('handle') for v in records}==set(originals),label+': complete6 protected original elevations')
    result={}
    for entry in records:
        h=entry.get('handle');r=entry.get('readback') or {};texts=check_public_readback(r,h,check,label+'/'+str(h))
        check(entry.get('type')=='TDbSymbElevation' and entry.get('proxy') is False and near(r.get('before'),originals.get(h)),label+': actual original root geometry '+str(h))
        result[h]={'texts':texts,'empty':r.get('emptyPublicRepresentations')}
    return result


def inspect(path):
    out={'contract':'stage1.elevation-roundtrip-ui-check.v4','result':'FAIL','assertions':0,'failures':[],'businessRoundtrip':'UNVERIFIED',
         'sameQueueMessageTimeoutBound':'NOT_BOUNDED_BY_200MS; missing final native report remains unverified'}
    def check(ok,label):
        out['assertions']+=1
        if not ok:out['failures'].append(label)
        return bool(ok)
    try:
        if not path.is_file():check(False,'native UI elevation roundtrip evidence missing');return out
        d=json.loads(path.read_text('utf-8-sig'));run=path.resolve().parent
        check(d.get('contract')=='stage1.elevation-roundtrip-native.v1' and d.get('api')=='ObjectARX native + public Tianzheng command + isolated standard Windows controls; no COM/CUA','explicit isolated native/UI public route')
        check(d.get('error') is None and d.get('restoreError') is None and all(d.get(k) is True for k in ('forwardVerified','reverseVerified','defaultsCaptured','defaultsRestorationAttempted','defaultsRestored','defaultsObservedRestored','roundtripAttempted','roundtripCompleted','completed','safeToContinue')),'complete actual roundtrip and defaults lifecycle, no partial success')
        check(Decimal(str(d.get('plannedDeltaMetres')))==DELTA,'explicit metre test delta, not project parameter')
        corepath=Path(d.get('initialCoreReport',''));check(corepath.is_file() and corepath.resolve()==run/'elevation-roundtrip-initial-core-native.json','actual initial non-UI subreport belongs to this run')
        cr=inspect_core(corepath);check(cr.get('result')=='PASS' and cr.get('assertions',0)>1,'independent full initial Core checks: '+str(cr.get('failures',[])[:2]))
        core=json.loads(corepath.read_text('utf-8-sig'));old=d.get('originalBefore') or [];h=d.get('targetHandle')
        check(len(old)==213 and len({x.get('handle') for x in old})==213 and near(old,d.get('originalAfter')) and near(old,core.get('originalBefore')),'all213 public originals bound to frozen Core and unchanged')
        check(h==core.get('targetHandle') and h not in {x.get('handle') for x in old},'only Core actual pure-X native clone may be edited')
        check(Path(d.get('controlDWG','')).resolve()==run/'control.dwg'==Path(core.get('controlDWG','')).resolve() and (run/'control.dwg').is_file(),'same owned control path as initial Core')
        og={v['handle']:v for v in old if v.get('type')=='TDbSymbElevation'}
        original_sig=protected_signature(d.get('originalElevationBefore') or [],og,check,'original baseline')
        check(near(d.get('originalElevationBefore'),core.get('originalElevationBefore')),'six actual original value baselines bound to Core')
        states=d.get('stateObservations') or [];expected_stages=['baseline','after-increase-setup','increased','after-decrease-setup','restored','final']
        check([v.get('stage') for v in states]==expected_stages,'all actual readback stages present in predeclared order')
        bystage={v.get('stage'):v for v in states};base=bystage.get('baseline') or {};baseg=base.get('geometry') or {};baseanchor=base.get('anchor');exact=None
        for state in states:
            stage=state.get('stage');g=state.get('geometry') or {};anchor=state.get('anchor');r=state.get('actualReadback') or {}
            check(state.get('completed') is True and state.get('safeToContinue') is True and state.get('error') is None and state.get('modelCount')==214 and state.get('originalGeometryUnchanged') is True,stage+': complete214 original-protected state')
            check(g.get('handle')==h and g.get('type')=='TDbSymbElevation' and g.get('proxy') is False and near(g,r.get('before')),stage+': actual unchanged native root identity')
            gp=(g.get('grip') or {}).get('points') or [];sp=(g.get('stretch') or {}).get('points') or []
            check(finite_point(anchor) and bool(gp) and bool(sp) and near(anchor,gp[0]) and near(anchor,sp[0]) and state.get('anchorDefinition')=='first actual public grip cross-checked with first stretch point',stage+': insertion anchor independently equals public GP0/SP0')
            check(state.get('anchorXYUnchanged') is True and finite_point(baseanchor) and near(anchor[:2],baseanchor[:2]),stage+': XY placement unchanged, not Y-driven numeric substitution')
            wanted=DELTA if stage in ('increased','after-decrease-setup') else Decimal('0')
            text=check_actual_value(r,state.get('actualValue') or {},h,wanted,check,stage,exact if stage in ('after-increase-setup','restored','final') else None)
            if stage=='baseline':exact=text;check(near(g,(core.get('translation') or {}).get('target')),'initial target geometry bound to Core pure-X clone')
            if stage in ('after-increase-setup','restored','final'):check(near(g,baseg),stage+': original full public geometry/layer restored')
            check(near(protected_signature(state.get('protectedOriginalElevations') or [],og,check,stage+'/protected'),original_sig),stage+': original6 actual texts and empty structures preserved')
            check(near(r.get('emptyPublicRepresentations'),(base.get('actualReadback') or {}).get('emptyPublicRepresentations')),stage+': target empty public structures not guessed or changed')
        for key,stage,wanted in (('increasedSnapshot','increased',DELTA),('restoredSnapshot','restored',Decimal('0'))):
            s=d.get(key) or {};check(s.get('stage')==stage and s.get('completed') is True and s.get('ownershipSafe') is True and s.get('firstReaderDestroyed') is True and s.get('reopenedModelCount')==214 and all(s.get(k)==0 for k in ('saveStatus','reopenStatus','closeInputStatus')),key+': actual independent fresh214 persistence')
            output=Path(s.get('outputDWG',''));check(output.is_file() and output.resolve()==run/('elevation-'+stage+'.dwg'),key+': new isolated actual snapshot file')
            check(near(s.get('originalSaved'),old) and near(s.get('targetSaved'),(bystage.get(stage) or {}).get('geometry')),key+': complete original213 and target public geometry persist')
            for sub in ('liveReadback','savedReadback'):
                rr=s.get(sub) or {};check_actual_value(rr,(bystage.get(stage) or {}).get('actualValue') or {},h,wanted,check,key+'/'+sub,exact if stage=='restored' else None)
                check(near(rr.get('before'),s.get('targetSaved') if sub=='savedReadback' else (bystage.get(stage) or {}).get('geometry')),key+'/'+sub+': actual root geometry bound to this live/saved stage')
            for sub in ('originalElevationLive','originalElevationSaved'):check(near(protected_signature(s.get(sub) or [],og,check,key+'/'+sub),original_sig),key+'/'+sub+': original6 visible and empty structures persist')
        ops=d.get('bridgeOperations') or [];ids=['increase-setup','increase-apply','decrease-setup','decrease-apply','restore-defaults','verify-defaults'];check([v.get('id') for v in ops]==ids,'actual six operation wrappers in declared order')
        first=((ops[0].get('result') or {}).get('attempt') or {}).get('originalDefaults') if ops else None
        check(default_valid(d.get('originalDefaults') or {}) and near(d.get('originalDefaults'),first),'original defaults come from first actual unmodified panel')
        unsafe=False
        for i,v in enumerate(ops):
            r=v.get('result') or {};check_elevation_ui_bridge_operation(r,check,'bridge'+str(i))
            if unsafe:check(r.get('commandDispatched') is not True,'unsafe callback must forbid every later command')
            unsafe=unsafe or r.get('safeToContinue') is not True
            raw='0.123' if i<4 else (d.get('originalDefaults') or {}).get('rawValue');inc=i<2 if i<4 else (d.get('originalDefaults') or {}).get('increaseChecked');mode='Apply' if i in (1,3) else 'ObserveOnly' if i==5 else 'SetupOnly'
            check(r.get('id')==v.get('id') and r.get('mode')==mode and v.get('mode')=={'SetupOnly':'setup','Apply':'apply','ObserveOnly':'observe'}[mode] and r.get('requestedRawValue')==v.get('requestedRawValue')==raw and r.get('requestedIncrease') is inc and v.get('increase') is inc and v.get('targetHandle')==r.get('targetHandle')==h,'bridge'+str(i)+': actual request bound to correct delta/direction/clone')
            for at in (r.get('preflight'),r.get('attempt')):
                if isinstance(at,dict):
                    check(Path(at.get('controlDWG','')).resolve()==run/'control.dwg','bridge'+str(i)+': all command attempts bind this same owned control')
                    for key in ('protectedBefore','protectedAfter'):
                        pm={e.get('handle'):e for e in at.get(key) or []}
                        check(all(e.get('handle') in pm and near(pm[e['handle']],e) for e in old),'bridge'+str(i)+'/'+key+': every actual original213 equals frozen Core')
            if i in (1,3):
                at=r.get('attempt') or {};b=bystage.get('after-increase-setup' if i==1 else 'after-decrease-setup') or {};a=bystage.get('increased' if i==1 else 'restored') or {}
                check(near(at.get('targetBefore'),b.get('geometry')) and near(at.get('targetAfter'),a.get('geometry')),'bridge'+str(i)+': actual selected clone before/after bound to independent business readback stages')
        observed=(ops[-1].get('result') or {}).get('originalDefaults') if ops else None
        check(near(d.get('observedRestoredDefaults'),observed) and near(d.get('originalDefaults'),observed),'default restoration proved by independent ObserveOnly reopen, not merely UI setter/close')
        if unsafe:check(not d.get('increasedSnapshot') and not d.get('restoredSnapshot'),'unsafe callback must forbid subsequent save operations')
        check(near(d.get('initialSnapshot'),core.get('initialSnapshot')),'actual initial snapshot is the verified non-UI Core stage')
    except (OSError,ValueError,TypeError,KeyError,IndexError,AttributeError,InvalidOperation) as error:check(False,'invalid/incomplete roundtrip evidence: '+str(error))
    out['result']='FAIL' if out['failures'] else 'PASS';out['businessRoundtrip']='PASS_ACTUAL_PUBLIC_METRES_0_PLUS_0_123_ZERO_AND_DEFAULTS_RESTORED' if not out['failures'] else 'UNVERIFIED'
    return out


def main(path):
    r=inspect(path)
    for f in r['failures']:print('FAIL:',f)
    print(f"assertions={r['assertions']} passed={r['assertions']-len(r['failures'])} failed={len(r['failures'])} result={r['result']}")
    print('elevation_roundtrip='+r['businessRoundtrip']);return int(r['result']!='PASS')


if __name__=='__main__':raise SystemExit(main(Path(sys.argv[1])))
