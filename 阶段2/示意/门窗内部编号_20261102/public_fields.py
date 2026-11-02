"""Parse public SDK LIST output. Never decodes or writes private object data."""
import re,json,math
from pathlib import Path
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def fields_in_job(job):
 result={}
 for p in (Path(job)/'public-properties').glob('*.log'):
  raw=p.read_bytes();s=raw.decode('utf-16' if raw[:2] in [b'\xff\xfe',b'\xfe\xff'] else 'gb18030',errors='strict')
  for token,h,body in re.findall(r'<BCP_BEGIN_(\d+)_([A-F0-9]+)>(.*?)<BCP_END_\1_\2>',s,re.S):
   values={}
   for line in body.splitlines():
    if ':' not in line:continue
    k,v=line.split(':',1);k=k.strip();v=v.strip()
    if k in values:assert values[k]==v,('conflicting public LIST fields',h,k)
    values[k]=v
   assert h not in result and values.get('对象句柄')==h
   result[h]=dict(fields=values,log=str(p))
 assert result
 return result
def validate_number_targets(manifest,reads):
 result=[]
 for f in manifest['floors']:
  values=fields_in_job(reads[f['floor']]['job']);rows=[]
  for t in f['targets']:
   fields=values[t['opening']]['fields'];assert fields['门窗编号']==t['number'],('stored number mismatch',t,fields)
   for k in ['宽度','高度','离地高度','转角']:assert math.isfinite(float(fields[k]))
   rows.append(dict(**t,width=float(fields['宽度']),height=float(fields['高度']),elevation=float(fields['离地高度']),angle=float(fields['转角']),actualInternalNumber=fields['门窗编号']))
  result.append(dict(floor=f['floor'],targets=rows,allCurrentOpeningNumbers={h:v['fields']['门窗编号'] for h,v in values.items() if v['fields'].get('DXF名称') in ['TCH_OPENING','TCH_GROUP_OPENING']},allFields=values))
 return result
