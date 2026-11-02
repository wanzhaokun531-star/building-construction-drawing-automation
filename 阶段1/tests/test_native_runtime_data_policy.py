import unittest,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'原生接口探测'))
from native_runtime_data_policy import data_names_allowed,add_data_support_path
class NativeRuntimeDataPolicy(unittest.TestCase):
 def test_known_missing_data_allowed(self):self.assertTrue(data_names_allowed(['linetype.dbf']))
 def test_startup_executable_never_allowed(self):
  for n in ['acaddoc.lsp','acad.rx','test.arx','test.dll','form.dcl','font.shx','../linetype.dbf']:self.assertFalse(data_names_allowed(['linetype.dbf',n]),n)
 def test_original_support_order_and_spelling_preserved(self):self.assertEqual('D:\\A;C:\\Data;',add_data_support_path('D:\\A;','C:\\Data'))
 def test_no_duplicate_support_entry(self):self.assertEqual('D:\\A;C:\\DATA;',add_data_support_path('D:\\A;C:\\DATA;','c:\\data'))
if __name__=='__main__':unittest.main()
