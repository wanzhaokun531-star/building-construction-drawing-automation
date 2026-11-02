import importlib.util, tempfile, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('bcp_package',ROOT/'build_package.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class PackageTests(unittest.TestCase):
    def test_resources_are_portable_and_have_no_finished_plan(self):
        with tempfile.TemporaryDirectory() as temp:
            result=module.stage_resources(Path(temp))
            self.assertEqual(len(result),8)
            self.assertEqual({p.name for p in Path(temp).rglob('*.dwg')},{'reference.dwg','axes.dwg','layers.dwg'})
            for row in result:
                self.assertFalse(Path(row['relative']).is_absolute())
                self.assertEqual(len(row['SHA256']),64)
            self.assertNotIn('drawApron=1',(Path(temp)/'profile.ini').read_text(encoding='utf-16'))
    def test_resource_change_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            result=module.stage_resources(Path(temp));(Path(temp)/result[0]['relative']).write_bytes(b'broken')
            with self.assertRaises(ValueError):module.verify_resources(Path(temp))

if __name__=='__main__':unittest.main()
