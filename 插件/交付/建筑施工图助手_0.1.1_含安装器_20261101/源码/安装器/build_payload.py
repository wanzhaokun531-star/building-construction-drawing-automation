from pathlib import Path
import json,hashlib
P=Path(__file__).resolve().parent;APP=P.parent;SOURCE=APP/'交付/建筑施工图助手_0.1.1_20261101/BuildingPlan2020.bundle';B=P/'build';B.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
manifest=json.loads((SOURCE/'SHA256.json').read_text(encoding='utf-8'));assert all(sha(SOURCE/k)==v for k,v in manifest.items())
files=sorted(p for p in SOURCE.rglob('*') if p.is_file());rows=[];rc=['#include <windows.h>'];header=['#pragma once','struct PayloadFile {int id;const wchar_t* name;const char* sha;};','const PayloadFile payload[]={']
for i,f in enumerate(files,101):
 name=str(f.relative_to(SOURCE));digest=sha(f);header.append('{%d,L%s,"%s"},'%(i,json.dumps(name,ensure_ascii=False),digest));rc.append('%d RCDATA %s'%(i,json.dumps(str(f.resolve()),ensure_ascii=False)));rows.append(dict(id=i,relative=name,SHA256=digest))
header.append('};');(B/'Payload.h').write_text('\n'.join(header),encoding='utf-8');(B/'Payload.rc').write_text('\n'.join(rc),encoding='utf-16');(B/'payload.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8');print('embedded files',len(rows))
