param([Parameter(Mandatory=$true)][string]$Session)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskLaunch=Get-Content -LiteralPath (Join-Path $Session 'isolation-host-receipt.json') -Raw -Encoding UTF8|ConvertFrom-Json
$taskCfg=Get-Content -LiteralPath (Join-Path $Session 'worker-config.json') -Raw -Encoding UTF8|ConvertFrom-Json
if($taskLaunch.nonce -ne $taskCfg.nonce -or $taskLaunch.session -ne [IO.Path]::GetFullPath($Session)){throw 'launch binding differs'}
$taskExtension=@'
namespace CodexStage1Isolation {
 public static class StartupTipRecovery {
  [System.Runtime.InteropServices.DllImport("user32.dll",SetLastError=true)] static extern bool SetThreadDesktop(System.IntPtr desktop);
  public static StartupTipAction Close(string desk,uint pid,uint thread,string image,string created){
   StartupTipAction result=null;System.Exception error=null;
   var worker=new System.Threading.Thread(delegate(){System.IntPtr target=System.IntPtr.Zero;var previous=Native.GetThreadDesktop(Native.GetCurrentThreadId());try{target=Native.OpenDesktopW(desk,0,false,0xC1);if(target==System.IntPtr.Zero||!SetThreadDesktop(target))throw Native.Error("attach own helper thread desktop");result=CloseOnOwnThread(desk,pid,thread,image,created);}catch(System.Exception e){error=e;}finally{SetThreadDesktop(previous);if(target!=System.IntPtr.Zero)Native.CloseDesktop(target);}});
   worker.Start();if(!worker.Join(5000))throw new System.TimeoutException("owned tip helper thread timeout");if(error!=null)throw error;return result;
  }
  static StartupTipAction CloseOnOwnThread(string desk,uint pid,uint thread,string image,string created){
   var proof=Native.InspectOwnedDesktop(desk,pid,thread);
   if(!proof.PidFound||!proof.ExpectedThreadFound||!Native.IdentityMatches(proof.Executable,proof.CreationFileTime,proof.DesktopName,image,created,desk))throw new System.InvalidOperationException("exact owned identity required");
   foreach(var record in proof.Windows){
    if(record.Title!="日积月累"||record.ClassName!="#32770"||record.PidAtEnumeration!=pid||record.ThreadAtEnumeration!=thread)continue;
    var hwnd=new System.IntPtr(record.Hwnd);uint actual;uint tid=Native.GetWindowThreadProcessId(hwnd,out actual);var title=new System.Text.StringBuilder(4096);Native.GetWindowTextW(hwnd,title,title.Capacity);
    if(actual!=pid||tid!=thread||Native.ClassName(hwnd)!="#32770"||title.ToString()!="日积月累")throw new System.InvalidOperationException("actual known startup tip differs pid="+actual+" tid="+tid+" class="+Native.ClassName(hwnd)+" title="+title.ToString()+" expected="+pid+"/"+thread+"");
    return new StartupTipAction{Utc=System.DateTime.UtcNow.ToString("o"),DesktopName=desk,ProcessId=pid,Hwnd=record.Hwnd,Thread=tid,ClassName=record.ClassName,Title=record.Title,Matched=true,Posted=Native.PostMessageW(hwnd,0x0010,System.UIntPtr.Zero,System.IntPtr.Zero),Status="posted_WM_CLOSE_exact_owned_startup_tip"};
   }
   throw new System.InvalidOperationException("no exact owned known startup tip; no operation");
  }
 }
}
'@
Add-Type -TypeDefinition ((Get-Content -LiteralPath (Join-Path $taskRoot 'IsolatedCadHostNative.cs') -Raw -Encoding UTF8)+$taskExtension)
$taskAction=[CodexStage1Isolation.StartupTipRecovery]::Close($taskLaunch.desktopName,$taskLaunch.processId,$taskLaunch.initialThreadId,$taskLaunch.executable,$taskLaunch.creationFileTime)
$taskAction|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $Session 'startup-tip-recovery.json') -Encoding UTF8
if(!$taskAction.Posted){throw 'startup tip close post failed'}
$taskAction|ConvertTo-Json -Compress
