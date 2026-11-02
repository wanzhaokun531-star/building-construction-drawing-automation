param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference='Stop'
$stage=Split-Path -Parent $PSScriptRoot
$run=[IO.Path]::GetFullPath($RunDirectory)
if(!$run.StartsWith((Join-Path $stage 'runs\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Run outside stage1.'}
$inputDwg=Join-Path $run 'dimension-measure-grip-3.dwg'
$displayDwg=Join-Path $run 'display-dimension-measure.dwg'
if(!(Test-Path -LiteralPath $displayDwg)){[IO.File]::Copy($inputDwg,$displayDwg,$false)}
$cad=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
if(!$cad.GetAcadState().IsQuiescent){throw 'CAD busy.'}
if(@($cad.ListARX()|Where-Object{$_ -eq 'tch_kernal.arx'}).Count -ne 1){throw 'Selected host lacks Tianzheng.'}
$d=@($cad.Documents|Where-Object{$_.FullName -eq $displayDwg})|Select-Object -First 1
if($d){$d.Activate()}else{$d=$cad.Documents.Open($displayDwg,$false)}
function Wait-PlotIdle{
  $limit=(Get-Date).AddSeconds(20);$stable=0
  do{try{if($cad.GetAcadState().IsQuiescent -and $d.GetVariable('CMDNAMES') -eq ''){$stable++}else{$stable=0}}catch{$stable=0};if($stable -ge 3){return};Start-Sleep -Milliseconds 250}while((Get-Date)-lt$limit)
  throw 'CAD did not become idle.'
}
Wait-PlotIdle
$l=$d.ActiveLayout
if(!$l.ModelType){throw 'Expected model layout.'}
$l.ConfigName='AutoCAD PDF (General Documentation).pc3'
$l.RefreshPlotDeviceInfo()
$paper=@($l.GetCanonicalMediaNames()|Where-Object{$_ -eq 'ISO_A3_(420.00_x_297.00_MM)'})|Select-Object -First 1
if(!$paper){throw 'Expected ISO A3 media absent.'}
$oldBackground=$d.GetVariable('BACKGROUNDPLOT')
try{
  $d.SetVariable('BACKGROUNDPLOT',0)
  $l.CanonicalMediaName=$paper;$l.PaperUnits=1;$l.PlotRotation=0
  $l.SetWindowToPlot([double[]]@(34000,11000),[double[]]@(57500,37400))
  $l.PlotType=4;$l.CenterPlot=$true;$l.UseStandardScale=$false;$l.SetCustomScale(1.0,100.0)
  $l.PlotWithPlotStyles=$false;$l.PlotWithLineweights=$true
  $d.Regen(1);Wait-PlotIdle
  $pdf=Join-Path $run '原生测点改动验证.pdf'
  if(Test-Path -LiteralPath $pdf){throw 'Plot file already exists.'}
  $ok=$d.Plot.PlotToFile($pdf,$l.ConfigName);Wait-PlotIdle
  if(!$ok){throw 'PlotToFile returned false.'}
  $d.Save()
  [pscustomobject]@{PDF=$pdf;Success=$ok;Device=$l.ConfigName;Media=$paper;Scale='1:100';PlotStyles=$false;Content='Reference clone; dimension 19A0 grip3 moved X+500 through native ARX';OriginalDwgWritten=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'native-measure-plot.json') -Encoding UTF8
}finally{$d.SetVariable('BACKGROUNDPLOT',$oldBackground)}
Get-Item -LiteralPath $pdf|Select-Object FullName,Length|ConvertTo-Json
