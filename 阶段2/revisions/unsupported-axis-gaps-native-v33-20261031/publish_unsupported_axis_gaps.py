"""Publish one DWG only, after actual supported-interval geometry and closure checks."""
from pathlib import Path
import json,hashlib,subprocess,sys,shutil
S=Path(__file__).resolve().parent

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def require(v,m):
 if not v:raise RuntimeError(m)

# This check precedes output directory creation and copying.
subprocess.run([sys.executable,str(S/'verify_unsupported_axis_gaps.py'),'--check-only'],check=True)
n=read(S/'evidence/native-run-v33.json');g=read(S/'evidence/unsupported-axis-gaps-release-v33-final.json');c=read(S/'evidence/host-closure-v33.json')
require(g['passed'] and g['DWGSHA256']==n['DWGSHA256'],'geometry receipt mismatch')
require(all(x['snapshotUnchanged'] for x in c['inputs']) and not c['process']['ownProcessAlive'] and c['stagingRemoved'],'closure incomplete')
require(all(x['LOADCTRLS']==2 and x['action']=='read only' for x in c['sharedRegistrations']),'shared registration state differs')
f=c['final'];require(f['monitorError'] is None and not f['supervisorErrors'] and f['inputDesktopReadErrors']==0 and f['droppedEvents']==0 and f['workerUserDesktopShowEvents']==f['workerForegroundEvents']==f['workerForegroundSamples']==0,'owned host window monitoring incomplete')
for name,h in n['binding']['sourceFiles'].items():require(sha(S/'src'/name)==h,'source changed: '+name)
candidate=Path(n['session'])/'phase2-candidate'/Path(n['binding']['module']).name
require(sha(candidate)==n['binding']['moduleSHA256']==sha(S/'bin/BuildingPlan2020.arx'),'binary binding differs')
source=Path(n['output'])/'方案施工图_阶段2中间图.dwg';require(sha(source)==g['DWGSHA256'],'source DWG differs')
out=S/'交付/轴线取消跨空段_20261031';require(not out.exists(),'never overwrite prior delivery');out.mkdir(parents=True)
target=out/'方案施工图_轴线空档断开.dwg';shutil.copyfile(source,target);require(sha(target)==g['DWGSHA256'],'published DWG differs')
require(list(out.iterdir())==[target],'delivery must contain DWG only')
receipt=S/'evidence/unsupported-axis-gaps-delivery-v33.json';require(not receipt.exists(),'delivery receipt already exists')
result=dict(contract='bcp.unsupported-axis-gaps.delivery.v1',directory=str(out),DWG=str(target),DWGSHA256=sha(target),ARXSHA256=n['binding']['moduleSHA256'],gate=str(S/'evidence/unsupported-axis-gaps-release-v33-final.json'),gateSHA256=sha(S/'evidence/unsupported-axis-gaps-release-v33-final.json'),gateScriptSHA256=sha(S/'verify_unsupported_axis_gaps.py'),closureSHA256=sha(S/'evidence/host-closure-v33.json'),userDeliveryFiles=[target.name],PDFDelivered=False,constructionDrawingComplete=False)
receipt.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
