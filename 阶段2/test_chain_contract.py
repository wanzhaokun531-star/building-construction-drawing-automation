import copy
import unittest
from pathlib import Path
from chain_contract import build_contract, load_v67

class ChainContractTests(unittest.TestCase):
    def test_all_segments_become_whole_continuous_runs(self):
        a, p = load_v67()
        c = build_contract(a, p)
        self.assertEqual(len(c), 24)
        self.assertEqual(sum(len(x['segments']) for x in c), 106)
        self.assertEqual(sorted(len(x['segments']) for x in c if x['kind']=='third' and len(x['segments'])>1), [9,9,13,17])
        self.assertEqual(len([x for x in c if x['kind']=='third' and x['location']=='left']), 3)
        self.assertEqual([x for x in c if x['location']=='CDBC'][0]['values'], [700,2400,700])

    def test_wrong_baseline_rejected(self):
        a,p=load_v67();p=copy.deepcopy(p)
        p['dimensions'][0]['geometry'][0]['grip']['points'][0][1]+=800
        with self.assertRaisesRegex(ValueError,'baseline'):
            build_contract(a,p)

    def test_duplicate_source_rejected(self):
        a,p=load_v67();a=copy.deepcopy(a);a['dimensions'][1]['handle']=a['dimensions'][0]['handle']
        with self.assertRaisesRegex(ValueError,'duplicate'):
            build_contract(a,p)

if __name__=='__main__':unittest.main()
