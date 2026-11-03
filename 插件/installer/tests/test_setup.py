import unittest,tempfile,subprocess,json,hashlib,shutil,os
from pathlib import Path
P=Path(__file__).resolve().parents[1];EXE=Path(os.environ.get('BCP_TEST_SETUP_EXE',str(P/'bin/BuildingPlanSetup.exe')));SOURCE=Path(os.environ.get('BCP_TEST_SETUP_BUNDLE',str(P.parents[0]/'build/input-adaptation-reviewed-candidate/BuildingPlan2020.bundle')));OLD_VERSION=os.environ.get('BCP_TEST_SETUP_OLD_VERSION','0.1.1');NEW_VERSION=os.environ.get('BCP_TEST_SETUP_NEW_VERSION','0.1.2');PREVIOUS=Path(os.environ.get('BCP_TEST_SETUP_PREVIOUS',str(P.parents[0]/'交付/建筑施工图助手_0.1.1_20261101/BuildingPlan2020.bundle')))
class SetupTests(unittest.TestCase):
 def legacy_011(self):return P.parents[0]/'交付/建筑施工图助手_0.1.1_20261101/BuildingPlan2020.bundle'
 def snapshot(self,root):return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob('*') if p.is_file()}
 def test_011_can_upgrade_directly_with_complete_backup(self):
  with tempfile.TemporaryDirectory(prefix='upgrade-011-',dir=P/'tests') as temp:
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');target=root/'BuildingPlan2020.bundle';shutil.copytree(self.legacy_011(),target);before=self.snapshot(target)
   self.run_setup(root,0);backups=list(root.glob('BuildingPlan2020.bundle.backup-*'));self.assertEqual(len(backups),1);self.assertEqual(self.snapshot(backups[0]),before);self.assertEqual(self.snapshot(target),self.snapshot(SOURCE))
 def test_failed_011_upgrade_restores_every_file(self):
  with tempfile.TemporaryDirectory(prefix='rollback-011-',dir=P/'tests') as temp:
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');target=root/'BuildingPlan2020.bundle';shutil.copytree(self.legacy_011(),target);before=self.snapshot(target)
   run=subprocess.run([str(EXE),'--test-install-fail-publish',str(root)],timeout=30);self.assertEqual(run.returncode,2);self.assertIn('publication failure',json.loads((root/'test-result.json').read_text('utf-8'))['error']);self.assertEqual(self.snapshot(target),before);self.assertEqual(list(root.glob('BuildingPlan2020.bundle.backup-*')),[]);self.assertEqual(list(root.glob('.BCP-stage-*')),[])
 def test_modified_or_extra_011_files_are_preserved(self):
  for change in ['modified','extra']:
   with self.subTest(change=change), tempfile.TemporaryDirectory(prefix='custom-011-',dir=P/'tests') as temp:
    root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');target=root/'BuildingPlan2020.bundle';shutil.copytree(self.legacy_011(),target)
    if change=='modified':(target/'Contents/Win64/BuildingPlanPlugin2020.ini').write_text('custom settings')
    else:(target/'用户自定义.txt').write_text('keep')
    before=self.snapshot(target);self.run_setup(root,2);self.assertEqual(self.snapshot(target),before);self.assertEqual(list(root.glob('BuildingPlan2020.bundle.backup-*')),[])
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
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');target=root/'BuildingPlan2020.bundle';shutil.copytree(PREVIOUS,target);self.run_setup(root,0);backups=list(root.glob('BuildingPlan2020.bundle.backup-*'));self.assertEqual(len(backups),1);self.assertIn('AppVersion="'+OLD_VERSION+'"',(backups[0]/'PackageContents.xml').read_text(encoding='utf-8'));self.assertIn('AppVersion="'+NEW_VERSION+'"',(target/'PackageContents.xml').read_text(encoding='utf-8'))
 def test_failed_upgrade_restores_original(self):
  with tempfile.TemporaryDirectory(prefix='rollback-',dir=P/'tests') as temp:
   root=Path(temp);(root/'.bcp-setup-test').write_text('BCPSETUPTEST1');old=PREVIOUS;target=root/'BuildingPlan2020.bundle';shutil.copytree(old,target);run=subprocess.run([str(EXE),'--test-install-fail-publish',str(root)],timeout=30);self.assertEqual(run.returncode,2)
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
