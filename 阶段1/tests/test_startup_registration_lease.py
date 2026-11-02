from pathlib import Path
import sys,unittest,copy
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'原生接口探测'))
from startup_registration_lease import StartupLease,EXPECTED,RegistryBackend
class Fake:
 def __init__(self):self.rows={k:dict(value=2,kind=4,loader=v) for k,v in EXPECTED.items()};self.writes=[];self.fail=None
 def read(self,k):return copy.deepcopy(self.rows[k])
 def write(self,k,v):
  if self.fail==k and v==0:raise RuntimeError('simulated second write failure')
  self.rows[k]['value']=v;self.writes.append((k,v))
class StartupRegistrationLease(unittest.TestCase):
 def test_plan_never_writes(self):
  b=Fake();rows=StartupLease(b).plan();self.assertEqual(2,len(rows));self.assertEqual([],b.writes)
 def test_exact_two_values_restored(self):
  b=Fake();lease=StartupLease(b);lease.plan();lease.apply();self.assertTrue(all(x['value']==0 for x in b.rows.values()));lease.restore();self.assertTrue(all(x['value']==2 for x in b.rows.values()))
 def test_partial_apply_failure_restores_first(self):
  b=Fake();b.fail='CP_NET';lease=StartupLease(b);lease.plan()
  with self.assertRaises(RuntimeError):lease.apply()
  self.assertTrue(all(x['value']==2 for x in b.rows.values()))
 def test_external_change_not_overwritten(self):
  b=Fake();lease=StartupLease(b);lease.plan();lease.apply();b.rows['CP_NET']['value']=4;lease.restore();self.assertEqual(4,b.rows['CP_NET']['value']);self.assertEqual(2,b.rows['CP_ARX']['value'])
 def test_unexpected_loader_rejected_without_writes(self):
  b=Fake();b.rows['CP_NET']['loader']='C:/Other.dll'
  with self.assertRaises(ValueError):StartupLease(b).plan()
  self.assertEqual([],b.writes)
 def test_user_prohibition_blocks_real_backend_before_any_registry_open(self):
  with patch('winreg.OpenKey',side_effect=AssertionError('registry must not be opened for writes')):
   with self.assertRaises(PermissionError):RegistryBackend().write('CP_NET',0)
if __name__=='__main__':unittest.main()
