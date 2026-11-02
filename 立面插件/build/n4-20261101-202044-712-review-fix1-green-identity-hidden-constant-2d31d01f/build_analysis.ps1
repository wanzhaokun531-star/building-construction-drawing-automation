param([string]$Label = 'red')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildName = 'n4-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + ($Label -replace '[^A-Za-z0-9_-]', '_') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot = Join-Path (Join-Path $projectRoot 'build') $buildName
$sourceRoot = Join-Path $buildRoot 'src'
$null = New-Item -ItemType Directory -Path $sourceRoot
$receipt = [ordered]@{ contract='bce.analysis-build.v1'; label=$Label; startedUtc=[DateTime]::UtcNow.ToString('o'); compiledSourcesDirectory=$sourceRoot; sourceFiles=[ordered]@{}; inputOrigins=@(); binary=$null; binarySHA256=$null; log=(Join-Path $buildRoot 'build.log'); exitCode=1; toolchain='AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17' }
$exitResult = 1
try {
    $inputs = @('src/ElevationModel.h','src/ElevationModel.cpp','src/RuleModel.h','src/Recognition.h','src/Recognition.cpp','src/SceneReader.h','src/SceneReader.cpp','src/AnalysisReader.h','src/AnalysisReader.cpp','tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','tests/native/analysis/AnalysisDriver.cpp','tests/native/analysis/ElevationAnalysisCases.vcxproj')
    foreach ($relative in $inputs) {
        $source = Join-Path $projectRoot $relative
        $destination = Join-Path $sourceRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $destination
        $digest = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $receipt.sourceFiles[$destination] = $digest
        $receipt.inputOrigins += [ordered]@{ origin=$source; compiledCopy=$destination; sha256=$digest }
    }
    $fixtureSource = Join-Path $projectRoot 'tests/native/analysis/AnalysisFixtures.h'
    $fixtureCopy = Join-Path $sourceRoot 'AnalysisFixtures.h'
    Copy-Item -LiteralPath $fixtureSource -Destination $fixtureCopy
    $fixtureDigest = (Get-FileHash -LiteralPath $fixtureCopy).Hash
    $receipt.sourceFiles[$fixtureCopy] = $fixtureDigest
    $receipt.inputOrigins += [ordered]@{ origin=$fixtureSource; compiledCopy=$fixtureCopy; sha256=$fixtureDigest }
    $expected = Join-Path $projectRoot 'tests/fixtures/scheme-expected.tsv'
    $expectedCopy = Join-Path $buildRoot 'scheme-expected.tsv'
    Copy-Item -LiteralPath $expected -Destination $expectedCopy
    if ((Get-FileHash -LiteralPath $expectedCopy).Hash -ne 'E5D1C9E7D74909A2D7D95DE75D42F1DD7B2A3000B1AE6C051ED84551529723C2') { throw 'Expected fixture SHA mismatch' }
    $rows = Import-Csv -LiteralPath $expectedCopy -Delimiter "`t"
    $header = '#pragma once' + "`n" + '#include <map>' + "`n" + '#include <string>' + "`n" + 'inline std::map<std::string,std::string> expectedMembership() { return {' + "`n"
    foreach ($row in $rows) { $header += '{"' + $row.handle + '","' + $row.titleHandle + '"},' + "`n" }
    $header += '}; }'
    $headerPath = Join-Path $sourceRoot 'ExpectedMembership.h'
    Set-Content -LiteralPath $headerPath -Value $header -Encoding utf8
    $receipt.sourceFiles[$headerPath] = (Get-FileHash -LiteralPath $headerPath).Hash
    $receipt.sourceFiles[$expectedCopy] = (Get-FileHash -LiteralPath $expectedCopy).Hash
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $buildRoot 'build_analysis.ps1')
    $receipt.sourceFiles[(Join-Path $buildRoot 'build_analysis.ps1')] = (Get-FileHash -LiteralPath (Join-Path $buildRoot 'build_analysis.ps1') -Algorithm SHA256).Hash
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path -LiteralPath $vcvars) -or !(Test-Path -LiteralPath $msbuild)) { throw 'v141 build tools missing' }
    $command = '@echo off' + "`r`n" + 'call "' + $vcvars + '" -vcvars_ver=14.16' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"' + $msbuild + '" ElevationAnalysisCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal' + "`r`n" + 'exit /b %errorlevel%' + "`r`n"
    Set-Content -LiteralPath (Join-Path $buildRoot 'build-command.cmd') -Value $command -Encoding ascii
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $exitResult = $LASTEXITCODE } finally { Pop-Location }
    if ($exitResult -eq 0) {
        $receipt.binary = Join-Path $buildRoot 'out/ElevationAnalysisCases.arx'
        $receipt.binarySHA256 = (Get-FileHash -LiteralPath $receipt.binary -Algorithm SHA256).Hash
    }
} catch {
    $receipt.error = $_.Exception.Message
    $exitResult = 1
} finally {
    $receipt.exitCode = $exitResult
    $receipt.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding utf8
    Write-Output ('RECEIPT=' + (Join-Path $buildRoot 'receipt.json'))
    Write-Output ('EXIT_CODE=' + $exitResult)
}
exit $exitResult

