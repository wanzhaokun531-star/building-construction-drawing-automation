"""Independent native symbol business acceptance; requested UI text is not output."""
import json
from pathlib import Path
import sys
import math


EXPECTED = {
    'slope': ('TCH_ARROW', [['2%'], ['2%']]),
    'section': ('TCH_SYMB_SECTION', [['31', 'S1-01'], ['32', 'S1-01']]),
    'pointer-index': ('TCH_INDEXPOINTER', [['7', 'S1-01'], ['8', 'S1-01']]),
    'section-index': ('TCH_INDEXPOINTER', [['7', 'S1-01'], ['8', 'S1-01']]),
}


def inspect(data):
    checks=[]
    def check(ok,name):checks.append((bool(ok),name))
    check(data.get('contract')=='stage1.symbol-business-native.v1','actual symbol business contract')
    kind=data.get('kind');check(kind in EXPECTED,'explicit supported fixture kind')
    check(data.get('completed') is True and data.get('error') is None,'native command/protection contract complete')
    check(data.get('safeToContinue') is True,'actual native callback lifecycle safe')
    check(len(data.get('originalBefore',[]))==213 and data.get('originalBefore')==data.get('originalAfter'),'all213 original public states retained')
    check(data.get('originalResourceBefore')==data.get('originalResourceAfter') and bool(data.get('originalResourceBefore')),'prior resource definitions retained')
    check(data.get('defaultsRestored') is True and data.get('originalDefaults')==data.get('observedRestoredDefaults') and bool(data.get('originalDefaults')),'actual original defaults independently reopened')
    check(data.get('modelCount')==215,'two actual native symbols plus original213')
    symbols=data.get('symbols',[]);check(len(symbols)==2,'actual two symbols')
    wanted=EXPECTED.get(kind,('',[[],[]]))
    handles=[]
    for i,symbol in enumerate(symbols):
        name=f'symbol{i}';handles.append(symbol.get('handle'))
        check(symbol.get('proxy') is False and symbol.get('dxfName')==wanted[0] and str(symbol.get('rxName','')).startswith('TDb'),name+' exact native TCH category')
        for stage in ('liveReadback','savedReadback'):
            read=symbol.get(stage) or {}
            check(read.get('contract')=='stage1.symbol-public-readback.v1' and read.get('completed') is True and read.get('ownershipSafe') is True,name+'/'+stage+' actual public representation')
            nodes=read.get('nodes',[])
            check(bool(nodes) and len({n.get('nodeId') for n in nodes})==len(nodes),name+'/'+stage+' distinct actual transient nodes')
            check(all(n.get('transient') is True and n.get('objectIdNull') is True and n.get('ownerIdNull') is True and n.get('databaseNull') is True for n in nodes),name+'/'+stage+' no borrowed database geometry')
            texts=[n.get('plainText') for n in read.get('texts',[])]
            for text in wanted[1][min(i,1)]:check(text in texts,name+'/'+stage+' actual text '+text)
            check(read.get('returnedTransientCount')==read.get('deletedTransientCount') and read.get('unresolvedOwnershipCount')==0,name+'/'+stage+' returned objects safely released')
        check(symbol.get('liveReadback',{}).get('texts')==(symbol.get('savedReadback') or {}).get('texts'),name+' visible text geometry persists')
    check(len(set(handles))==2 and all(handles),'distinct actual symbol handles')
    snapshot=data.get('snapshot') or {}
    check(snapshot.get('saveStatus')==snapshot.get('reopenStatus')==snapshot.get('closeInputStatus')==0 and snapshot.get('firstReaderDestroyed') is True,'save and new independent database reader')
    check(snapshot.get('modelCount')==215 and snapshot.get('originalSaved')==data.get('originalBefore'),'saved original213 retained')
    check(data.get('protectedElevationBefore')==data.get('protectedElevationAfter')==snapshot.get('protectedElevationSaved') and bool(data.get('protectedElevationBefore')),'actual six original elevation public values retained live/saved')
    operations=data.get('operations',[])
    check(bool(operations),'actual UI and public command operations')
    for i,op in enumerate(operations):
        check(op.get('completed') is True and op.get('safeToContinue') is True,f'operation{i} complete native lifecycle')
        check(op.get('configurationVerified') is True,f'operation{i} actual standard-control readback')
        trace=op.get('commandTrace',[]);names=[x.get('event') for x in trace if str(x.get('command','')).upper()==str(op.get('command','')).upper()]
        check(names.count('commandWillStart')==names.count('commandEnded')==1 and 'commandFailed' not in names and 'commandCancelled' not in names,f'operation{i} one normal public command')
        if op.get('mode')=='create':
            check(op.get('fieldSettersAttempted') is False,f'operation{i} creation uses independently preconfigured values')
            check(len(op.get('newEntities',[]))==1,f'operation{i} exactly one real new entity')
    return checks


if __name__=='__main__':
    data=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
    checks=inspect(data)
    for ok,name in checks:
        if not ok:print('FAIL:',name)
    failed=sum(not ok for ok,_ in checks)
    print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_NATIVE_SYMBOL_TEXT_AND_PERSISTENCE"}')
    print('Geometry/direction and rendered PDF require separate independent fixture review.')
    raise SystemExit(bool(failed))
