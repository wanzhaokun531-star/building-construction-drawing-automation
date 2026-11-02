"""Verify temporary visibility diagnostics without accepting them as final layout.

Usage: verify_axis_overlap_preview_native.py <overlap-preview.json> <singleton-native.json>
"""
import hashlib
import json
import sys
from pathlib import Path
from verify_four_axis_seed_native import near,entity_observation,auxiliary_state
from verify_four_axis_preview_native import check_singleton_manifest
from verify_four_side_singletons_native import validate as validate_singletons
from verify_native_plot import CONFIG_STAGES,ENGINE_STAGES


def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest().upper()


def visibility_map(values):
    if not isinstance(values,list):return None
    result={}
    for v in values:
        if not isinstance(v,dict):return None
        h=v.get('handle');value=v.get('visibility')
        if not isinstance(h,str) or not h or h in result or type(value) is not int or value not in (0,1):return None
        result[h]=value
    return result


def check_refresh(r,check,label):
    check(r.get('attempted') is True and r.get('completed') is True and r.get('commandName')=='_.REGEN'
          and r.get('commandStatus')==5100 and r.get('activeTransactionsBefore')==r.get('activeTransactionsAfter')==0,label+': actual public REGEN and zero transactions')


def check_visibility_page(record,geometry,visibility,hidden,companion,check,label):
    check(record.get('error') is None and record.get('completed') is True and record.get('safeToContinue') is True,label+': page/restoration completed safely')
    check(record.get('hiddenAxisHandle')==hidden and record.get('visibleCompanionHandle')==companion and hidden!=companion,label+': declared alternate native axis targets')
    check(record.get('activeTransactionsBefore')==record.get('activeTransactionsAfter')==0,label+': zero transactions around plot')
    check_refresh(record.get('refreshBefore') or {},check,label+'/before')
    for key in ('geometryBefore','geometryHidden','geometryRestored'):check(near(record.get(key),geometry),label+': complete235 public geometry '+key)
    check(isinstance(record.get('globalsBefore'),dict) and record.get('globalsBefore')==record.get('globalsAfter'),label+': page native globals unchanged')
    check(isinstance(record.get('layoutBefore'),dict) and record.get('layoutBefore')==record.get('layoutAfter'),label+': page saved layout unchanged')
    baseline=visibility_map(visibility);start=visibility_map(record.get('visibilityBefore'));during=visibility_map(record.get('visibilityHidden'));end=visibility_map(record.get('visibilityRestored'))
    check(baseline is not None and len(baseline)==235 and start==end==baseline,label+': complete235 visibility states restored')
    expected=dict(baseline or {})
    if hidden in expected:expected[hidden]=1
    check(baseline is not None and baseline.get(hidden)==baseline.get(companion)==0 and during==expected,label+': exactly one originally visible native collection hidden, companion and all others unchanged')
    mutation=record.get('visibilityOverride') or {}
    check(mutation.get('api')=='AcDbEntity::setVisibility' and mutation.get('doSubents') is False and mutation.get('axisHandle')==hidden,label+': public top-level visibility API only')
    check(mutation.get('originalVisibility')==0 and mutation.get('setAttempted') is True and mutation.get('setStatus')==mutation.get('setTransactionStatus')==0 and mutation.get('hiddenReadback')==1,label+': real hidden setter transaction and native readback')
    check(mutation.get('restoreAttempted') is True and mutation.get('restoreStatus')==mutation.get('restoreTransactionStatus')==0 and mutation.get('restoredReadback')==0 and mutation.get('restored') is True and mutation.get('restoreError') is None,label+': original visibility restored by native setter/readback')
    check_refresh(mutation.get('hiddenRefresh') or {},check,label+'/hidden')
    check_refresh(mutation.get('restoredRefresh') or {},check,label+'/restored')


