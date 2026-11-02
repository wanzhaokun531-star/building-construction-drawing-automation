$ErrorActionPreference='Stop'
$run=Join-Path $PSScriptRoot ('runs\retry-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($run) | Out-Null
$arx=Join-Path $PSScriptRoot 'bin\Stage1Probe.arx'
$scheme=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\阶段0\输入快照\方案平面图.dwg'))
$reference=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\阶段0\输入快照\施工图平面参考图.dwg'))
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'bin\probe.ini'),"[probe]`r`nscheme=$scheme`r`nreference=$reference`r`noutput=$run`r`n",[Text.Encoding]::Unicode)
$c=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$deadline=(Get-Date).AddSeconds(30)
do {try {$idle=$c.GetAcadState().IsQuiescent -and $c.ActiveDocument.GetVariable('CMDNAMES') -eq ''} catch {$idle=$false}; if(!$idle){Start-Sleep -Milliseconds 250}} while(!$idle -and (Get-Date)-lt $deadline)
if(!$idle){throw 'CAD is busy; no user command was cancelled.'}
$seed=Join-Path $PSScriptRoot 'runs\probe-20261028-163410-269\stage1-control.dwg'
$controlPath=Join-Path $run 'retry-control.dwg'
[IO.File]::Copy($seed,$controlPath,$false)
$d=$c.Documents.Open($controlPath,$false)
$deadline=(Get-Date).AddSeconds(30)
do {try {$idle=$c.GetAcadState().IsQuiescent -and $c.ActiveDocument.GetVariable('CMDNAMES') -eq ''} catch {$idle=$false}; if(!$idle){Start-Sleep -Milliseconds 250}} while(!$idle -and (Get-Date)-lt $deadline)
if(!$idle){throw 'New control document did not become idle.'}
$receipt=[ordered]@{Run=$run;PID=(Get-Process acad).Id;Arx=$arx;SHA256=(Get-FileHash -LiteralPath $arx).Hash;Security=$d.GetVariable('SECURELOAD');Command='(vl-catch-all-apply arxload) then S1READ';OriginalDwgWritten=$false;Start=(Get-Date -Format o)}
$receipt|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'loader-attempt.json') -Encoding UTF8
$run|Set-Content -LiteralPath (Join-Path $PSScriptRoot '最新运行目录.txt') -Encoding UTF8
function LispString([string]$text) { '"'+$text.Replace('\','/').Replace('"','\"')+'"' }
$logPath=Join-Path $run 'arxload-result.txt'
$lisp="(progn (setq s1r (vl-catch-all-apply 'arxload (list $(LispString $arx) `"ARXLOAD_RETURNED_FAILURE`"))) (setq s1f (open $(LispString $logPath) `"w`")) (write-line (vl-princ-to-string s1r) s1f) (close s1f) (princ))`n"
$d.SendCommand($lisp)
$receipt.End=(Get-Date -Format o)
$receipt.Loaded=@($c.ListARX()|Where-Object{$_ -match 'Stage1Probe'})
$receipt|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'loader-attempt.json') -Encoding UTF8
if($receipt.Loaded.Count -gt 0){$d.SendCommand("S1READ`n")}
$receipt|ConvertTo-Json
