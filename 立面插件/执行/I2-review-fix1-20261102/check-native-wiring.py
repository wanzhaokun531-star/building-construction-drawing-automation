from pathlib import Path
s=(Path(__file__).resolve().parents[2]/"tests/native/unitconfig/UnitConfigDriver.cpp").read_text(encoding="utf-8-sig")
assert s.count("recordPrinted(*s.evidence,")==3
for name in ("ConfigInvalidatesOrdinaryIssuePages", "SwitchInvalidatesOrdinaryIssuePages", "InfoDisclosesUnitOnlyScope"):
    assert 'expect("'+name+'",recordPrinted(*s.evidence,' in s, name
assert 'result.status, RTNORM, result.readOnly, result.restored, expectedText' in s
assert 'return accepted;' in s
for field in ("status", "successStatus", "readOnly", "restored", "expectedText", "accepted"):
    assert '{"'+field+'",' in s, field
print("PASS: all 3 native assertions consume the tested acceptance result; all 6 observation fields recorded")
