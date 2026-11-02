param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
$stage=Split-Path -Parent $PSScriptRoot
$old=Get-Content -LiteralPath (Join-Path $PSScriptRoot '新建轴号集合_输入复核_20261028-2118.json') -Raw -Encoding UTF8|ConvertFrom-Json
function Get-SharedSHA256([string]$Path){
 $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
 $algorithm=[Security.Cryptography.SHA256]::Create()
 try{return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','')}finally{$algorithm.Dispose();$stream.Dispose()}
}
$rows=@(foreach($item in $old){$a=Get-SharedSHA256 $item.Source;$b=Get-SharedSHA256 $item.Snapshot;[ordered]@{Id=$item.Id;Source=$item.Source;Snapshot=$item.Snapshot;SourceSHA256=$a;SnapshotSHA256=$b;SourceUnchanged=($a -eq $item.SourceSHA256);SnapshotUnchanged=($b -eq $item.SnapshotSHA256)}})
$pending=@(Get-ChildItem -LiteralPath 'C:\Users\PC\AppData\Roaming\Tangent\TArch20V8\sys' -File -Filter 'S1Native_*' | ForEach-Object{[ordered]@{Path=$_.FullName;Length=$_.Length}})
$report=[ordered]@{Contract='stage1.final-input-and-module-audit.v1';Recorded=(Get-Date -Format o);Baseline='新建轴号集合_输入复核_20261028-2118.json';Inputs=$rows;AllInputsUnchanged=(@($rows|Where-Object{!$_.SourceUnchanged -or !$_.SnapshotUnchanged}).Count -eq 0);TemporaryModulesRemaining=$pending;TemporaryModuleDirectoryEmpty=($pending.Count -eq 0);ReadScope='Read shared files and directory only; no DWG write/save/document switch or module removal'}
if(Test-Path -LiteralPath $OutputPath){throw 'Audit output already exists'}
$report|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $OutputPath -Encoding UTF8
[pscustomobject]@{AllInputsUnchanged=$report['AllInputsUnchanged'];TemporaryModuleDirectoryEmpty=$report['TemporaryModuleDirectoryEmpty']}|ConvertTo-Json
