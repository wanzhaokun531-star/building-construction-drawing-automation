param([ValidateSet('Scan','Exercise','AxisAlt','Scheme','SchemeAxis','WallAxis','Renumber','Styles','DimPoints','AxisLabels2','AxisLabels3','AxisLabels2C','AxisLabels3C')][string]$Mode='Scan')
$ErrorActionPreference='Stop'
$stage=[IO.Path]::GetFullPath($PSScriptRoot)
$project=Join-Path $stage '原生接口探测'
$run=Join-Path $stage ('runs\native-access-'+$Mode.ToLower()+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$r=[ordered]@{Run=$run;Mode=$Mode;Command=$null;Started=(Get-Date -Format o);ObjectAccess='ObjectARX native; no COM';Orchestration='External AutoCAD COM client';SecuritySettingsChanged=$false;Load='not executed';Unload='not executed';Cleanup='not executed';VerificationExitCode=$null;Boundary=$null;BoundaryHistory=@();HostObservations=@()}
function Save-Receipt{$r|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $run 'host-receipt.json') -Encoding UTF8}
function Set-ReceiptBoundary([string]$boundary){
  $r.Boundary=$boundary
  $r.BoundaryHistory+=,[ordered]@{Boundary=$boundary;At=(Get-Date -Format o)}
  Save-Receipt
}
function Get-ReceiptErrorDetails($record){
  [ordered]@{Message=$record.Exception.Message;ExceptionType=$record.Exception.GetType().FullName;HResult=$record.Exception.HResult;FullyQualifiedErrorId=$record.FullyQualifiedErrorId;Category=$record.CategoryInfo.ToString();Position=$record.InvocationInfo.PositionMessage;ScriptStackTrace=$record.ScriptStackTrace}
}
function Add-HostObservation{
  if(!$c){return}
  $observation=[ordered]@{At=(Get-Date -Format o);Boundary=$r.Boundary;ActiveDocument=$null;LastPrompt=$null;CmdNames=$null;IsQuiescent=$null;ReadErrors=@()}
  $observedDocument=$null
  try{$observedDocument=$c.ActiveDocument;$observation.ActiveDocument=$observedDocument.FullName}catch{$observation.ReadErrors+=('ActiveDocument: '+$_.Exception.Message)}
  if($observedDocument){
    try{$observation.LastPrompt=$observedDocument.GetVariable('LASTPROMPT')}catch{$observation.ReadErrors+=('LASTPROMPT: '+$_.Exception.Message)}
    try{$observation.CmdNames=$observedDocument.GetVariable('CMDNAMES')}catch{$observation.ReadErrors+=('CMDNAMES: '+$_.Exception.Message)}
  }
  try{$observation.IsQuiescent=$c.GetAcadState().IsQuiescent}catch{$observation.ReadErrors+=('IsQuiescent: '+$_.Exception.Message)}
  $r.HostObservations+=,$observation
}
$loaded=$false
$c=$null
$d=$null
$asyncMode=$Mode -in @('AxisLabels2C','AxisLabels3C')
$nativeCompletionConfirmed=!$asyncMode
try{
Set-ReceiptBoundary 'run-created'
$binary=Join-Path $project 'bin\NativeAccessProbe.arx'
$r.SourceBinary=$binary
Set-ReceiptBoundary 'binary-hash'
$hash=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
$r.BinarySHA256=$hash
Set-ReceiptBoundary 'host-connect'
$c=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$d=$c.ActiveDocument
$r.Host=$c.FullName;$r.HostVersion=$c.Version;$r.HWND=$c.HWND;$r.ControlDocument=$d.FullName
$controlOpenMethod='existing task-owned document'
Set-ReceiptBoundary 'host-idle-check'
if(!$c.GetAcadState().IsQuiescent -or $d.GetVariable('CMDNAMES') -ne ''){throw 'CAD is busy; no command cancelled.'}
if($Mode -in @('WallAxis','Renumber','DimPoints','AxisLabels2','AxisLabels3','AxisLabels2C','AxisLabels3C') -or $d.FullName -notlike 'D:\Codex\建筑施工图自动化\阶段1\runs\*'){
  $owned=$null
  if($Mode -notin @('WallAxis','Renumber','DimPoints','AxisLabels2','AxisLabels3','AxisLabels2C','AxisLabels3C')){$owned=@($c.Documents|Where-Object{$_.FullName -like 'D:\Codex\建筑施工图自动化\阶段1\runs\*'})|Select-Object -First 1}
  if(!$owned){
    Set-ReceiptBoundary 'control-source'
    $controlSource=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\方案平面图.dwg'))
    if($Mode -eq 'Renumber'){
      $controlSource=Join-Path $stage 'runs\native-access-schemeaxis-20261028-184302-829\scheme-axis-adjust.dwg'
      if((Get-FileHash -LiteralPath $controlSource).Hash -ne 'D8E92AE3AA956616401E0C58079AAA6CFCC3D2497277576D90F1A7198109B2D2'){throw 'Renumber fixture identity changed.'}
    }
    if($Mode -eq 'DimPoints'){
      $controlSource=Join-Path $stage 'runs\native-access-scheme-20261028-175845-433\scheme-added-dimension.dwg'
      if((Get-FileHash -LiteralPath $controlSource).Hash -ne '4A4ED7D3A87573720D36A2C25BABAD9A64FEAF2AA9E811A0F80BCFDCCBE61332'){throw 'Dimension fixture identity changed.'}
    }
    $controlPath=Join-Path $run 'control.dwg'
    $r.ControlSource=$controlSource;$r.RequestedControlDocument=$controlPath
    Set-ReceiptBoundary 'control-copy'
    [IO.File]::Copy($controlSource,$controlPath,$false)
    [IO.File]::SetAttributes($controlPath,([IO.File]::GetAttributes($controlPath) -band (-bnot [IO.FileAttributes]::ReadOnly)))
    Set-ReceiptBoundary 'activex-open'
    try{$owned=$c.Documents.Open($controlPath,$false);$controlOpenMethod='ActiveX Documents.Open';$r.ControlOpenMethod=$controlOpenMethod}catch{
      $openMessage=$_.Exception.Message
      $r.ActiveXOpenError=Get-ReceiptErrorDetails $_
      Add-HostObservation
      Save-Receipt
      if($openMessage -notmatch '执行环境无效|Invalid execution context'){throw}
      Set-ReceiptBoundary 'native-open-preflight'
      if(!$c.GetAcadState().IsQuiescent -or $c.ActiveDocument.GetVariable('CMDNAMES') -ne '' -or $c.ActiveDocument.GetVariable('SDI') -ne 0 -or $c.ActiveDocument.GetVariable('FILEDIA') -ne 0){throw 'Native OPEN fallback requires an idle MDI host and existing FILEDIA=0; no settings changed.'}
      $owned=@($c.Documents|Where-Object{$_.FullName -eq $controlPath})|Select-Object -First 1
      if(!$owned){
        $r.NativeOpenCommand="_.OPEN`n"+'"'+$controlPath+'"'+"`n"
        Set-ReceiptBoundary 'native-open-send'
        $c.ActiveDocument.SendCommand($r.NativeOpenCommand)
        $r.NativeOpenSendReturned=$true
        Set-ReceiptBoundary 'native-open-wait'
        $openDeadline=(Get-Date).AddSeconds(20)
        do{try{$owned=@($c.Documents|Where-Object{$_.FullName -eq $controlPath})|Select-Object -First 1}catch{$owned=$null};if(!$owned){Start-Sleep -Milliseconds 250}}while(!$owned -and (Get-Date) -lt $openDeadline)
        if(!$owned){throw 'Native OPEN did not produce this run control document; no other document changed.'}
      }
      $controlOpenMethod='native _.OPEN after ActiveX invalid execution context; existing SDI=0 FILEDIA=0'
      $r.ControlOpenMethod=$controlOpenMethod
    }
  }
  Set-ReceiptBoundary 'control-ready-wait'
  $deadline=(Get-Date).AddSeconds(20);$ready=$false;$stable=0
  do{try{if($c.GetAcadState().IsQuiescent -and $c.ActiveDocument.FullName -ne $owned.FullName){$owned.Activate()};$ready=$c.GetAcadState().IsQuiescent -and $c.ActiveDocument.FullName -eq $owned.FullName -and $owned.GetVariable('CMDNAMES') -eq ''}catch{$ready=$false};if($ready){$stable++}else{$stable=0};if($stable -ge 3){break};Start-Sleep -Milliseconds 250}while((Get-Date)-lt$deadline)
  if($stable -lt 3){throw 'Task-owned control document did not activate and remain idle.'}
  $d=$owned
  $r.ControlDocument=$d.FullName
}
Set-ReceiptBoundary 'host-preflight'
if($d.FullName -notlike 'D:\Codex\建筑施工图自动化\阶段1\runs\*'){throw 'Expected a task-owned control/display document.'}
if(@($c.ListARX()|Where-Object{$_ -eq 'tch_kernal.arx'}).Count -ne 1){throw 'The selected host does not have Tianzheng loaded.'}
$trusted='C:\Users\PC\AppData\Roaming\Tangent\TArch20V8\sys'
$trustedPaths=@($d.GetVariable('TRUSTEDPATHS').Split(';')|ForEach-Object{[IO.Path]::GetFullPath($_).TrimEnd('\')})
if($trustedPaths -notcontains $trusted.TrimEnd('\')){throw 'Runtime directory is not already trusted.'}
$target=Join-Path $trusted ('S1Native_'+$hash.Substring(0,12)+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.arx')
$ini=[IO.Path]::ChangeExtension($target,'.ini')
$scheme=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\方案平面图.dwg'))
$reference=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\施工图平面参考图.dwg'))
$command=switch($Mode){'Scan'{'S1NATIVESCAN'}'Exercise'{'S1NATIVEEXERCISE'}'AxisAlt'{'S1AXISALT'}'Scheme'{'S1SCHEMEVERIFY'}'SchemeAxis'{'S1SCHEMEAXIS'}'WallAxis'{'S1WALLAXISVERIFY'}'Renumber'{'S1AXISRENUMBER'}'Styles'{'S1STYLEAUDIT'}'DimPoints'{'S1DIMPOINTS'}'AxisLabels2'{'S1AXISLABELS2'}'AxisLabels3'{'S1AXISLABELS3'}'AxisLabels2C'{'S1AXISLABELS2C'}'AxisLabels3C'{'S1AXISLABELS3C'}}
$validator=switch($Mode){'Scan'{'verify_native_access.py'}'Exercise'{'verify_native_exercise.py'}'AxisAlt'{'verify_native_axis_alt.py'}'Scheme'{'verify_scheme_native.py'}'SchemeAxis'{'verify_scheme_axis.py'}'WallAxis'{'verify_wall_axis_native.py'}'Renumber'{'verify_axis_renumber_native.py'}'Styles'{'verify_native_styles.py'}'DimPoints'{'verify_dim_points_native.py'}'AxisLabels2'{'verify_axis_labels_native.py'}'AxisLabels3'{'verify_axis_labels_native.py'}'AxisLabels2C'{'verify_axis_labels_native.py'}'AxisLabels3C'{'verify_axis_labels_native.py'}}
$resultFile=switch($Mode){'Scan'{'native-access.json'}'Exercise'{'native-exercise.json'}'AxisAlt'{'native-axis-alt.json'}'Scheme'{'scheme-native.json'}'SchemeAxis'{'scheme-axis.json'}'WallAxis'{'wall-axis-native.json'}'Renumber'{'axis-renumber-native.json'}'Styles'{'native-styles.json'}'DimPoints'{'dim-points-native.json'}'AxisLabels2'{'axis-labels-native.json'}'AxisLabels3'{'axis-labels-native.json'}'AxisLabels2C'{'axis-labels-native.json'}'AxisLabels3C'{'axis-labels-native.json'}}
$r.Command=$command;$r.Binary=$target
Set-ReceiptBoundary 'input-evidence-hash'
$r.SourceSHA256=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeAccessProbe.cpp')).Hash
$r.ProjectSHA256=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeAccessProbe.vcxproj')).Hash
$r.ValidatorSHA256=(Get-FileHash -LiteralPath (Join-Path $stage ('tests\'+$validator))).Hash
$r.SchemeSHA256=(Get-FileHash -LiteralPath $scheme).Hash
$r.ReferenceSHA256=(Get-FileHash -LiteralPath $reference).Hash
$r.OriginalSecurity=$d.GetVariable('SECURELOAD')
$r.ControlOpenMethod=$controlOpenMethod
$dependencies=[ordered]@{}
$dependencies[$validator]=(Get-FileHash -LiteralPath (Join-Path $stage ('tests\'+$validator))).Hash
if($Mode -eq 'AxisAlt'){$dependencies['verify_native_exercise.py']=(Get-FileHash -LiteralPath (Join-Path $stage 'tests\verify_native_exercise.py')).Hash}
if($Mode -eq 'SchemeAxis'){$dependencies['verify_scheme_native.py']=(Get-FileHash -LiteralPath (Join-Path $stage 'tests\verify_scheme_native.py')).Hash}
if($Mode -eq 'WallAxis'){$dependencies['verify_scheme_native.py']=(Get-FileHash -LiteralPath (Join-Path $stage 'tests\verify_scheme_native.py')).Hash}
if($Mode -eq 'Renumber'){$dependencies['verify_scheme_native.py']=(Get-FileHash -LiteralPath (Join-Path $stage 'tests\verify_scheme_native.py')).Hash}
if($Mode -eq 'DimPoints'){$dependencies['verify_scheme_native.py']=(Get-FileHash -LiteralPath (Join-Path $stage 'tests\verify_scheme_native.py')).Hash}
if($Mode -in @('AxisLabels2','AxisLabels3','AxisLabels2C','AxisLabels3C')){$dependencies['verify_scheme_native.py']=(Get-FileHash -LiteralPath (Join-Path $stage 'tests\verify_scheme_native.py')).Hash}
$r.ValidatorDependencies=$dependencies
$sourceFiles=[ordered]@{'NativeAccessProbe.cpp'=$r.SourceSHA256}
if(Test-Path -LiteralPath (Join-Path $project 'src\NativeStyleAudit.inc')){$sourceFiles['NativeStyleAudit.inc']=(Get-FileHash -LiteralPath (Join-Path $project 'src\NativeStyleAudit.inc')).Hash}
$r.SourceFiles=$sourceFiles
if($Mode -in @('Renumber','DimPoints')){$r.FixturePath=$controlSource;$r.FixtureSHA256=(Get-FileHash -LiteralPath $controlSource).Hash}
  Set-ReceiptBoundary 'staging-module'
  [IO.File]::Copy($binary,$target,$false)
  [IO.File]::WriteAllText($ini,"[probe]`r`nroot=$stage`r`nscheme=$scheme`r`nreference=$reference`r`noutput=$run`r`n",[Text.Encoding]::Unicode)
  Save-Receipt
  $run|Set-Content -LiteralPath (Join-Path $project '最新运行目录.txt') -Encoding UTF8
  Set-ReceiptBoundary 'module-load'
  $c.LoadARX($target);$loaded=$true;$r.Load='returned successfully';Save-Receipt
  $r.ProbeCommand=$command+"`n"
  Set-ReceiptBoundary 'probe-send'
  $d.SendCommand($command+"`n")
  $r.ProbeSendReturned=$true;Save-Receipt
  if($asyncMode){
    Set-ReceiptBoundary 'native-completion-wait'
    $donePath=Join-Path $run 'axis-labels-native.done'
    $completionDeadline=(Get-Date).AddSeconds(30)
    while(!(Test-Path -LiteralPath $donePath) -and (Get-Date) -lt $completionDeadline){Start-Sleep -Milliseconds 100}
    if(!(Test-Path -LiteralPath $donePath)){throw 'Native coroutine completion timed out; module and INI retained without cancelling CAD.'}
    $completion=Get-Content -LiteralPath $donePath -Raw -Encoding UTF8|ConvertFrom-Json
    if($completion.run -ne $run -or $completion.safeToUnload -ne $true){throw 'Native completion marker identity or cleanup state invalid; module retained.'}
    $nativeCompletionConfirmed=$true
    $r.NativeCompletion=$completion
    Set-ReceiptBoundary 'post-completion-idle'
    $idleDeadline=(Get-Date).AddSeconds(20);$idleStable=0
    do{try{$idle=$c.GetAcadState().IsQuiescent -and $c.ActiveDocument.FullName -like ($run+'\*') -and $c.ActiveDocument.GetVariable('CMDNAMES') -eq ''}catch{$idle=$false};if($idle){$idleStable++}else{$idleStable=0};if($idleStable -ge 3){break};Start-Sleep -Milliseconds 250}while((Get-Date) -lt $idleDeadline)
    if($idleStable -lt 3){$nativeCompletionConfirmed=$false;throw 'Native completion exists but owned document did not become idle; module retained.'}
  }
  $python='C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
  Set-ReceiptBoundary 'validator'
  & $python (Join-Path $stage ('tests\'+$validator)) (Join-Path $run $resultFile) *> (Join-Path $run 'verify-result.txt')
  $r.VerificationExitCode=$LASTEXITCODE
  if($LASTEXITCODE -ne 0){throw 'Native probe evidence validation failed; inspect result and host messages.'}
}catch{
  $r.Error=$_.Exception.Message
  $r.ErrorDetails=Get-ReceiptErrorDetails $_
  $r.FailureBoundary=$r.Boundary
  Add-HostObservation
}
finally{
  if($loaded -and $nativeCompletionConfirmed){Set-ReceiptBoundary 'module-unload';try{$c.UnloadARX($target);$loaded=$false;$r.Unload='returned successfully'}catch{$r.Unload=$_.Exception.Message;$r.UnloadErrorDetails=Get-ReceiptErrorDetails $_}}
  if($loaded -and !$nativeCompletionConfirmed){$r.Unload='not attempted: native asynchronous completion not confirmed'}
  if(!$loaded -and $r.Unload -eq 'returned successfully' -and (Get-FileHash -LiteralPath $target).Hash -eq $hash){Set-ReceiptBoundary 'module-cleanup';Remove-Item -LiteralPath $target;Remove-Item -LiteralPath $ini;$r.Cleanup='exact temporary ARX and INI removed after unload'}
  $r.Finished=(Get-Date -Format o);Save-Receipt
}
$r|ConvertTo-Json -Depth 10
Get-Content -LiteralPath (Join-Path $run 'verify-result.txt') -Tail 18 -ErrorAction SilentlyContinue
if($r.Error -or $r.VerificationExitCode -ne 0 -or $r.Unload -ne 'returned successfully' -or $r.Cleanup -ne 'exact temporary ARX and INI removed after unload'){exit 1}
exit 0
