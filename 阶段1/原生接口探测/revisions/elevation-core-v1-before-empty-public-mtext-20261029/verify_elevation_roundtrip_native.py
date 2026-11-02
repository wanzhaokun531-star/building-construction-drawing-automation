"""Non-UI elevation core only; never treats GUI input or return codes as values.

The business +0.123/-0.123 route will require its own discovered-control contract,
six protected elevation value baselines, actual value readbacks and defaults
restoration. This checker deliberately cannot mark that unperformed route PASS.
"""
import json
import re
import sys
from collections import Counter
from decimal import Decimal,InvalidOperation
from pathlib import Path
from verify_four_axis_seed_native import near,bag


def numeric_candidate(text):
    """Strict whole-text candidates, never extract an arbitrary embedded number."""
    if not isinstance(text,str):return None
    s=text.strip();sign='ordinary'
    if s.startswith('±'):sign='plus-minus-zero';s=s[1:]
    elif s[:3].lower()=='%%p':sign='autocad-plus-minus-zero';s=s[3:]
    s=s.replace('−','-')
    if not re.fullmatch(r'[+-]?\d+(?:\.\d+)?',s):return None
    try:value=Decimal(s)
    except InvalidOperation:return None
    if sign!='ordinary' and value!=0:return None
    return {'actualText':text,'decimalValue':str(value),'decimalPlaces':len(s.partition('.')[2]),'format':sign}


