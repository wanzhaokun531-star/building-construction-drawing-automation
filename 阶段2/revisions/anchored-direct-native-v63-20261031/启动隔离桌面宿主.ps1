param(
 [Parameter(Mandatory=$true,ParameterSetName='Launch')][string]$BootScriptPath,
 [Parameter(Mandatory=$true,ParameterSetName='Launch')][Parameter(Mandatory=$true,ParameterSetName='Stop')][string]$RunDirectory,
 [Parameter(ParameterSetName='Launch')][string]$AcadPath='D:\CAD2020\AutoCAD 2020\acad.exe',
 [Parameter(ParameterSetName='Launch')][string]$Profile='TArch20V8',
 [Parameter(Mandatory=$true,ParameterSetName='Launch')][string]$Template,
 [Parameter(ParameterSetName='Launch')][string]$EarlyNativeModule,
 [Parameter(ParameterSetName='Launch')][ValidatePattern('^[0-9a-fA-F]{32}$')][string]$Nonce,
 [Parameter(ParameterSetName='Launch')][ValidateRange(10,86400)][int]$TimeoutSeconds=900,
 [Parameter(ParameterSetName='Launch')][ValidateRange(100,5000)][int]$SampleMilliseconds=500,
 [Parameter(ParameterSetName='Launch')][uint32[]]$RelatedProcessIds=@(),
 [Parameter(ParameterSetName='Launch')][switch]$CloseOwnStartupTips,
 [Parameter(ParameterSetName='Launch')][string]$ReadyMarkerName='quiet-ready.json',
 [Parameter(ParameterSetName='Launch')][string]$QuitMarkerName='native.quit.done',
 [Parameter(Mandatory=$true,ParameterSetName='Stop')][switch]$StopOwnedWorker,
 [Parameter(Mandatory=$true,ParameterSetName='Stop')][ValidatePattern('^[0-9a-fA-F]{32}$')][string]$ExpectedNonce,
 [Parameter(Mandatory=$true,ParameterSetName='SelfTest')][switch]$SelfTest,
 [Parameter(ParameterSetName='SelfTest')][string]$SelfTestOutputPath
)
$ErrorActionPreference='Stop'
function Test-PostExitPrivateEnumerationFailure {
 param([string]$Message,[bool]$OriginalProcessSignaled,[bool]$NativeWin32Failure)
 return ($OriginalProcessSignaled -and $NativeWin32Failure -and $Message -ceq 'EnumDesktopWindows private desktop')
}

