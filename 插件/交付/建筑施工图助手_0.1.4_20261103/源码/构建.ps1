$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$taskMSBuild='C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe'
$taskBuild=Join-Path $taskRoot ('构建产物_'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$taskBin=Join-Path $taskBuild 'bin'
$taskObj=Join-Path $taskBuild 'obj'
& $taskMSBuild (Join-Path $taskRoot '插件/src/BuildingPlanPlugin2020.vcxproj') /p:Configuration=Release /p:Platform=x64 "/p:OutDir=$taskBin\" "/p:IntDir=$taskObj\" /m /nologo
if($LASTEXITCODE -ne 0){throw 'ARX build failed'}
$taskBundle=Join-Path $taskBuild 'BuildingPlan2020.bundle'
$taskContent=Join-Path $taskBundle 'Contents/Win64'
New-Item -ItemType Directory -Path $taskContent -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $taskBin 'BuildingPlanPlugin2020.arx') -Destination $taskContent
Copy-Item -LiteralPath (Join-Path $taskRoot '插件/PackageContents.xml') -Destination $taskBundle
Copy-Item -LiteralPath (Join-Path $taskRoot '插件/使用说明.md') -Destination $taskBundle
Copy-Item -LiteralPath (Join-Path $taskRoot '插件/resources/runtime') -Destination (Join-Path $taskContent 'resources') -Recurse
[System.IO.File]::WriteAllText((Join-Path $taskContent 'BuildingPlanPlugin2020.ini'),"[product]`r`ntangentDirectory=C:\Tangent\TArchT20V8\SYS23x64`r`n",[System.Text.Encoding]::Unicode)
Write-Host ('ARX and bundle: '+$taskBundle)
