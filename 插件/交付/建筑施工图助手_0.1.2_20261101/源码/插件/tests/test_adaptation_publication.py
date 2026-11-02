import unittest,importlib.util,sys,copy
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'插件/tools'));sys.path.insert(0,str(ROOT/'阶段2'))
import publish_input_adaptation as g
import release_host_checks as common
class PublicationTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binding=g.read(ROOT/'阶段2/evidence/native-host-binding-v115.json');cls.bundle=ROOT/'插件/build/input-adaptation-final-candidate/BuildingPlan2020.bundle';cls.payload=g.read(ROOT/'插件/evidence/installer-input-adaptation-qa.json')['payload'];cls.closure=g.read(ROOT/'阶段2/evidence/host-closure-v115.json')
 def test_verified_resources_and_host(self):g.validate_runtime_resources(self.binding,self.bundle,self.payload);g.validate_host_evidence(self.binding,self.closure)
 def test_self_consistent_wrong_resource_is_rejected(self):
  manifest=g.read(self.bundle/'SHA256.json');manifest['Contents/Win64/resources/scheme.tsv']='0'*64;oldRead=g.read;oldSHA=g.sha
  def changed_read(p):return manifest if Path(p)==self.bundle/'SHA256.json' else oldRead(p)
  def changed_sha(p):return '0'*64 if Path(p)==self.bundle/'Contents/Win64/resources/scheme.tsv' else oldSHA(p)
  with mock.patch.object(g,'read',side_effect=changed_read),mock.patch.object(g,'sha',side_effect=changed_sha):
   with self.assertRaises(ValueError):g.validate_runtime_resources(self.binding,self.bundle,self.payload)
 def test_different_installer_payload_is_rejected(self):
  payload=copy.deepcopy(self.payload);payload[0]['SHA256']='0'*64
  with self.assertRaises(ValueError):g.validate_runtime_resources(self.binding,self.bundle,payload)
 def test_incomplete_monitoring_is_rejected(self):
  for change in [dict(droppedEvents=1),dict(monitorStarted=False),dict(monitorStopped=False),dict(timedOut=True),dict(nonce='0'*32),dict(processId=1)]:
   with self.subTest(change=change):
    closure=copy.deepcopy(self.closure);final=closure['final'];final.update(change);original=common.read
    def changed_read(p):return final if Path(p)==Path(closure['finalPath']) else original(p)
    with mock.patch.object(common,'read',side_effect=changed_read):
     with self.assertRaises(ValueError):g.validate_host_evidence(self.binding,closure)
if __name__=='__main__':unittest.main()
