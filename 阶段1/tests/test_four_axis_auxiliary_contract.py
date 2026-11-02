"""Regression checks for v2's narrow native auxiliary-handle exception.

Uses the unchanged failed v1 run as raw evidence only. Synthetic horizontal
states exercise validation logic and never count as a successful native trial.
"""
import copy
import json
import unittest
from pathlib import Path
from verify_four_axis_seed_native import (
    auxiliary_state, entity_observation, near, singleton_controls_preserved,
    station_grips, validate,
)

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'阶段1/runs/quiet-seed-20261029-094645-181680/four-axis-seed-native.json'


class AuxiliaryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads(RUN.read_text(encoding='utf-8-sig'))
        cls.before=cls.data['seeds'][0]['before']
        cls.after=cls.data['seeds'][0]['after']

    def test_observed_vertical_keeps_all_six_controls_and_four_stretch(self):
        self.assertTrue(singleton_controls_preserved(self.before,self.after,True))
        self.assertEqual(auxiliary_state(self.before,True)['sign'],-1)
        self.assertEqual(auxiliary_state(self.after,True)['sign'],1)

    def test_either_joint_state_is_valid(self):
        changed=copy.deepcopy(self.after)
        first=entity_observation(changed,True)['stations'][0]
        for p in changed['grip']['points']:
            if not near(p[0],first):p[0]-=800
        self.assertTrue(singleton_controls_preserved(self.before,changed,True))

    def test_illegal_auxiliary_states_rejected(self):
        first=entity_observation(self.after,True)['stations'][0]
        indices=[i for i,p in enumerate(self.after['grip']['points']) if not near(p[0],first)]
        for name,mutate in [
            ('mixed signs',lambda p:p.__setitem__(0,p[0]-800)),
            ('intermediate offset',lambda p:p.__setitem__(0,p[0]-200)),
            ('wrong edge',lambda p:p.__setitem__(1,p[1]+1)),
            ('wrong z',lambda p:p.__setitem__(2,p[2]+1)),
        ]:
            with self.subTest(name=name):
                changed=copy.deepcopy(self.after);mutate(changed['grip']['points'][indices[0]])
                self.assertFalse(singleton_controls_preserved(self.before,changed,True))
        for name,mutate in [
            ('extra grip',lambda p:p.append(list(p[0]))),
            ('missing grip',lambda p:p.pop(indices[0])),
        ]:
            with self.subTest(name=name):
                changed=copy.deepcopy(self.after);mutate(changed['grip']['points'])
                self.assertFalse(singleton_controls_preserved(self.before,changed,True))

    def test_all_real_controls_and_bounds_remain_strict(self):
        o=entity_observation(self.after,True);real=station_grips(o,True)
        index=next(i for i,p in enumerate(self.after['grip']['points']) if near(p,real[0]))
        for name,mutate in [
            ('real grip',lambda e:e['grip']['points'][index].__setitem__(1,e['grip']['points'][index][1]+1)),
            ('stretch',lambda e:e['stretch']['points'][0].__setitem__(1,e['stretch']['points'][0][1]+1)),
            ('x bounds',lambda e:e['min'].__setitem__(0,e['min'][0]-1)),
            ('circle radius',lambda e:e['min'].__setitem__(1,e['min'][1]-1)),
        ]:
            with self.subTest(name=name):
                changed=copy.deepcopy(self.after);mutate(changed)
                self.assertFalse(singleton_controls_preserved(self.before,changed,True))

    def test_horizontal_joint_states_are_validator_examples_only(self):
        source=self.data['seeds'][1]['saved'];bo=entity_observation(source,False)
        first=bo['stations'][0];changed=copy.deepcopy(source)
        changed['stretch']['points']=[p for p in bo['stretch'] if near(p[1],first)]
        changed['grip']['points']=[p for p in bo['grips'] if near(p[1],first) or near(p[1],first-400)]
        changed['min'][1]=first-400;changed['max'][1]=first+400
        self.assertTrue(singleton_controls_preserved(source,changed,False))
        for p in changed['grip']['points']:
            if not near(p[1],first):p[1]+=800
        self.assertTrue(singleton_controls_preserved(source,changed,False))

    def test_failed_v1_native_trial_stays_failed(self):
        count,failures=validate(RUN)
        self.assertGreater(count,0)
        self.assertIn('contract',failures)
        self.assertIn('both single-station semantic outcomes',failures)
        self.assertFalse(self.data['completed'])


if __name__=='__main__':unittest.main()
