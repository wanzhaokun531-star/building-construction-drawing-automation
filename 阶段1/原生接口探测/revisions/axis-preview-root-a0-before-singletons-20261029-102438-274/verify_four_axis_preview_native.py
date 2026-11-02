"""Bind an unchanged native full-axis/number fixture to its complete 1:100 PDF.

Usage: verify_four_axis_preview_native.py <axis-preview-native.json> <source-native.json>
Visible circle numbers still need independent rendered review.
"""
import hashlib
import json
import sys
from pathlib import Path
from verify_four_axis_seed_native import near,entity_observation
from verify_native_plot import ENGINE_STAGES,CONFIG_STAGES
from verify_axis_continuous_number_native import check_number_request
from verify_four_side_axis_native import validate as validate_full


def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest().upper()


def validate(path,source_report):
    count=0;failures=[]
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
        return bool(ok)
    if not path.is_file() or not source_report.is_file():return 1,['native preview and explicitly bound source report are required']
    d=json.loads(path.read_text('utf-8-sig'));source=json.loads(source_report.read_text('utf-8-sig'));run=path.resolve().parent
    check(d.get('contract')=='stage1.four-axis-preview-native.v1' and d.get('api')=='ObjectARX native plot; no COM','native preview contract')
    check(d.get('error') is None and d.get('fixtureVerified') is True,'native fixture guard and operation')
    kind=d.get('sourceKind');full=kind=='full-four-side-215';expected=215 if full else 214
    check(kind in ('full-four-side-215','single-template-number-214'),'explicit limited source kind')
    if full:
        c,f,_=validate_full(source_report)
        check(c>1 and not f,'source full four-side native structural contract: '+('; '.join(f[:3]) if f else 'PASS'))
        source_objects=(source.get('originalSaved') or [])+(source.get('savedAxes') or [])
    else:
        check(source.get('contract')=='stage1.axis-continuous-number-native.v1' and source.get('error') is None,'source numbering fixture contract')
        check(source.get('initialModelCount')==source.get('finalModelCount')==source.get('reopenedModelCount')==214 and source.get('sourceCount')==213,'source complete213 plus one real native set')
        check(near(source.get('originalBefore'),source.get('originalAfter')) and near(source.get('originalBefore'),source.get('originalSaved')),'source original preservation and persistence')
        check_number_request(source.get('operation') or {},check,'source numbering')
        for key in ('saveStatus','reopenStatus','closeInputStatus'):check(source.get(key)==0,'source '+key)
        check(near(source.get('axisAfter'),source.get('axisSaved')),'source axis persistence')
        source_objects=(source.get('originalSaved') or [])+([source['axisSaved']] if isinstance(source.get('axisSaved'),dict) else [])
    control=run/'control.dwg';output=Path(source.get('outputDWG',''))
    for key in ('inputDWG','controlDWG'):check(Path(d.get(key,'')).resolve()==control,key+' owned path')
    check(control.is_file() and output.is_file() and digest(control)==digest(output),'exact source output DWG SHA256 equals isolated control bytes')
    check(d.get('initialModelCount')==d.get('finalModelCount')==expected and d.get('dwgSaved') is False,'complete source count and no DWG save')
    before=d.get('originalBefore') or [];after=d.get('originalAfter') or []
    check(len(before)==len(after)==expected and near(before,after),'all model entities unchanged by plotting')
    bb={e.get('handle'):e for e in before};ss={e.get('handle'):e for e in source_objects}
    check(len(bb)==len(ss)==expected and set(bb)==set(ss),'complete source native entity identity binding')
    for h in set(bb)&set(ss):check(near(bb[h],ss[h]),'source saved public geometry '+str(h))
    axes=d.get('axisObjects') or []
    check(len(axes)==expected-213,'exact native axis collection cohort')
    check({a.get('nativeGeometry',{}).get('handle') for a in axes}=={e.get('handle') for e in before if e.get('type')=='TDbAxisLabelSet'},'actual axis manifest matches model')
    for a in axes:
        e=a.get('nativeGeometry') or {};vertical=a.get('orientation')=='vertical';obs=entity_observation(e,vertical)
        check(e.get('type')=='TDbAxisLabelSet' and e.get('proxy') is False and near(e,bb.get(e.get('handle'))),'native proprietary axis geometry')
        check(obs is not None and a.get('stationCount')==len(obs['stations']),'actual station count')
    if full:check({(a.get('orientation'),a.get('stationCount')) for a in axes}=={('vertical',10),('horizontal',12)},'all four-side10X/12Y carriers present')
    if before:
        check(all(e.get('extentStatus')==0 and len(e.get('min',[]))==len(e.get('max',[]))==3 for e in before),'all model bounds available')
        lo=[min(e['min'][i] for e in before) for i in range(2)];hi=[max(e['max'][i] for e in before) for i in range(2)]
        check(near(d.get('modelBoundsMin'),lo) and near(d.get('modelBoundsMax'),hi),'whole model bounds independently measured')
        w=[lo[0]-500,lo[1]-500,hi[0]+500,hi[1]+500]
        check(d.get('marginModel')==500 and near(d.get('windowWCS'),w),'entire model and both axis directions included with500 model-mm margin')
    else:w=[0,0,0,0]
    wd=d.get('windowDCS') or []
    check(d.get('windowTransformStatuses')==[5100]*4 and len(wd)==4 and near([wd[2]-wd[0],wd[3]-wd[1]],[w[2]-w[0],w[3]-w[1]]),'WCS/DCS full plot window')
    view=d.get('viewCheck') or {}
    check(all(view.get(k)==5100 for k in ('directionStatus','twistStatus','perspectiveStatus')) and near(view.get('direction',[0,0,0])[:2],[0,0]) and view.get('direction',[0,0,0])[2]>0 and near(view.get('twist'),0) and view.get('perspective')==0,'actual untwisted orthographic plan view')
    check(isinstance(d.get('globalsBefore'),dict) and d.get('globalsBefore')==d.get('globalsAfter'),'all captured globals exactly restored')
    check(isinstance(d.get('layoutBefore'),dict) and d.get('layoutBefore')==d.get('layoutAfter'),'saved layout settings unchanged')
    bg=d.get('backgroundOverride') or {}
    check(bg.get('processPlotStateBefore')==0 and bg.get('getStatus')==5100 and bg.get('setStatus')==bg.get('readbackStatus')==5100 and bg.get('newValue')==0,'foreground plotting captured/set/read back')
    check(bg.get('restoreAttempted') is True and bg.get('restoreStatus')==bg.get('restoreReadbackStatus')==5100 and bg.get('restored') is True and bg.get('restoredValue')==bg.get('oldValue') and bg.get('restoreThrew') is False,'foreground override restored')
    p=d.get('page') or {};paper=d.get('requestedPaperMM')
    check(paper in ([594,420],[841,594],[1189,841]) and p.get('requestedPaperMM')==paper and p.get('id')=='axis-preview','explicit A2/A1/A0 paper')
    check(p.get('error') is None and p.get('copySettingsStatus')==p.get('validateStatus')==p.get('deviceListStatus')==0 and p.get('isValidated') is True and 'DWG To PDF.pc3' in p.get('devices',[]),'real native PDF override validation')
    check(p.get('matchingPolicy')==2 and p.get('processPlotStateBefore')==p.get('processPlotStateAfter')==0,'existing media and idle plot state')
    for key in CONFIG_STAGES:check(p.get('configurationStatuses',{}).get(key)==0,'configuration '+key)
    for key in ENGINE_STAGES:check(p.get('engineStatuses',{}).get(key)==0,'engine '+key)
    check(p.get('engineDestroyed') is True and p.get('cleanupThrew') is False and p.get('pipelineComplete') is True,'native plot pipeline completed and destroyed')
    for key in ('settings','validatedSettings'):
        s=p.get(key) or {};dims=s.get('paperMM') or [];margins=s.get('marginsMM') or [];rotation=s.get('rotation')
        check(all(s.get(k)==0 for k in ('deviceStatus','mediaStatus','paperSizeStatus','marginsStatus','windowStatus','scaleStatus')),'actual getters '+key)
        check(s.get('device')=='DWG To PDF.pc3' and len(dims)==2 and near(sorted(dims),sorted(paper or []),0.1),key+' physical ISO paper')
        check(s.get('paperUnits')==1 and s.get('plotType')==4 and s.get('useStandardScale') is False and near(s.get('scale'),[1,100]) and near(s.get('windowDCS'),wd),key+' exact1:100 window, no Fit')
        if check(len(dims)==2 and len(margins)==4 and rotation in (0,1),key+' actual printable margins'):
            printable=[dims[0]-margins[0]-margins[2],dims[1]-margins[1]-margins[3]]
            if rotation==1:printable.reverse()
            check(printable[0]>=(w[2]-w[0])/100-0.1 and printable[1]>=(w[3]-w[1])/100-0.1,key+' complete model and axis circles fit actual printable area at1:100')
    pdf=run/'axis-preview.pdf'
    check(Path(d.get('outputPDF','')).resolve()==pdf and Path(p.get('outputPDF','')).resolve()==pdf and pdf.is_file() and p.get('fileObserved') is True,'actual isolated PDF path')
    if pdf.is_file():
        check(pdf.stat().st_size==p.get('fileBytes') and pdf.stat().st_size>5 and pdf.read_bytes()[:5]==b'%PDF-','actual PDF bytes and signature')
        from pypdf import PdfReader
        pages=PdfReader(str(pdf)).pages;check(len(pages)==1,'actual one PDF page')
        if pages:check(near(sorted([float(pages[0].mediabox.width)*25.4/72,float(pages[0].mediabox.height)*25.4/72]),sorted(paper or []),0.5*25.4/72+1e-6),'actual PDF MediaBox matches requested physical paper')
    check(d.get('numberingSemanticVerified') is False and d.get('visualReview')=='pending independent rendering and inspection','printed circle numbers remain an independent rendered check')
    return count,failures


def main(path,source):
    try:count,failures=validate(path,source)
    except (OSError,ValueError,KeyError,TypeError,AttributeError,IndexError,ImportError) as e:count,failures=1,['invalid/incomplete native preview evidence: '+str(e)]
    for failure in failures:print('FAIL:',failure)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print('Scope: explicit source binding, native complete-window PDF, exact1:100, unchanged objects/layout/globals; circle text requires rendered inspection.')
    if source.is_file():print('sourceReport='+str(source.resolve())+' SHA256='+digest(source))
    return int(bool(failures))


if __name__=='__main__':
    if len(sys.argv)!=3:raise SystemExit('Usage: verify_four_axis_preview_native.py <preview-native.json> <source-native.json>')
    raise SystemExit(main(Path(sys.argv[1]),Path(sys.argv[2])))
