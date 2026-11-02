import copy,unittest
from pathlib import Path
from chain_contract import S,read
from verify_door_caption_clearance import verify_new,old_geometry,caption_source,own_door_collisions

class CaptionClearanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        r=read(S/'evidence/native-caption-run-v79.json');p=read(S/'evidence/caption-preview-v79.json');cls.report=read(Path(r['job'])/'door-caption-clearance.json');cls.preview=read(Path(p['job'])/'stage2-preview-native.json')
    def test_actual_saved_candidate(self):verify_new(self.report,self.preview)
    def test_old_twelve_overlaps_rejected(self):
        self.assertEqual(len(own_door_collisions(old_geometry(),caption_source())),12)
        p=copy.deepcopy(self.preview);p['originalBefore']=old_geometry();p['originalAfter']=copy.deepcopy(p['originalBefore'])
        with self.assertRaises(ValueError):verify_new(self.report,p)
    def test_restoring_one_bad_caption_is_rejected(self):
        p=copy.deepcopy(self.preview);old=next(e for e in old_geometry() if e['handle']=='D214');p['originalBefore']=[old if e['handle']=='D214' else e for e in p['originalBefore']];p['originalAfter']=copy.deepcopy(p['originalBefore'])
        with self.assertRaises(ValueError):verify_new(self.report,p)
    def test_dimension_change_is_rejected(self):
        p=copy.deepcopy(self.preview);e=next(x for x in p['originalBefore'] if x['type']=='TDbDimension2');e['grip']['points'][0][0]+=100;p['originalAfter']=copy.deepcopy(p['originalBefore'])
        with self.assertRaises(ValueError):verify_new(self.report,p)
    def test_window_caption_change_is_rejected(self):
        p=copy.deepcopy(self.preview);next(x for x in p['originalBefore'] if x['handle']=='D211')['min'][0]+=100;p['originalAfter']=copy.deepcopy(p['originalBefore'])
        with self.assertRaises(ValueError):verify_new(self.report,p)
    def test_style_route_or_side_change_is_rejected(self):
        for mutate in (lambda r:r['targets'][0].update(styleUnchanged=False),lambda r:r.update(COMUsed=True),lambda r:r['targets'][0].update(side=-r['targets'][0]['side'])):
            r=copy.deepcopy(self.report);mutate(r)
            with self.assertRaises(ValueError):verify_new(r,self.preview)
    def test_door_number_on_dimension_text_rejected(self):
        p=copy.deepcopy(self.preview);r=copy.deepcopy(self.report);e=next(x for x in p['originalBefore'] if x['handle']=='D225')
        for field in ('min','max'):e[field][0]+=475
        for field in ('grip','stretch'):
            for point in e[field]['points']:point[0]+=475
        p['originalAfter']=copy.deepcopy(p['originalBefore']);next(x for x in r['targets'] if x['handle']=='D225')['after'][0][0]+=475
        with self.assertRaisesRegex(ValueError,'dimension text or baseline'):verify_new(r,p)

if __name__=='__main__':unittest.main()
