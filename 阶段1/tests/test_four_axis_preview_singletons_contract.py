"""Synthetic singleton-manifest guards only, never native drawing evidence."""
import unittest
from verify_four_axis_preview_native import check_singleton_manifest


def entity(handle,station,vertical):
    gp=[[station,20,0],[station,30,0],[station,26,0],[station,0,0],[station,-10,0],[station,-6,0],[station-4,26,0],[station-4,-6,0]]
    sp=[[station,20,0],[station,26,0],[station,0,0],[station,-6,0]]
    if not vertical:
        for p in gp+sp:p[0],p[1]=p[1],p[0]
    return {'handle':handle,'type':'TDbAxisLabelSet','proxy':False,'grip':{'status':0,'points':gp},'stretch':{'status':0,'points':sp}}


def manifest():
    xs=list(range(100,1100,100));ys=list(range(2000,3200,100));axes=[]
    for orientation,values in (('vertical',xs),('horizontal',ys)):
        for i,s in enumerate(values):axes.append({'orientation':orientation,'stationCount':1,'nativeGeometry':entity(orientation+str(i),s,orientation=='vertical')})
    return axes,{'targetXStations':xs,'targetYStations':ys}


class SingletonPreviewManifestTests(unittest.TestCase):
    def failures(self,axes,source):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_singleton_manifest(axes,source,check)
        return failures

    def test_complete22_singletons_cover_all_true_candidates(self):
        self.assertEqual(self.failures(*manifest()),[])

    def test_missing_collection_cannot_pass_with_partial_coverage(self):
        axes,source=manifest();axes.pop()
        self.assertTrue(self.failures(axes,source))

    def test_duplicate_station_is_not_complete_coverage(self):
        axes,source=manifest();axes[1]['nativeGeometry']=entity('different-handle',100,True)
        self.assertTrue(any('covered exactly once' in f for f in self.failures(axes,source)))

    def test_duplicate_handle_cannot_replace_distinct_collection(self):
        axes,source=manifest();axes[1]['nativeGeometry']['handle']=axes[0]['nativeGeometry']['handle']
        self.assertTrue(any('distinct singleton' in f for f in self.failures(axes,source)))

    def test_wrong_orientation_tag_cannot_hide_real_station_layout(self):
        axes,source=manifest();axes[0]['orientation']='horizontal'
        self.assertTrue(self.failures(axes,source))

    def test_standard_block_cannot_replace_native_singleton(self):
        axes,source=manifest();axes[0]['nativeGeometry']['type']='AcDbBlockReference'
        self.assertTrue(any('proprietary native type' in f for f in self.failures(axes,source)))

    def test_hidden_second_station_cannot_pass_as_singleton(self):
        axes,source=manifest();e=axes[0]['nativeGeometry']
        e['stretch']['points']+=entity('hidden',150,True)['stretch']['points']
        self.assertTrue(any('exactly one complete' in f for f in self.failures(axes,source)))


if __name__=='__main__':unittest.main()
