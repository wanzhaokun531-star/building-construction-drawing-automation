param([string]$Label='red',[ValidateSet('Codec','Native')][string]$Target='Codec')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$buildRoot=Join-Path $projectRoot ('build/g2-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+$Label+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
$sourceRoot=Join-Path $buildRoot 'src'
$null=New-Item -ItemType Directory -Path $sourceRoot
$receipt=[ordered]@{contract='bce.store-build.v1';target=$Target;label=$Label;sourceFiles=[ordered]@{};inputOrigins=@();binary=$null;binarySHA256=$null;exitCode=1;testExitCode=$null;log=(Join-Path $buildRoot 'build.log');toolchain='v141 14.16.27023 x64 C++17 ObjectARX2020'}
$inputs=@('src/ElevationModel.h','src/RuleModel.h','src/Recognition.h','src/GenerationState.h','src/ProjectStoreCodec.h','src/ProjectStoreCodec.cpp','tests/native/store/StoreFixture.h')
if($Target -eq 'Codec') { $inputs+=@('tests/native/store/StoreCodecTests.cpp','tests/native/store/StoreCodecTests.vcxproj'); $project='StoreCodecTests'; $extension='.exe' }
else { $inputs+=@('src/ElevationModel.cpp','src/WriteContracts.h','tests/native/inplace/NativeInPlaceAudit.h','src/SceneReader.h','src/SceneReader.cpp','src/ProjectStore.h','src/ProjectStore.cpp','tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','tests/native/store/StoreDriver.cpp','tests/native/store/StoreNativeCases.vcxproj'); $project='StoreNativeCases'; $extension='.arx' }
$result=1
try {
 foreach($relative in $inputs) { $origin=Join-Path $projectRoot $relative; $copy=Join-Path $sourceRoot (Split-Path $relative -Leaf); Copy-Item -LiteralPath $origin -Destination $copy; $sha=(Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash; $receipt.sourceFiles[$copy]=$sha; $receipt.inputOrigins+=@{origin=$origin;compiledCopy=$copy;sha256=$sha} }
 Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $buildRoot 'build_store.ps1')
 $vcvars='C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
 $msbuild='C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
 $command='@echo off'+"`r`n"+'call "'+$vcvars+'" -vcvars_ver=14.16'+"`r`n"+'if errorlevel 1 exit /b %errorlevel%'+"`r`n"+'"'+$msbuild+'" '+$project+'.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal'+"`r`n"+'exit /b %errorlevel%'
 Set-Content -LiteralPath (Join-Path $buildRoot 'build-command.cmd') -Value $command -Encoding ascii
 Push-Location $sourceRoot
 try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $result=$LASTEXITCODE } finally { Pop-Location }
 if($result -eq 0) { $receipt.binary=Join-Path $buildRoot ('out/'+$project+$extension); $receipt.binarySHA256=(Get-FileHash -LiteralPath $receipt.binary -Algorithm SHA256).Hash; if($Target -eq 'Codec') { & $receipt.binary > (Join-Path $buildRoot 'tests.log') 2>&1; $receipt.testExitCode=$LASTEXITCODE } }
} catch { $receipt.error=$_.Exception.Message } finally { $receipt.exitCode=$result; $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding utf8; Write-Output ('RECEIPT='+ (Join-Path $buildRoot 'receipt.json')); Write-Output ('BUILD='+$result+' TEST='+$receipt.testExitCode) }
exit $result

