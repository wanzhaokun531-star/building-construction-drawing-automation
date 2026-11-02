"""Explicitly approved two-value startup lease; planning is read-only by default."""
from pathlib import PureWindowsPath
KEYS=('CP_ARX','CP_NET')
EXPECTED={'CP_ARX':'c:/sow/智源·云设计/arx_64/23.1/cpcore.arx','CP_NET':'c:/sow/智源·云设计/net40/cp.cad.dll'}
def norm(path):return str(PureWindowsPath(path)).casefold()
class StartupLease:
 def __init__(self,backend):self.backend=backend;self.before={};self.changed=[]
 def plan(self):
  rows=[]
  for key in KEYS:
   row=self.backend.read(key)
   if row['kind']!=4 or row['value']!=2 or norm(row['loader'])!=norm(EXPECTED[key]):raise ValueError('startup registration differs; no changes allowed: '+key)
   self.before[key]=dict(row);rows.append(dict(application=key,valueName='LOADCTRLS',before=2,during=0,restore=2,loader=row['loader'],kind=4))
  return rows
 def apply(self):
  if set(self.before)!=set(KEYS):raise ValueError('read-only plan must be reviewed first')
  try:
   for key in KEYS:
    if self.backend.read(key)!=self.before[key]:raise ValueError('registration changed since plan; refusing overwrite')
    self.changed.append(key);self.backend.write(key,0)
    if self.backend.read(key)['value']!=0:raise RuntimeError('startup disable readback failed')
  except BaseException:
   self.restore();raise
 def restore(self):
  result=[]
  for key in reversed(self.changed):
   row=self.backend.read(key)
   if row['value']==0:
    self.backend.write(key,self.before[key]['value']);row=self.backend.read(key)
    if row['value']!=self.before[key]['value']:raise RuntimeError('startup restore readback failed: '+key)
    result.append(dict(key=key,restored=True))
   else:result.append(dict(key=key,restored=False,reason='value changed independently; not overwritten',observed=row['value']))
  self.changed=[];return result
class RegistryBackend:
 ROOT=r'Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804\Applications'
 def read(self,name):
  import winreg
  if name not in KEYS:raise ValueError('not an approved application name')
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,self.ROOT+'\\'+name) as key:
   value,kind=winreg.QueryValueEx(key,'LOADCTRLS');loader=winreg.QueryValueEx(key,'LOADER')[0];return dict(value=value,kind=kind,loader=loader)
 def write(self,name,value):
  raise PermissionError('User explicitly prohibits shared startup registration changes; actual registry backend is read-only')
