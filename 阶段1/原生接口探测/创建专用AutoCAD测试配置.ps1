param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
$profiles='HKCU:\Software\Autodesk\AutoCAD\R23.1\ACAD-3001:804\Profiles'
$source=Join-Path $profiles '<<未命名配置>>'
$token=[Guid]::NewGuid().ToString('N')
$name='CodexS1Native_'+$token.Substring(0,12)
$target=Join-Path $profiles $name
$component='C:\Tangent\TArchT20V8\SYS23x64\tch_kernal.arx'
$support='C:\Tangent\TArchT20V8\SYS23x64'
if(!(Test-Path -LiteralPath $source) -or (Test-Path -LiteralPath $target) -or !(Test-Path -LiteralPath $component)){throw 'Profile/component preflight failed'}
$output=[IO.Path]::GetFullPath($OutputPath)
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'evidence'))+[IO.Path]::DirectorySeparatorChar
if(!$output.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $output)){throw 'New task evidence path required'}
function Get-ProfileFingerprint([string]$ProfilePath){
 $rows=@();$keys=@((Get-Item -LiteralPath $ProfilePath))+@(Get-ChildItem -LiteralPath $ProfilePath -Recurse)
 foreach($key in $keys){foreach($valueName in @($key.GetValueNames()|Sort-Object)){$rows+=@{key=$key.Name.Substring((Get-Item -LiteralPath $ProfilePath).Name.Length);name=$valueName;kind=$key.GetValueKind($valueName).ToString();value=$key.GetValue($valueName,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}}}
 $bytes=[Text.Encoding]::UTF8.GetBytes(($rows|ConvertTo-Json -Depth 8 -Compress));$h=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($h.ComputeHash($bytes))).Replace('-','')}finally{$h.Dispose()}
}
$before=Get-ProfileFingerprint $source
$priorDefault=(Get-Item -LiteralPath $profiles).GetValue('')
Copy-Item -LiteralPath $source -Destination $target -Recurse
$variables=Join-Path $target 'Variables'
$general=Join-Path $target 'General'
$oldTrusted=(Get-Item -LiteralPath $variables).GetValue('TRUSTEDPATHS','')
$trusted=@($oldTrusted.Split(';')|Where-Object {$_})+@($support)
Set-ItemProperty -LiteralPath $variables -Name TRUSTEDPATHS -Value (($trusted|Select-Object -Unique)-join ';')
Set-ItemProperty -LiteralPath $variables -Name SECURELOAD -Value '1'
$oldAcad=(Get-Item -LiteralPath $general).GetValue('ACAD','')
$acad=@($oldAcad.Split(';')|Where-Object {$_})+@($support)
Set-ItemProperty -LiteralPath $general -Name ACAD -Value (($acad|Select-Object -Unique)-join ';')
$after=Get-ProfileFingerprint $source
if($before -cne $after -or (Get-Item -LiteralPath $profiles).GetValue('') -cne $priorDefault){throw 'Source profile/default unexpectedly changed; preserve for review'}
$result=[ordered]@{contract='stage1.dedicated-autocad-profile.v1';recorded=(Get-Date -Format o);authorization='User: 允许，仅修改专用测试配置';profile=$name;registryPath=$target;sourceProfile='<<未命名配置>>';sourceProfileSHA256Before=$before;sourceProfileSHA256After=$after;defaultProfileBefore=$priorDefault;defaultProfileAfter=(Get-Item -LiteralPath $profiles).GetValue('');componentPath=$component;componentSHA256=(Get-FileHash -LiteralPath $component -Algorithm SHA256).Hash;trustedPaths=(Get-Item -LiteralPath $variables).GetValue('TRUSTEDPATHS');SECURELOAD=(Get-Item -LiteralPath $variables).GetValue('SECURELOAD');supportPath=(Get-Item -LiteralPath $general).GetValue('ACAD');COMUsed=$false;ComputerUseUsed=$false;TianzhengCommandsUsed=$false;scope='Only newly created task profile; original profile/default and global application registration unchanged. Native module loading/read/write still requires runtime evidence.'}
[IO.File]::WriteAllText($output,($result|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
$result|ConvertTo-Json -Depth 7