def check_plot(p,run,page_id,window,wd,check,label):
    check(p.get('id')==page_id and p.get('requestedPaperMM')==[594,420],label+': explicit A2 native page')
    check(p.get('error') is None and p.get('copySettingsStatus')==p.get('validateStatus')==p.get('deviceListStatus')==0 and p.get('isValidated') is True and 'DWG To PDF.pc3' in p.get('devices',[]),label+': actual native plot validation')
    check(p.get('matchingPolicy')==2 and p.get('processPlotStateBefore')==p.get('processPlotStateAfter')==0,label+': existing paper and idle plot states')
    for key in CONFIG_STAGES:check((p.get('configurationStatuses') or {}).get(key)==0,label+': configure '+key)
    for key in ENGINE_STAGES:check((p.get('engineStatuses') or {}).get(key)==0,label+': native engine '+key)
    check(p.get('engineDestroyed') is True and p.get('cleanupThrew') is False and p.get('pipelineComplete') is True,label+': native engine clean completion')
    for key in ('settings','validatedSettings'):
        s=p.get(key) or {};dims=s.get('paperMM') or [];margins=s.get('marginsMM') or [];rotation=s.get('rotation')
        check(all(s.get(k)==0 for k in ('deviceStatus','mediaStatus','paperSizeStatus','marginsStatus','windowStatus','scaleStatus')),label+': actual settings getters '+key)
        check(s.get('device')=='DWG To PDF.pc3' and len(dims)==2 and near(sorted(dims),[420,594],0.1),label+': actual physical A2 paper '+key)
        check(s.get('paperUnits')==1 and s.get('plotType')==4 and s.get('useStandardScale') is False and near(s.get('scale'),[1,100]) and near(s.get('windowDCS'),wd),label+': exact full-window1:100, no Fit '+key)
        if check(len(dims)==2 and len(margins)==4 and rotation in (0,1),label+': validated printable margins '+key):
            printable=[dims[0]-margins[0]-margins[2],dims[1]-margins[1]-margins[3]]
            if rotation==1:printable.reverse()
            check(printable[0]>=(window[2]-window[0])/100-0.1 and printable[1]>=(window[3]-window[1])/100-0.1,label+': entire235 model fits actual A2 printable area '+key)
    pdf=run/(page_id+'.pdf')
    check(Path(p.get('outputPDF','')).resolve()==pdf and pdf.is_file() and p.get('fileObserved') is True,label+': actual isolated diagnostic PDF')
    if pdf.is_file():
        check(pdf.stat().st_size==p.get('fileBytes') and pdf.stat().st_size>5 and pdf.read_bytes()[:5]==b'%PDF-',label+': actual PDF signature/bytes')
        from pypdf import PdfReader
        pages=PdfReader(str(pdf)).pages;check(len(pages)==1,label+': one actual PDF page')
        if pages:check(near(sorted([float(pages[0].mediabox.width)*25.4/72,float(pages[0].mediabox.height)*25.4/72]),[420,594],0.5*25.4/72+1e-6),label+': actual physical A2 MediaBox')


