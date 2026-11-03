param([string]$Label='red')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$buildName='i3b-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+($Label -replace '[^A-Za-z0-9_-]','_')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$buildRoot=Join-Path (Join-Path $projectRoot 'build') $buildName
$sourceRoot=Join-Path $buildRoot 'src'
$null=New-Item -ItemType Directory -Path $sourceRoot
$receipt=[ordered]@{contract='bce.project-fact-source-build.v1';label=$Label;caseName='CurrentProjectFactSources';startedUtc=[DateTime]::UtcNow.ToString('o');compiledSourcesDirectory=$sourceRoot;sourceFiles=[ordered]@{};inputOrigins=@();binary=$null;binarySHA256=$null;log=(Join-Path $buildRoot 'build.log');exitCode=1;nativeExecuted=$false;toolchain='AutoCAD2020 SDK23.1 / v141 14.16.27023 / x64 / C++17'}
$exitResult=1
try {
    $inputs=@('src/ElevationModel.h','src/ElevationModel.cpp','src/RuleModel.h','src/Recognition.h','src/Recognition.cpp','src/SceneReader.h','src/SceneReader.cpp','src/AnalysisReader.h','src/AnalysisReader.cpp','src/ProjectFacts.h','src/ProjectFactsReader.h','src/ProjectFactsReader.cpp','tests/native/NativeEvidence.h','tests/native/NativeEvidence.cpp','tests/native/project-facts/ProjectFactsFixture.h','tests/native/project-facts/ProjectFactsDriver.cpp','tests/native/project-facts/ElevationProjectFactCases.vcxproj')
    foreach($relative in $inputs) {
        $source=Join-Path $projectRoot $relative; $copy=Join-Path $sourceRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $copy
        $digest=(Get-FileHash -LiteralPath $copy).Hash
        $receipt.sourceFiles[$copy]=$digest; $receipt.inputOrigins += [ordered]@{origin=$source;compiledCopy=$copy;sha256=$digest}
    }
    foreach($relative in @('evidence/current-project-datum-audit.json','tests/fixtures/scheme-scene.tsv')) {
        $source=Join-Path $projectRoot $relative; $copy=Join-Path $buildRoot (Split-Path $relative -Leaf)
        Copy-Item -LiteralPath $source -Destination $copy
        $digest=(Get-FileHash -LiteralPath $copy).Hash
        $receipt.sourceFiles[$copy]=$digest; $receipt.inputOrigins += [ordered]@{origin=$source;compiledCopy=$copy;sha256=$digest}
    }
    $audit=Get-Content -Raw -LiteralPath (Join-Path $buildRoot 'current-project-datum-audit.json') | ConvertFrom-Json
    if($audit.schemeSHA256 -ne '595CE82FBD20C81E83A857AB569385BEFA98ADAF743300A42668E280561559C1') { throw 'Audited scheme identity mismatch' }
    $rows=Import-Csv -LiteralPath (Join-Path $buildRoot 'scheme-scene.tsv') -Delimiter "`t"
    $header='#pragma once'+"`n"+'#include <string>'+"`n"+'#include <vector>'+"`n"+'struct AuditedView { std::string role,title,datum,lower,upper,titleText; double x,y,upperY; };'+"`n"+'inline std::vector<AuditedView> auditedViews() { return {'+"`n"
    foreach($v in $audit.views) {
        $title=($rows | Where-Object { $_.handle -eq $v.titleHandle }).text
        if(!$title -or $title.Contains('"') -or $title.Contains('\')) { throw 'Invalid title fixture' }
        $x=([double]$v.datumPoint[0]).ToString('R',[Globalization.CultureInfo]::InvariantCulture)
        $y=([double]$v.datumPoint[1]).ToString('R',[Globalization.CultureInfo]::InvariantCulture)
        $uy=([double]$v.upperPoint[1]).ToString('R',[Globalization.CultureInfo]::InvariantCulture)
        $header+='{"'+$v.role+'","'+$v.titleHandle+'","'+$v.datumSourceHandle+'","'+$v.labelHandles[0]+'","'+$v.labelHandles[1]+'",u8"'+$title+'",'+$x+','+$y+','+$uy+'},'+"`n"
    }
    $header+='}; }'+"`n"
    $headerPath=Join-Path $sourceRoot 'AuditedFactLocations.h'; Set-Content -LiteralPath $headerPath -Value $header -Encoding utf8
    $receipt.sourceFiles[$headerPath]=(Get-FileHash -LiteralPath $headerPath).Hash
    $scriptCopy=Join-Path $buildRoot 'build_project_facts_reader.ps1'; Copy-Item -LiteralPath $PSCommandPath -Destination $scriptCopy
    $receipt.sourceFiles[$scriptCopy]=(Get-FileHash -LiteralPath $scriptCopy).Hash
    $receipt.inputOrigins += [ordered]@{origin=$PSCommandPath;compiledCopy=$scriptCopy;sha256=$receipt.sourceFiles[$scriptCopy]}
    $vcvars='C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat'
    $msbuild='C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
    $command='@echo off'+"`r`n"+'call "'+$vcvars+'" -vcvars_ver=14.16'+"`r`n"+'if errorlevel 1 exit /b %errorlevel%'+"`r`n"+'"'+$msbuild+'" ElevationProjectFactCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal'+"`r`n"+'exit /b %errorlevel%'+"`r`n"
    $receipt.commandFile=Join-Path $buildRoot 'build-command.cmd'; Set-Content -LiteralPath $receipt.commandFile -Value $command -Encoding ascii
    $receipt.sourceFiles[$receipt.commandFile]=(Get-FileHash -LiteralPath $receipt.commandFile).Hash
    Push-Location $sourceRoot
    try { & $env:ComSpec /d /c '..\build-command.cmd' > $receipt.log 2>&1; $exitResult=$LASTEXITCODE } finally { Pop-Location }
    foreach($entry in $receipt.inputOrigins) { if((Get-FileHash -LiteralPath $entry.origin).Hash -ne $entry.sha256) { throw 'Live input changed during build' } }
    if($exitResult -eq 0) { $receipt.binary=Join-Path $buildRoot 'out/ElevationProjectFactCases.arx'; $receipt.binarySHA256=(Get-FileHash -LiteralPath $receipt.binary).Hash }
} catch { $receipt.error=$_.Exception.Message; $exitResult=1 }
finally { $receipt.exitCode=$exitResult; $receipt.finishedUtc=[DateTime]::UtcNow.ToString('o'); $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'receipt.json') -Encoding utf8; Write-Output ('RECEIPT='+(Join-Path $buildRoot 'receipt.json')); Write-Output ('EXIT_CODE='+$exitResult) }
exit $exitResult
