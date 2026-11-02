"""Offline regression: execute only the watchers' closure aggregation expression.

AST isolation prevents importing or running any CAD lifecycle code. Expectations
come from the recorded explicit stop, not from the supervisor's QUIT marker.
"""
import ast
import copy
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SESSION = ROOT / '阶段1/runs/elevation-native-20261101-215047-9c4d780f'
WATCHERS = ('watch_native_case.py', 'watch_command_case.py')
RETIREMENTS = ('阶段1/工具/retire_reader_staging.py', 'tools/retire_command_staging.py')


def read(name):
    return json.loads((SESSION / name).read_text(encoding='utf-8-sig'))


def aggregate(watcher, final, requested):
    tree = ast.parse((ROOT / '立面插件/tools' / watcher).read_text(encoding='utf-8-sig'))
    calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call)
             and isinstance(node.func, ast.Attribute) and node.func.attr == 'update'
             and any(item.arg == 'normalQuit' for item in node.keywords)]
    if len(calls) != 1:
        raise AssertionError('Expected one closure aggregation expression')
    # The SHA call is outside the evidence decision; do not execute file helpers.
    fields = [item for item in calls[0].keywords if item.arg != 'supervisorFinalSHA256']
    expression = ast.Expression(ast.Dict(
        keys=[ast.Constant(item.arg) for item in fields],
        values=[item.value for item in fields]))
    return eval(compile(ast.fix_missing_locations(expression), watcher, 'eval'),
                {'__builtins__': {}}, {'final': final, 'stop_needed': requested})


def aggregate_retirement(script, final, session):
    tree = ast.parse((ROOT / '立面插件' / script).read_text(encoding='utf-8-sig'))
    calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call)
             and isinstance(node.func, ast.Name) and node.func.id == 'dict'
             and any(item.arg == 'normalQuit' for item in node.keywords)]
    if len(calls) != 1:
        raise AssertionError('Expected one retirement evidence expression')
    fields = [item for item in calls[0].keywords if item.arg in
              ('normalQuit', 'forcedTermination', 'explicitStopEvidencePresent')]
    expression = ast.Expression(ast.Dict(
        keys=[ast.Constant(item.arg) for item in fields], values=[item.value for item in fields]))
    return eval(compile(ast.fix_missing_locations(expression), script, 'eval'),
                {'__builtins__': {'any': any}}, {'final': final, 'session': session})


class ClosureEvidenceTests(unittest.TestCase):
    def test_retirement_recorded_exit_70_and_stop_are_not_normal(self):
        final = read('isolation-final.json')
        self.assertTrue(read('reader-retirement.json')['normalQuit'])
        for script in RETIREMENTS:
            with self.subTest(script=script):
                result = aggregate_retirement(script, final, SESSION)
                self.assertFalse(result['normalQuit'])
                self.assertFalse(result['forcedTermination'])
                self.assertTrue(result['explicitStopEvidencePresent'])

    def test_retirement_exit_code_and_stop_receipt_controls(self):
        no_stop = ROOT / '立面插件/tests/fixtures/closure-no-stop-control'
        self.assertFalse(any(no_stop.glob('isolation-stop-*.json')))
        for script in RETIREMENTS:
            for code, session, expected in ((0, no_stop, True), (70, no_stop, False),
                                            (None, no_stop, False), (0, SESSION, False)):
                with self.subTest(script=script, exitCode=code, hasStop=session == SESSION):
                    final = {'nativeQuitConfirmed': True, 'forcedTermination': False}
                    if code is not None:
                        final['workerExitCode'] = code
                    result = aggregate_retirement(script, final, session)
                    self.assertIs(result['normalQuit'], expected)
                    self.assertFalse(result['forcedTermination'])
                    self.assertIs(result.get('explicitStopEvidencePresent'), session == SESSION)

    def test_recorded_fallback_is_not_normal_quit(self):
        final = read('isolation-final.json')
        old = read('native-result-closure.json')
        stop = read('isolation-stop-20261101-215403-047.json')
        self.assertEqual((stop['nonce'], stop['processId']), (final['nonce'], final['processId']))
        self.assertTrue(stop['forcedTermination'])
        self.assertEqual(stop['exitCode'], 70)
        self.assertEqual(old['explicitStopHelperExitCode'], 0)
        self.assertTrue(old['normalQuit'])  # Historical false positive is retained.
        untouched = copy.deepcopy(final)
        for watcher in WATCHERS:
            with self.subTest(watcher=watcher):
                result = aggregate(watcher, final, True)
                self.assertFalse(result['normalQuit'])
                self.assertTrue(result['explicitStopRequested'])
                self.assertTrue(result['normalQuitUnprovenDueToExplicitStop'])
                self.assertFalse(result['forcedTermination'])  # Raw supervisor value.
                self.assertFalse(result['timedOut'])
                self.assertEqual(final, untouched)

    def test_no_fallback_normal_control(self):
        # Synthetic control, not a new assertion of real CAD success.
        final = {'nativeQuitConfirmed': True, 'forcedTermination': False,
                 'timedOut': False, 'workerExitCode': 0}
        for watcher in WATCHERS:
            with self.subTest(watcher=watcher):
                result = aggregate(watcher, final, False)
                self.assertTrue(result['normalQuit'])
                self.assertIs(result.get('explicitStopRequested'), False)
                self.assertIs(result.get('normalQuitUnprovenDueToExplicitStop'), False)

    def test_recorded_exit_70_without_stop_flag_is_not_normal(self):
        final = read('isolation-final.json')
        self.assertEqual(final['workerExitCode'], 70)
        for watcher in WATCHERS:
            with self.subTest(watcher=watcher):
                result = aggregate(watcher, final, False)
                self.assertFalse(result['normalQuit'])
                self.assertFalse(result['forcedTermination'])

    def test_missing_exit_code_is_not_proof_of_normal_quit(self):
        final = {'nativeQuitConfirmed': True, 'forcedTermination': False, 'timedOut': False}
        for watcher in WATCHERS:
            with self.subTest(watcher=watcher):
                self.assertFalse(aggregate(watcher, final, False)['normalQuit'])

    def test_request_alone_does_not_invent_forced_termination(self):
        final = {'nativeQuitConfirmed': True, 'forcedTermination': False,
                 'timedOut': False, 'workerExitCode': 0}
        for watcher in WATCHERS:
            with self.subTest(watcher=watcher):
                result = aggregate(watcher, final, True)
                self.assertFalse(result['normalQuit'])
                self.assertFalse(result['forcedTermination'])
                self.assertTrue(result['explicitStopRequested'])

    def test_existing_negative_evidence_stays_negative(self):
        for marker, forced in ((False, False), (True, True)):
            final = {'nativeQuitConfirmed': marker, 'forcedTermination': forced,
                     'timedOut': True, 'workerExitCode': 0}
            for watcher in WATCHERS:
                with self.subTest(watcher=watcher, marker=marker, forced=forced):
                    result = aggregate(watcher, final, False)
                    self.assertFalse(result['normalQuit'])
                    self.assertEqual(result['forcedTermination'], forced)
                    self.assertTrue(result['timedOut'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
