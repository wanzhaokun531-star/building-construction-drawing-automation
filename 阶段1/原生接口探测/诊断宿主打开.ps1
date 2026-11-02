$ErrorActionPreference='Stop'
$stage=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$run=Join-Path $stage ('runs\native-open-diagnostic-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$r=[ordered]@{Run=$run;Started=(Get-Date -Format o);Phase='initial';Events=@();SecuritySettingsChanged=$false}
function Save-Diagnostic { $r|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $run 'open-diagnostic.json') -Encoding UTF8 }
function Event-Diagnostic($name,$value) { $r.Events+=@{Time=(Get-Date -Format o);Name=$name;Value=$value};Save-Diagnostic }
Save-Diagnostic
$cad=$null;$original=$null;$prior=@{};$changed=@()
try {
  $cad=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
  $original=$cad.ActiveDocument
  if(!$cad.GetAcadState().IsQuiescent -or $original.GetVariable('CMDNAMES') -ne ''){throw 'Host is busy; no input sent.'}
  $r.OriginalDocument=$original.FullName;$r.Host=$cad.FullName;$r.Security=$original.GetVariable('SECURELOAD')
  $snapshot=[IO.Path]::GetFullPath((Join-Path $stage '..\阶段0\输入快照\方案平面图.dwg'))
  $copy=Join-Path $run 'control.dwg';$r.Input=$snapshot;$r.InputSHA256=(Get-FileHash -LiteralPath $snapshot).Hash
  if($r.InputSHA256 -ne '5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E'){throw 'Frozen scheme identity differs.'}
  [IO.File]::Copy($snapshot,$copy,$false)
  [IO.File]::SetAttributes($copy,([IO.File]::GetAttributes($copy) -band (-bnot [IO.FileAttributes]::ReadOnly)))
  foreach($name in @('LOGFILEPATH','LOGFILEMODE','CMDECHO')){$prior[$name]=$original.GetVariable($name)}
  $r.PriorSettings=$prior
  $original.SetVariable('LOGFILEPATH',$run+'\');$changed+='LOGFILEPATH'
  $original.SetVariable('LOGFILEMODE',1);$changed+='LOGFILEMODE'
  $original.SetVariable('CMDECHO',1);$changed+='CMDECHO'
  Event-Diagnostic 'log-path' $original.GetVariable('LOGFILENAME')
  $r.Phase='native-open'
  $input='_.OPEN'+[char]10+'"'+$copy+'"'+[char]10
  Event-Diagnostic 'sent-open-text' $input
  $original.SendCommand($input)
  Event-Diagnostic 'send-returned-lastprompt' $cad.ActiveDocument.GetVariable('LASTPROMPT')
  $deadline=(Get-Date).AddSeconds(10)
  do {
    $owned=@($cad.Documents|Where-Object{$_.FullName -eq $copy})|Select-Object -First 1
    if($owned){break}
    Start-Sleep -Milliseconds 250
  }while((Get-Date) -lt $deadline)
  Event-Diagnostic 'after-open' @{Active=$cad.ActiveDocument.FullName;LastPrompt=$cad.ActiveDocument.GetVariable('LASTPROMPT');CmdNames=$cad.ActiveDocument.GetVariable('CMDNAMES');Opened=($null -ne $owned)}
  $r.Phase='finished';$r.Opened=($null -ne $owned)
}catch{$r.Error=$_.Exception.Message;$r.ErrorDetail=$_.ToString()}
finally {
  if($original){
    $restored=@{}
    foreach($name in @('CMDECHO','LOGFILEMODE','LOGFILEPATH')){
      if($changed -contains $name){try{$original.SetVariable($name,$prior[$name]);$restored[$name]=($original.GetVariable($name) -eq $prior[$name])}catch{$restored[$name]=$_.Exception.Message}}
    }
    $r.Restored=$restored
  }
  if($r.Input){$r.InputFinalSHA256=(Get-FileHash -LiteralPath $r.Input).Hash}
  $r.Finished=(Get-Date -Format o);Save-Diagnostic
}
$r|ConvertTo-Json -Depth 8
if($r.Error -or !$r.Opened){exit 1}
