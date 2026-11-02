$ErrorActionPreference='Stop'
$path=Join-Path (Split-Path $PSScriptRoot -Parent) '启动隔离桌面宿主.ps1'
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Supervisor parse failed'}
$fn=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Test-PostExitPrivateEnumerationFailure'},$true)
if(!$fn){throw 'Expected missing post-exit classification before fix'}
. ([scriptblock]::Create($fn.Extent.Text))
$cases=@(
 @('EnumDesktopWindows private desktop',$true,$true,$true),
 @('EnumDesktopWindows private desktop',$false,$true,$false),
 @('EnumDesktopWindows private desktop',$true,$false,$false),
 @('EnumDesktopWindows default desktop',$true,$true,$false),
 @('OpenInputDesktop',$true,$true,$false),
 @('owned process wait',$true,$true,$false),
 @('EnumDesktopWindows private desktop other',$true,$true,$false),
 @('enumdesktopwindows private desktop',$true,$true,$false))
foreach($case in $cases){$actual=Test-PostExitPrivateEnumerationFailure -Message $case[0] -OriginalProcessSignaled $case[1] -NativeWin32Failure $case[2];if($actual -ne $case[3]){throw ('Policy case rejected: '+($case -join '|'))}}
Write-Output 'post-exit window policy: 8 passed; supervisor AST parsed; no CAD launched'