def inspect(path):
    result={'contract':'stage1.elevation-roundtrip-core-check.v1','result':'FAIL','assertions':0,'failures':[],'businessRoundtrip':'NOT_ATTEMPTED','numericReadiness':'UNCONFIRMED'}
    def check(ok,label):
        result['assertions']+=1
        if not ok:result['failures'].append(label)
        return bool(ok)
    def readback(r,label,expected_handle):
        check(r.get('contract')=='stage1.elevation-public-readback.v1' and r.get('completed') is True and r.get('error') is None,label+': generic public readback completed')
        check(r.get('rootHandle')==expected_handle and near(r.get('before'),r.get('after')) and (r.get('before') or {}).get('proxy') is False,label+': actual native root unchanged by const explode')
        nodes=r.get('nodes') or [];texts=r.get('texts') or [];calls=r.get('explodeCalls') or []
        check(r.get('rootExplodeStatus')==0 and r.get('allLeavesRead') is True and r.get('numericValueInferred') is False,label+': no unknown leaves or inferred values')
        check(r.get('returnedTransientCount')==r.get('deletedTransientCount')==len(nodes) and bool(nodes) and r.get('ownershipSafe') is True and all(r.get(k)==0 for k in ('unresolvedOwnershipCount','rejectedBorrowedCount','duplicatePointerCount')),label+': every caller-owned transient released exactly once')
        ids=[n.get('nodeId') for n in nodes];index={n.get('nodeId'):n for n in nodes}
        check(ids==list(range(1,len(nodes)+1)),label+': complete unique transient node identities')
        check(all(n.get('kind') in ('text','mtext','geometry','compound') and all(n.get(k) is True for k in ('transient','objectIdNull','ownerIdNull','databaseNull')) and isinstance(n.get('depth'),int) and 1<=n['depth']<=8 and ((n.get('parentNodeId')==0 and n['depth']==1) or n.get('parentNodeId') in index and index[n['parentNodeId']].get('kind')=='compound' and n['depth']==index[n['parentNodeId']]['depth']+1) for n in nodes),label+': actual generic transient tree, without Axis/circle assumption')
        expected=[n for n in nodes if n.get('kind') in ('text','mtext')]
        check(bool(texts) and near(texts,expected) and all(isinstance(t.get('plainText'),str) and isinstance(t.get('rawText'),str) and isinstance(t.get('position'),list) and len(t['position'])==3 for t in texts),label+': actual native Text/MText payloads recorded')
        check(bool(calls) and calls[0].get('nodeId')==0 and all(c.get('status')==0 and c.get('returnedCount',0)>0 for c in calls),label+': real public explode calls complete')
        return texts
    try:
        if not path.is_file():
            check(False,'native elevation core evidence missing');return result
        d=json.loads(path.read_text('utf-8-sig'));run=path.resolve().parent
        check(d.get('contract')=='stage1.elevation-roundtrip-core.v1' and d.get('api')=='ObjectARX public native clone/transform/explode/save; no COM','explicit non-UI core contract')
        check(d.get('completed') is True and d.get('error') is None,'native clone/readback/persistence core completion')
        check(all(d.get(k) is False for k in ('uiCommandsAttempted','uiControlsAccessed','roundtripAttempted','roundtripCompleted')) and near(d.get('plannedDeltaMetres'),0.123),'unperformed command/UI/business route remains unclaimed')
        check(d.get('sourceHandle')=='CE8F' and d.get('sourceType')=='TDbSymbElevation','actual scheme native elevation source')
        old=d.get('originalBefore') or [];check(len(old)==213 and len({e.get('handle') for e in old})==213 and near(old,d.get('originalAfter')),'all frozen original213 public snapshots preserved')
        target=d.get('targetHandle');check(isinstance(target,str) and bool(target) and target not in {e.get('handle') for e in old},'one distinct native elevation clone')
        translation=d.get('translation') or {};source=translation.get('source') or {};initial=translation.get('target') or {}
        check(near(translation.get('offset'),[40000,0,0]) and translation.get('allPublicGeometryExactlyTranslated') is True and translation.get('YChangedToSetElevation') is False and translation.get('ZChangedToSetElevation') is False,'pure-X fixture placement, no Y/Z elevation substitute')
        check(source.get('handle')=='CE8F' and initial.get('handle')==target and source.get('type')==initial.get('type')=='TDbSymbElevation' and source.get('proxy') is False and initial.get('proxy') is False,'original and clone retain actual proprietary elevation class')
        def shifted(points):return [[p[0]+40000,p[1],p[2]] for p in points]
        for key in ('grip','stretch'):
            a=source.get(key) or {};b=initial.get(key) or {}
            check(a.get('status')==b.get('status')==0 and bag(shifted(a.get('points') or []),b.get('points') or []),'complete public '+key+' positions exactly X+40000')
        check(source.get('extentStatus')==initial.get('extentStatus')==0 and near(initial.get('min'),shifted([source['min']])[0]) and near(initial.get('max'),shifted([source['max']])[0]),'native bounds exactly pure-X translated')
        source_text=readback(d.get('sourceReadback') or {},'source','CE8F');initial_text=readback(d.get('initialReadback') or {},'initial',target)
        check(Counter(t['plainText'] for t in source_text)==Counter(t['plainText'] for t in initial_text),'actual displayed source text preserved by clone/placement')
        expected_text=[(t['plainText'],[t['position'][0]+40000,t['position'][1],t['position'][2]]) for t in source_text]
        check(bag([[s,p] for s,p in expected_text],[[t['plainText'],t['position']] for t in initial_text]),'actual exploded text positions follow same pure-X placement')
        snapshot=d.get('initialSnapshot') or {};output=Path(snapshot.get('outputDWG',''))
        check(snapshot.get('stage')=='initial' and snapshot.get('completed') is True and snapshot.get('ownershipSafe') is True,'initial native persistence stage completed')
        check(all(snapshot.get(k)==0 for k in ('saveStatus','reopenStatus','closeInputStatus')) and snapshot.get('firstReaderDestroyed') is True and snapshot.get('reopenedModelCount')==214,'full save and independent fresh214 reader')
        check(near(snapshot.get('originalSaved'),old) and near(snapshot.get('targetSaved'),initial),'original213 and native clone geometry persist')
        live=readback(snapshot.get('liveReadback') or {},'live snapshot',target);saved=readback(snapshot.get('savedReadback') or {},'saved snapshot',target)
        check(Counter(t['plainText'] for t in source_text)==Counter(t['plainText'] for t in live)==Counter(t['plainText'] for t in saved),'actual original text persists through separate saved reader')
        original_elevations={e.get('handle'):e for e in old if e.get('type')=='TDbSymbElevation'}
        check(len(original_elevations)==6 and all(e.get('proxy') is False for e in original_elevations.values()),'all6 actual original elevation roots bound to frozen213 inventory')
        protected_values=[]
        for key,records in (('baseline',d.get('originalElevationBefore') or []),('live',snapshot.get('originalElevationLive') or []),('saved',snapshot.get('originalElevationSaved') or [])):
            check(len(records)==6 and {r.get('handle') for r in records}==set(original_elevations),key+': complete actual6 original elevation handle set')
            values={}
            for record in records:
                handle=record.get('handle');r=record.get('readback') or {}
                check(record.get('type')=='TDbSymbElevation' and record.get('proxy') is False and near(r.get('before'),original_elevations.get(handle)),key+': actual native original elevation type and geometry '+str(handle))
                rows=readback(r,key+' original '+str(handle),handle);values[handle]=Counter(t['plainText'] for t in rows)
            protected_values.append(values)
        check(len(protected_values)==3 and protected_values[0]==protected_values[1]==protected_values[2],'all6 original elevation actual text/zero-sign formatting unchanged live and after fresh save')
        check(output.is_file() and output.resolve()==run/'elevation-initial.dwg','isolated initial snapshot file')
        check(Path(d.get('controlDWG','')).resolve()==run/'control.dwg' and (run/'control.dwg').is_file(),'owned frozen-control copy')
        candidates=[numeric_candidate(t.get('plainText')) for t in saved];candidates=[c for c in candidates if c is not None]
        result['actualSavedText']=[t['plainText'] for t in saved];result['numericCandidates']=candidates
        result['threeDecimalCandidateAvailable']=len(candidates)==1 and candidates[0]['decimalPlaces']>=3
        result['numericReadiness']='PENDING_ACTUAL_CONTROLS_AND_INCREMENT_DECREMENT_EVIDENCE' if result['threeDecimalCandidateAvailable'] else 'MISSING_UNIQUE_THREE_DECIMAL_FORMAT_BINDING'
        result['requiredBusinessEvidence']=['Actual discovered control roles/defaults; no guessed IDs','Preserve actual public text of all6 original elevation symbols at every stage','Increase by0.123m: actual unique numeric display difference, unchanged XY placement, fresh saved reader','Decrease by0.123m: original exact text/zero-sign formatting restored and fresh reader','Restore original radio/value defaults and reopen read-only to verify']
    except (OSError,ValueError,TypeError,KeyError,IndexError,AttributeError) as e:check(False,'incomplete elevation core evidence: '+str(e))
    result['result']='FAIL' if result['failures'] else 'PASS'
    return result


def main(path):
    r=inspect(path)
    for f in r['failures']:print('FAIL:',f)
    print(f"assertions={r['assertions']} passed={r['assertions']-len(r['failures'])} failed={len(r['failures'])} core={r['result']} elevation_roundtrip=NOT_ATTEMPTED")
    print('actualSavedText='+json.dumps(r.get('actualSavedText',[]),ensure_ascii=False))
    print('numericReadiness='+r['numericReadiness'])
    return int(r['result']!='PASS')


if __name__=='__main__':raise SystemExit(main(Path(sys.argv[1])))