$helper=Join-Path $PSScriptRoot 'IsolatedCadHostNative.cs'
if(!('CodexStage1Isolation.Native' -as [type])){Add-Type -LiteralPath $helper}
if(!('CodexStage1AtomicFile' -as [type])){Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CodexStage1AtomicFile {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 [return: MarshalAs(UnmanagedType.Bool)]
 public static extern bool MoveFileExW(string existingFileName, string newFileName, uint flags);
}
'@}
function Get-Full([string]$Path){return [IO.Path]::GetFullPath($Path)}
function Get-LocalSHA256([string]$Path){
 $stream=[IO.File]::Open((Get-Full $Path),[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
 $algorithm=[Security.Cryptography.SHA256]::Create()
 try{return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','')}finally{$algorithm.Dispose();$stream.Dispose()}
}
function Add-LaunchHashes($Receipt,[string]$Boot,[string]$Controller,[string]$NativeHelper,[scriptblock]$HashProvider){
 if(!$HashProvider){$HashProvider={param($Path) Get-LocalSHA256 $Path}}
 $errors=[Collections.Generic.List[string]]::new()
 foreach($pair in @(@('bootSHA256',$Boot),@('controllerSHA256',$Controller),@('nativeHelperSHA256',$NativeHelper))){
  try{$Receipt[$pair[0]]=& $HashProvider $pair[1]}catch{$Receipt[$pair[0]]=$null;$errors.Add(($pair[0]+': '+$_.Exception.Message))}
 }
 $Receipt['metadataErrors']=$errors.ToArray();$Receipt['metadataState']=if($errors.Count -eq 0){'complete'}else{'partial_hash_failure'}
}
function Get-IsolatedRun([string]$Path){
 $value=Get-Full $Path;$root=(Get-Full (Join-Path (Split-Path -Parent $PSScriptRoot) 'runs'))+'\'
 if(!$value.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Run directory must stay within this stage1/runs root.'}
 if(!(Test-Path -LiteralPath $value -PathType Container)){throw 'Root must create the isolated run directory first.'}
 return $value.TrimEnd('\')
}
function Write-OwnAtomicJson([string]$Path,$Value){
 $full=Get-Full $Path
 if(!$full.StartsWith($script:run+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Supervisor output escaped owned run.'}
 $temporary=$full+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
 [IO.File]::WriteAllText($temporary,($Value|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
 for($retry=0;$retry -lt 6;$retry++){
  # Same-directory atomic rename; supports first publication and replacement.
  # Avoid WinPS5's null-to-empty-string binding for File.Replace backup paths.
  if([CodexStage1AtomicFile]::MoveFileExW($temporary,$full,9)){return}
  $publishError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
  if($retry -eq 5){throw [ComponentModel.Win32Exception]::new($publishError,'MoveFileExW atomic JSON publication failed')}
  Start-Sleep -Milliseconds 30
 }
}
function Read-OwnedMarker([string]$Path,[string]$WantedNonce,[uint32]$WantedPid){
 if(!(Test-Path -LiteralPath $Path -PathType Leaf)){return $false}
 try{
  $value=Get-Content -LiteralPath $Path -Raw -Encoding UTF8|ConvertFrom-Json
  if($value.nonce -cne $WantedNonce){return $false}
  $location=$value.session;if(!$location){$location=$value.run}
  if(!$location -or (Get-Full $location) -ne $script:run){return $false}
  $ids=@($value.pid,$value.process,$value.processId)|Where-Object{$null -ne $_}
  return $ids.Count -ge 1 -and @($ids|Where-Object{[uint32]$_ -ne $WantedPid}).Count -eq 0
 }catch{return $false}
}
if($SelfTest){
 $facts=[ordered]@{contract='stage1.isolation-supervisor-selftest.v1';recorded=(Get-Date -Format o);cadStarted=$false;checks=[ordered]@{}}
 $facts.checks.EmptyArgument=([CodexStage1Isolation.Native]::Quote('') -ceq '""')
 $facts.checks.SpaceArgument=([CodexStage1Isolation.Native]::Quote('a b') -ceq '"a b"')
 $facts.checks.TrailingSlash=([CodexStage1Isolation.Native]::Quote('C:\a\') -ceq '"C:\a\\"')
 $facts.checks.EmbeddedQuote=([CodexStage1Isolation.Native]::Quote('a"b') -ceq '"a\"b"')
 $facts.checks.IdentityPositive=[CodexStage1Isolation.Native]::IdentityMatches('D:\CAD\acad.exe','123','CodexS1_x','d:\cad\acad.exe','123','CodexS1_x')
 $facts.checks.IdentityRejectPidReuse=![CodexStage1Isolation.Native]::IdentityMatches('D:\CAD\acad.exe','124','CodexS1_x','D:\CAD\acad.exe','123','CodexS1_x')
 $facts.checks.IdentityRejectOtherDesktop=![CodexStage1Isolation.Native]::IdentityMatches('D:\CAD\acad.exe','123','Default','D:\CAD\acad.exe','123','CodexS1_x')
 $hashTest=Join-Path ([IO.Path]::GetTempPath()) ('codex-s1-sha-'+[Guid]::NewGuid().ToString('N')+'.bin')
 try{[IO.File]::WriteAllBytes($hashTest,[Text.Encoding]::ASCII.GetBytes('abc'));$facts.checks.LocalFileSHA256=(Get-LocalSHA256 $hashTest) -ceq 'BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD'}finally{if([IO.File]::Exists($hashTest)){[IO.File]::Delete($hashTest)}}
 $simulated=[ordered]@{nonce='synthetic-no-process';processId=0;creationFileTime='synthetic-only';metadataState='minimal_ownership_persisted'}
 Add-LaunchHashes $simulated 'unused-boot' 'unused-controller' 'unused-helper' {param($Path) throw 'injected hash metadata failure; no process started'}
 $facts.checks.MetadataFailureDoesNotThrow=($simulated.metadataState -eq 'partial_hash_failure' -and $simulated.metadataErrors.Count -eq 3)
 $facts.checks.MetadataFailurePreservesOwnership=($simulated.nonce -eq 'synthetic-no-process' -and $simulated.creationFileTime -eq 'synthetic-only')
 $normalHashes=[ordered]@{};Add-LaunchHashes $normalHashes $helper $PSCommandPath $helper
 $facts.checks.DefaultLocalMetadataHashes=($normalHashes.metadataState -eq 'complete' -and $normalHashes.metadataErrors.Count -eq 0 -and $normalHashes.controllerSHA256 -ceq (Get-LocalSHA256 $PSCommandPath))
 $ownershipTest=Join-Path ([IO.Path]::GetTempPath()) ('codex-s1-ownership-'+[Guid]::NewGuid().ToString('N'));$priorSelfTestRun=$script:run
 try{
  [void][IO.Directory]::CreateDirectory($ownershipTest);$script:run=Get-Full $ownershipTest;$testReceipt=Join-Path $ownershipTest 'ownership-synthetic.json'
  $testData=[ordered]@{origin='Synthetic_NoProcess';nonce='synthetic-no-process';processId=0;creationFileTime='synthetic-only';metadataState='minimal_ownership_persisted'}
  Write-OwnAtomicJson $testReceipt $testData;$beforeMetadata=[IO.File]::ReadAllText($testReceipt)|ConvertFrom-Json
  Add-LaunchHashes $testData 'unused' 'unused' 'unused' {param($Path) throw 'injected metadata failure'}
  $afterFailure=[IO.File]::ReadAllText($testReceipt)|ConvertFrom-Json
  $facts.checks.PhysicalMinimalReceiptSurvivesMetadataFailure=($beforeMetadata.metadataState -eq 'minimal_ownership_persisted' -and $afterFailure.nonce -eq 'synthetic-no-process' -and $afterFailure.creationFileTime -eq 'synthetic-only')
  $publications=@([ordered]@{phase='minimal';nonce=$beforeMetadata.nonce;creationFileTime=$beforeMetadata.creationFileTime})
  Add-LaunchHashes $testData $helper $PSCommandPath $helper
  Write-OwnAtomicJson $testReceipt $testData;$fullMetadata=[IO.File]::ReadAllText($testReceipt)|ConvertFrom-Json
  $facts.checks.PhysicalFullReceiptReplacesMinimal=($fullMetadata.metadataState -eq 'complete' -and $fullMetadata.metadataErrors.Count -eq 0 -and $fullMetadata.nonce -eq 'synthetic-no-process')
  $publications+=@([ordered]@{phase='full_metadata';metadataState=$fullMetadata.metadataState;nonce=$fullMetadata.nonce;controllerSHA256=$fullMetadata.controllerSHA256})
  $heartbeatReplacementsCorrect=$true
  for($sequence=1;$sequence -le 3;$sequence++){
   Write-OwnAtomicJson $testReceipt ([ordered]@{contract='Synthetic_NoProcess_Heartbeat';nonce='synthetic-no-process';sequence=$sequence})
   $actualHeartbeat=[IO.File]::ReadAllText($testReceipt)|ConvertFrom-Json
   $heartbeatReplacementsCorrect=$heartbeatReplacementsCorrect -and $actualHeartbeat.sequence -eq $sequence -and $actualHeartbeat.nonce -eq 'synthetic-no-process'
   $publications+=@([ordered]@{phase='heartbeat';sequence=$actualHeartbeat.sequence;nonce=$actualHeartbeat.nonce})
  }
  $facts.checks.PhysicalHeartbeatReplacesSameFileThreeTimes=$heartbeatReplacementsCorrect
  $facts.atomicPublicationEvidence=[ordered]@{method='MoveFileExW';flags=9;writeCount=$publications.Count;allWritesSameFile=$true;readbacks=$publications;powershellVersion=$PSVersionTable.PSVersion.ToString();powershellExecutable=[Diagnostics.Process]::GetCurrentProcess().MainModule.FileName;cadStarted=$false}
 }finally{
  $script:run=$priorSelfTestRun
  if([IO.File]::Exists((Join-Path $ownershipTest 'ownership-synthetic.json'))){[IO.File]::Delete((Join-Path $ownershipTest 'ownership-synthetic.json'))}
  if([IO.Directory]::Exists($ownershipTest)){[IO.Directory]::Delete($ownershipTest,$false)}
 }
 $before=[CodexStage1Isolation.Native]::InputDesktopName();$threadBefore=[CodexStage1Isolation.Native]::CurrentThreadDesktopName();$monitor=[CodexStage1Isolation.EventMonitor]::new();$desktop=$null
 try{
  $monitor.Start();Start-Sleep -Milliseconds 250
  $desktop=[CodexStage1Isolation.DesktopWorker]::new(('CodexS1_SelfTest_'+[Guid]::NewGuid().ToString('N')))
  $facts.checks.PrivateDesktopAllocated=($desktop.DesktopHandle -ne 0)
  $facts.checks.InputDesktopNotSwitched=([CodexStage1Isolation.Native]::InputDesktopName() -ceq $before)
  $facts.checks.SupervisorThreadDesktopUnchanged=([CodexStage1Isolation.Native]::CurrentThreadDesktopName() -ceq $threadBefore)
  $facts.checks.NoProcessCreated=(!$desktop.Created)
 }finally{
  if($desktop){$desktop.Dispose()};$monitor.Stop()
  $facts.checks.MonitorStarted=$monitor.Started;$facts.checks.MonitorStopped=$monitor.Stopped;$facts.checks.MonitorNoError=[string]::IsNullOrEmpty($monitor.Error)
  $facts.checks.MonitorNoDrops=($monitor.Dropped -eq 0);$facts.checks.DesktopClosed=($desktop -and $desktop.DesktopClosed);$monitor.Dispose()
 }
 $facts.result=if(@($facts.checks.GetEnumerator()|Where-Object{!$_.Value}).Count -eq 0){'PASS'}else{'FAIL'}
 if($SelfTestOutputPath){if(Test-Path -LiteralPath $SelfTestOutputPath){throw 'Selftest output already exists.'};[IO.File]::WriteAllText((Get-Full $SelfTestOutputPath),($facts|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))}
 $facts|ConvertTo-Json -Depth 7
 if($facts.result -ne 'PASS'){exit 1};exit 0
}
$script:run=Get-IsolatedRun $RunDirectory
$launchReceipt=Join-Path $run 'isolation-host-receipt.json'
if($StopOwnedWorker){
 $receipt=Get-Content -LiteralPath $launchReceipt -Raw -Encoding UTF8|ConvertFrom-Json
 if($receipt.contract -ne 'stage1.isolated-host-launch.v1' -or $receipt.nonce -cne $ExpectedNonce.ToLowerInvariant() -or $receipt.origin -ne 'CreateProcessW' -or $receipt.session -ne $run){throw 'Stop receipt/nonce/session not verified; no process terminated.'}
 if($receipt.desktopName -cne ('CodexS1_'+$ExpectedNonce.ToLowerInvariant())){throw 'Owned desktop/nonce mismatch; no process terminated.'}
 $code=[CodexStage1Isolation.Native]::StopVerified([uint32]$receipt.processId,[uint32]$receipt.initialThreadId,$receipt.executable,$receipt.creationFileTime,$receipt.desktopName)
 $forced=($code -eq 70)
 $stopScope=if($forced){'Explicit StopOwnedWorker only; exe/creation time/OpenDesktop+EnumDesktopWindows PID and attested GUI thread verified immediately before termination'}else{'Native helper observed already-exited process and did not call TerminateProcess; its zero return is not an actual process exit-code assertion; consult supervisor final'}
 $stop=[ordered]@{contract='stage1.isolated-host-explicit-stop.v1';recorded=(Get-Date -Format o);nonce=$receipt.nonce;processId=$receipt.processId;creationFileTime=$receipt.creationFileTime;desktopName=$receipt.desktopName;exitCode=$code;forcedTermination=$forced;userCadTouched=$false;scope=$stopScope}
 Write-OwnAtomicJson (Join-Path $run ('isolation-stop-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.json')) $stop
 $stop|ConvertTo-Json;exit 0
}
$acad=Get-Full $AcadPath;$boot=Get-Full $BootScriptPath;$templatePath=Get-Full $Template
foreach($file in @($acad,$boot,$templatePath)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw ('Required file missing: '+$file)}}
if(!$boot.StartsWith($run+'\',[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetExtension($boot) -ne '.scr'){throw 'Bootstrap must be a saved .scr inside this owned session directory.'}
$acadVersion=(Get-Item -LiteralPath $acad).VersionInfo
if([IO.Path]::GetFileName($acad) -ine 'acad.exe' -or $acadVersion.FileVersion -notmatch '^R?23\.1(?:\.|$)' -or $acadVersion.ProductMajorPart -ne 23 -or $acadVersion.ProductMinorPart -ne 1){throw 'Explicit AutoCAD2020/R23.1 executable required.'}
if(!$Profile -or $Profile.IndexOfAny([char[]]@([char]0,[char]10,[char]13)) -ge 0){throw 'Invalid profile argument.'}
foreach($name in @($ReadyMarkerName,$QuitMarkerName)){if([IO.Path]::GetFileName($name) -cne $name){throw 'Marker names must be simple run-local filenames.'}}
if(!$Nonce){$Nonce=[Guid]::NewGuid().ToString('N')};$Nonce=$Nonce.ToLowerInvariant();$desktopName='CodexS1_'+$Nonce
foreach($file in @('isolation-host-receipt.json','isolation-binding.json','isolation-final.json','isolation-user-events.jsonl','isolation-window-samples.jsonl')){if(Test-Path -LiteralPath (Join-Path $run $file)){throw ('Supervisor output already exists: '+$file)}}
$binding=[ordered]@{contract='stage1.isolated-host-binding.v1';nonce=$Nonce;session=$run;desktopName=$desktopName;desktopPath=('WinSta0\'+$desktopName);expectedExecutable=$acad;bootScript=$boot;profile=$Profile;template=$templatePath}
Write-OwnAtomicJson (Join-Path $run 'isolation-binding.json') $binding
$relatedSet=[Collections.Generic.HashSet[uint32]]::new()
foreach($id in $RelatedProcessIds){[void]$relatedSet.Add($id)}
foreach($process in @(Get-Process|Where-Object{$_.ProcessName -match '^(acad|tarch|tch|t20)'})){[void]$relatedSet.Add([uint32]$process.Id)}
$relatedAtStart=@($relatedSet|Sort-Object)
$monitor=[CodexStage1Isolation.EventMonitor]::new();$worker=$null;$eventsWriter=$null;$samplesWriter=$null;$relatedWriter=$null;$tipWriter=$null
$startupTipHandles=[Collections.Generic.HashSet[long]]::new();$startupTipActions=0;$startupTipPosts=0
$created=$false;$timedOut=$false;$readyConfirmed=$false;$quitConfirmed=$false;$workerExited=$false;$forced=$false;$inputChanged=$false;$desktopReadErrors=0;$sampleCount=0;$fgEvents=0;$showEvents=0;$fgSamples=0;$supervisorErrors=@();$postExitWindowDiagnostics=@();$exitCode=$null;$workerPid=0
$relatedEvents=0;$relatedShows=0;$relatedForegrounds=0
$inputBefore=[CodexStage1Isolation.Native]::InputDesktopName();$foregroundBefore=[CodexStage1Isolation.Native]::Foreground();$started=Get-Date;$lastConsole=$started;$stopDeferred=$false
function Flush-UserEvents {
 foreach($event in $script:monitor.Drain()){
  $script:eventsWriter.WriteLine(($event|ConvertTo-Json -Depth 5 -Compress))
  if($script:workerPid -ne 0 -and $event.Pid -eq $script:workerPid){if($event.Event -eq 3){$script:fgEvents++};if($event.Event -eq 0x8002){$script:showEvents++}}
  $base=if($event.ProcessImage){[IO.Path]::GetFileNameWithoutExtension($event.ProcessImage)}else{''}
  if($event.Pid -ne $script:workerPid -and ($script:relatedSet.Contains([uint32]$event.Pid) -or $base -match '^(acad|tarch|tch|t20)')){
   $script:relatedEvents++;if($event.Event -eq 3){$script:relatedForegrounds++};if($event.Event -eq 0x8002){$script:relatedShows++}
   $script:relatedWriter.WriteLine(($event|ConvertTo-Json -Depth 5 -Compress))
  }
 }
}
try{
 $eventsWriter=[IO.StreamWriter]::new([IO.File]::Open((Join-Path $run 'isolation-user-events.jsonl'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read),[Text.UTF8Encoding]::new($false));$eventsWriter.AutoFlush=$true
 $samplesWriter=[IO.StreamWriter]::new([IO.File]::Open((Join-Path $run 'isolation-window-samples.jsonl'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read),[Text.UTF8Encoding]::new($false));$samplesWriter.AutoFlush=$true
 $relatedWriter=[IO.StreamWriter]::new([IO.File]::Open((Join-Path $run 'isolation-related-cad-events.jsonl'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read),[Text.UTF8Encoding]::new($false));$relatedWriter.AutoFlush=$true
 $tipWriter=[IO.StreamWriter]::new([IO.File]::Open((Join-Path $run 'isolation-startup-tip-actions.jsonl'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read),[Text.UTF8Encoding]::new($false));$tipWriter.AutoFlush=$true
 $monitor.Start();$monitorStartedAt=[DateTime]::UtcNow
 if($monitor.DesktopName -cne $inputBefore){throw 'Monitor thread is not on the observed user input desktop; no worker launched.'}
 $worker=[CodexStage1Isolation.DesktopWorker]::new($desktopName)
 if($EarlyNativeModule){
  $earlyConfig=Get-Content -LiteralPath (Join-Path $run 'worker-config.json') -Raw -Encoding UTF8|ConvertFrom-Json
  if($earlyConfig.nonce -cne $Nonce -or (Get-Full $earlyConfig.session) -ne $run -or $earlyConfig.profile -cne $Profile -or $Profile -notlike 'CodexS1Native_*' -or !$earlyConfig.earlyNativeAutoload){throw 'Early native startup requires this exact owned dedicated session config.'}
  if((Get-Full $EarlyNativeModule) -ne (Get-Full $earlyConfig.stagedARX) -or (Get-LocalSHA256 $EarlyNativeModule) -cne $earlyConfig.binarySHA256){throw 'Early native ARX path/hash differs from attested candidate.'}
 }
 $worker.Launch($acad,$Profile,$templatePath,$boot,$run,$EarlyNativeModule);$created=$worker.Created;$workerPid=$worker.ProcessId
 # Persist termination ownership FIRST. Optional hashes cannot erase the sole
 # nonce/PID/exe/creation-time/desktop proof after CreateProcess has succeeded.
 $receipt=[ordered]@{contract='stage1.isolated-host-launch.v1';origin='CreateProcessW';recorded=[DateTime]::UtcNow.ToString('o');nonce=$Nonce;session=$run;processId=$worker.ProcessId;initialThreadId=$worker.InitialThreadId;creationFileTime=$worker.CreationFileTime;creationUtc=$worker.CreationUtc;desktopName=$desktopName;desktopPath=$worker.DesktopPath;desktopHandle=$worker.DesktopHandle;executable=$worker.Image;commandLine=$worker.CommandLine;metadataState='minimal_ownership_persisted';switchDesktopCalled=$false;userCadTouched=$false}
 Write-Output ('ISOLATED_OWNERSHIP_MINIMAL '+($receipt|ConvertTo-Json -Depth 5 -Compress))
 Write-OwnAtomicJson $launchReceipt $receipt
 $receipt['ownershipPersistedUtc']=[DateTime]::UtcNow.ToString('o');$receipt['bootScript']=$boot;$receipt['template']=$templatePath;$receipt['profile']=$Profile
 $receipt['readyMarkerName']=$ReadyMarkerName;$receipt['quitMarkerName']=$QuitMarkerName;$receipt['monitorStartedBeforeWorker']=($monitorStartedAt -le [DateTimeOffset]::Parse($worker.CreationUtc).UtcDateTime)
 $receipt['closeOwnStartupTipsEnabled']=[bool]$CloseOwnStartupTips
 $receipt['monitorStartedUtc']=$monitorStartedAt.ToString('o');$receipt['monitorDesktop']=$monitor.DesktopName;$receipt['inputDesktopBefore']=$inputBefore;$receipt['foregroundBefore']=$foregroundBefore;$receipt['relatedProcessIdsAtStart']=$relatedAtStart
 $receipt['controllerPath']=$PSCommandPath;$receipt['nativeHelperPath']=$helper;$receipt['timeoutPolicy']='Report timeout and keep supervising; no automatic termination'
 Add-LaunchHashes $receipt $boot $PSCommandPath $helper
 if($receipt.metadataErrors.Count -gt 0){$supervisorErrors+=@($receipt.metadataErrors);Write-Output 'ISOLATED_METADATA_WARNING; ownership retained; monitoring continues'}
 try{Write-OwnAtomicJson $launchReceipt $receipt}catch{$supervisorErrors+=('FullReceiptMetadata: '+$_.Exception.Message);Write-Output 'ISOLATED_METADATA_WRITE_WARNING; minimal ownership receipt retained; monitoring continues'}
 Write-Output ('ISOLATED_CAD_CREATED pid='+$workerPid+' nonce='+$Nonce+' desktop='+$desktopName)
 while($true){
  Flush-UserEvents
  $foreground=[CodexStage1Isolation.Native]::Foreground();if($foreground.Pid -eq $workerPid){$fgSamples++}
  try{$inputNow=[CodexStage1Isolation.Native]::InputDesktopName();if($inputNow -cne $inputBefore){$inputChanged=$true}}catch{$inputNow=$null;$desktopReadErrors++}
  $windows=@();$windowSnapshotCompleted=$false;$processSignaledAtSnapshot=$worker.HasExited
  if(!$processSignaledAtSnapshot){
   try{$windows=@($worker.Windows());$windowSnapshotCompleted=$true}catch{
    $windowFailure=$_.Exception;$baseFailure=$windowFailure.GetBaseException();$signaledAfterFailure=$false
    try{$signaledAfterFailure=$worker.HasExited}catch{$supervisorErrors+=('OwnedExitState: '+$_.Exception.Message)}
    if(Test-PostExitPrivateEnumerationFailure -Message ($baseFailure.Message) -OriginalProcessSignaled $signaledAfterFailure -NativeWin32Failure ($baseFailure -is [ComponentModel.Win32Exception])){
     $processSignaledAtSnapshot=$true
     $postExitWindowDiagnostics+=([ordered]@{utc=[DateTime]::UtcNow.ToString('o');nonce=$Nonce;processId=$workerPid;creationFileTime=$worker.CreationFileTime;exitCode=$worker.ExitCode;originalProcessHandleSignaled=$true;operation=$baseFailure.Message;nativeErrorCode=$baseFailure.NativeErrorCode;scope='private desktop enumeration raced with original owned process exit; user-desktop event monitor remains active'})
    }else{$supervisorErrors+=('WindowSnapshot: '+$windowFailure.Message)}
   }
  }
  if($CloseOwnStartupTips){
   foreach($window in $windows){
    if($window.PidAtEnumeration -eq $workerPid -and $window.EnumerationDesktop -ceq $desktopName -and $window.ClassName -ceq '#32770' -and $window.Title -ceq '日积月累' -and $startupTipHandles.Add([long]$window.Hwnd)){
     $action=$worker.PostCloseBenignStartupTip([long]$window.Hwnd);$startupTipActions++;if($action.Posted){$startupTipPosts++}
     $row=[ordered]@{contract='stage1.isolated-startup-tip-action.v1';nonce=$Nonce;session=$run;sequence=$startupTipActions;nativeReadyAlreadyConfirmed=$readyConfirmed;action=$action}
     $tipWriter.WriteLine(($row|ConvertTo-Json -Depth 7 -Compress))
     if(!$action.Posted){$supervisorErrors+=('StartupTipClose: '+$action.Status+' error='+$action.Error)}
    }
   }
  }
  $sampleCount++;$sample=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');sequence=$sampleCount;processId=$workerPid;inputDesktop=$inputNow;foreground=$foreground;privateDesktop=$desktopName;workerWindows=$windows;windowSnapshotCompleted=$windowSnapshotCompleted;ownedProcessSignaledAtWindowSnapshot=$processSignaledAtSnapshot}
  $samplesWriter.WriteLine(($sample|ConvertTo-Json -Depth 7 -Compress))
  $readyConfirmed=Read-OwnedMarker (Join-Path $run $ReadyMarkerName) $Nonce $workerPid
  $quitConfirmed=Read-OwnedMarker (Join-Path $run $QuitMarkerName) $Nonce $workerPid
  $workerExited=$worker.HasExited
  if(!$timedOut -and ((Get-Date)-$started).TotalSeconds -ge $TimeoutSeconds){$timedOut=$true;Write-OwnAtomicJson (Join-Path $run 'isolation-timeout.json') ([ordered]@{contract='stage1.isolated-host-timeout.v1';nonce=$Nonce;session=$run;processId=$workerPid;recorded=(Get-Date -Format o);action='report only; supervisor and desktop retained; use explicit verified StopOwnedWorker if needed'});Write-Output 'ISOLATED_CAD_TIMEOUT_REPORTED; no process killed; supervisor continues'}
  $heartbeat=[ordered]@{contract='stage1.isolated-host-heartbeat.v1';nonce=$Nonce;session=$run;processId=$workerPid;recorded=(Get-Date -Format o);sampleCount=$sampleCount;readyIdentityConfirmed=$readyConfirmed;nativeQuitConfirmed=$quitConfirmed;workerExited=$workerExited;workerForegroundEvents=$fgEvents;workerUserDesktopShowEvents=$showEvents;workerForegroundSamples=$fgSamples;relatedCadEvents=$relatedEvents;relatedCadShowEvents=$relatedShows;relatedCadForegroundEvents=$relatedForegrounds;relatedCadEventsNeedReview=($relatedShows -gt 0 -or $relatedForegrounds -gt 0);inputDesktopChanged=$inputChanged;inputDesktopReadErrors=$desktopReadErrors;droppedEvents=$monitor.Dropped;monitorError=$monitor.Error;timedOut=$timedOut;privateWindowCount=$windows.Count;latestWindows=$windows;closeOwnStartupTipsEnabled=[bool]$CloseOwnStartupTips;startupTipActions=$startupTipActions;startupTipPosts=$startupTipPosts;supervisorErrors=$supervisorErrors;postExitWindowDiagnostics=$postExitWindowDiagnostics}
  Write-OwnAtomicJson (Join-Path $run 'isolation-host-heartbeat.json') $heartbeat
  $stopFile=Join-Path $run 'isolation-supervisor.stop.json'
  if(!$stopDeferred -and (Test-Path -LiteralPath $stopFile)){
   try{$stopRequest=Get-Content -LiteralPath $stopFile -Raw -Encoding UTF8|ConvertFrom-Json;if($stopRequest.nonce -ceq $Nonce){$stopDeferred=$true;Write-Output 'SUPERVISOR_STOP_REQUESTED; waits for native quit/process exit; no automatic kill'}}catch{}
  }
  if($workerExited){$exitCode=$worker.ExitCode;break}
  if(((Get-Date)-$lastConsole).TotalSeconds -ge 10){Write-Output ('ISOLATED_CAD_MONITOR pid='+$workerPid+' samples='+$sampleCount+' ready='+$readyConfirmed+' userShows='+$showEvents+' foreground='+$fgEvents+' timeout='+$timedOut);$lastConsole=Get-Date}
  Start-Sleep -Milliseconds $SampleMilliseconds
 }
}catch{
 if($worker -and $worker.Created){$created=$true;$workerPid=$worker.ProcessId;try{$worker.RefreshIdentity()}catch{}}
 $supervisorErrors+=($_.Exception.ToString());Write-Output ('ISOLATED_SUPERVISOR_ERROR: '+$_.Exception.Message)
 # Never terminate a process in this catch. Its nonce-bound receipt remains the
 # sole source for a later explicit StopOwnedWorker invocation.
}finally{
 try{$monitor.Stop();if($eventsWriter){Flush-UserEvents}}catch{$supervisorErrors+=('MonitorCleanup: '+$_.Exception.Message)}
 if($eventsWriter){$eventsWriter.Dispose()};if($samplesWriter){$samplesWriter.Dispose()};if($relatedWriter){$relatedWriter.Dispose()};if($tipWriter){$tipWriter.Dispose()}
 if($worker){try{if(!$workerExited -and $worker.Created){$workerExited=$worker.HasExited;if($workerExited){$exitCode=$worker.ExitCode}}}catch{};$worker.Dispose()}
 $forced=@(Get-ChildItem -LiteralPath $run -File -Filter 'isolation-stop-*.json').Count -gt 0
 $final=[ordered]@{contract='stage1.isolated-host-final.v1';recorded=(Get-Date -Format o);nonce=$Nonce;session=$run;workerCreated=$created;processId=$workerPid;workerExited=$workerExited;workerExitCode=$exitCode;readyIdentityConfirmed=$readyConfirmed;nativeQuitConfirmed=$quitConfirmed;forcedTermination=$forced;timedOut=$timedOut;desktopName=$desktopName;inputDesktopBefore=$inputBefore;inputDesktopChanged=$inputChanged;inputDesktopReadErrors=$desktopReadErrors;monitorStartedBeforeWorker=($receipt -and $receipt.monitorStartedBeforeWorker);monitorStarted=$monitor.Started;monitorStopped=$monitor.Stopped;monitorError=$monitor.Error;droppedEvents=$monitor.Dropped;eventFileWritten=(Test-Path -LiteralPath (Join-Path $run 'isolation-user-events.jsonl'));sampleCount=$sampleCount;workerForegroundEvents=$fgEvents;workerUserDesktopShowEvents=$showEvents;workerForegroundSamples=$fgSamples;relatedCadEvents=$relatedEvents;relatedCadShowEvents=$relatedShows;relatedCadForegroundEvents=$relatedForegrounds;relatedCadEventsNeedReview=($relatedShows -gt 0 -or $relatedForegrounds -gt 0);desktopClosed=($worker -and $worker.DesktopClosed);processHandlesClosed=($worker -and $worker.ProcessHandlesClosed);switchDesktopCalled=$false;userCadTouched=$false;supervisorErrors=$supervisorErrors;postExitWindowDiagnostics=$postExitWindowDiagnostics;stillRunningOwnedWorker=($created -and !$workerExited);closeOwnStartupTipsEnabled=[bool]$CloseOwnStartupTips;startupTipActions=$startupTipActions;startupTipPosts=$startupTipPosts;scope='Observed supervisor/owned worker only; existing related CAD events retained for attribution review, not blamed automatically; drawing business correctness requires separate native evidence'}
 Write-OwnAtomicJson (Join-Path $run 'isolation-final.json') $final
 $monitor.Dispose();$final|ConvertTo-Json -Depth 7
}
if(!$created -or !$workerExited -or $exitCode -ne 0 -or !$readyConfirmed -or !$quitConfirmed -or $forced -or $timedOut -or $fgEvents -gt 0 -or $showEvents -gt 0 -or $fgSamples -gt 0 -or $inputChanged -or $desktopReadErrors -gt 0 -or $monitor.Dropped -gt 0 -or $monitor.Error -or $supervisorErrors.Count -gt 0){exit 1}
exit 0
