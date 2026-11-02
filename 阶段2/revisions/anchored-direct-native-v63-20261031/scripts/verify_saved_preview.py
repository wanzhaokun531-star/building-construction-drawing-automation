from pathlib import Path
import json,hashlib,datetime
from dimension_display_policy import displayed_number
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
receipt=read(S/'evidence/native-preview-run-v7.json');b=read(S/'evidence/native-host-binding-v7.json');job=Path(receipt['job']);report=job/'stage2-preview-native.json';r=read(report)
assert sha(Path(receipt['source']))==receipt['sourceSHA256']==b['previewSourceSHA256']==sha(job/'control.dwg') and sha(job/'annotation-index.tsv')==b['previewIndexSHA256']
assert r['error'] is None and r['initialModelCount']==r['finalModelCount']==269 and r['originalBefore']==r['originalAfter'] and r['globalsBefore']==r['globalsAfter'] and r['layoutBefore']==r['layoutAfter']
assert len(r['dimensions'])==36
for d in r['dimensions']:assert d['override'] in ['', '<>'] and abs(displayed_number(d['formatted'])-d['expected'])<1e-6 and abs(d['measurement']-d['expected'])<1e-6,d
page=r['page'];assert page['error'] is None and page['pipelineComplete'] is True and page['engineDestroyed'] is True and page['fileObserved'] is True and page['processPlotStateAfter']==0
settings=page['validatedSettings'];assert settings['paperMM']==[420,297] and settings['scale']==[1,100] and settings['useStandardScale'] is False
pdf=Path(page['outputPDF']);assert pdf.stat().st_size==page['fileBytes'] and sha(job/'control.dwg')==receipt['sourceSHA256']
result=dict(contract='bcp.stage2.saved-preview-verification.v1',recorded=datetime.datetime.now().astimezone().isoformat(),source=str(receipt['source']),sourceSHA256=receipt['sourceSHA256'],job=str(job),nativeReportSHA256=sha(report),PDF=str(pdf),PDFSHA256=sha(pdf),formattedDimensionsVerified=36,all269PublicGeometrySnapshotsEqual=True,plotSettingsRestored=True,strictA3OneTo100=True,priorValidationFailureRetained=str(S/'evidence/native-preview-run-v7.json'),correction='only observed one/two backslash A1 alignment prefix removed; rejects other content',formalFrameComplete=False,visualReview='pending',stage2Complete=False)
out=S/'evidence/native-preview-v7-verified.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(PDF=str(pdf),dimensionsVerified=36,A3=True,scale='1:100'),ensure_ascii=False))
