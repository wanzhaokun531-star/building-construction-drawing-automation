"""Validate read-only runtime discovery; never infer symbol business success."""
import json
from pathlib import Path
import sys


def inspect(data):
    results = []
    def check(ok, name):
        results.append((bool(ok), name))
    check(data.get('contract') == 'stage1.symbol-discovery-native.v1', 'actual native discovery contract')
    check(data.get('completed') is True and data.get('error') is None and data.get('callbackError') is None, 'native discovery completed without hidden failure')
    check(data.get('fieldSettersAttempted') is False and data.get('businessValuesProven') is False, 'discovery is read-only and not business proof')
    expected=215 if data.get('command')=='TObjEdit' else 213
    check(data.get('initialCount') == data.get('finalCount') == expected, 'explicit source membership count')
    check(len(data.get('originalBefore', [])) == expected and data.get('originalBefore') == data.get('originalAfter'), 'actual source public states unchanged')
    check(isinstance(data.get('dbmodBefore'), int) and data['dbmodBefore'] >= 0 and data['dbmodBefore'] == data.get('dbmodAfter'), 'actual drawing-modified state unchanged')
    check(data.get('timerStopped') is True and data.get('observerRemoved') is True, 'native callbacks detached')
    check(str(data.get('desktop', '')).startswith('CodexS1_') and data.get('processId', 0) > 0 and data.get('threadId', 0) > 0, 'private native UI identity')
    snapshots = data.get('snapshots', [])
    check(bool(snapshots), 'actual panel snapshots, not documentation')
    for index, capture in enumerate(snapshots):
        panel = capture.get('panel', {})
        check(panel.get('processId') == data.get('processId') and panel.get('threadId') == data.get('threadId') and panel.get('desktop') == data.get('desktop'), f'panel{index} actual owned thread/desktop')
        check(panel.get('enumerationComplete') is True and bool(panel.get('controls')), f'panel{index} actual complete controls')
        check(all(c.get('processId') == data.get('processId') for c in panel.get('controls', [])), f'panel{index} no foreign controls')
    commands = data.get('commandTrace', [])
    name = str(data.get('command', '')).upper()
    events = [x.get('event') for x in commands if str(x.get('command', '')).upper() == name]
    check(events.count('commandWillStart') == 1 and events.count('commandEnded') + events.count('commandCancelled') == 1 and 'commandFailed' not in events, 'one actual public command with a terminal event')
    return results


if __name__ == '__main__':
    data = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
    checks = inspect(data)
    for ok, name in checks:
        if not ok:
            print('FAIL:', name)
    failed = sum(not ok for ok, _ in checks)
    print(f'assertions={len(checks)} passed={len(checks)-failed} failed={failed} result={"FAIL" if failed else "PASS_READONLY_DISCOVERY_ONLY"}')
    raise SystemExit(bool(failed))
