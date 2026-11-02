import unittest,tempfile,subprocess,json,hashlib,shutil
from pathlib import Path
P=Path(__file__).resolve().parents[1];EXE=P/'bin/BuildingPlanSetup.exe';SOURCE=P.parents[0]/'build/input-adaptation-reviewed-candidate/BuildingPlan2020.bundle'
class SetupTests(unittest.TestCase):
 def test_built_native_installer_exists(self):self.assertTrue(EXE.is_file(),'missing executable installer')
 def run_setup(self,root,expected):
  run=subprocess.run([str(EXE),'--test-install',str(root)],timeout=30);self.assertEqual(run.returncode,expected);return json.loads((root/'test-result.json').read_text(encoding='utf-8'))
 def test_install_and_repeat_preserve_payload(self):
  with tempfile.TemporaryDirectory(prefix='setup-',dir=P/'tests') as temp:
   root=Path(temp)/'中文 测试';root.mkdir();(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');self.assertFalse(self.run_setup(root,0)['alreadyInstalled']);target=root/'BuildingPlan2020.bundle'
   for src in SOURCE.rglob('*'):
    if src.is_file():self.assertEqual(src.read_bytes(),(target/src.relative_to(SOURCE)).read_bytes())
   (target/'用户文件.txt').write_text('keep');self.assertTrue(self.run_setup(root,0)['alreadyInstalled']);self.assertEqual((target/'用户文件.txt').read_text(),'keep')
 def test_verified_old_version_is_backed_up_before_upgrade(self):
  with tempfile.TemporaryDirectory(prefix='upgrade-',dir=P/'tests') as temp:
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');target=root/'BuildingPlan2020.bundle';shutil.copytree(P.parents[0]/'交付/建筑施工图助手_0.1.1_20261101/BuildingPlan2020.bundle',target);self.run_setup(root,0);backups=list(root.glob('BuildingPlan2020.bundle.backup-*'));self.assertEqual(len(backups),1);self.assertIn('AppVersion="0.1.1"',(backups[0]/'PackageContents.xml').read_text(encoding='utf-8'));self.assertIn('AppVersion="0.1.2"',(target/'PackageContents.xml').read_text(encoding='utf-8'))
 def test_failed_upgrade_restores_original(self):
  with tempfile.TemporaryDirectory(prefix='rollback-',dir=P/'tests') as temp:
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');old=P.parents[0]/'交付/建筑施工图助手_0.1.1_20261101/BuildingPlan2020.bundle';target=root/'BuildingPlan2020.bundle';shutil.copytree(old,target);run=subprocess.run([str(EXE),'--test-install-fail-publish',str(root)],timeout=30);self.assertEqual(run.returncode,2)
   for src in old.rglob('*'):
    if src.is_file():self.assertEqual(src.read_bytes(),(target/src.relative_to(old)).read_bytes())
   self.assertEqual(list(root.glob('BuildingPlan2020.bundle.backup-*')),[]);self.assertEqual(list(root.glob('.BCP-stage-*')),[])
 def test_different_existing_install_not_overwritten(self):
  with tempfile.TemporaryDirectory(prefix='setup-',dir=P/'tests') as temp:
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');target=root/'BuildingPlan2020.bundle';target.mkdir();(target/'PackageContents.xml').write_text('user data');r=self.run_setup(root,2);self.assertFalse(r['ok']);self.assertEqual((target/'PackageContents.xml').read_text(),'user data');self.assertEqual(list(root.glob('.BCP-stage-*')),[])
 def test_test_mode_requires_owned_marker(self):
  with tempfile.TemporaryDirectory(prefix='setup-',dir=P/'tests') as temp:
   root=Path(temp);r=subprocess.run([str(EXE),'--test-install',str(root)],timeout=30);self.assertEqual(r.returncode,3);self.assertEqual(list(root.iterdir()),[])
if __name__=='__main__':unittest.main()
