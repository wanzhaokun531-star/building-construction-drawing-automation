param([string]$Label = 'run')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$safeLabel = $Label -replace '[^A-Za-z0-9_-]', '_'
$buildRoot = Join-Path $projectRoot ('build/n9b-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $safeLabel + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$null = New-Item -ItemType Directory -Path $buildRoot
$receipt = [ordered]@{label=$Label; startedUtc=[DateTime]::UtcNow.ToString('o'); buildRoot=$buildRoot; inputs=@(); buildExitCode=$null; testExitCode=$null; outputs=@(); exitCode=1}
$finalExit = 1
try {
    $inputs = @('src/ElevationModel.h','src/TextOccupancy.h','src/TextOccupancy.cpp','tests/text-occupancy/TextOccupancyTests.cpp','tools/build_text_occupancy.ps1')
    if (Test-Path -LiteralPath (Join-Path $projectRoot 'tests/text-occupancy/TextOccupancyAdapter.cpp')) { $inputs += 'tests/text-occupancy/TextOccupancyAdapter.cpp' }
    if (Test-Path -LiteralPath (Join-Path $projectRoot 'tools/verify_text_occupancy.py')) { $inputs += 'tools/verify_text_occupancy.py' }
    foreach ($relative in $inputs) {
        $destination = Join-Path $buildRoot ('input/' + $relative)
        $null = New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent)
        Copy-Item -LiteralPath (Join-Path $projectRoot $relative) -Destination $destination
        $receipt.inputs += [ordered]@{path=$relative;sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash}
    }
    $null = New-Item -ItemType Directory -Path (Join-Path $buildRoot 'out')
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $command = @('@echo off',('call "' + $vcvars + '" -vcvars_ver=14.16'),'if errorlevel 1 exit /b %errorlevel%',
        'cl /nologo /W4 /WX /EHsc /std:c++17 /MT /utf-8 /O2 /Fe:out\TextOccupancyTests.exe input\src\TextOccupancy.cpp input\tests\text-occupancy\TextOccupancyTests.cpp',
        'if errorlevel 1 exit /b %errorlevel%')
    if ($inputs -contains 'tests/text-occupancy/TextOccupancyAdapter.cpp') {
        $command += 'cl /nologo /W4 /WX /EHsc /std:c++17 /MT /utf-8 /O2 /Fe:out\TextOccupancyAdapter.exe input\src\TextOccupancy.cpp input\tests\text-occupancy\TextOccupancyAdapter.cpp'
        $command += 'if errorlevel 1 exit /b %errorlevel%'
    }
    $command += 'exit /b 0'
    Set-Content -LiteralPath (Join-Path $buildRoot 'build-command.cmd') -Value ($command -join "`r`n") -Encoding ASCII
    Push-Location $buildRoot
    try {
        & $env:ComSpec /d /c 'build-command.cmd' > 'build.log' 2>&1
        $receipt.buildExitCode = $LASTEXITCODE
        if ($receipt.buildExitCode -eq 0) {
            & '.\out\TextOccupancyTests.exe' > 'focused-tests.log' 2>&1
            $receipt.testExitCode = $LASTEXITCODE
            $finalExit = $receipt.testExitCode
        } else { $finalExit = $receipt.buildExitCode }
    } finally { Pop-Location }
} catch { $receipt.error = $_.Exception.Message; $finalExit = 1 }
finally {
    foreach ($file in Get-ChildItem -LiteralPath $buildRoot -File -Recurse | Where-Object { $_.FullName -notlike '*\input\*' -and $_.Name -ne 'receipt.json' }) {
        $receipt.outputs += [ordered]@{path=$file.FullName.Substring($buildRoot.Length+1);sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
    }
    $receipt.exitCode=$finalExit; $receipt.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding UTF8
    Write-Output ('RECEIPT=' + (Join-Path $buildRoot 'receipt.json')); Write-Output ('EXIT_CODE=' + $finalExit)
}
exit $finalExit
