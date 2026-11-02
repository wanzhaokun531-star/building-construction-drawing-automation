$ErrorActionPreference='Stop'
$c=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$d=$c.ActiveDocument
if($d.FullName -notlike '*建筑施工图自动化\阶段1\runs\*\retry-control.dwg'){throw 'Expected this task control drawing.'}
if(!$c.GetAcadState().IsQuiescent){throw 'CAD busy.'}
$run=Split-Path -Parent $d.FullName
$oldPath=$d.GetVariable('LOGFILEPATH');$oldMode=$d.GetVariable('LOGFILEMODE')
$logPath=$null
try {
  $d.SetVariable('LOGFILEMODE',0)
  $d.SetVariable('LOGFILEPATH',($run+'\'))
  $d.SetVariable('LOGFILEMODE',1)
  $logPath=$d.GetVariable('LOGFILENAME')
  $arx=(Join-Path $PSScriptRoot 'bin\Stage1Probe.arx').Replace('\','/')
  $d.SendCommand("(progn (arxload `"$arx`" `"ARXLOAD_FAILED_WITH_LOG`") (princ))`n")
} finally {
  $d.SetVariable('LOGFILEMODE',0)
  $d.SetVariable('LOGFILEPATH',$oldPath)
  $d.SetVariable('LOGFILEMODE',$oldMode)
  [pscustomobject]@{Log=$logPath;OriginalPath=$oldPath;OriginalMode=$oldMode;Restored=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'log-capture.json') -Encoding UTF8
}
Get-ChildItem -LiteralPath $run -Filter '*.log'|Select-Object FullName,Length|ConvertTo-Json
