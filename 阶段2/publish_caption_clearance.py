"""Publish only the saved and independently verified door-number correction."""
from pathlib import Path
import json,shutil,datetime
from chain_contract import S,read,sha
from verify_whole_chains import check
from verify_door_caption_clearance import verify_new
from release_host_checks import validate_host_records
VERSION='v79'
REQUIRED_REVIEW_FILES={'src/NativeDoorCaptionClearance.inc','src/Stage2Arx.cpp','verify_door_caption_clearance.py','prepare_caption_host.py','collect_caption_run.py','preview_caption_clearance.py','publish_caption_clearance.py','release_host_checks.py','close_host.py','test_caption_clearance.py','test_caption_output_binding.py','test_whole_chain_publisher.py','publish_whole_chains.py','../阶段1/原生接口探测/src/NativeStage2Bridge.inc','../阶段1/原生接口探测/启动隔离桌面宿主.ps1','../阶段1/原生接口探测/IsolatedCadHostNative.cs'}
def validate_review_files(review):
    check(REQUIRED_REVIEW_FILES.issubset(review['reviewedFiles']),'mandatory review coverage missing')
    for name,wanted in review['reviewedFiles'].items():check(sha(S/name)==wanted,'reviewed implementation/gate changed: '+name)
def validate_output_binding(binding,run,preview,qa,review,report):
    job=Path(binding['job']).resolve();source=job/'方案施工图_门编号避让.dwg'
    check(Path(run['job']).resolve()==job and Path(run['DWG']).resolve()==source,'run points outside exact native caption output')
    check(Path(preview['source']).resolve()==source,'preview belongs to another DWG')
    control=Path(preview['job'])/'control.dwg';native_preview=Path(preview['job'])/'stage2-preview-native.json'
    actual=sha(source);check(actual==run['DWGSHA256']==qa['DWGSHA256']==review['DWGSHA256']==report['outputSHA256']==preview['sourceSHA256']==sha(control),'native output/preview-control DWG fingerprints differ')
    check(Path(read(native_preview)['controlDWG']).resolve()==control.resolve(),'actual preview control path differs')
def main():
    b=read(S/f'evidence/native-host-binding-{VERSION}.json');r=read(S/f'evidence/native-caption-run-{VERSION}.json');pr=read(S/f'evidence/caption-preview-{VERSION}.json');c=read(S/f'evidence/host-closure-{VERSION}.json');qa=read(S/f'evidence/caption-release-{VERSION}.json');review=read(S/f'evidence/caption-independent-review-{VERSION}.json');session=Path(b['session']);job=Path(r['job'])
    check(r['error'] is None and pr['error'] is None,'native caption/preview failed')
    check(b['nonce']==r['nonce']==pr['nonce']==c['final']['nonce']==qa['nonce'],'caption session nonce differs')
    check(r['session']==pr['session']==c['session']==b['session'] and r['processId']==pr['processId']==c['final']['processId'],'caption session/process differs')
    validate_host_records(b,r,c,read(session/'worker-config.json'),read(session/'isolation-host-receipt.json'),read(Path(c['finalPath'])))
    check(c['process']['ownProcessAlive'] is False and c['stagingRemoved'] and len(c['files'])==4 and all(x['removed'] for x in c['files']),'owned modules not cleaned')
    check(len(c['inputs'])==6 and all(x['snapshotUnchanged'] for x in c['inputs']) and all(x['LOADCTRLS']==2 and x['action']=='read only' for x in c['sharedRegistrations']),'frozen inputs/shared registration changed')
    check(review['passed'] and qa['visualReviewed'],'caption review incomplete');validate_review_files(review)
    for name,wanted in b['sourceFiles'].items():check(sha(S/'src'/name)==wanted,'candidate source changed: '+name)
    check(sha(S/'bin/BuildingPlan2020.arx')==b['moduleSHA256'],'candidate binary changed')
    report=job/'door-caption-clearance.json';preview=Path(pr['job'])/'stage2-preview-native.json';source=Path(r['DWG'])
    validate_output_binding(b,r,pr,qa,review,read(report))
    check(sha(report)==r['reportSHA256']==qa['reportSHA256'],'native report changed')
    check(sha(preview)==pr['nativeReportSHA256']==qa['previewReportSHA256'],'preview changed')
    check(sha(source)==r['DWGSHA256']==qa['DWGSHA256']==review['DWGSHA256'],'caption DWG changed')
    check(sha(b['input'])==b['inputSHA256']=='4CAEC8BAABCAA0CCFB04594A3A01796FE68BBA5CE85ED8A389699AD9A7C8D6DF','v74 input changed')
    check(sha(b['captionContract'])==b['captionContractSHA256']==read(report)['contractSHA256'],'door target contract changed')
    audit=read(Path(pr['job'])/'native-mask-document-audit.json');check(audit['error'] is None and audit['masks']==4 and audit['frame70']==audit['WIPEOUTFRAME']==audit['publicVisibleFrames']==0,'saved mask frame changed')
    result=verify_new(read(report),read(preview));destination=S/'交付/门编号避让_20261031';check(not destination.exists(),'never overwrite prior delivery');destination.mkdir();target=destination/'方案施工图_门编号避让.dwg';shutil.copyfile(source,target);check(sha(target)==r['DWGSHA256'],'DWG copy changed');check(list(destination.iterdir())==[target],'DWG only')
    receipt=dict(contract='bcp.door-caption-delivery.v1',recorded=datetime.datetime.now().astimezone().isoformat(),version=VERSION,file=str(target),SHA256=sha(target),verification=result,reviewSHA256=sha(S/f'evidence/caption-independent-review-{VERSION}.json'),closureSHA256=sha(S/f'evidence/host-closure-{VERSION}.json'),publisherSHA256=sha(Path(__file__)),constructionDrawingComplete=False,PDFDelivered=False)
    out=S/f'evidence/caption-delivery-{VERSION}.json';check(not out.exists(),'delivery receipt exists');out.write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(receipt,ensure_ascii=False))
if __name__=='__main__':main()