def validate(path,source_report):
    count=0;failures=[]
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file() or not source_report.is_file():return 1,['explicit overlap preview and successful Singleton source reports required']
    d=json.loads(path.read_text('utf-8-sig'));source=json.loads(source_report.read_text('utf-8-sig'));run=path.resolve().parent
    check(d.get('contract')=='stage1.axis-overlap-preview-native.v1' and d.get('api')=='ObjectARX native diagnostic AcPl; no COM' and d.get('sourceKind')=='full-four-side-singletons-235','explicit diagnostic235 native contract')
    check(d.get('error') is None and d.get('completed') is True and d.get('safeToContinue') is True,'native diagnostic/restoration completed safely')
    c,f=validate_singletons(source_report);check(c>1 and not f,'successful complete Singleton source: '+('; '.join(f[:3]) if f else 'PASS'))
    control=run/'control.dwg';output=Path(source.get('outputDWG',''))
    check(Path(d.get('controlDWG','')).resolve()==control and control.is_file() and output.is_file() and digest(control)==digest(output),'control bytes remain SHA256-equal to exact successful source output DWG')
    check(d.get('initialModelCount')==d.get('finalModelCount')==235 and d.get('dwgSaved') is False,'complete235 model and no DWG save')
    before=d.get('originalBefore') or [];after=d.get('originalAfter') or [];saved=(source.get('originalSaved') or [])+(source.get('savedAxes') or [])
    bb={e.get('handle'):e for e in before};ss={e.get('handle'):e for e in saved}
    check(len(before)==len(after)==len(bb)==len(ss)==235 and set(bb)==set(ss) and near(before,after),'complete235 source identities and final public geometry unchanged')
    for h in set(bb)&set(ss):check(near(bb[h],ss[h]),'exact saved source public geometry '+str(h))
    visibility=d.get('visibilityBefore') or [];vm=visibility_map(visibility)
    check(vm is not None and set(vm)==set(bb) and vm==visibility_map(d.get('visibilityAfter')),'all235 initial/final visibility states identical')
    axes=d.get('axisObjects') or [];check_singleton_manifest(axes,source,check)
    check({a.get('nativeGeometry',{}).get('handle') for a in axes}=={h for h,e in bb.items() if e.get('type')=='TDbAxisLabelSet'},'all22 actual axis objects match full model')
    horizontal=[]
    for a in axes:
        e=a.get('nativeGeometry') or {};h=e.get('handle');vertical=a.get('orientation')=='vertical'
        check(near(e,bb.get(h)) and vm is not None and vm.get(h)==0,'actual native axis geometry/initial visibility '+str(h))
        if not vertical:
            o=entity_observation(e,False);aux=auxiliary_state(e,False)
            if check(o is not None and len(o['stations'])==1 and aux is not None and aux['boundsValid'] and near(aux['radius'],400),'actual horizontal singleton circle geometry '+str(h)):
                horizontal.append((o['stations'][0],h,aux['radius']))
    horizontal.sort();pairs=[]
    for i,(a,h,r) in enumerate(horizontal):
        for b,k,s in horizontal[i+1:]:
            if b-a>1e-6 and b-a<r+s-1e-6:pairs.append((a,h,b,k,r,s))
    pair=d.get('overlapPair') or {}
    if check(len(pairs)==1,'exactly one actual horizontal circle-overlap station pair'):
        a,h,b,k,r,s=pairs[0]
        check(pair.get('orientation')=='horizontal' and pair.get('selectionRule')=='exactly one actual native station pair with 0 < deltaY < 2R'
              and pair.get('lowerHandle')==h and pair.get('upperHandle')==k and near(pair.get('lowerStation'),a) and near(pair.get('upperStation'),b)
              and near(pair.get('radius'),r) and near(r,s) and near(pair.get('stationDifference'),b-a),'diagnostic pair independently derived from actual native circle geometry')
    if check(bool(before) and all(e.get('extentStatus')==0 and len(e.get('min',[]))==len(e.get('max',[]))==3 for e in before),'all235 native bounds available'):
        window=[min(e['min'][0] for e in before)-500,min(e['min'][1] for e in before)-500,max(e['max'][0] for e in before)+500,max(e['max'][1] for e in before)+500]
    else:window=[0,0,0,0]
    wd=d.get('windowDCS') or []
    check(near(d.get('windowWCS'),window) and d.get('requestedPaperMM')==[594,420] and d.get('scale')==[1,100],'same complete-model500mm-margin A2 window at exact1:100')
    check(d.get('windowTransformStatuses')==[5100]*4 and len(wd)==4 and near([wd[2]-wd[0],wd[3]-wd[1]],[window[2]-window[0],window[3]-window[1]]),'actual WCS/DCS same full-window transform')
    view=d.get('viewCheck') or {};direction=view.get('direction') or []
    check(all(view.get(k)==5100 for k in ('directionStatus','twistStatus','perspectiveStatus')) and len(direction)==3 and near(direction[:2],[0,0]) and direction[2]>0 and near(view.get('twist'),0) and view.get('perspective')==0,'actual existing untwisted orthographic plan view')
    check(isinstance(d.get('globalsBefore'),dict) and d.get('globalsBefore')==d.get('globalsAfter'),'all captured native globals restored')
    check(isinstance(d.get('layoutBefore'),dict) and d.get('layoutBefore')==d.get('layoutAfter'),'saved layout settings restored')
    bg=d.get('backgroundOverride') or {}
    check(bg.get('processPlotStateBefore')==0 and bg.get('getStatus')==bg.get('setStatus')==bg.get('readbackStatus')==5100 and bg.get('newValue')==0,'native foreground plotting scope captured/set/read back')
    check(bg.get('restoreAttempted') is True and bg.get('restoreStatus')==bg.get('restoreReadbackStatus')==5100 and bg.get('restored') is True and bg.get('restoredValue')==bg.get('oldValue') and bg.get('restoreThrew') is False,'foreground plotting original value restored')
    pages=d.get('pages') or [];check(len(pages)==2,'exactly two alternate temporary-hide diagnostic pages')
    for i,page in enumerate(pages):
        if i>=2:break
        hidden=pair.get('lowerHandle') if i==0 else pair.get('upperHandle');companion=pair.get('upperHandle') if i==0 else pair.get('lowerHandle');page_id='axis-overlap-hide-lower' if i==0 else 'axis-overlap-hide-upper'
        check(page.get('id')==page_id,'explicit alternate page identifier '+str(i));check_visibility_page(page,before,visibility,hidden,companion,check,'page'+str(i));check_plot(page.get('plot') or {},run,page_id,window,wd,check,'page'+str(i))
    check(d.get('diagnosticOnly') is True and d.get('numberingSemanticVerified') is False,'temporary isolation is not final overlap resolution or invented circle text proof')
    return count,failures


def main(path,source):
    try:count,failures=validate(path,source)
    except (OSError,ValueError,KeyError,TypeError,AttributeError,IndexError,ImportError) as e:count,failures=1,['invalid/incomplete diagnostic evidence: '+str(e)]
    for failure in failures:print('FAIL:',failure)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print('number_semantics=UNVERIFIED; final_overlap=UNCHANGED; diagnostic temporary visibility restoration only')
    return int(bool(failures))


if __name__=='__main__':
    if len(sys.argv)!=3:raise SystemExit('Usage: verify_axis_overlap_preview_native.py <preview-json> <successful-singleton-json>')
    raise SystemExit(main(Path(sys.argv[1]),Path(sys.argv[2])))
