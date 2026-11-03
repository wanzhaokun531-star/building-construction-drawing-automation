param([string]$Label = 'red',[string]$CaseName = 'DimensionAdaptMm100Nontrivial')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildName = 'n8a-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + ($Label -replace '[^A-Za-z0-9_-]', '_') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot = Join-Path (Join-Path $projectRoot 'build') $buildName
$sourceRoot = Join-Path $buildRoot 'src'
$null = New-Item -ItemType Directory -Path $sourceRoot
$receipt = [ordered]@{ contract='bce.dimension-adapt-build.v1'; label=$Label; caseName=$CaseName; startedUtc=[DateTime]::UtcNow.ToString('o'); compiledSourcesDirectory=$sourceRoot; sourceFiles=[ordered]@{}; inputOrigins=@(); binary=$null; binarySHA256=$null; log=(Join-Path $buildRoot 'build.log'); exitCode=1; nativeExecuted=$false; toolchain='AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17' }
$exitResult = 1
try {
    $inputs = @('tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','tests/native/dimension-adapt/DimensionAdaptDriver.cpp','tests/native/dimension-adapt/DimensionAdaptGreen.inc','tests/native/dimension-adapt/DimensionTopology.h','tests/native/dimension-adapt/DimensionTopologyTests.cpp','tests/native/dimension-adapt/RejectionAcceptance.h','tests/native/dimension-adapt/RejectionAcceptanceTests.cpp','tests/native/dimension-adapt/RejectionFocus.inc','tests/native/dimension-adapt/ElevationDimensionAdaptCases.vcxproj')
    foreach ($relative in $inputs) {
        $source = Join-Path $projectRoot $relative
        $destination = Join-Path $sourceRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $destination
        $digest = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $receipt.sourceFiles[$destination] = $digest
        $receipt.inputOrigins += [ordered]@{ origin=$source; compiledCopy=$destination; sha256=$digest }
    }
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $buildRoot 'build_dimension_adapt.ps1')
    $receipt.sourceFiles[(Join-Path $buildRoot 'build_dimension_adapt.ps1')] = (Get-FileHash -LiteralPath (Join-Path $buildRoot 'build_dimension_adapt.ps1')).Hash
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path -LiteralPath $vcvars) -or !(Test-Path -LiteralPath $msbuild)) { throw 'v141 build tools missing' }
    $command = '@echo off' + "`r`n" + 'call "' + $vcvars + '" -vcvars_ver=14.16' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"' + $msbuild + '" ElevationDimensionAdaptCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + 'cl /nologo /std:c++17 /EHsc /MD /W4 /WX /utf-8 DimensionTopologyTests.cpp /Fe"..\out\DimensionTopologyTests.exe"' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"..\out\DimensionTopologyTests.exe"' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + 'cl /nologo /std:c++17 /EHsc /MD /W4 /WX /utf-8 RejectionAcceptanceTests.cpp /Fe"..\out\RejectionAcceptanceTests.exe"' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"..\out\RejectionAcceptanceTests.exe"' + "`r`n" + 'exit /b %errorlevel%' + "`r`n"
    $receipt.commandFile = Join-Path $buildRoot 'build-command.cmd'
    Set-Content -LiteralPath $receipt.commandFile -Value $command -Encoding ascii
    $receipt.commandSHA256 = (Get-FileHash -LiteralPath $receipt.commandFile).Hash
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $exitResult = $LASTEXITCODE } finally { Pop-Location }
    foreach ($entry in $receipt.inputOrigins) { if ((Get-FileHash -LiteralPath $entry.origin).Hash -ne $entry.sha256) { throw 'Live input changed during build' } }
    if ($exitResult -eq 0) {
        $receipt.binary = Join-Path $buildRoot 'out/ElevationDimensionAdaptCases.arx'
        $receipt.binarySHA256 = (Get-FileHash -LiteralPath $receipt.binary -Algorithm SHA256).Hash
    }
} catch { $receipt.error = $_.Exception.Message; $exitResult = 1 }
finally {
    $receipt.exitCode = $exitResult; $receipt.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding utf8
    Write-Output ('RECEIPT=' + (Join-Path $buildRoot 'receipt.json')); Write-Output ('EXIT_CODE=' + $exitResult)
}
exit $exitResult
