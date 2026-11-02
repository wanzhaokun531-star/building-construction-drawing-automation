param(
 [Parameter(Mandatory=$true)][ValidateSet('Family','Safety','Geometry','Plot','Combine','Text','List','AxisDelete','PreviewText','PreviewCombine','PreviewGeometry','Merge','AxisTwo','AxisThree','SymbolText','HorizontalAxis','VersionAudit')][string]$Mode,
 [Parameter(Mandatory=$true)][string]$HostEvidencePath,
 [string]$ControlSource
)
$ErrorActionPreference='Stop'
throw 'Visible shared-CAD test runner retired: document activation disturbs the user. Use the isolated desktop native worker; no COM connection/load/command has been performed.'
$project=[IO.Path]::GetFullPath($PSScriptRoot)
$stage=Split-Path -Parent $project
$workspace=Split-Path -Parent $stage
$hostEvidence=Get-Content -LiteralPath $HostEvidencePath -Raw -Encoding UTF8|ConvertFrom-Json
if($hostEvidence.Contract -ne 'stage1.host-discovery.v1' -or $hostEvidence.Version -notlike '23.1*' -or $hostEvidence.Executable -ne 'D:\CAD2020\AutoCAD 2020\acad.exe' -or $hostEvidence.ARX -notcontains 'tch_kernal.arx'){throw 'Verified AutoCAD 2020/Tianzheng host evidence required.'}
$run=Join-Path $stage ('runs\coverage-'+$Mode.ToLower()+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$scheme=Join-Path $workspace '阶段0\输入快照\方案平面图.dwg'
$reference=Join-Path $workspace '阶段0\输入快照\施工图平面参考图.dwg'
if((Get-FileHash -LiteralPath $scheme).Hash -ne '5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E' -or (Get-FileHash -LiteralPath $reference).Hash -ne '8C3F87274CCE16373DF7B2AD4BDD5090F1EE7ADA8DE97F0E3814D179F1F5AB27'){throw 'Frozen drawing identity differs.'}
$modes=@{
 Family=@{Command='S1OBJECTFAMILY';Report='object-family-native.json';Validator='verify_object_family_native.py'}
 Safety=@{Command='S1SAFETYRESOURCES';Report='native-safety-resources.json';Validator='verify_native_safety_resources.py'}
 Geometry=@{Command='S1STANDARDGEOMETRY';Report='standard-geometry-native.json';Validator='verify_standard_geometry_native.py'}
 Plot=@{Command='S1NATIVEPLOT';Report='native-plot.json';Validator='verify_native_plot.py';Control=$true}
 Combine=@{Command='S1DIMCOMBINE';Report='dimension-bridge-native.json';Validator='verify_dimension_bridge_native.py';Control=$true}
 Text=@{Command='S1TEXTBRIDGE';Report='text-bridge-native.json';Validator='verify_text_bridge_native.py';Control=$true}
 List=@{Command='S1BUSINESSLIST';Report='business-list-native.json';Validator='verify_business_list_native.py';Control=$true}
 AxisDelete=@{Command='S1AXISTEMPLATEDELETE';Report='axis-template-delete-native.json';Validator='verify_axis_template_delete_native.py';Control=$true}
 Merge=@{Command='S1DIMMERGE';Report='dimension-merge-native.json';Validator='verify_dimension_merge_native.py';Control=$true}
 AxisTwo=@{Command='S1AXISTEMPLATETWO';Report='axis-template-prune-native.json';Validator='verify_axis_template_prune_native.py';Control=$true}
 AxisThree=@{Command='S1AXISTEMPLATETHREE';Report='axis-template-prune-native.json';Validator='verify_axis_template_prune_native.py';Control=$true}
 SymbolText=@{Command='S1SYMBOLTEXTBRIDGE';Report='symbol-text-bridge-native.json';Validator='verify_symbol_text_bridge_native.py';Control=$true}
 HorizontalAxis=@{Command='S1HORIZONTALAXIS';Report='horizontal-axis-native.json';Validator='verify_horizontal_axis_native.py'}
 VersionAudit=@{Command='S1INPUTVERSIONAUDIT';Report='input-version-audit.json';Validator='verify_input_version_audit.py';ObservedSource='D:\cad插件\方案平面图.dwg'}
 PreviewText=@{Command='S1TEXTPreview';Report='nativeTextPreview.json';Validator='verify_native_preview.py';Control=$true;Source='coverage-text-20261029-003350-351\text-bridge-generated.dwg'}
 PreviewCombine=@{Command='S1COMBINEPreview';Report='nativeCombinePreview.json';Validator='verify_native_preview.py';Control=$true;Source='coverage-combine-20261028-234100-432\dimension-combine-generated.dwg'}
 PreviewGeometry=@{Command='S1GEOMETRYPreview';Report='nativeGeometryPreview.json';Validator='verify_native_preview.py';Control=$true;Source='coverage-geometry-20261028-232845-683\standard-geometry-generated.dwg'}
}
$spec=$modes[$Mode]
$control=Join-Path $run 'control.dwg'
if($spec.Control){
 if($Mode -eq 'Combine' -and !$ControlSource){throw 'Combine requires an explicitly identified stage-1 dimension fixture.'}
 if(!$ControlSource){if($spec.Source){$ControlSource=Join-Path $stage ('runs\'+$spec.Source)}else{$ControlSource=$scheme}}
 $resolvedControl=[IO.Path]::GetFullPath($ControlSource)
 if($resolvedControl -ne $scheme -and !$resolvedControl.StartsWith(($stage+'\runs\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Plot source must be the frozen scheme or stage-1 test output.'}
 [IO.File]::Copy($resolvedControl,$control,$false)
 [IO.File]::SetAttributes($control,([IO.File]::GetAttributes($control) -band (-bnot [IO.FileAttributes]::ReadOnly)))
}
if($spec.ObservedSource){
 function Get-VersionAuditSharedHash([string]$Path){
  $readStream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete));$hashAlgorithm=[Security.Cryptography.SHA256]::Create()
  try{return [BitConverter]::ToString($hashAlgorithm.ComputeHash($readStream)).Replace('-','')}finally{$hashAlgorithm.Dispose();$readStream.Dispose()}
 }
 $observedSource=[IO.Path]::GetFullPath($spec.ObservedSource);$observedCopy=Join-Path $run 'observed-original.dwg';$observedExpected='92E01E8DD5BD4BA2B65CA48C6A3E8F6C38E3CB5F7EF3C3C36E8B692980B8BDD6'
 $observedBeforeHash=Get-VersionAuditSharedHash $observedSource
 if($observedBeforeHash -ne $observedExpected){throw 'Current original version changed again; no native command/load attempted.'}
 $observedInput=[IO.File]::Open($observedSource,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete));$observedOutput=[IO.File]::Open($observedCopy,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
 try{$observedInput.CopyTo($observedOutput)}finally{$observedOutput.Dispose();$observedInput.Dispose()}
 $observedCopyHash=(Get-FileHash -LiteralPath $observedCopy).Hash;$observedAfterHash=Get-VersionAuditSharedHash $observedSource
 $observedReceipt=[ordered]@{Contract='stage1.observed-input-copy.v1';Original=$observedSource;Copy=$observedCopy;OriginalSHA256Before=$observedBeforeHash;OriginalSHA256After=$observedAfterHash;CopySHA256=$observedCopyHash;OriginalRecordedLastWriteTime=(Get-Item -LiteralPath $observedSource).LastWriteTime.ToString('o');Method='Shared read of original into unique task copy; original never opened for write; native will read only the copy'}
 $observedReceipt|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'observed-input-copy.json') -Encoding UTF8
 if($observedCopyHash -ne $observedExpected -or $observedAfterHash -ne $observedExpected){throw 'Observed original copy is not stable; no native command/load attempted.'}
 [ordered]@{contract='stage1.input-version-bindings.v1';frozenPath=$scheme;frozenSHA256=(Get-FileHash -LiteralPath $scheme).Hash;observedCopyPath=$observedCopy;observedCopySHA256=$observedCopyHash;observedSourcePath=$observedSource;observedSourceSHA256=$observedAfterHash}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'input-version-bindings.json') -Encoding UTF8
}
$cad=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$verifiedProcess=Get-Process -Id $hostEvidence.ProcessId
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Stage1CoverageWindowProcess {
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
}
'@
[uint32]$windowOwner=0
[void][Stage1CoverageWindowProcess]::GetWindowThreadProcessId([IntPtr][long]$cad.HWND,[ref]$windowOwner)
if([long]$cad.HWND -ne [long]$hostEvidence.HWND -or $windowOwner -ne $hostEvidence.ProcessId -or $verifiedProcess.Path -ne $hostEvidence.Executable){throw 'Explicit host identity differs; no command/load executed.'}
$initial=$cad.ActiveDocument
if(!$cad.GetAcadState().IsQuiescent -or $initial.GetVariable('CMDNAMES') -ne '' -or $initial.GetVariable('CMDACTIVE') -ne 0){throw 'Verified host is busy; no command cancelled.'}
if(@($cad.ListARX()|Where-Object{$_ -eq 'tch_kernal.arx'}).Count -ne 1){throw 'Tianzheng kernel is not loaded.'}
$trusted='C:\Users\PC\AppData\Roaming\Tangent\TArch20V8\sys'
if(@($initial.GetVariable('TRUSTEDPATHS').Split(';')|ForEach-Object{[IO.Path]::GetFullPath($_).TrimEnd('\')}) -notcontains $trusted){throw 'Existing trusted directory not configured; security left unchanged.'}
$binary=Join-Path $project 'bin\NativeAccessProbe.arx'
$binaryHash=(Get-FileHash -LiteralPath $binary).Hash
$target=Join-Path $trusted ('S1Native_'+$binaryHash.Substring(0,12)+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.arx')
$ini=[IO.Path]::ChangeExtension($target,'.ini')
$python='C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$receipt=[ordered]@{Contract='stage1.coverage-host.v1';Mode=$Mode;Run=$run;Started=(Get-Date -Format o);HostEvidence=[IO.Path]::GetFullPath($HostEvidencePath);HostEvidenceSHA256=(Get-FileHash -LiteralPath $HostEvidencePath).Hash;ProcessId=$hostEvidence.ProcessId;HWND=[long]$cad.HWND;Host=$cad.FullName;Version=$cad.Version;InitialDocument=$initial.FullName;ObjectAPI='ObjectARX native; no COM';Orchestration='External COM module loading and native command dispatch only';Binary=$target;BinarySHA256=$binaryHash;ControllerSHA256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;SchemeSHA256=(Get-FileHash -LiteralPath $scheme).Hash;ReferenceSHA256=(Get-FileHash -LiteralPath $reference).Hash;SECURELOAD=$initial.GetVariable('SECURELOAD');SecuritySettingsChanged=$false;Load='not attempted';Unload='not attempted';Cleanup='not attempted';VerificationExitCode=$null;SourceFiles=[ordered]@{};ValidatorDependencies=[ordered]@{}}
$cpp=Join-Path $project 'src\NativeAccessProbe.cpp'
$receipt.SourceFiles['NativeAccessProbe.cpp']=(Get-FileHash -LiteralPath $cpp).Hash
$receipt.SourceFiles['NativeAccessProbe.vcxproj']=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeAccessProbe.vcxproj')).Hash
foreach($match in [regex]::Matches([IO.File]::ReadAllText($cpp),'#include\s+"([^"]+\.inc)"')){$name=$match.Groups[1].Value;$receipt.SourceFiles[$name]=(Get-FileHash -LiteralPath (Join-Path $project ('src\'+$name))).Hash}
$validatorNames=@($spec.Validator,'verify_scheme_native.py')
if($Mode -like 'Preview*' -or $Mode -eq 'Merge'){$validatorNames+='verify_native_plot.py'}
if($Mode -eq 'HorizontalAxis'){$validatorNames+='verify_object_family_native.py'}
foreach($name in $validatorNames){$receipt.ValidatorDependencies[$name]=(Get-FileHash -LiteralPath (Join-Path $stage ('tests\'+$name))).Hash}
if($spec.ObservedSource){$receipt.ObservedSource=$observedSource;$receipt.ObservedCopy=$observedCopy;$receipt.ObservedSourceSHA256=$observedAfterHash;$receipt.ObservedCopySHA256=$observedCopyHash}
if($spec.Control){$receipt.ControlSource=$resolvedControl;$receipt.ControlSourceSHA256=(Get-FileHash -LiteralPath $resolvedControl).Hash;$receipt.ControlCopySHA256=(Get-FileHash -LiteralPath $control).Hash;if($receipt.ControlCopySHA256 -ne $receipt.ControlSourceSHA256){throw 'Owned control copy differs from the bound source; nothing loaded.'}}
function Save-CoverageReceipt{$receipt|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $run 'host-receipt.json') -Encoding UTF8}
function Wait-CoverageIdle{
 $limit=(Get-Date).AddSeconds(20);$stable=0
 do{try{$ready=$cad.GetAcadState().IsQuiescent -and $cad.ActiveDocument.GetVariable('CMDNAMES') -eq '' -and $cad.ActiveDocument.GetVariable('CMDACTIVE') -eq 0}catch{$ready=$false};if($ready){$stable++}else{$stable=0};if($stable -ge 3){return};Start-Sleep -Milliseconds 250}while((Get-Date) -lt $limit)
 throw 'CAD did not become idle; no command cancelled and module retained.'
}
function Send-CoverageCommand($Document,[string]$Text){
 $limit=(Get-Date).AddSeconds(15)
 do{try{$Document.SendCommand($Text);return}catch{if($_.Exception.Message -notmatch '0x80010001|RPC_E_CALL_REJECTED|Call was rejected by callee'){throw};Start-Sleep -Milliseconds 250}}while((Get-Date) -lt $limit)
 throw 'Command dispatch rejected before execution for 15 seconds.'
}
function Wait-CoverageFile([string]$Name,[int]$Seconds){
 $file=Join-Path $run $Name;$limit=(Get-Date).AddSeconds($Seconds)
 while(!(Test-Path -LiteralPath $file) -and (Get-Date) -lt $limit){Start-Sleep -Milliseconds 200}
 if(!(Test-Path -LiteralPath $file)){throw ('Native report did not appear; module retained: '+$Name)}
}
$loaded=$false;$safe=$true
try{
 [IO.File]::Copy($binary,$target,$false)
 [IO.File]::WriteAllText($ini,"[probe]`r`nroot=$stage`r`nscheme=$scheme`r`nreference=$reference`r`noutput=$run`r`n",[Text.Encoding]::Unicode)
 Save-CoverageReceipt
 $cad.LoadARX($target);$loaded=$true;$receipt.Load='returned successfully';Save-CoverageReceipt
 Wait-CoverageIdle
 if($spec.Control){
  $safe=$false;Send-CoverageCommand $cad.ActiveDocument "S1OPENPROBECONTROL`n"
  Wait-CoverageFile 'native-open.done' 25
  $marker=Get-Content -LiteralPath (Join-Path $run 'native-open.done') -Raw -Encoding UTF8|ConvertFrom-Json
  if($marker.run -ne $run -or $marker.safeToUnload -ne $true){throw 'Native open completion identity differs.'}
  Wait-CoverageIdle;$safe=$true
  if($cad.ActiveDocument.FullName -ne $control){throw 'Plot control identity differs.'}
 }
 $safe=$false;Send-CoverageCommand $cad.ActiveDocument ($spec.Command+"`n")
 Wait-CoverageFile $spec.Report 120
 Wait-CoverageIdle;$safe=$true
 $priorValidationPreference=$ErrorActionPreference
 try{$ErrorActionPreference='Continue';& $python (Join-Path $stage ('tests\'+$spec.Validator)) (Join-Path $run $spec.Report) *> (Join-Path $run 'verify-result.txt');$validationExit=$LASTEXITCODE}finally{$ErrorActionPreference=$priorValidationPreference}
 $receipt.VerificationExitCode=$validationExit
 if($validationExit -ne 0){throw 'Native capability assertions failed; original evidence preserved.'}
}catch{$receipt.Error=$_.Exception.Message}
finally{
 if($loaded -and $safe){try{$cad.UnloadARX($target);$loaded=$false;$receipt.Unload='returned successfully'}catch{$receipt.Unload=$_.Exception.Message}}
 if($loaded -and !$safe){$receipt.Unload='not attempted: native completion/idle not confirmed'}
 if(!$loaded -and $receipt.Unload -eq 'returned successfully' -and (Get-FileHash -LiteralPath $target).Hash -eq $binaryHash){Remove-Item -LiteralPath $target;Remove-Item -LiteralPath $ini;$receipt.Cleanup='exact temporary ARX and INI removed after unload'}
 $receipt.Finished=(Get-Date -Format o);Save-CoverageReceipt
}
$receipt|ConvertTo-Json -Depth 7
Get-Content -LiteralPath (Join-Path $run 'verify-result.txt') -Tail 14 -Encoding UTF8 -ErrorAction SilentlyContinue
if($receipt.Error -or $receipt.VerificationExitCode -ne 0 -or $receipt.Unload -ne 'returned successfully' -or $receipt.Cleanup -ne 'exact temporary ARX and INI removed after unload'){exit 1}
