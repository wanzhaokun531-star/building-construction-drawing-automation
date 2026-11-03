param([string]$Label = 'red')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildName = 'm2-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + ($Label -replace '[^A-Za-z0-9_-]', '_') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot = Join-Path (Join-Path $projectRoot 'build') $buildName
$sourceRoot = Join-Path $buildRoot 'src'
$null = New-Item -ItemType Directory -Path $sourceRoot
$receipt = [ordered]@{ contract='bce.material-tiles-build.v1'; label=$Label; startedUtc=[DateTime]::UtcNow.ToString('o'); compiledSourcesDirectory=$sourceRoot; sourceFiles=[ordered]@{}; inputOrigins=@(); binary=$null; binarySHA256=$null; log=(Join-Path $buildRoot 'build.log'); exitCode=1; nativeExecuted=$false;toolchain='AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17' }
$exitResult = 1
try {
    $inputs = @('src/ElevationModel.h','src/ElevationModel.cpp','src/RuleModel.h','src/Recognition.h','src/MaterialRules.h','src/MaterialRules.cpp','src/NativeMaterials.h','src/NativeMaterials.cpp','tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','src/NativeMaterialTiles.h','src/NativeMaterialTiles.cpp','tests/native/material-tiles/MaterialTileDriver.cpp','tests/native/material-tiles/MaterialTileFixtures.h','tests/native/material-tiles/OwnedApiCompileCheck.cpp','tests/native/material-tiles/ElevationMaterialTileCases.vcxproj')
    $m1Root = Join-Path $projectRoot 'build/m1-20261101-221054-935-review-fix1-three-contracts-green-f81070ee'
    $m1ReceiptPath = Join-Path $m1Root 'receipt.json'
    $m1Sha = 'A898B95C981FD852953F8500FCBFC244201C022A102CEC28E0D69A511B587BE6'
    if ((Get-FileHash -LiteralPath $m1ReceiptPath).Hash -ne $m1Sha) { throw 'M1 receipt changed' }
    $m1 = Get-Content -Raw -LiteralPath $m1ReceiptPath | ConvertFrom-Json
    $receipt.m1Dependency = @{receipt=$m1ReceiptPath;sha256=$m1Sha}
    foreach ($relative in $inputs) {
        $source = Join-Path $projectRoot $relative
        if ($relative -notmatch 'NativeMaterialTiles|material-tiles') {
            $source = Join-Path (Join-Path $m1Root 'src') (Split-Path $relative -Leaf)
            $expected = $m1.sourceFiles.PSObject.Properties[$source].Value
            if (!$expected -or (Get-FileHash -LiteralPath $source).Hash -ne $expected) { throw ('Frozen dependency changed: '+$source) }
        }
        $destination = Join-Path $sourceRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $destination
        $digest = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $receipt.sourceFiles[$destination] = $digest
        $receipt.inputOrigins += [ordered]@{ origin=$source; compiledCopy=$destination; sha256=$digest }
    }
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $buildRoot 'build_material_tiles.ps1')
    $receipt.sourceFiles[(Join-Path $buildRoot 'build_material_tiles.ps1')] = (Get-FileHash -LiteralPath (Join-Path $buildRoot 'build_material_tiles.ps1') -Algorithm SHA256).Hash
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path -LiteralPath $vcvars) -or !(Test-Path -LiteralPath $msbuild)) { throw 'v141 build tools missing' }
    $command = '@echo off' + "`r`n" + 'call "' + $vcvars + '" -vcvars_ver=14.16' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"' + $msbuild + '" ElevationMaterialTileCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n"
    # Separate compile-only checks deliberately omit BCE_MATERIAL_TILE_DIAGNOSTICS.
    $clOptions = 'cl /nologo /c /std:c++17 /EHsc /MD /W4 /WX /sdl /permissive- /utf-8 /wd4828 /DWIN32 /D_WINDOWS /D_USRDLL /DUNICODE /D_UNICODE /I"C:\Autodesk\ObjectARX_for_AutoCAD_2020_Win_64_bit\inc" /I"C:\Autodesk\ObjectARX_for_AutoCAD_2020_Win_64_bit\inc-x64" '
    $command += $clOptions + 'NativeMaterialTiles.cpp /Fo"..\obj\NativeMaterialTiles.product.obj"' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n"
    $command += $clOptions + 'OwnedApiCompileCheck.cpp /Fo"..\obj\OwnedApiCompileCheck.obj"' + "`r`n" + 'exit /b %errorlevel%' + "`r`n"
    Set-Content -LiteralPath (Join-Path $buildRoot 'build-command.cmd') -Value $command -Encoding ascii
    $receipt.commandFile = Join-Path $buildRoot 'build-command.cmd'
    $receipt.commandSHA256 = (Get-FileHash -LiteralPath $receipt.commandFile).Hash
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $exitResult = $LASTEXITCODE } finally { Pop-Location }
    foreach ($entry in $receipt.inputOrigins) { if ((Get-FileHash -LiteralPath $entry.origin).Hash -ne $entry.sha256) { throw 'Live input changed during build' } }
    if ($exitResult -eq 0) {
        $receipt.binary = Join-Path $buildRoot 'out/ElevationMaterialTileCases.arx'
        $receipt.binarySHA256 = (Get-FileHash -LiteralPath $receipt.binary -Algorithm SHA256).Hash
        $receipt.offlineOwnedContract = [ordered]@{diagnosticsDefined=$false;hostExecuted=$false;productObject=(Join-Path $buildRoot 'obj/NativeMaterialTiles.product.obj');apiObject=(Join-Path $buildRoot 'obj/OwnedApiCompileCheck.obj')}
        $receipt.offlineOwnedContract.productObjectSHA256 = (Get-FileHash -LiteralPath $receipt.offlineOwnedContract.productObject).Hash
        $receipt.offlineOwnedContract.apiObjectSHA256 = (Get-FileHash -LiteralPath $receipt.offlineOwnedContract.apiObject).Hash
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






