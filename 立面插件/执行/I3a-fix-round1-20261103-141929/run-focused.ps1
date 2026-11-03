param([ValidateSet('red','green')][string]$Phase)
$ErrorActionPreference = 'Stop'
$fixRoot = $PSScriptRoot
$projectRoot = Split-Path (Split-Path $fixRoot -Parent) -Parent
$runRoot = Join-Path $fixRoot $Phase
$null = New-Item -ItemType Directory -Path $runRoot
$originalRoot = Join-Path (Split-Path $fixRoot -Parent) 'I3a-evidence-20261103-140525/final-green'
$original = Get-Content -Raw -LiteralPath (Join-Path $originalRoot 'receipt.json') | ConvertFrom-Json
$receipt = [ordered]@{phase=$Phase; startedUtc=[DateTime]::UtcNow.ToString('o'); inputs=@(); commands=@(); buildExitCode=$null; tests=@(); outputs=@(); exitCode=1}
foreach ($entry in $original.inputs) {
    $source = Join-Path $projectRoot $entry.path
    $dest = Join-Path (Join-Path $runRoot 'input') $entry.path
    $null = New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent)
    Copy-Item -LiteralPath $source -Destination $dest
    $receipt.inputs += [ordered]@{path=$entry.path;sha256=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash}
}
Copy-Item -LiteralPath (Join-Path $originalRoot 'build-command.cmd') -Destination (Join-Path $runRoot 'build-command.cmd')
$receipt.commands += 'cmd.exe /d /c ..\..\build-command.cmd (cwd=input/tests; original frozen v141 MSBuild /t:Rebuild command)'
Push-Location (Join-Path $runRoot 'input/tests')
try { & $env:ComSpec /d /c '..\..\build-command.cmd' > (Join-Path $runRoot 'build.log') 2>&1; $receipt.buildExitCode=$LASTEXITCODE } finally { Pop-Location }
$cases=@('FactRotatedWindowRetainsFourCorners','FactElevationUsesConfirmedUnitsDirectionAndDatum')
if ($Phase -eq 'green') { $cases += @('FactSelectorsReadActualDefinitionPoints','FactChangesLocateOnlyRequiredSubset','FactReopenRequiresFreshConsistentObservation','FactUnsupportedSourcesAndNonfiniteValuesAreRejected','FactObservationRejectsAmbiguityAndUnsupportedChanges','FactSubsetMatchNeverAuthorizesGeneration') }
$failed = 0
if ($receipt.buildExitCode -eq 0) {
    foreach ($case in $cases) {
        $receipt.commands += ('out/ElevationCoreTests.exe --case ' + $case)
        $ErrorActionPreference = 'Continue'
        & (Join-Path $runRoot 'out/ElevationCoreTests.exe') --case $case > (Join-Path $runRoot ($case + '.log')) 2>&1
        $caseExit = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        $receipt.tests += [ordered]@{case=$case;exitCode=$caseExit}
        if ($caseExit -ne 0) { $failed++ }
    }
}
$receipt.exitCode = if ($receipt.buildExitCode -ne 0) { $receipt.buildExitCode } elseif ($failed -ne 0) { 1 } else { 0 }
$receipt.finishedUtc=[DateTime]::UtcNow.ToString('o')
foreach ($file in Get-ChildItem -LiteralPath $runRoot -File -Recurse | Where-Object { $_.FullName -notlike '*\input\*' }) {
    $receipt.outputs += [ordered]@{path=$file.FullName.Substring($runRoot.Length+1);sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
}
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runRoot 'receipt.json') -Encoding UTF8
Write-Output ('RECEIPT=' + (Join-Path $runRoot 'receipt.json'))
Write-Output ('BUILD_EXIT=' + $receipt.buildExitCode + ' FAILED_CASES=' + $failed)
exit $receipt.exitCode
