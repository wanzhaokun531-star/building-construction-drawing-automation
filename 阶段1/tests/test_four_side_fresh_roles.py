"""Replay real native inner-end coupling; reject stale-coordinate role claims."""
import copy
import json
import unittest
from pathlib import Path
from verify_four_side_axis_native import check_fit_move,expected_points,public_points
from verify_four_axis_seed_native import near

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'阶段1/runs/quiet-fourside-20261029-101538-343619/four-side-axis-native.json'


class FreshRoleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads(SOURCE.read_text('utf-8-sig'));cls.fit=cls.data['fitSteps'][0]
        cls.move=copy.deepcopy(cls.fit['moves'][2])
        cls.move['selectionPolicy']='fresh actual four-stretch-point order for every operation'
        _,points=expected_points(cls.data['wallBodyBounds'],cls.data['outerDimensionBaselines'],cls.data['referenceNearCircleGaps'],cls.data['referenceRadii'],True,[cls.fit['targetStation']])
        cls.target=sorted(p[1] for p in points)

    def failures(self,move):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_fit_move(move,True,self.fit['targetStation'],self.target,check,'case')
        return failures

    def test_real_inner_move_uses_actual_index_and_reaches_its_target(self):
        self.assertEqual(self.failures(self.move),[])

    def test_real_native_coupling_invalidates_cached_outer_coordinate(self):
        initial=max(p[1] for p in public_points(self.fit['translated'],'stretch'))
        actual=public_points(self.move['after'],'stretch')
        self.assertFalse(any(near(p[1],initial) for p in actual))
        self.assertAlmostEqual(max(p[1] for p in actual)-initial,self.move['targetAcross']-self.move['currentAcross'],places=6)

    def test_stale_current_coordinate_rejected(self):
        m=copy.deepcopy(self.move);m['currentAcross']-=1
        self.assertTrue(any('delta derives' in x for x in self.failures(m)))

    def test_wrong_public_index_rejected(self):
        m=copy.deepcopy(self.move);m['indices']=[(m['indices'][0]+1)%4]
        self.assertTrue(any('current sorted role' in x for x in self.failures(m)))

    def test_target_cannot_be_derived_from_shifted_actual_output(self):
        m=copy.deepcopy(self.move);m['targetAcross']+=1
        self.assertTrue(any('fixed declared target' in x for x in self.failures(m)))

    def test_success_code_without_target_geometry_is_rejected(self):
        m=copy.deepcopy(self.move);m['after']=copy.deepcopy(m['before'])
        self.assertTrue(any('actually reaches' in x for x in self.failures(m)))

    def test_wrong_native_status_rejected(self):
        m=copy.deepcopy(self.move);m['moveStatus']=1
        self.assertTrue(any('real native operation' in x for x in self.failures(m)))


if __name__=='__main__':unittest.main()
