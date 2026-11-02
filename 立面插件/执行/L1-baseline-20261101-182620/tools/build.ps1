param(
    [ValidateSet('CoreTests', 'NativeCases', 'All')][string]$Target = 'CoreTests',
    [string]$Label = 'run'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$safeLabel = $Label -replace '[^A-Za-z0-9_-]', '_'
$buildName = 'f1-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $safeLabel + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot = Join-Path (Join-Path $projectRoot 'build') $buildName
$null = New-Item -ItemType Directory -Path $buildRoot
$receipt = [ordered]@{ target=$Target; label=$Label; startedUtc=[DateTime]::UtcNow.ToString('o'); buildRoot=$buildRoot; inputs=@(); commands=@(); outputs=@(); buildExitCode=$null; testExitCode=$null; unknownCaseExitCode=$null; exitCode=1 }
$finalExit = 1
try {
    $inputs = @('src/ElevationModel.h','src/ElevationModel.cpp','src/Recognition.h','src/Recognition.cpp','src/RuleModel.h','src/AnnotationRules.h','src/AnnotationRules.cpp','src/MaterialRules.h','src/MaterialRules.cpp','tests/AnnotationRulesTests.cpp','tests/MaterialRulesTests.cpp','tests/CoreTests.cpp','tests/RecognitionTests.cpp','tests/ReferenceTests.cpp','tests/ScopeTests.cpp','tests/TestSupport.h','tests/ElevationCoreTests.vcxproj','tools/build.ps1','tests/fixtures/scheme-scene.tsv','tests/fixtures/scheme-expected.tsv','tests/fixtures/scheme-scene-manifest.json')
    foreach ($relative in $inputs) {
        $source = Join-Path $projectRoot $relative
        $destination = Join-Path (Join-Path $buildRoot 'input') $relative
        $null = New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent)
        Copy-Item -LiteralPath $source -Destination $destination
        $receipt.inputs += [ordered]@{ path=$relative; sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash }
    }
    $fixtureManifest = Get-Content -Raw -LiteralPath (Join-Path $buildRoot 'input/tests/fixtures/scheme-scene-manifest.json') | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath (Join-Path $buildRoot 'input/tests/fixtures/scheme-scene.tsv') -Algorithm SHA256).Hash -ne $fixtureManifest.fixtureSHA256) { throw 'Scene fixture does not match manifest.' }
    if ((Get-FileHash -LiteralPath (Join-Path $buildRoot 'input/tests/fixtures/scheme-expected.tsv') -Algorithm SHA256).Hash -ne $fixtureManifest.expectedMembershipTSVSHA256) { throw 'Expected membership fixture does not match manifest.' }
    $receipt.fixtureSourceSHA256 = $fixtureManifest.schemeSHA256
    if ($Target -ne 'CoreTests') {
        $receipt.reason = 'NOT_IMPLEMENTED: NativeCases/All are unavailable until native projects are added.'
        Set-Content -LiteralPath (Join-Path $buildRoot 'not-implemented.log') -Value $receipt.reason -Encoding UTF8
        $finalExit = 3
    } else {
        $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
        $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
        if (!(Test-Path -LiteralPath $vcvars) -or !(Test-Path -LiteralPath $msbuild)) { throw 'VS2019 v141 build environment not found.' }
        $command = '@echo off' + "`r`n" + 'call "' + $vcvars + '" -vcvars_ver=14.16' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"' + $msbuild + '" ElevationCoreTests.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal' + "`r`n" + 'exit /b %errorlevel%' + "`r`n"
        $commandFile = Join-Path $buildRoot 'build-command.cmd'
        Set-Content -LiteralPath $commandFile -Value $command -Encoding ASCII
        $receipt.commands += 'cmd.exe /d /c ..\..\build-command.cmd (cwd=input/tests)'
        Push-Location (Join-Path $buildRoot 'input/tests')
        try {
            & $env:ComSpec /d /c '..\..\build-command.cmd' > (Join-Path $buildRoot 'build.log') 2>&1
            $receipt.buildExitCode = $LASTEXITCODE
        } finally { Pop-Location }
        if ($receipt.buildExitCode -ne 0) { $finalExit = $receipt.buildExitCode }
        else {
            $exe = Join-Path $buildRoot 'out/ElevationCoreTests.exe'
            $receipt.commands += 'out/ElevationCoreTests.exe --all'
            try {
                $ErrorActionPreference = 'Continue' # Native stderr is test output, not a PowerShell exception.
                & $exe --all > (Join-Path $buildRoot 'tests-all.log') 2>&1
                $receipt.testExitCode = $LASTEXITCODE
            } finally { $ErrorActionPreference = 'Stop' }
            $receipt.commands += 'out/ElevationCoreTests.exe --case UnknownCase (expected 2)'
            try {
                $ErrorActionPreference = 'Continue'
                & $exe --case UnknownCase > (Join-Path $buildRoot 'tests-unknown.log') 2>&1
                $receipt.unknownCaseExitCode = $LASTEXITCODE
            } finally { $ErrorActionPreference = 'Stop' }
            $finalExit = if ($receipt.testExitCode -ne 0) { $receipt.testExitCode } elseif ($receipt.unknownCaseExitCode -ne 2) { 1 } else { 0 }
        }
    }
} catch {
    $receipt.error = $_.Exception.Message
    Set-Content -LiteralPath (Join-Path $buildRoot 'error.log') -Value ($_ | Out-String) -Encoding UTF8
    $finalExit = 1
} finally {
    foreach ($file in Get-ChildItem -LiteralPath $buildRoot -File -Recurse | Where-Object { $_.FullName -notlike '*\input\*' -and $_.Name -ne 'receipt.json' }) {
        $receipt.outputs += [ordered]@{ path=$file.FullName.Substring($buildRoot.Length+1); sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $receipt.exitCode = $finalExit
    $receipt.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding UTF8
    Write-Output ('RECEIPT=' + (Join-Path $buildRoot 'receipt.json'))
    Write-Output ('EXIT_CODE=' + $finalExit)
}
exit $finalExit
