$ErrorActionPreference='Stop'
$c=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
if(!$c.GetAcadState().IsQuiescent){throw 'CAD busy; no command cancelled.'}
$d=$c.ActiveDocument
if($d.FullName -notlike '*建筑施工图自动化\阶段1\runs\*\retry-control.dwg'){throw 'Expected task-owned control document.'}
$run=Split-Path -Parent $d.FullName
$trusted='C:\Users\PC\AppData\Roaming\Tangent\TArch20V8\sys'
$existing=@($d.GetVariable('TRUSTEDPATHS').Split(';') | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\') })
if($existing -notcontains $trusted.TrimEnd('\')){throw 'Deployment directory is not already trusted.'}
if(!(Test-Path -LiteralPath $trusted -PathType Container)){throw 'Existing trusted directory absent.'}
$source=Join-Path $PSScriptRoot 'bin\Stage1Probe.arx'
$hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$target=Join-Path $trusted ('S1Probe_'+$hash.Substring(0,12)+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.arx')
[IO.File]::Copy($source,$target,$false)
$r=[ordered]@{Source=$source;Target=$target;SHA256=$hash;OriginalSecurity=$d.GetVariable('SECURELOAD');SecuritySettingsChanged=$false;Load='not completed';Unload='not executed';LoadedModules=@()}
$r|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'trusted-deployment.json') -Encoding UTF8
try {
  $c.LoadARX($target)
  $r.Load='returned successfully'
  $r.LoadedModules=@($c.ListARX()|Where-Object {$_ -like ('*'+[IO.Path]::GetFileName($target)+'*')})
  $c.UnloadARX($target)
  $r.Unload='returned successfully'
}catch{$r.Error=$_.Exception.Message}
finally{
  $r|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $run 'trusted-deployment.json') -Encoding UTF8
}
$r|ConvertTo-Json -Depth 5
