param([string]$Label = 'red', [string]$ReaderReceipt = '', [string]$StoreReceipt = '', [string]$SceneReceipt = '')
$ErrorActionPreference = 'Stop'
$baseline = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/i1-20261101-210949-709-fix1-critical-output-paged-issues-green-772efe6e/receipt.json'
$base = Get-Content -LiteralPath $baseline -Raw -Encoding UTF8 | ConvertFrom-Json
if ($ReaderReceipt -or $StoreReceipt -or $SceneReceipt) { throw 'I2 dependencies are fixed to reviewed I1 receipts' }
$ReaderReceipt = $base.dependencyReceipts[0].path
$StoreReceipt = $base.dependencyReceipts[1].path
$SceneReceipt = $base.dependencyReceipts[2].path

$projectRoot = Split-Path $PSScriptRoot -Parent
$buildName = 'i2-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + ($Label -replace '[^A-Za-z0-9_-]', '_') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot = Join-Path (Join-Path $projectRoot 'build') $buildName
$sourceRoot = Join-Path $buildRoot 'src'
$null = New-Item -ItemType Directory -Path $sourceRoot
$receipt = [ordered]@{ contract='bce.unitconfig-build.v1'; label=$Label; startedUtc=[DateTime]::UtcNow.ToString('o'); compiledSourcesDirectory=$sourceRoot; sourceFiles=[ordered]@{}; inputOrigins=@(); dependencyReceipts=@(); binary=$null; binarySHA256=$null; productBinary=$null; productSHA256=$null; log=(Join-Path $buildRoot 'build.log'); exitCode=1; toolchain='AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17' }
$exitResult = 1
try {
    $dependencySourceHashes = @{}
    foreach ($dependencyPath in @($ReaderReceipt, $StoreReceipt, $SceneReceipt)) {
        if ([string]::IsNullOrWhiteSpace($dependencyPath)) { continue }
        $resolvedDependency = (Resolve-Path -LiteralPath $dependencyPath).Path
        $dependency = Get-Content -LiteralPath $resolvedDependency -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($dependency.exitCode -ne 0) { throw 'Dependency receipt is not a successful frozen build' }
        $verifiedCount = 0
        $isStore = $dependency.contract -eq 'bce.store-build.v1'
        $isScene = $dependency.contract -eq 'bce.inplace-build.v1'
        if (!$isStore -and !$isScene -and $dependency.contract -ne 'bce.analysis-build.v1') { throw 'Unknown dependency contract' }
        foreach ($origin in $dependency.inputOrigins) {
            $leaf = Split-Path $origin.origin -Leaf
            $owned = if ($isStore) { $leaf -in @('ProjectStore.h','ProjectStore.cpp','ProjectStoreCodec.h','ProjectStoreCodec.cpp') } elseif ($isScene) { $leaf -in @('SceneReader.h','SceneReader.cpp') } else { [string]::IsNullOrWhiteSpace($SceneReceipt) -or $leaf -notin @('SceneReader.h','SceneReader.cpp') }
            if ((Split-Path $origin.origin -Parent) -eq (Join-Path $projectRoot 'src') -and $owned) {
                $verifiedCount += 1
                if ((Get-FileHash -LiteralPath $origin.origin).Hash -ne $origin.sha256 -or (Get-FileHash -LiteralPath $origin.compiledCopy).Hash -ne $origin.sha256) { throw ('Dependency source differs from frozen candidate: ' + $origin.origin) }
                if ($dependencySourceHashes.ContainsKey($leaf) -and $dependencySourceHashes[$leaf] -ne $origin.sha256) { throw ('Conflicting dependency ownership: ' + $leaf) }
                $dependencySourceHashes[$leaf] = $origin.sha256
            }
        }
        $expectedCount = if ($isStore) { 4 } elseif ($isScene) { 2 } elseif ([string]::IsNullOrWhiteSpace($SceneReceipt)) { 9 } else { 7 }
        if ($verifiedCount -ne $expectedCount) { throw ('Dependency source set incomplete: ' + $dependency.contract + ' count=' + $verifiedCount) }
        $receipt.dependencyReceipts += [ordered]@{ path=$resolvedDependency; SHA256=(Get-FileHash -LiteralPath $resolvedDependency).Hash; verifiedOwnedSources=$verifiedCount }
    }
    $inputs = @('src/ElevationModel.h','src/ElevationModel.cpp','src/RuleModel.h','src/Recognition.h','src/Recognition.cpp','src/SceneReader.h','src/SceneReader.cpp','src/AnalysisReader.h','src/AnalysisReader.cpp','src/ProjectStore.h','src/ProjectStore.cpp','src/ProjectStoreCodec.h','src/ProjectStoreCodec.cpp','src/GenerationState.h','src/GenerationState.cpp','src/WriteContracts.h','src/UnitConfig.h','src/UnitConfig.cpp','src/ConfigCommands.h','src/ConfigCommands.cpp','src/AnalysisCommands.h','src/AnalysisCommands.cpp','src/AnalysisCommandAbi.h','src/ElevationArx.cpp','src/BuildingElevation2020.vcxproj','tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','tests/native/unitconfig/UnitConfigDriver.cpp','tests/native/command/CommandTranscript.h','tests/native/unitconfig/UnitConfigTranscript.h','tests/native/unitconfig/PrintedCommandAcceptance.h','tests/native/unitconfig/PrintedCommandAcceptanceTests.cpp','tests/native/unitconfig/ElevationUnitConfigCases.vcxproj')
    foreach ($relative in $inputs) {
        $source = Join-Path $projectRoot $relative
        $destination = Join-Path $sourceRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $destination
        $digest = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $sourceLeaf = Split-Path $relative -Leaf
        if ($dependencySourceHashes.ContainsKey($sourceLeaf) -and $dependencySourceHashes[$sourceLeaf] -ne $digest) { throw ('Dependency changed while freezing: ' + $sourceLeaf) }
        $receipt.sourceFiles[$destination] = $digest
        $receipt.inputOrigins += [ordered]@{ origin=$source; compiledCopy=$destination; sha256=$digest }
    }
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
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $buildRoot 'build_unitconfig.ps1')
    $receipt.sourceFiles[(Join-Path $buildRoot 'build_unitconfig.ps1')] = (Get-FileHash -LiteralPath (Join-Path $buildRoot 'build_unitconfig.ps1') -Algorithm SHA256).Hash
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path -LiteralPath $vcvars) -or !(Test-Path -LiteralPath $msbuild)) { throw 'v141 build tools missing' }
    $command = '@echo off' + "`r`n" + 'call "' + $vcvars + '" -vcvars_ver=14.16' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"' + $msbuild + '" BuildingElevation2020.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal' + "`r`n" + 'exit /b %errorlevel%' + "`r`n"
    Set-Content -LiteralPath (Join-Path $buildRoot 'build-command.cmd') -Value $command -Encoding ascii
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $exitResult = $LASTEXITCODE } finally { Pop-Location }
    if ($exitResult -ne 0) { throw 'Formal product compilation failed' }
    $receipt.productBinary = Join-Path $buildRoot 'out/BuildingElevation2020.arx'
    $receipt.productSHA256 = (Get-FileHash -LiteralPath $receipt.productBinary).Hash
    $header = Join-Path $sourceRoot 'ProductBinding.h'
    Set-Content -LiteralPath $header -Value ('#pragma once' + "`n" + '#define BCE_PRODUCT_SHA256 "' + $receipt.productSHA256 + '"') -Encoding ascii
    $receipt.sourceFiles[$header] = (Get-FileHash -LiteralPath $header).Hash
    $driverCommand = $command.Replace('BuildingElevation2020.vcxproj','ElevationUnitConfigCases.vcxproj')
    Set-Content -LiteralPath (Join-Path $buildRoot 'driver-command.cmd') -Value $driverCommand -Encoding ascii
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\driver-command.cmd' >> $receipt.log 2>&1; $exitResult = $LASTEXITCODE } finally { Pop-Location }
    if ($exitResult -eq 0) {
        $receipt.binary = Join-Path $buildRoot 'out/ElevationUnitConfigCases.arx'
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

