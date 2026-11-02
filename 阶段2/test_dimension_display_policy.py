import unittest
from dimension_display_policy import displayed_number
class DisplayTests(unittest.TestCase):
 def test_numeric(self):self.assertEqual(displayed_number('1100'),1100)
 def test_single_prefix(self):self.assertEqual(displayed_number(r'\A1;1100'),1100)
 def test_doubled_prefix(self):self.assertEqual(displayed_number(r'\\A1;1100'),1100)
 def test_reject_extra_number(self):
  with self.assertRaises(ValueError):displayed_number(r'\A1;1100 50')
 def test_reject_unknown_format(self):
  for s in [r'\H2;1100','<>','1100abc',r'\A2;1100']:
   with self.assertRaises(ValueError):displayed_number(s)
if __name__=='__main__':unittest.main()
