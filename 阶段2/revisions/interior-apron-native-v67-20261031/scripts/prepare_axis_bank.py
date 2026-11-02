"""Index actual pre-existing native numbered templates; never alter their numbers."""
from pathlib import Path
import json,hashlib,shutil
S=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def main():
 donor=S.parent/'阶段1/runs/quiet-singletons-20261029-111721-958785';report=donor/'four-side-singletons-native.json';source=donor/'four-side-singletons-generated.dwg';r=json.loads(report.read_text(encoding='utf-8-sig'));assert r['completed'] and r['error'] is None and r['finalModelCount']==235
 folder=S/'resources';folder.mkdir(exist_ok=True);target=folder/'native-axis-number-templates.dwg';index=folder/'native-axis-number-templates.tsv';assert not target.exists() and not index.exists();shutil.copyfile(source,target);assert sha(target)==sha(source)
 rows=['S2AXISTEMPLATES1 '+sha(target),'B '+' '.join(format(v,'.17g') for v in r['wallBodyBounds'])]
 for item in r['axisTargets']:rows.append(('V' if item['orientation']=='vertical' else 'H')+' '+item['expectedNumber']+' '+item['axisHandle'])
 index.write_text('\n'.join(rows)+'\n',encoding='ascii');(folder/'native-axis-number-templates.json').write_text(json.dumps(dict(source=str(source),sourceSHA256=sha(source),donorReport=str(report),donorReportSHA256=sha(report),library=str(target),librarySHA256=sha(target),index=str(index),indexSHA256=sha(index),scope='prebuilt native TCH number materials for this sample:1..10 and A..M skipping I;no arbitrary-number setter claim;runtime never invokes historical business commands'),ensure_ascii=False,indent=2),encoding='utf-8');print('22 native numbered templates indexed')
if __name__=='__main__':main()
