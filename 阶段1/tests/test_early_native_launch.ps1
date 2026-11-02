$ErrorActionPreference='Stop'
Add-Type -LiteralPath (Join-Path $PSScriptRoot '..\原生接口探测\IsolatedCadHostNative.cs')
$taskLegacy=[CodexStage1Isolation.DesktopWorker]::BuildCadCommandLine('D:\CAD Path\acad.exe','CodexS1Native_test','D:\a\acadiso.dwt','D:\r\boot.scr',$null)
if($taskLegacy.Contains(' /ld ') -or !$taskLegacy.EndsWith(' /b "D:\r\boot.scr"')){throw 'legacy launch changed'}
$taskEarly=[CodexStage1Isolation.DesktopWorker]::BuildCadCommandLine('D:\CAD Path\acad.exe','CodexS1Native_test','D:\a\acadiso.dwt','D:\r\boot.scr','C:\Program Files\owned\S1.arx')
if(!$taskEarly.Contains(' /ld "C:\Program Files\owned\S1.arx"')){throw 'early native module missing or not quoted'}
if($taskEarly.IndexOf(' /ld ') -gt $taskEarly.IndexOf(' /b ')){throw 'bootstrap must be supplied before script switch'}
if(!$taskEarly.StartsWith('"D:\CAD Path\acad.exe" /nologo /p "CodexS1Native_test"')){throw 'executable/profile quoting changed'}
Write-Output 'EARLY_NATIVE_LAUNCH assertions=4 failed=0; no CAD or desktop created'
