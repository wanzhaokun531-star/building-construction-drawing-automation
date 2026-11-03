from pathlib import Path
import json,hashlib,sys
P=Path(__file__).resolve().parent;APP=P.parent;SOURCE=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else APP/'build/input-adaptation-reviewed-candidate/BuildingPlan2020.bundle';B=P/'build';B.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
manifest=json.loads((SOURCE/'SHA256.json').read_text(encoding='utf-8'));assert all(sha(SOURCE/k)==v for k,v in manifest.items())
files=sorted(p for p in SOURCE.rglob('*') if p.is_file());rows=[];rc=['#include <windows.h>'];header=['#pragma once','struct PayloadFile {int id;const wchar_t* name;const char* sha;};','const PayloadFile payload[]={']
for i,f in enumerate(files,101):
 name=str(f.relative_to(SOURCE));digest=sha(f);header.append('{%d,L%s,"%s"},'%(i,json.dumps(name,ensure_ascii=False),digest));rc.append('%d RCDATA %s'%(i,json.dumps(str(f.resolve()),ensure_ascii=False)));rows.append(dict(id=i,relative=name,SHA256=digest))
header.append('};');previous=json.loads((Path(sys.argv[2]).resolve() if len(sys.argv)>2 else P/'previous-version-0.1.1.json').read_text(encoding='utf-8'));header.append('const PayloadFile previousPayload[]={');header.extend('{0,L%s,"%s"},'%(json.dumps(f['relative'],ensure_ascii=False),f['SHA256']) for f in previous);header.append('};');(B/'Payload.h').write_text('\n'.join(header),encoding='utf-8');(B/'Payload.rc').write_text('\n'.join(rc),encoding='utf-16');(B/'payload.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8');print('embedded files',len(rows))
