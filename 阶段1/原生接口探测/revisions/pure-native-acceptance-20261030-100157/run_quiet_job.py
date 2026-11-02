"""Dispatch only file-bound native commands to the dedicated non-input-desktop worker."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid
from datetime import datetime

STAGE=Path(__file__).resolve().parents[1]
PROJECT=Path(__file__).resolve().parent
SCHEME=STAGE.parent/"阶段0/输入快照/方案平面图.dwg"
REFERENCE=STAGE.parent/"阶段0/输入快照/施工图平面参考图.dwg"
MODES={
 "TextFinderNative":("S1NATIVETEXTFINDER","native-text-finder.json","verify_pure_native_continuation.py",False),
 "SchemeCatalogNative":("S1SCHEMECATALOG","native-semantic-catalog.json","verify_pure_native_continuation.py",True),
 "ReferenceCatalogNative":("S1REFERENCECATALOG","native-semantic-catalog.json","verify_pure_native_continuation.py",True),
 "ActualFontsNative":("S1ACTUALFONTS","native-actual-fonts.json","verify_pure_native_continuation.py",False),
 "GenericTextNative":("S1GENERICTEXTCOPY","native-generic-text-copy.json","verify_pure_native_continuation.py",False),
 "RxDiscoveryNative":("S1NATIVERXDISCOVERY","native-rx-discovery.json","verify_native_rx_discovery.py",False),
 "BusinessReadNative":("S1BUSINESSLIST","business-list-native.json","verify_business_list_native.py",True),
 "CandidateDiscardNative":("S1NATIVECANDIDATEDISCARD","native-candidate-discard.json","verify_native_candidate_lifecycle.py",False),
 "ReopenDimensionNative":("S1NATIVEREOPENDIMENSION","native-reopen-dimension.json","verify_native_candidate_lifecycle.py",False),
 "FamilyNative":("S1OBJECTFAMILY","object-family-native.json","verify_object_family_native.py",False),
 "StylesNative":("S1STYLEAUDIT","native-styles.json","verify_native_styles.py",False),
 "SafetyNative":("S1SAFETYRESOURCES","native-safety-resources.json","verify_native_safety_resources.py",False),
 "GeometryNative":("S1STANDARDGEOMETRY","standard-geometry-native.json","verify_standard_geometry_native.py",False),
 "AutoCadSupport":("S1AUTOCADSUPPORT","autocad-support-load-native.json","verify_autocad_support_load_native.py",False),
 "AutoCadSupportedHost":("S1AUTOCADSUPPORTED","autocad-host-native.json","verify_autocad_host_native.py",False),
 "SchemeNative":("S1SCHEMEVERIFY","scheme-native.json","verify_scheme_native.py",False),
 "AutoCadHost":("S1AUTOCADHOST","autocad-host-native.json","verify_autocad_host_native.py",False),
 "HostScan":("S1NATIVESCAN","native-access.json",None,False),
 "ArrowEditDiscover":("S1ARROWEDITDISCOVER","symbol-discovery-native.json","verify_symbol_discovery_native.py",True),
 "SymbolPreview":("S1SYMBOLPREVIEW","symbol-preview-native.json","verify_symbol_preview_native.py",True),
 "Slope":("S1SLOPEBUSINESS","symbol-business-native.json","verify_symbol_business_native.py",True),
 "Section":("S1SECTIONBUSINESS","symbol-business-native.json","verify_symbol_business_native.py",True),
 "PointerIndex":("S1POINTERBUSINESS","symbol-business-native.json","verify_symbol_business_native.py",True),
 "SectionIndex":("S1SECTINDEXBUSINESS","symbol-business-native.json","verify_symbol_business_native.py",True),
 "SlopeDiscover":("S1SLOPEDISCOVER","symbol-discovery-native.json","verify_symbol_discovery_native.py",True),
 "SectionDiscover":("S1SECTIONDISCOVER","symbol-discovery-native.json","verify_symbol_discovery_native.py",True),
 "PointerDiscover":("S1POINTERDISCOVER","symbol-discovery-native.json","verify_symbol_discovery_native.py",True),
 "SectionIndexDiscover":("S1SECTINDEXDISCOVER","symbol-discovery-native.json","verify_symbol_discovery_native.py",True),
 "Ping":("S1QUIETPING","quiet-ping.json",None,False),
 "Clean":("S1QUIETPING","quiet-ping.json",None,True),
 "Seed":("S1FOURAXISSEED","four-axis-seed-native.json","verify_four_axis_seed_native.py",True),
 "FourSide":("S1FOURSIDEAXIS","four-side-axis-native.json","verify_four_side_axis_native.py",True),
 "Singletons":("S1FOURSIDESINGLETONS","four-side-singletons-native.json","verify_four_side_singletons_native.py",True),
 "MergeTwo":("S1AXISMERGETWO","axis-merge-two-native.json","verify_axis_merge_two_native.py",True),
 "Number1":("S1AXISNUMBER1","axis-continuous-number-native.json","verify_axis_continuous_number_native.py",True),
 "NumberA":("S1AXISNUMBERA","axis-continuous-number-native.json","verify_axis_continuous_number_native.py",True),
 "NumberG":("S1AXISNUMBERG","axis-continuous-number-native.json","verify_axis_continuous_number_native.py",True),
 "Preview":("S1FOURAXISPREVIEW","axis-preview-native.json","verify_four_axis_preview_native.py",True),
 "Overlap":("S1AXISOVERLAPPREVIEW","axis-overlap-preview-native.json","verify_axis_overlap_preview_native.py",True),
 "ModernGrip":("S1MODERNAXISGRIP","modern-axis-grip-native.json","verify_modern_axis_grip_native.py",True),
 "Explode":("S1AXISEXPLODEREADBACK","axis-explode-readback-native.json","verify_axis_explode_readback_native.py",True),
 "ElevationDialog":("S1ELEVATIONDIALOGDISCOVER","elevation-dialog-native.json","verify_elevation_dialog_native.py",True),
 "ElevationCore":("S1ELEVATIONROUNDTRIPCORE","elevation-core-native.json","verify_elevation_roundtrip_native.py",True),
 "ElevationRoundTrip":("S1ELEVATIONROUNDTRIP","elevation-roundtrip-native.json","verify_elevation_roundtrip_ui_native.py",True),
}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def read(p):return json.loads(p.read_text(encoding="utf-8-sig"))
def atomic_json(p,d):
 t=p.with_suffix(p.suffix+".tmp");t.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding="utf-8");os.replace(t,p)
def process_alive(pid):
 k=ctypes.WinDLL("kernel32",use_last_error=True);k.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];k.OpenProcess.restype=ctypes.c_void_p;k.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_uint32];k.CloseHandle.argtypes=[ctypes.c_void_p]
 h=k.OpenProcess(0x00100000,False,pid)
 if not h:return False
 try:return k.WaitForSingleObject(h,0)==258
 finally:k.CloseHandle(h)
def wait_until(fn,session,pid,seconds):
 limit=time.monotonic()+seconds
 while time.monotonic()<limit:
  result=fn()
  if result:return result
  if not process_alive(pid):raise RuntimeError("only the owned worker exited; no further dispatch")
  time.sleep(.15)
 raise TimeoutError("native worker did not complete within limit; no CAD command cancelled")

def verify_normal_exit(session,ready,seconds):
 started=time.monotonic();limit=started+seconds
 while time.monotonic()<limit:
  final_path=session/'isolation-final.json'
  if final_path.is_file():
   final=read(final_path)
   if final.get('nonce')!=ready['nonce'] or final.get('processId')!=ready['processId'] or final.get('session')!=str(session):raise RuntimeError('normal-exit supervisor identity differs')
   if final.get('workerExited') is not True or final.get('workerExitCode')!=0 or final.get('forcedTermination') is not False or final.get('nativeQuitConfirmed') is not True:raise RuntimeError('normal QUIT was not observed; exact final evidence retained')
   if final.get('desktopClosed') is not True or final.get('processHandlesClosed') is not True:raise RuntimeError('normal exit did not release supervisor process/desktop handles')
   events_path=session/'native-quit-events.jsonl'
   events=[json.loads(line) for line in events_path.read_text(encoding='utf-8-sig').splitlines()]
   if not events or any(e.get('nonce')!=ready['nonce'] or e.get('processId')!=ready['processId'] for e in events):raise RuntimeError('native QUIT event identity missing or mismatched')
   names=[e.get('event') for e in events]
   if not all(name in names for name in ('nativeQuitQueued','beginQuit','quitWillStart','acrxQuitMsg')) or 'quitAborted' in names:raise RuntimeError('actual native shutdown notifications not complete')
   quit_start=[e for e in events if e.get('event')=='commandWillStart' and str(e.get('command','')).upper()=='QUIT']
   if len(quit_start)!=1:raise RuntimeError('actual public QUIT command did not start exactly once')
   elapsed=datetime.fromisoformat(final['recorded']).timestamp()-(session/'native.quit.done').stat().st_mtime
   result=dict(contract='stage1.normal-quit-observed.v1',nonce=ready['nonce'],processId=ready['processId'],session=str(session),exitCode=0,forcedTermination=False,waitedSeconds=time.monotonic()-started,queueToSupervisorExitSeconds=elapsed,supervisorFinal=str(final_path),supervisorFinalSHA256=digest(final_path),nativeQuitEvents=str(events_path),nativeQuitEventsSHA256=digest(events_path),COMUsed=False,ComputerUseUsed=False,completed=True,scope='Actual native QUIT notifications, process exit and supervisor cleanup; not merely a queued command; wall time includes slow AutoCAD/TCH shutdown')
   output=session/'normal-quit-verification.json'
   with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,ensure_ascii=False,indent=2)
   return result
  time.sleep(.25)
 evidence=dict(contract='stage1.normal-quit-wait-timeout.v1',nonce=ready['nonce'],processId=ready['processId'],session=str(session),waitedSeconds=time.monotonic()-started,completed=False,forcedTermination=False,action='report only; supervisor continues, no process or window action')
 atomic_json(session/('normal-quit-timeout-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'.json'),evidence)
 raise TimeoutError('native QUIT queued but normal process exit not observed within limit; no process terminated')
def dispatch(session,ready,command,run,expected_report=None,seconds=90):
 serial=uuid.uuid4().hex;q=session/"queue";q.mkdir(exist_ok=True)
 ini=f"[job]\nnonce={ready['nonce']}\nserial={serial}\ncommand={command}\nrun={run}\n"
 tmp=q/(serial+".tmp");tmp.write_text(ini,encoding="utf-16");os.replace(tmp,q/(serial+".ini"))
 file=session/("dispatch_"+serial+".json");result=wait_until(lambda:read(file) if file.is_file() else None,session,ready['processId'],seconds)
 if result.get("nonce")!=ready['nonce'] or result.get("processId")!=ready['processId'] or result.get("queueStatus")!=0 or result.get("error") is not None:raise RuntimeError("native dispatch guard rejected request: "+str(result))
 def settled():
  try:s=read(session/"worker-state.json")
  except (OSError,ValueError):return None
  if s.get("serial")!=serial:return None
  if s.get("commandFailed") or s.get("phase")=="failed":raise RuntimeError("native command failed; exact state retained")
  if s.get("phase")=="idle" and s.get("commandStarted") and s.get("commandEnded") and s.get("cmdActive")==0 and s.get("inputPending")==0 and not s.get("nativePending"):
   if s.get("nonce")!=ready['nonce'] or s.get("processId")!=ready['processId'] or s.get("desktop")!=ready['desktop'] or not s.get("inputDesktop") or s.get("inputDesktop")==s.get("desktop"):raise RuntimeError("native worker isolation identity changed")
   if expected_report and not (run/expected_report).is_file():return None
   return s
  return None
 state=wait_until(settled,session,ready['processId'],seconds)
 return {"serial":serial,"dispatch":str(file),"idleState":state}
def main(args):
 session=args.session.resolve();ready=read(session/"quiet-worker-ready.json");config=read(session/"worker-config.json")
 if ready.get("nonce")!=config['nonce'] or ready.get("session")!=str(session) or ready.get("desktop")!="CodexS1_"+ready['nonce'] or not ready.get("inputDesktop") or ready['inputDesktop']==ready['desktop'] or ready.get("error") is not None:raise RuntimeError("unverified private-desktop native worker")
 if digest(Path(config['stagedARX']))!=config['binarySHA256']:raise RuntimeError("staged module identity changed")
 if digest(SCHEME)!="5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E" or digest(REFERENCE)!="8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27":raise RuntimeError("frozen input identity changed")
 if args.mode=="Exit":
  serial=uuid.uuid4().hex;q=session/"queue";q.mkdir(exist_ok=True);temp=q/(serial+".tmp");temp.write_text(f"[job]\nnonce={ready['nonce']}\nserial={serial}\ncommand=S1QUIETEXIT\nrun={session}\n",encoding="utf-16");os.replace(temp,q/(serial+".ini"))
  done=wait_until(lambda:read(session/"native.quit.done") if (session/"native.quit.done").is_file() else None,session,ready['processId'],args.timeout)
  if done.get("nonce")!=ready['nonce'] or done.get("processId")!=ready['processId'] or done.get("error") is not None:raise RuntimeError("worker quit identity differs")
  print(json.dumps(verify_normal_exit(session,ready,args.exit_timeout),ensure_ascii=False));return 0
 command,report,validator,control=MODES[args.mode]
 run=session if args.mode=="Ping" and not (session/report).exists() else STAGE/"runs"/("quiet-"+args.mode.lower()+"-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 run.mkdir(exist_ok=True);receipt={"contract":"stage1.quiet-job.v1","mode":args.mode,"run":str(run),"session":str(session),"nonce":ready['nonce'],"processId":ready['processId'],"desktop":ready['desktop'],"binarySHA256":config['binarySHA256'],"sourceFiles":config['sourceFiles'],"controllerSHA256":digest(Path(__file__)),"schemeSHA256":digest(SCHEME),"referenceSHA256":digest(REFERENCE),"COMUsed":False,"steps":[],"verificationExitCode":None,"error":None}
 try:
  shutil.copyfile(Path(__file__),run/'run_quiet_job_at_run.py')
  if args.mode=='ReopenDimensionNative':
   if not args.source:raise ValueError('Explicit saved native dimension fixture required')
   source=args.source.resolve()
   if not source.is_relative_to((STAGE/'runs').resolve()) or source.name!='scheme-added-dimension.dwg':raise ValueError('Reopen input must be saved stage1 added-dimension fixture')
   shutil.copyfile(source,run/'control.dwg');receipt.update(controlSource=str(source),controlSourceSHA256=digest(source),controlCopySHA256=digest(run/'control.dwg'),controlOpenedAsDocument=False)
  if control:
   if args.mode!="Seed" and not args.source:raise ValueError("explicit validated stage1 source required")
   source=(args.source or SCHEME).resolve()
   if source!=SCHEME.resolve() and not source.is_relative_to((STAGE/"runs").resolve()):raise ValueError("control source outside approved frozen/stage1 paths")
   shutil.copyfile(source,run/"control.dwg");os.chmod(run/"control.dwg",0o666);receipt['controlSource']=str(source);receipt['controlSourceSHA256']=digest(source);receipt['controlCopySHA256']=digest(run/"control.dwg");assert receipt['controlCopySHA256']==receipt['controlSourceSHA256']
   receipt['steps'].append(dispatch(session,ready,"S1OPENPROBECONTROL",run,"native-open.done",args.timeout))
   marker=read(run/"native-open.done");opened=read(run/"native-open.json")
   if marker.get("run")!=str(run) or marker.get("safeToUnload") is not True or opened.get("openStatus")!=0 or opened.get("error") is not None:raise RuntimeError("native open guard failed")
  receipt['steps'].append(dispatch(session,ready,command,run,report,args.timeout))
  if validator:
   v=STAGE/"tests"/validator;receipt['validatorSHA256']=digest(v);env=os.environ.copy();env['PYTHONIOENCODING']='utf-8';command_args=[sys.executable,str(v),str(run/report)]
   if args.mode=="AutoCadSupportedHost":command_args.append('--supported')
   if args.mode in ("Preview","Overlap","SymbolPreview"):
    source_report=args.source_report.resolve();receipt['previewSourceReport']=str(source_report);receipt['previewSourceReportSHA256']=digest(source_report);command_args.append(str(source_report))
   r=subprocess.run(command_args,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60,env=env);(run/"verify-result.txt").write_text(r.stdout+r.stderr,encoding='utf-8');receipt['verificationExitCode']=r.returncode
   if r.returncode:raise RuntimeError("native business assertions failed; original evidence retained")
  else:receipt['verificationExitCode']=0
 except Exception as e:receipt['error']=str(e)
 finally:atomic_json(run/"quiet-job-receipt.json",receipt)
 print(json.dumps({"run":str(run),"mode":args.mode,"verificationExitCode":receipt['verificationExitCode'],"error":receipt['error']},ensure_ascii=False))
 return int(receipt['error'] is not None)
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("session",type=Path);p.add_argument("mode",choices=list(MODES)+["Exit"]);p.add_argument("--source",type=Path);p.add_argument("--source-report",type=Path);p.add_argument("--timeout",type=int,default=90);p.add_argument("--exit-timeout",type=int,default=900);args=p.parse_args()
 if args.mode in ("Preview","Overlap","SymbolPreview") and (not args.source or not args.source_report):p.error("Preview/Overlap/SymbolPreview requires explicit --source and --source-report")
 try:raise SystemExit(main(args))
 except Exception as e:print("FAIL:",str(e));raise SystemExit(1)
