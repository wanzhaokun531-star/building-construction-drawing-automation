"""Only mirror the observed missing data and append the task-specific support path."""
from pathlib import Path
import json,hashlib,shutil,winreg,datetime
from native_runtime_data_policy import data_names_allowed,add_data_support_path
P=Path(__file__).resolve().parent;S=P.parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
config=json.loads((P/'evidence/autocad-support-profile-20261029.json').read_text(encoding='utf-8-sig'));profile=config['profile'];assert profile=='CodexS1Native_6f6df19579f3'
component=Path(config['componentPath']);assert sha(component)==config['componentSHA256']
source=component.parent.parent/'SYS/linetype.dbf';assert source.is_file() and data_names_allowed([source.name])
destination=S/'测试资源/天正运行数据';destination.mkdir(parents=True,exist_ok=True);target=destination/source.name
if target.exists():assert sha(target)==sha(source)
else:shutil.copyfile(source,target)
assert sha(target)==sha(source) and set(x.name for x in destination.iterdir())=={'linetype.dbf'}
root=r'Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804';keyPath=root+'\\Profiles\\'+profile+'\\General'
with winreg.OpenKey(winreg.HKEY_CURRENT_USER,keyPath,0,winreg.KEY_QUERY_VALUE|winreg.KEY_SET_VALUE) as key:
 before,kind=winreg.QueryValueEx(key,'ACAD');assert kind in (winreg.REG_SZ,winreg.REG_EXPAND_SZ);after=add_data_support_path(before,str(destination));assert winreg.QueryValueEx(key,'ACAD')[0]==before
 winreg.SetValueEx(key,'ACAD',0,kind,after);assert winreg.QueryValueEx(key,'ACAD')[0]==after
for name in ['CP_ARX','CP_NET']:
 with winreg.OpenKey(winreg.HKEY_CURRENT_USER,root+'\\Applications\\'+name) as key:assert winreg.QueryValueEx(key,'LOADCTRLS')[0]==2
result=dict(contract='stage1.dedicated-native-data-mirror.v1',recorded=datetime.datetime.now().astimezone().isoformat(),source=str(source),sourceSHA256=sha(source),target=str(target),targetSHA256=sha(target),profile=profile,onlyProfileValueChanged=keyPath+'\\ACAD',supportBefore=before,supportAfter=after,copiedNames=['linetype.dbf'],executableOrStartupFilesCopied=False,sharedStartupRegistrationsChanged=False,reason='Attested private AutoCAD startup dialog reported missing linetype.dbf; SYS support directory is excluded to avoid startup scripts')
out=P/'evidence/native-runtime-data-mirror-20261030.json';assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(profile=profile,mirrored=source.name,SHA256=sha(target),sharedRegistryUnchanged=True),ensure_ascii=False))
