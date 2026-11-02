param(
 [Parameter(Mandatory=$true)][string]$RunDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{32}$')][string]$ExpectedNonce,
 [Parameter(Mandatory=$true)][uint32]$OwnedProcessId,
 [uint32]$AttestedGuiThreadId=0,
 [string]$ExpectedExecutable,
 [string]$ExpectedCreationFileTime
)
$ErrorActionPreference='Stop'
$run=[IO.Path]::GetFullPath($RunDirectory).TrimEnd('\')
$runsRoot=[IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $PSScriptRoot) 'runs'))+'\'
if(!$run.StartsWith($runsRoot,[StringComparison]::OrdinalIgnoreCase) -or !(Test-Path -LiteralPath $run -PathType Container)){throw 'Existing stage1/runs session required.'}
$nonce=$ExpectedNonce.ToLowerInvariant();$desktop='CodexS1_'+$nonce
$binding=Get-Content -LiteralPath (Join-Path $run 'isolation-binding.json') -Raw -Encoding UTF8|ConvertFrom-Json
if($binding.nonce -cne $nonce -or $binding.desktopName -cne $desktop -or [IO.Path]::GetFullPath($binding.session) -ine $run){throw 'Nonce/desktop/session binding mismatch; no inspection.'}
if(!$ExpectedExecutable){$ExpectedExecutable=$binding.expectedExecutable}
if(!('CodexStage1Isolation.Native' -as [type])){Add-Type -LiteralPath (Join-Path $PSScriptRoot 'IsolatedCadHostNative.cs')}
$inputBefore=[CodexStage1Isolation.Native]::InputDesktopName()
$proof=[CodexStage1Isolation.Native]::InspectOwnedDesktop($desktop,$OwnedProcessId,$AttestedGuiThreadId)
$proofRecord=[ordered]@{
 contract='stage1.isolated-host-readonly-recovery.v1';recorded=[DateTime]::UtcNow.ToString('o');nonce=$nonce;session=$run
 processId=$OwnedProcessId;attestedGuiThreadId=$AttestedGuiThreadId;guiThreadIsNotAssumedInitialThread=$true
 expectedExecutable=$ExpectedExecutable;expectedCreationFileTime=$ExpectedCreationFileTime
 executableMatches=([IO.Path]::GetFullPath($proof.Executable) -ieq [IO.Path]::GetFullPath($ExpectedExecutable))
 creationTimeMatches=if($ExpectedCreationFileTime){$proof.CreationFileTime -ceq $ExpectedCreationFileTime}else{$null}
 proof=$proof;inputDesktopBefore=$inputBefore;inputDesktopAfter=[CodexStage1Isolation.Native]::InputDesktopName()
 processControlPerformed=$false;windowMessagesPosted=$false;switchDesktopCalled=$false
 scope='Read-only OpenProcess query + OpenDesktop(READOBJECTS|ENUMERATE) + EnumDesktopWindows. No normal launch receipt fabricated; failed monitoring remains failed.'
}
$output=Join-Path $run ('isolation-readonly-recovery-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.json')
[IO.File]::WriteAllText($output,($proofRecord|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
$proofRecord|ConvertTo-Json -Depth 12
