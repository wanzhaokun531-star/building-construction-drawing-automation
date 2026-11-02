"""Check public transient explode readback without inferring labels from requests.

Default success proves readback/ownership/model protection only. Optional
--expect-four-side compares 44 actual circle/text pairs with confirmed rules.
Never contacts CAD, modifies a DWG, or runs an EXPLODE command.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path


def finite(value):
    return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value)


def point(value):
    return isinstance(value,list) and len(value)==3 and all(finite(v) for v in value)


def anchors(node):
    bounds=node.get('bounds') or {};result=[]
    if bounds.get('status')==0 and point(bounds.get('min')) and point(bounds.get('max')):
        result.append([(a+b)/2 for a,b in zip(bounds['min'],bounds['max'])])
    for key in ('alignmentPoint','position'):
        if point(node.get(key)):result.append(node[key])
    return result


def circle_readbacks(axis):
    texts=[n for n in axis.get('nodes',[]) if n.get('kind') in ('text','mtext')]
    circles=[n for n in axis.get('nodes',[]) if n.get('kind')=='circle']
    rows=[]
    for circle in circles:
        candidates=[]
        if point(circle.get('center')) and finite(circle.get('radius')) and circle['radius']>0:
            for text in texts:
                positions=anchors(text)
                if positions and min(math.dist(p,circle['center']) for p in positions)<=circle['radius']*.9:
                    candidates.append(dict(nodeId=text.get('nodeId'),actualText=text.get('plainText'),position=text.get('position')))
        rows.append(dict(circleNodeId=circle.get('nodeId'),center=circle.get('center'),radius=circle.get('radius'),
                         candidates=candidates,uniqueActualText=candidates[0]['actualText'] if len(candidates)==1 else None))
    return rows


def inspect(path,expect_four_side=False,source_dwg=None):
    result=dict(contract='stage1.axis-explode-readback-check.v1',result='FAIL',assertions=0,failures=[],
                numberingSemantics='PENDING' if expect_four_side else 'READBACK_ONLY',readbacks=[])
    def check(ok,label):
        result['assertions']+=1
        if not ok:result['failures'].append(label)
        return bool(ok)
    try:
        d=json.loads(path.read_text('utf-8-sig'));run=path.resolve().parent
        check(d.get('contract')=='stage1.axis-explode-readback-native.v1','native readback contract')
        check(d.get('api')=='ObjectARX native public AcDbEntity::explode readback; no COM','public native API route')
        check(d.get('error') is None and d.get('completed') is True and d.get('ownershipSafe') is True,'native completed without unsupported/ownership failure')
        count=d.get('initialModelCount');expected_axes={215:2,235:22}.get(count)
        check(expected_axes is not None and d.get('finalModelCount')==count and d.get('axisCount')==expected_axes
              and d.get('protectedNonAxisCount')==213,'Seed215 or Singleton235 complete protected model count')
        before=d.get('allModelBefore') or [];after=d.get('allModelAfter') or []
        check(len(before)==count and before==after and d.get('modelMembershipUnchanged') is True,'every original and axis public entity snapshot strictly unchanged')
        handles=[e.get('handle') for e in before]
        check(len(set(handles))==len(handles) and all(isinstance(h,str) and h for h in handles),'complete unique model handles')
        check(d.get('dbmodBefore')==d.get('dbmodAfter') and isinstance(d.get('dbmodBefore'),int)
              and d['dbmodBefore']>=0 and d.get('activeTransactionsBefore')==d.get('activeTransactionsAfter')==0,'DBMOD unchanged and zero transaction boundary')
        for key in ('dwgSaved','persistentAppendAttempted','persistentEraseAttempted','privateFieldsAccessed','commandBridgeUsed','comUsed'):
            check(d.get(key) is False,'no '+key)
        control=Path(d.get('controlDWG',''))
        check(control.is_file() and control.resolve()==run/'control.dwg','owned real control DWG only')
        if control.is_file():result['controlSHA256']=hashlib.sha256(control.read_bytes()).hexdigest().upper()
        result['sourceFileUnchangedScope']='Native report-only operation and unchanged public model/DBMOD; controller must retain before/after file hashes for on-disk identity.'
        if source_dwg is not None:
            check(source_dwg.is_file() and control.is_file() and source_dwg.read_bytes()==control.read_bytes(),'explicit source DWG SHA/bytes equals control copy')
            if source_dwg.is_file():result['sourceSHA256']=hashlib.sha256(source_dwg.read_bytes()).hexdigest().upper()
        axes=d.get('axes') or [];check(len(axes)==expected_axes,'all native axis roots observed')
        model_axes={e.get('handle'):e for e in before if e.get('type')=='TDbAxisLabelSet'}
        check(len(model_axes)==expected_axes and {a.get('axisHandle') for a in axes}==set(model_axes),'exact native root set in original model')
        for axis in axes:
            h=axis.get('axisHandle');label='axis '+str(h);nodes=axis.get('nodes') or [];calls=axis.get('explodeCalls') or []
            check(axis.get('error') is None and axis.get('completed') is True and axis.get('rootExplodeStatus')==0,label+' public explode supported')
            check(axis.get('before')==axis.get('after')==model_axes.get(h)
                  and (axis.get('before') or {}).get('proxy') is False,label+' native TCH root unchanged')
            check(axis.get('returnedTransientCount')==axis.get('deletedTransientCount')==axis.get('explodedNodeCount')==len(nodes)
                  and len(nodes)>0 and axis.get('ownershipSafe') is True
                  and axis.get('unresolvedOwnershipCount')==axis.get('rejectedBorrowedCount')==axis.get('duplicatePointerCount')==0,
                  label+' every independent transient deleted exactly once; no uncertain/borrowed/duplicate ownership')
            ids=[n.get('nodeId') for n in nodes];by_id={n.get('nodeId'):n for n in nodes}
            check(ids==list(range(1,len(nodes)+1)),label+' unique contiguous transient node identities')
            check(all(n.get('transient') is True and n.get('objectIdNull') is True
                      and n.get('databaseNull') is True and n.get('ownerIdNull') is True
                      and isinstance(n.get('type'),str) and n['type']
                      and isinstance(n.get('depth'),int) and 1<=n['depth']<=8
                      and ((n.get('parentNodeId')==0 and n['depth']==1)
                           or n.get('parentNodeId') in by_id and by_id[n['parentNodeId']].get('kind')=='compound'
                           and n['depth']==by_id[n['parentNodeId']]['depth']+1)
                      for n in nodes),label+' caller-owned transient proof and bounded parent/depth tree')
            call_ids=[c.get('nodeId') for c in calls]
            check(len(call_ids)==len(set(call_ids)) and 0 in call_ids and all(c.get('status')==0
                  and c.get('returnedCount')==sum(n.get('parentNodeId')==c.get('nodeId') for n in nodes)
                  and c.get('returnedCount',0)>0 and (c.get('depth')==0 if c.get('nodeId')==0
                    else c.get('nodeId') in by_id and by_id[c['nodeId']].get('kind')=='compound'
                    and c.get('depth')==by_id[c['nodeId']].get('depth')) for c in calls),label+' every public explode status and returned child count captured')
            check(all(n.get('kind') in ('text','mtext','line','circle','compound')
                      and (n.get('kind')!='compound' or n.get('nodeId') in call_ids) for n in nodes)
                  and axis.get('allLeavesRead') is True,label+' no unsupported or skipped transient leaves')
            texts=[n for n in nodes if n.get('kind') in ('text','mtext')];circles=[n for n in nodes if n.get('kind')=='circle']
            check(bool(texts) and bool(circles) and axis.get('hasText') is True and axis.get('hasCircle') is True
                  and any(isinstance(n.get('plainText'),str) and n['plainText'] for n in texts),label+' actual nonempty public text and circle geometry available')
            for n in nodes:
                kind=n.get('kind');node_label=label+' node '+str(n.get('nodeId'))
                if kind in ('text','mtext'):
                    check(isinstance(n.get('text'),str) and isinstance(n.get('plainText'),str)
                          and point(n.get('position')) and finite(n.get('height')) and n['height']>0
                          and finite(n.get('rotation')) and (kind!='text' or point(n.get('alignmentPoint'))),node_label+' actual text and finite placement')
                    b=n.get('bounds') or {}
                    check(isinstance(b.get('status'),int) and (b['status']!=0 or point(b.get('min')) and point(b.get('max'))),node_label+' actual public text extents status')
                elif kind=='circle':check(point(n.get('center')) and finite(n.get('radius')) and n['radius']>0 and point(n.get('normal')),node_label+' finite positive circle')
                elif kind=='line':check(point(n.get('start')) and point(n.get('end')),node_label+' finite line')
            rows=circle_readbacks(axis)
            result['readbacks'].append(dict(axisHandle=h,texts=[{k:n.get(k) for k in ('nodeId','type','plainText','position','alignmentPoint','bounds')} for n in texts],circles=rows))
        if expect_four_side:
            stations={'vertical':{},'horizontal':{}}
            for axis,readback in zip(axes,result['readbacks']):
                circles=readback['circles'];xs={round(r['center'][0],5) for r in circles if point(r.get('center'))};ys={round(r['center'][1],5) for r in circles if point(r.get('center'))}
                orientation='vertical' if len(xs)==1 and len(ys)>1 or len(ys)==2 and len(xs)>2 else 'horizontal' if len(ys)==1 and len(xs)>1 or len(xs)==2 and len(ys)>2 else None
                if not check(orientation is not None,'four-side root direction from actual circle centers '+str(axis.get('axisHandle'))):continue
                used=[]
                for row in circles:
                    candidates=row['candidates'];check(len(candidates)==1,'every actual circle has exactly one public text '+str(axis.get('axisHandle')))
                    if len(candidates)==1:used.append(candidates[0]['nodeId'])
                    primary=row['center'][0 if orientation=='vertical' else 1];key=round(primary,5)
                    stations[orientation].setdefault(key,[]).append(dict(row,axisHandle=axis.get('axisHandle')))
                check(len(used)==len(set(used)),'one public text is not reused across circles '+str(axis.get('axisHandle')))
            expected={'vertical':[str(i) for i in range(1,11)],'horizontal':list('ABCDEFGHJKLM')}
            semantic=[]
            for orientation,labels in expected.items():
                ordered=sorted(stations[orientation].items());check(len(ordered)==len(labels),'actual '+orientation+' station count')
                for (station,rows),wanted in zip(ordered,labels):
                    labels_actual=[r['uniqueActualText'] for r in rows]
                    check(len(rows)==2 and labels_actual==[wanted,wanted],'actual '+orientation+' station '+str(station)+' bilateral number '+wanted)
                    semantic.append(dict(orientation=orientation,station=station,actualLabels=labels_actual,expectedLabel=wanted))
            check(sum(len(rows) for group in stations.values() for rows in group.values())==44,'44 actual circles, never inferred from requests')
            result['fourSideComparison']=semantic
            result['numberingSemantics']='FAIL' if result['failures'] else 'PASS_ACTUAL_PUBLIC_TEXT_44_CIRCLES'
    except (OSError,ValueError,KeyError,TypeError,AttributeError,OverflowError) as error:
        check(False,'incomplete or malformed native evidence: '+str(error))
    result['failed']=len(result['failures']);result['passed']=result['assertions']-result['failed'];result['result']='FAIL' if result['failures'] else 'PASS'
    result['scope']='Public transient text/circle/line readback and ownership/model preservation; never a replacement of the persistent TCH roots.'
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('report',type=Path);p.add_argument('--expect-four-side',action='store_true');p.add_argument('--source-dwg',type=Path);args=p.parse_args()
    result=inspect(args.report,args.expect_four_side,args.source_dwg);print(json.dumps(result,ensure_ascii=False,indent=2));return int(result['result']!='PASS')


if __name__=='__main__':raise SystemExit(main())
