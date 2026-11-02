param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputPath){throw 'Host discovery output already exists.'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Stage1WindowProcess {
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
}
'@
$cad=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$window=[IntPtr][Int64]$cad.HWND
[uint32]$acadProcessId=0
[void][Stage1WindowProcess]::GetWindowThreadProcessId($window,[ref]$acadProcessId)
if($acadProcessId -eq 0){throw 'Cannot resolve the AutoCAD COM host process.'}
$acadProcess=Get-Process -Id $acadProcessId
$document=$cad.ActiveDocument
$receipt=[ordered]@{
 Contract='stage1.host-discovery.v1';Recorded=(Get-Date -Format o)
 Scope='Read-only COM host identity and native process module metadata; no command, load, document switch or save'
 ProcessId=$acadProcessId;HWND=$window.ToInt64();Responding=$acadProcess.Responding
 Executable=$acadProcess.Path;ExecutableFileVersion=$acadProcess.MainModule.FileVersionInfo.FileVersion
 COMExecutable=$cad.FullName;Version=$cad.Version;Caption=$acadProcess.MainWindowTitle
 ActiveDocument=$document.FullName;CMDNAMES=$document.GetVariable('CMDNAMES')
 CMDACTIVE=$document.GetVariable('CMDACTIVE');IsQuiescent=$cad.GetAcadState().IsQuiescent
 SECURELOAD=$document.GetVariable('SECURELOAD');TRUSTEDPATHS=$document.GetVariable('TRUSTEDPATHS')
 ARX=@($cad.ListARX());NativeModules=@($acadProcess.Modules|Where-Object{$_.ModuleName -match '^(tch|TCH)|acdb23|accore|acad.exe'}|ForEach-Object{[ordered]@{Name=$_.ModuleName;Path=$_.FileName;Version=$_.FileVersionInfo.FileVersion}})
}
$receipt|ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
$receipt|Select-Object ProcessId,HWND,Executable,Version,ActiveDocument,IsQuiescent,CMDACTIVE,CMDNAMES | ConvertTo-Json -Depth 3
