param([string]$VerifyOnlyRunDirectory)
$ErrorActionPreference='Stop'
function Get-RunExitCode($record) {
  if($record.Error -or $record.ReadExitCode -ne 0 -or $record.RoundtripExitCode -ne 0 -or $record.Unload -ne 'returned successfully' -or $record.Cleanup -ne 'exact temporary ARX and INI removed after unload'){return 1}
  return 0
}
if($VerifyOnlyRunDirectory){
  $saved=Get-Content -LiteralPath (Join-Path $VerifyOnlyRunDirectory 'host-receipt.json') -Raw|ConvertFrom-Json
  exit (Get-RunExitCode $saved)
}
$stage=[IO.Path]::GetFullPath($PSScriptRoot)
$run=Join-Path $stage ('runs\native-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$source=Join-Path $stage 'bin\Stage1Probe.arx'
$hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$c=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$d=$c.ActiveDocument
if(!$c.GetAcadState().IsQuiescent -or $d.GetVariable('CMDNAMES') -ne ''){throw 'CAD busy; no command cancelled.'}
if($d.FullName -notlike '*建筑施工图自动化\阶段1\runs\*\retry-control.dwg'){throw 'Activate the task-owned retry control document before this run.'}
$trusted='C:\Users\PC\AppData\Roaming\Tangent\TArch20V8\sys'
$existing=@($d.GetVariable('TRUSTEDPATHS').Split(';')|ForEach-Object{[IO.Path]::GetFullPath($_).TrimEnd('\')})
if($existing -notcontains $trusted.TrimEnd('\')){throw 'Runtime directory is not already trusted.'}
$target=Join-Path $trusted ('S1Probe_'+$hash.Substring(0,12)+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.arx')
$ini=[IO.Path]::ChangeExtension($target,'.ini')
$scheme=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\方案平面图.dwg'))
$reference=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\施工图平面参考图.dwg'))
$python='C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$receipt=[ordered]@{Run=$run;Started=(Get-Date -Format o);Host=$c.FullName;HostVersion=$c.Version;HWND=$c.HWND;PID=@(Get-Process acad|Where-Object{$_.MainWindowHandle.ToInt64() -eq $c.HWND}|Select-Object -ExpandProperty Id);Control=$d.FullName;OriginalSecurity=$d.GetVariable('SECURELOAD');SecuritySettingsChanged=$false;ARX=$target;ArxSHA256=$hash;SourceSHA256=(Get-FileHash -LiteralPath (Join-Path $stage 'src\Stage1Probe.cpp')).Hash;ProjectSHA256=(Get-FileHash -LiteralPath (Join-Path $stage 'src\Stage1Probe.vcxproj')).Hash;SchemeSHA256=(Get-FileHash -LiteralPath $scheme).Hash;ReferenceSHA256=(Get-FileHash -LiteralPath $reference).Hash;Load='not executed';Read='not executed';Roundtrip='not executed';Unload='not executed';Cleanup='not executed'}
function Save-Receipt { $receipt|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $run 'host-receipt.json') -Encoding UTF8 }
$loaded=$false
try {
  [IO.File]::Copy($source,$target,$false)
  [IO.File]::WriteAllText($ini,"[probe]`r`nroot=$stage`r`nscheme=$scheme`r`nreference=$reference`r`noutput=$run`r`n",[Text.Encoding]::Unicode)
  Save-Receipt
  $run|Set-Content -LiteralPath (Join-Path $stage '最新运行目录.txt') -Encoding UTF8
  $c.LoadARX($target);$loaded=$true;$receipt.Load='returned successfully';Save-Receipt
  $d.SendCommand("S1READ`n")
  & $python (Join-Path $stage 'tests\verify_native_read.py') (Join-Path $run 'native-read.json') *> (Join-Path $run 'verify-native-read.txt')
  $receipt.ReadExitCode=$LASTEXITCODE
  $receipt.Read='native output generated and assertions executed';Save-Receipt
  if($LASTEXITCODE -ne 0){throw 'Native read assertions failed; roundtrip not started.'}
  $d.SendCommand("S1ROUNDTRIP`n")
  & $python (Join-Path $stage 'tests\verify_roundtrip.py') $run *> (Join-Path $run 'verify-roundtrip.txt')
  $receipt.RoundtripExitCode=$LASTEXITCODE
  $receipt.Roundtrip='native output generated and assertions executed';Save-Receipt
}catch{$receipt.Error=$_.Exception.Message}
finally {
  if($loaded){try{$c.UnloadARX($target);$receipt.Unload='returned successfully';$loaded=$false}catch{$receipt.Unload=$_.Exception.Message}}
  if(!$loaded -and $receipt.Unload -eq 'returned successfully' -and (Get-FileHash -LiteralPath $target).Hash -eq $hash){
    Remove-Item -LiteralPath $target
    Remove-Item -LiteralPath $ini
    $receipt.Cleanup='exact temporary ARX and INI removed after unload'
  }
  $receipt.Finished=(Get-Date -Format o);Save-Receipt
}
$receipt|ConvertTo-Json -Depth 6
Get-Content -LiteralPath (Join-Path $run 'verify-native-read.txt') -Tail 12 -ErrorAction SilentlyContinue
Get-Content -LiteralPath (Join-Path $run 'verify-roundtrip.txt') -Tail 12 -ErrorAction SilentlyContinue
exit (Get-RunExitCode $receipt)
