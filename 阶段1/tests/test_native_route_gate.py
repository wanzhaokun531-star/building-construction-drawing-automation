import importlib.util,unittest
from types import SimpleNamespace
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'原生接口探测/run_quiet_job.py';s=importlib.util.spec_from_file_location('quiet_native_gate',p);module=importlib.util.module_from_spec(s);s.loader.exec_module(module)
class NativeRouteGate(unittest.TestCase):
 def test_known_native_operations_allowed(self):
  for name in ['PlainTextReplacementNative','PublicContractAuditNative','AutoCadSupportedHost','SchemeNative','MTextNative','Clean','Exit']:self.assertTrue(module.native_only_mode_allowed(name),name)
 def test_tch_command_or_panel_operations_rejected(self):
  for name in ['NumberA','Singletons','ElevationRoundTrip','ElevationDialog','Slope','Section','MergeTwo']:self.assertFalse(module.native_only_mode_allowed(name),name)
 def test_heap_failure_symbol_path_cannot_be_retried(self):
  for name in ['NativeElevationPatch','NativeSectionPatch']:self.assertFalse(module.native_only_mode_allowed(name),name)
 def test_unknown_name_rejected(self):self.assertFalse(module.native_only_mode_allowed('S1_UNVERIFIED'))
 def test_dispatch_rejected_before_session_access(self):
  with self.assertRaisesRegex(RuntimeError,'native operations only'):module.main(SimpleNamespace(mode='Slope'))
if __name__=='__main__':unittest.main()
