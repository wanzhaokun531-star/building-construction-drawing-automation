"""Append native text content/extents verbatim; semantic recognition is in C++."""
from pathlib import Path
import json,hashlib
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def main():
 original=read(S/'inputs/frozen-scheme-v1.json');fields=Path(original['nativeFields']['path']);native=Path(original['nativeReport']['path']);assert sha(fields)==original['nativeFields']['SHA256'] and sha(native)==original['nativeReport']['SHA256'];f=read(fields);r=read(native);g={x['handle']:x for x in r['originalBefore']};rows=Path(original['cache']['path']).read_text(encoding='ascii').splitlines();rows[0]=rows[0].replace('S2READCACHE1','S2READCACHE2');records=list(original['records']);count=0
 for e in f['entities']:
  if e['nativeType']!='TDbText':continue
  text=[x['rawValue'] for x in e['fields'] if x['name']=='文字内容'];assert len(text)==1;entity=g[e['handle']];assert entity['extentStatus']==0 and not entity['proxy'];values=['R',e['handle'],*entity['min'][:2],*entity['max'][:2],text[0].encode('utf-8').hex().upper()];rows.append('\t'.join(format(x,'.17g') if isinstance(x,float) else str(x) for x in values));records.append(dict(handle=e['handle'],type=e['nativeType'],sourceFields=e['fields'],sourceGeometry=entity));count+=1
 cache=S/'inputs/frozen-scheme-v2.tsv';manifest=S/'inputs/frozen-scheme-v2.json';assert not cache.exists() and not manifest.exists();cache.write_text('\n'.join(rows)+'\n',encoding='ascii');out=dict(original);out.update(contract='bcp.stage2.native-read-cache.v2',cache=dict(path=str(cache),SHA256=sha(cache)),records=records,roomLabelCount=count,semanticGeometryObjectCount=len(records),purpose='native wall/opening/text values only; role and region decisions remain in C++');manifest.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print('native records',len(records),'text labels',count)
if __name__=='__main__':main()
