import copy,unittest
from pathlib import Path
from chain_contract import S,read
from publish_caption_clearance import validate_output_binding

class CaptionOutputBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        b=read(S/'evidence/native-host-binding-v79.json');r=read(S/'evidence/native-caption-run-v79.json');p=read(S/'evidence/caption-preview-v79.json');native=read(Path(r['job'])/'door-caption-clearance.json');cls.records=[b,r,p,dict(DWGSHA256=r['DWGSHA256']),dict(DWGSHA256=r['DWGSHA256']),native]
    def test_actual_output_binding(self):validate_output_binding(*self.records)
    def test_old_v74_cannot_use_new_report(self):
        b,r,p,q,v,n=copy.deepcopy(self.records);old=read(S/'evidence/whole-chain-delivery-v74.json');r.update(DWG=old['file'],DWGSHA256=old['SHA256']);q['DWGSHA256']=v['DWGSHA256']=old['SHA256']
        with self.assertRaises(ValueError):validate_output_binding(b,r,p,q,v,n)
    def test_native_output_hash_is_required(self):
        records=copy.deepcopy(self.records);records[-1]['outputSHA256']='0'*64
        with self.assertRaises(ValueError):validate_output_binding(*records)
    def test_preview_source_must_be_same_dwg(self):
        records=copy.deepcopy(self.records);records[2]['source']=read(S/'evidence/whole-chain-delivery-v74.json')['file']
        with self.assertRaises(ValueError):validate_output_binding(*records)
    def test_preview_control_hash_is_required(self):
        records=copy.deepcopy(self.records);records[2]['sourceSHA256']='0'*64
        with self.assertRaises(ValueError):validate_output_binding(*records)

if __name__=='__main__':unittest.main()
