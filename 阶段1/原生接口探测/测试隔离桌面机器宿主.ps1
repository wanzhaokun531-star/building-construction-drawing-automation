param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference='Stop'
$run=[IO.Path]::GetFullPath($RunDirectory).TrimEnd('\');$runsRoot=[IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $PSScriptRoot) 'runs'))+'\'
if(!$run.StartsWith($runsRoot,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $run)){throw 'A new unused stage1/runs directory is required.'}
[void][IO.Directory]::CreateDirectory($run)
Add-Type -LiteralPath (Join-Path $PSScriptRoot 'IsolatedCadHostNative.cs')
$exe=Join-Path $run 'IsolatedDesktopGuiTestHost.exe'
Add-Type -LiteralPath (Join-Path $PSScriptRoot 'IsolatedDesktopGuiTestHost.cs') -OutputAssembly $exe -OutputType WindowsApplication
$facts=[ordered]@{contract='stage1.isolation-crossdesktop-machine-test.v1';recorded=[DateTime]::UtcNow.ToString('o');cadStarted=$false;checks=[ordered]@{};samples=@();actions=@();testProcessIds=@();errors=@()}
$before=[CodexStage1Isolation.Native]::InputDesktopName();$monitor=[CodexStage1Isolation.EventMonitor]::new();$worker=$null;$tip=$null
try{
 $monitor.Start()
 $worker=[CodexStage1Isolation.DesktopWorker]::new(('CodexS1_'+[Guid]::NewGuid().ToString('N')))
 $worker.LaunchTestHost($exe,'Codex S1 isolation machine test',$run);$facts.testProcessIds+=@($worker.ProcessId)
 $until=[DateTime]::UtcNow.AddSeconds(5);do{Start-Sleep -Milliseconds 100;$windows=@($worker.Windows())}while(!$windows.Count -and [DateTime]::UtcNow -lt $until)
 $proof=[CodexStage1Isolation.Native]::InspectOwnedDesktop($worker.DesktopName,$worker.ProcessId,$worker.InitialThreadId);$facts.samples+=@($proof)
 $facts.checks.ActualPrivateChildCreated=$worker.Created;$facts.checks.EnumDesktopPidAndThread=($proof.PidFound -and $proof.ExpectedThreadFound)
 $facts.checks.EnumContextStamped=(@($windows|Where-Object{$_.EnumerationDesktop -cne $worker.DesktopName -or $_.PidAtEnumeration -ne $worker.ProcessId -or $_.ThreadAtEnumeration -eq 0}).Count -eq 0 -and $windows.Count -gt 0)
 $facts.checks.ForeignThreadDesktopUnknownPreserved=(@($windows|Where-Object{!$_.ThreadDesktopKnown -and $_.ThreadDesktopError -eq 5}).Count -gt 0)
 $facts.checks.UnknownDialogRefused=$false
 foreach($window in $windows){if($window.ClassName -eq '#32770'){$refused=$worker.PostCloseBenignStartupTip($window.Hwnd);$facts.actions+=@($refused);$facts.checks.UnknownDialogRefused=(!$refused.Matched -and !$refused.Posted)}}
 $refusal=$false;try{[void][CodexStage1Isolation.Native]::StopVerified($worker.ProcessId,$worker.InitialThreadId,$worker.Image,'0',$worker.DesktopName)}catch{$refusal=$true}
 $facts.checks.WrongCreationTimeStopRefused=($refusal -and !$worker.HasExited)
 $code=[CodexStage1Isolation.Native]::StopVerified($worker.ProcessId,$worker.InitialThreadId,$worker.Image,$worker.CreationFileTime,$worker.DesktopName)
 $facts.checks.ExactOwnHelperExplicitStop=($code -eq 70 -and $worker.HasExited)
 $worker.Dispose();$facts.checks.FirstOwnedHandlesClosed=($worker.DesktopClosed -and $worker.ProcessHandlesClosed)
 $tip=[CodexStage1Isolation.DesktopWorker]::new(('CodexS1_'+[Guid]::NewGuid().ToString('N')))
 $tip.LaunchTestHost($exe,'日积月累',$run);$facts.testProcessIds+=@($tip.ProcessId)
 $until=[DateTime]::UtcNow.AddSeconds(5);do{Start-Sleep -Milliseconds 100;$windows=@($tip.Windows())}while(!@($windows|Where-Object{$_.ClassName -eq '#32770' -and $_.Title -ceq '日积月累'}).Count -and [DateTime]::UtcNow -lt $until)
 $facts.samples+=@([CodexStage1Isolation.Native]::InspectOwnedDesktop($tip.DesktopName,$tip.ProcessId,$tip.InitialThreadId))
 $exact=@($windows|Where-Object{$_.ClassName -eq '#32770' -and $_.Title -ceq '日积月累'})
 $facts.checks.ExactKnownStartupTipObserved=($exact.Count -eq 1)
 if($exact.Count -eq 1){$posted=$tip.PostCloseBenignStartupTip($exact[0].Hwnd);$facts.actions+=@($posted);$facts.checks.ExactKnownStartupTipPosted=($posted.Matched -and $posted.Posted -and $posted.Error -eq 0)}
 $until=[DateTime]::UtcNow.AddSeconds(5);while(!$tip.HasExited -and [DateTime]::UtcNow -lt $until){Start-Sleep -Milliseconds 100}
 $facts.checks.KnownTipClosedNormally=($tip.HasExited -and $tip.ExitCode -eq 0)
 $tip.Dispose();$facts.checks.SecondOwnedHandlesClosed=($tip.DesktopClosed -and $tip.ProcessHandlesClosed)
 $facts.checks.InputDesktopUnchanged=([CodexStage1Isolation.Native]::InputDesktopName() -ceq $before)
}catch{$facts.errors+=@($_.Exception.ToString())}finally{
 $monitor.Stop();$events=@($monitor.Drain());$facts.events=$events
 $facts.checks.NoHelperForegroundOrShow=(@($events|Where-Object{$facts.testProcessIds -contains $_.Pid -and $_.Event -in @(3,0x8002)}).Count -eq 0)
 $facts.checks.MonitorNoError=(!$monitor.Error -and $monitor.Dropped -eq 0 -and $monitor.Started -and $monitor.Stopped)
 if($worker){$worker.Dispose()};if($tip){$tip.Dispose()};$monitor.Dispose()
}
$facts.result=if($facts.errors.Count -eq 0 -and @($facts.checks.GetEnumerator()|Where-Object{!$_.Value}).Count -eq 0 -and $facts.checks.Count -ge 15){'PASS'}else{'FAIL'}
[IO.File]::WriteAllText((Join-Path $run 'machine-test.json'),($facts|ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false))
$facts|ConvertTo-Json -Depth 15
if($facts.result -ne 'PASS'){exit 1}
