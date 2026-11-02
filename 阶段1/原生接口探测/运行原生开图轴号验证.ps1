param([ValidateSet(2,3)][int]$AxisCount=2,[ValidateSet('TMultAxisDim','GRIDS')][string]$NativeCommand='TMultAxisDim',[switch]$UsePickfirst,[long]$ExpectedHostHWND=788856,[int]$ExpectedHostPID=0)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath($PSScriptRoot)
$stage=Split-Path -Parent $project
$variant=if($UsePickfirst){if($NativeCommand -eq 'GRIDS'){'p'}else{'pt'}}elseif($NativeCommand -eq 'GRIDS'){'g'}else{'c'}
$run=Join-Path $stage ('runs\native-open-axislabels'+$AxisCount+$variant+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$binary=Join-Path $project 'bin\NativeAccessProbe.arx'
$hash=(Get-FileHash -LiteralPath $binary).Hash
$scheme=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\方案平面图.dwg'))
$reference=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\施工图平面参考图.dwg'))
if((Get-FileHash -LiteralPath $scheme).Hash -ne '5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'){throw 'Frozen scheme identity changed.'}
$control=Join-Path $run 'control.dwg'
[IO.File]::Copy($scheme,$control,$false)
[IO.File]::SetAttributes($control,([IO.File]::GetAttributes($control) -band (-bnot [IO.FileAttributes]::ReadOnly)))
$cad=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
if([long]$cad.HWND -ne $ExpectedHostHWND){throw 'The explicitly verified Tianzheng host was not returned; no action taken on another CAD instance.'}
if($ExpectedHostPID -gt 0){
 $verifiedHost=Get-Process -Id $ExpectedHostPID
 if($verifiedHost.MainWindowHandle.ToInt64() -ne $ExpectedHostHWND -or $verifiedHost.Path -ne 'D:\CAD2020\AutoCAD 2020\acad.exe'){throw 'Verified host process identity differs.'}
}
$initial=$cad.ActiveDocument
if(!$cad.GetAcadState().IsQuiescent -or $initial.GetVariable('CMDNAMES') -ne ''){throw 'CAD is busy; no command cancelled.'}
if(@($cad.ListARX()|Where-Object{$_ -eq 'tch_kernal.arx'}).Count -ne 1){throw 'Expected Tianzheng host.'}
$trusted='C:\Users\PC\AppData\Roaming\Tangent\TArch20V8\sys'
$trustedPaths=@($initial.GetVariable('TRUSTEDPATHS').Split(';')|ForEach-Object{[IO.Path]::GetFullPath($_).TrimEnd('\')})
if($trustedPaths -notcontains $trusted.TrimEnd('\')){throw 'Runtime directory is not already trusted.'}
$target=Join-Path $trusted ('S1Native_'+$hash.Substring(0,12)+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.arx')
$ini=[IO.Path]::ChangeExtension($target,'.ini')
$python='C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$r=[ordered]@{Run=$run;AxisCount=$AxisCount;Started=(Get-Date -Format o);Host=$cad.FullName;HostVersion=$cad.Version;HWND=$cad.HWND;InitialDocument=$initial.FullName;ControlDocument=$control;ControlOpenMethod='ObjectARX application-context document manager';ObjectAccess='ObjectARX native; no COM';Orchestration='External AutoCAD COM load and command scheduling';SourceSHA256=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeAccessProbe.cpp')).Hash;BinarySHA256=$hash;Binary=$target;ProjectSHA256=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeAccessProbe.vcxproj')).Hash;SchemeSHA256=(Get-FileHash -LiteralPath $scheme).Hash;ReferenceSHA256=(Get-FileHash -LiteralPath $reference).Hash;OriginalSecurity=$initial.GetVariable('SECURELOAD');SecuritySettingsChanged=$false;Load='not executed';Unload='not executed';Cleanup='not executed';OpenVerificationExitCode=$null;VerificationExitCode=$null}
$r.SourceFiles=[ordered]@{'NativeAccessProbe.cpp'=$r.SourceSHA256;'NativeStyleAudit.inc'=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeStyleAudit.inc')).Hash}
$r.NativeCommand=$NativeCommand
$r.ExpectedHostHWND=$ExpectedHostHWND
$r.ExpectedHostPID=$ExpectedHostPID
$r.UsePickfirst=[bool]$UsePickfirst
$r.ControllerSHA256=(Get-FileHash -LiteralPath $PSCommandPath).Hash
$r.CommandRejections=0
$r.ValidatorDependencies=[ordered]@{}
foreach($name in @('verify_native_control_open.py','verify_axis_labels_native.py','verify_scheme_native.py')){$r.ValidatorDependencies[$name]=(Get-FileHash -LiteralPath (Join-Path $stage ('tests\'+$name))).Hash}
function Save-Receipt{$r|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $run 'host-receipt.json') -Encoding UTF8}
function Send-NativeCommand($Document,[string]$Text){
  $limit=(Get-Date).AddSeconds(20)
  do{
    try{$Document.SendCommand($Text);return}catch{
      if($_.Exception.Message -notmatch '0x80010001|RPC_E_CALL_REJECTED|Call was rejected by callee'){throw}
      $r.CommandRejections++
      Start-Sleep -Milliseconds 250
    }
  }while((Get-Date) -lt $limit)
  throw 'Native command scheduling was rejected before execution for 20 seconds.'
}
function Wait-NativeMarker([string]$Name,[int]$Seconds){
  $path=Join-Path $run $Name
  $limit=(Get-Date).AddSeconds($Seconds)
  while(!(Test-Path -LiteralPath $path) -and (Get-Date) -lt $limit){Start-Sleep -Milliseconds 100}
  if(!(Test-Path -LiteralPath $path)){throw ('Native completion timed out: '+$Name+'; module retained without cancelling CAD.')}
  $marker=Get-Content -LiteralPath $path -Raw -Encoding UTF8|ConvertFrom-Json
  if($marker.run -ne $run -or $marker.safeToUnload -ne $true){throw 'Native completion identity or cleanup invalid.'}
}
function Wait-OwnedIdle([bool]$RequireOwned=$true){
  $limit=(Get-Date).AddSeconds(20);$stable=0
  do{try{$ready=$cad.GetAcadState().IsQuiescent -and (!$RequireOwned -or $cad.ActiveDocument.FullName -like ($run+'\*')) -and $cad.ActiveDocument.GetVariable('CMDNAMES') -eq ''}catch{$ready=$false};if($ready){$stable++}else{$stable=0};if($stable -ge 3){return};Start-Sleep -Milliseconds 250}while((Get-Date) -lt $limit)
  throw 'Task-owned document did not become idle; module retained.'
}
$loaded=$false;$safeToUnload=$true
try{
  [IO.File]::Copy($binary,$target,$false)
  [IO.File]::WriteAllText($ini,"[probe]`r`nroot=$stage`r`nscheme=$scheme`r`nreference=$reference`r`noutput=$run`r`n",[Text.Encoding]::Unicode)
  Save-Receipt
  $cad.LoadARX($target);$loaded=$true;$r.Load='returned successfully';Save-Receipt
  Wait-OwnedIdle $false
  $safeToUnload=$false
  Send-NativeCommand $initial "S1OPENPROBECONTROL`n"
  Wait-NativeMarker 'native-open.done' 20
  $safeToUnload=$true
  & $python (Join-Path $stage 'tests\verify_native_control_open.py') (Join-Path $run 'native-open.json') *> (Join-Path $run 'verify-open.txt')
  $r.OpenVerificationExitCode=$LASTEXITCODE
  if($LASTEXITCODE -ne 0){throw 'Native control opening verification failed.'}
  $safeToUnload=$false
  Wait-OwnedIdle
  $safeToUnload=$true
  if($cad.ActiveDocument.FullName -ne $control){throw 'Control document identity changed before native axis creation.'}
  $safeToUnload=$false
  $suffix=if($UsePickfirst){if($NativeCommand -eq 'GRIDS'){'P'}else{'PT'}}elseif($NativeCommand -eq 'GRIDS'){'G'}else{'C'}
  Send-NativeCommand $cad.ActiveDocument (('S1AXISLABELS'+$AxisCount+$suffix)+"`n")
  Wait-NativeMarker 'axis-labels-native.done' 30
  Wait-OwnedIdle $false
  $safeToUnload=$true
  $r.ActiveDocumentAfterCompletion=$cad.ActiveDocument.FullName
  & $python (Join-Path $stage 'tests\verify_axis_labels_native.py') (Join-Path $run 'axis-labels-native.json') *> (Join-Path $run 'verify-result.txt')
  $r.VerificationExitCode=$LASTEXITCODE
  if($LASTEXITCODE -ne 0){throw 'Native axis-label creation verification failed.'}
}catch{$r.Error=$_.Exception.Message}
finally{
  if($loaded -and $safeToUnload){try{$cad.UnloadARX($target);$loaded=$false;$r.Unload='returned successfully'}catch{$r.Unload=$_.Exception.Message}}
  if($loaded -and !$safeToUnload){$r.Unload='not attempted: native completion not confirmed'}
  if(!$loaded -and $r.Unload -eq 'returned successfully' -and (Get-FileHash -LiteralPath $target).Hash -eq $hash){Remove-Item -LiteralPath $target;Remove-Item -LiteralPath $ini;$r.Cleanup='exact temporary ARX and INI removed after unload'}
  $r.Finished=(Get-Date -Format o);Save-Receipt
}
$r|ConvertTo-Json -Depth 7
Get-Content -LiteralPath (Join-Path $run 'verify-result.txt') -Tail 14 -ErrorAction SilentlyContinue
if($r.Error -or $r.VerificationExitCode -ne 0 -or $r.Unload -ne 'returned successfully' -or $r.Cleanup -ne 'exact temporary ARX and INI removed after unload'){exit 1}
exit 0
