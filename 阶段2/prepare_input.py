"""Transcribe previously verified native values; no architectural decisions here."""
from pathlib import Path
import json,hashlib,math
S=Path(__file__).resolve().parent;ROOT=S.parent
RUN=ROOT/'阶段1/runs/quiet-schemecatalognative-20261030-093625-625530'
SHA='5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def values(entity,name):
 v=[x['rawValue'] for x in entity['fields'] if x['name']==name]
 if len(v)!=1:raise ValueError(f'{entity["handle"]}: absent/ambiguous source field {name}')
 return v[0]
def num(entity,name):
 v=float(values(entity,name))
 if not math.isfinite(v):raise ValueError('nonfinite native value')
 return v
def main():
 source=ROOT/'阶段0/输入快照/方案平面图.dwg';native=RUN/'native-semantic-catalog.json';fields=RUN/'native-catalog-fields.json';receipt=RUN/'quiet-job-receipt.json'
 assert sha(source)==SHA;f=read(fields);r=read(native);job=read(receipt)
 assert job['schemeSHA256']==SHA and job['error'] is None and job['verificationExitCode']==0
 assert f['nativeReportSHA256']==sha(native) and f['logSHA256']==sha(Path(f['log']))
 assert r['error'] is None and r['originalBefore']==r['originalAfter'] and r['initialModelCount']==213
 geometry={x['handle']:x for x in r['originalBefore']};assert len(geometry)==213
 lines=[f'S2READCACHE1 {SHA} 213 mm'];records=[]
 for e in f['entities']:
  g=geometry[e['handle']];assert e['nativeType']==g['type'] and not g['proxy'];kind=e['nativeType']
  if kind=='TDbWall':
   c=g['curve'];assert c['startStatus']==c['endStatus']==0
   fields_out=['W',e['handle'],*c['start'][:2],*c['end'][:2],num(e,'左厚度'),num(e,'右厚度'),'U']
  elif kind in ['TDbOpening','TDbGroupOpening']:
   pts=g['grip']['points'];assert g['grip']['status']==0 and len(pts)>=3
   observed={'门':'D','窗':'W','组合门':'G'}.get(values(e,'门窗类型'))
   assert observed is not None
   fields_out=['O',e['handle'],*pts[1][:2],*pts[2][:2],num(e,'宽度'),num(e,'高度'),observed,'-']
  else:continue
  lines.append('\t'.join(format(x,'.17g') if isinstance(x,float) else str(x) for x in fields_out));records.append(dict(handle=e['handle'],type=kind,sourceFields=e['fields'],sourceGeometry=g))
 folder=S/'inputs';folder.mkdir(exist_ok=True);cache=folder/'frozen-scheme-v1.tsv';manifest=folder/'frozen-scheme-v1.json'
 assert not cache.exists() and not manifest.exists();cache.write_text('\n'.join(lines)+'\n',encoding='ascii')
 manifest.write_text(json.dumps(dict(contract='bcp.stage2.native-read-cache.v1',source=dict(path=str(source),SHA256=SHA),cache=dict(path=str(cache),SHA256=sha(cache)),nativeReport=dict(path=str(native),SHA256=sha(native)),nativeFields=dict(path=str(fields),SHA256=sha(fields)),nativeReceipt=dict(path=str(receipt),SHA256=sha(receipt)),log=dict(path=f['log'],SHA256=f['logSHA256']),records=records,sourceObjectCount=213,units='mm',purpose='transcribed source values; wall use and subtype not inferred'),ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps(dict(cache=str(cache),records=len(records),SHA256=sha(cache)),ensure_ascii=False))
if __name__=='__main__':main()
