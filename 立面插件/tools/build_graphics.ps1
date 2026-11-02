param([string]$Label = 'candidate')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildName = 'n5-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + ($Label -replace '[^A-Za-z0-9_-]', '_') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot = Join-Path (Join-Path $projectRoot 'build') $buildName
$sourceRoot = Join-Path $buildRoot 'src'
$null = New-Item -ItemType Directory -Path $sourceRoot
$receipt = [ordered]@{ contract='bce.graphics-build.v1'; label=$Label; startedUtc=[DateTime]::UtcNow.ToString('o'); compiledSourcesDirectory=$sourceRoot; sourceFiles=[ordered]@{}; inputOrigins=@(); binary=$null; binarySHA256=$null; log=(Join-Path $buildRoot 'build.log'); exitCode=1; toolchain='AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17' }
$exitResult = 1
try {
    $inputs = @('tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','tests/native/graphics-read/GraphicsReadProbe.cpp','tests/native/graphics-read/GiCapture.cpp','tests/native/graphics-read/GiCapture.h','tests/native/graphics-read/GraphicsPolicy.h','tests/native/graphics-read/GraphicsPolicyTests.cpp','tests/native/graphics-read/GraphicsControlPolicy.h','tests/native/graphics-read/GraphicsControlPolicyTests.cpp','tests/native/graphics-read/ElevationGraphicsCases.vcxproj')
    foreach ($relative in $inputs) {
        $source = Join-Path $projectRoot $relative
        $destination = Join-Path $sourceRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $destination
        $digest = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $receipt.sourceFiles[$destination] = $digest
        $receipt.inputOrigins += [ordered]@{ origin=$source; compiledCopy=$destination; sha256=$digest }
    }
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $buildRoot 'build_graphics.ps1')
    $receipt.sourceFiles[(Join-Path $buildRoot 'build_graphics.ps1')] = (Get-FileHash -LiteralPath (Join-Path $buildRoot 'build_graphics.ps1') -Algorithm SHA256).Hash
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path -LiteralPath $vcvars) -or !(Test-Path -LiteralPath $msbuild)) { throw 'v141 build tools missing' }
    $command = '@echo off' + "`r`n" + 'call "' + $vcvars + '" -vcvars_ver=14.16' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + 'cl /nologo /EHsc /std:c++17 /W4 /WX /Fe:GraphicsPolicyTests.exe GraphicsPolicyTests.cpp' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + 'GraphicsPolicyTests.exe' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + 'cl /nologo /EHsc /std:c++17 /W4 /WX /utf-8 /Fe:GraphicsControlPolicyTests.exe GraphicsControlPolicyTests.cpp' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + 'GraphicsControlPolicyTests.exe' + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + '"' + $msbuild + '" ElevationGraphicsCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal' + "`r`n" + 'exit /b %errorlevel%' + "`r`n"
    Set-Content -LiteralPath (Join-Path $buildRoot 'build-command.cmd') -Value $command -Encoding ascii
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $exitResult = $LASTEXITCODE } finally { Pop-Location }
    if ($exitResult -eq 0) {
        $receipt.binary = Join-Path $buildRoot 'out/ElevationGraphicsCases.arx'
        $receipt.binarySHA256 = (Get-FileHash -LiteralPath $receipt.binary -Algorithm SHA256).Hash
        $receipt.policyBinarySHA256 = (Get-FileHash -LiteralPath (Join-Path $sourceRoot 'GraphicsPolicyTests.exe') -Algorithm SHA256).Hash
        $receipt.controlPolicyBinarySHA256 = (Get-FileHash -LiteralPath (Join-Path $sourceRoot 'GraphicsControlPolicyTests.exe') -Algorithm SHA256).Hash
        foreach ($entry in $receipt.inputOrigins) {
            if ((Get-FileHash -LiteralPath $entry.origin).Hash -ne $entry.sha256) { throw ('Source changed during build: ' + $entry.origin) }
        }
    }
} catch {
    $receipt.error = $_.Exception.Message
    $exitResult = 1
} finally {
    $receipt.exitCode = $exitResult
    $receipt.finishedUtc = [DateTime]::UtcNow.ToString('o')
    if (Test-Path -LiteralPath $receipt.log) { $receipt.logSHA256 = (Get-FileHash -LiteralPath $receipt.log).Hash }
    $commandFile = Join-Path $buildRoot 'build-command.cmd'
    if (Test-Path -LiteralPath $commandFile) { $receipt.commandSHA256 = (Get-FileHash -LiteralPath $commandFile).Hash }
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding utf8
    Write-Output ('RECEIPT=' + (Join-Path $buildRoot 'receipt.json'))
    Write-Output ('EXIT_CODE=' + $exitResult)
}
exit $exitResult

