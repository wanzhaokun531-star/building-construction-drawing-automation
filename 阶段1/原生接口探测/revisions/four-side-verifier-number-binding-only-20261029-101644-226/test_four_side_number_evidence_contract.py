"""Adversarial evidence contracts only; synthetic data are not native proof."""
import copy
import unittest
from verify_four_side_axis_native import check_number_axis_binding


def binding_fixture():
    axis={"handle":"V1","type":"TDbAxisLabelSet","grip":{"status":0,"points":[[10,20,0]]}}
    return ({"axisHandle":"V1","before":copy.deepcopy(axis),"after":copy.deepcopy(axis)},
            {"mergedAxisHandle":"V1","afterSets":[copy.deepcopy(axis)]},
            copy.deepcopy(axis),copy.deepcopy(axis))


class NumberAxisBindingTests(unittest.TestCase):
    def failures(self,values):
        failures=[]
        def check(ok,label):
            if not ok:failures.append(label)
            return bool(ok)
        check_number_axis_binding(*values,check,"number")
        return failures

    def test_exact_same_direction_chain_is_accepted(self):
        self.assertEqual(self.failures(binding_fixture()),[])

    def test_valid_command_for_other_direction_is_rejected(self):
        number,merge,current,saved=binding_fixture()
        number["axisHandle"]="H1"
        for key in ("before","after"):number[key]["handle"]="H1"
        self.assertTrue(self.failures((number,merge,current,saved)))

    def test_matching_handle_does_not_hide_changed_merge_input(self):
        values=binding_fixture();values[0]["before"]["grip"]["points"][0][0]+=1
        self.assertTrue(any("input equals" in x for x in self.failures(values)))

    def test_matching_handle_does_not_hide_changed_final_output(self):
        values=binding_fixture();values[2]["grip"]["points"][0][0]+=1
        self.assertTrue(any("persisted same-direction" in x for x in self.failures(values)))

    def test_missing_same_direction_merge_output_is_rejected(self):
        values=binding_fixture();values[1]["afterSets"]=[]
        self.assertTrue(self.failures(values))


if __name__=="__main__":unittest.main()
