param([string]$Label='candidate')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$buildRoot=Join-Path (Join-Path $projectRoot 'build') ('n3-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+$Label+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$null=New-Item -ItemType Directory -Path (Join-Path $buildRoot 'src')
$receipt=[ordered]@{contract='bce.level-build.v1';label=$Label;exitCode=1;binary=(Join-Path $buildRoot 'out/ElevationLevelCases.arx');binarySHA256=$null;sourceFiles=[ordered]@{};liveSourceFiles=[ordered]@{};compiledSourcesDirectory=(Join-Path $buildRoot 'src');log=(Join-Path $buildRoot 'build.log');commandFile=(Join-Path $buildRoot 'build.cmd');commandSHA256=$null;nativeExecuted=$false;buildScript=$PSCommandPath;buildScriptSHA256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;policyTestsExitCode=$null}
try {
    $sources=@(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'tests/native/level') -File)
    $sources+=Get-Item -LiteralPath (Join-Path $projectRoot 'tests/native/NativeEvidence.h'),(Join-Path $projectRoot 'tests/native/NativeEvidence.cpp')
    foreach($file in $sources){Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $buildRoot 'src');$receipt.liveSourceFiles[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
    @'
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.16
if errorlevel 1 exit /b %errorlevel%
if not exist ..\out mkdir ..\out
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /MD LevelPolicyTests.cpp /Fe:..\out\LevelPolicyTests.exe /Fo:..\out\LevelPolicyTests.obj
if errorlevel 1 exit /b %errorlevel%
..\out\LevelPolicyTests.exe > ..\policy-tests.log 2>&1
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" ElevationLevelCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal
exit /b %errorlevel%
'@ | Set-Content -LiteralPath $receipt.commandFile -Encoding ascii
    $receipt.commandSHA256=(Get-FileHash -LiteralPath $receipt.commandFile).Hash
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $buildRoot 'src') -File){$receipt.sourceFiles[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
    Push-Location (Join-Path $buildRoot 'src')
    try {& $env:ComSpec /d /c '..\build.cmd' > $receipt.log 2>&1;$receipt.exitCode=$LASTEXITCODE}finally{Pop-Location}
    foreach($file in $sources){if((Get-FileHash -LiteralPath $file.FullName).Hash -ne $receipt.liveSourceFiles[$file.FullName]){throw 'Live source changed during build'}}
    $policyLog=Join-Path $buildRoot 'policy-tests.log'
    if(Test-Path -LiteralPath $policyLog){$receipt.policyTestsLog=$policyLog;$receipt.policyTestsLogSHA256=(Get-FileHash -LiteralPath $policyLog).Hash;if((Get-Content -LiteralPath $policyLog -Raw) -match 'FAILED=0'){$receipt.policyTestsExitCode=0}}
    if(Test-Path -LiteralPath $receipt.binary){$receipt.binarySHA256=(Get-FileHash -LiteralPath $receipt.binary).Hash}
    if($receipt.exitCode -eq 0 -and ($null -eq $receipt.binarySHA256 -or $receipt.policyTestsExitCode -ne 0)){throw 'Build/test proof incomplete'}
}catch{$receipt.error=$_.Exception.Message;$receipt.exitCode=1}finally{
    $receipt|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $buildRoot 'build-receipt.json') -Encoding utf8
    if(Test-Path -LiteralPath $receipt.log){Get-Content -LiteralPath $receipt.log -Tail 35}
    Write-Output ('BUILD_ROOT='+$buildRoot)
    Write-Output ('SHA='+$receipt.binarySHA256)
    Write-Output ('EXIT_CODE='+$receipt.exitCode)
}
exit $receipt.exitCode
