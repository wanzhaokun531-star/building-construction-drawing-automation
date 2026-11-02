import copy,unittest
from pathlib import Path
from chain_contract import S,read,sha
from publish_whole_chains import validate_host_records,validate_review_files,REQUIRED_REVIEW_FILES

class PublisherEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        b=read(S/'evidence/native-host-binding-v74.json');r=read(S/'evidence/native-whole-chain-run-v74.json');c=read(S/'evidence/host-closure-v74.json');session=Path(b['session'])
        cls.records=[b,r,c,read(session/'worker-config.json'),read(session/'isolation-host-receipt.json'),read(c['finalPath'])]
    def test_actual_host_evidence(self):validate_host_records(*self.records)
    def test_missing_actual_final_path(self):
        records=copy.deepcopy(self.records);records[2]['finalPath']=str(Path(records[0]['session'])/'missing.json')
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_modified_actual_final(self):
        records=copy.deepcopy(self.records);records[5]['nonce']='wrong'
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_input_desktop_read_error(self):
        records=copy.deepcopy(self.records);records[2]['final']['inputDesktopReadErrors']=1;records[5]['inputDesktopReadErrors']=1
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_bridge_binary_drift(self):
        records=copy.deepcopy(self.records);records[3]['binarySHA256']='0'*64
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_bridge_source_drift(self):
        records=copy.deepcopy(self.records);records[3]['sourceFiles']['NativeStage2Bridge.inc']='0'*64
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_controller_drift(self):
        records=copy.deepcopy(self.records);records[4]['controllerSHA256']='0'*64
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_helper_drift(self):
        records=copy.deepcopy(self.records);records[4]['nativeHelperSHA256']='0'*64
        with self.assertRaises(ValueError):validate_host_records(*records)
    def test_empty_review_coverage(self):
        with self.assertRaises(ValueError):validate_review_files(dict(reviewedFiles={}))
    def test_actual_review_coverage_hashes(self):
        validate_review_files(dict(reviewedFiles={name:sha(S/name) for name in REQUIRED_REVIEW_FILES}))

if __name__=='__main__':unittest.main()
