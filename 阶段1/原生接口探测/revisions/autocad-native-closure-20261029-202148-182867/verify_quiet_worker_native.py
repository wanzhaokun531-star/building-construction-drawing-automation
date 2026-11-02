"""A private worker must prove desktop identity, native TCH and actual file dispatch."""
import json
import sys
from pathlib import Path

def main(folder):
    failures=[];count=0
    def check(ok,label):
        nonlocal count
        count+=1
        if not ok:failures.append(label)
    ready=folder/"quiet-worker-ready.json"
    if not ready.is_file():
        print("FAIL: isolated native worker startup evidence missing")
        print("assertions=1 passed=0 failed=1 result=FAIL")
        return 1
    d=json.loads(ready.read_text(encoding="utf-8-sig"));nonce=d.get("nonce","")
    check(d.get("contract")=="stage1.quiet-worker-ready.v1" and d.get("error") is None,"native ready contract")
    check(len(nonce)==32 and all(c in "0123456789abcdef" for c in nonce.lower()),"explicit session nonce")
    check(d.get("desktop")=="CodexS1_"+nonce and d.get("inputDesktop")!=d.get("desktop"),"private non-input desktop")
    check(d.get("timerStarted") is True and d.get("comUsed") is False,"native timer/file queue, no COM")
    ping=folder/"quiet-ping.json"
    if ping.is_file():
        p=json.loads(ping.read_text(encoding="utf-8-sig"));check(p.get("nonce")==nonce and p.get("processId")==d.get("processId"),"same owned native host")
        check(p.get("desktop")==d.get("desktop") and p.get("inputDesktop")!=p.get("desktop"),"still isolated")
        check(p.get("readStatus")==p.get("closeInputStatus")==0 and p.get("modelCount")==213 and p.get("nativeWall") is True,"native Tianzheng frozen scheme read")
    else:check(False,"native ping dispatch evidence missing")
    for e in failures:print("FAIL:",e)
    print(f"assertions={count} passed={count-len(failures)} failed={len(failures)} result={'FAIL' if failures else 'PASS'}")
    print("Window noninterference also requires supervisor WinEvent logs; ready/Ping alone do not prove it.")
    return int(bool(failures))
if __name__=="__main__":raise SystemExit(main(Path(sys.argv[1])))
