param([Parameter(Mandatory=$true)][string]$RunDirectory,[ValidateSet('Dimension','Axis','WallAxis','Renumber','DimPoints')][string]$TrialSet='Dimension')
$ErrorActionPreference='Stop'
$stage=Split-Path -Parent $PSScriptRoot
$run=[IO.Path]::GetFullPath($RunDirectory)
if(!$run.StartsWith((Join-Path $stage 'runs\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Run outside stage1.'}
$cad=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
if(!$cad.GetAcadState().IsQuiescent -or @($cad.ListARX()|Where-Object{$_ -eq 'tch_kernal.arx'}).Count -ne 1){throw 'Tianzheng host is not ready.'}
function Wait-Idle($doc){
  $limit=(Get-Date).AddSeconds(20);$stable=0
  do{try{if($cad.GetAcadState().IsQuiescent -and $doc.GetVariable('CMDNAMES') -eq ''){$stable++}else{$stable=0}}catch{$stable=0};if($stable -ge 3){return};Start-Sleep -Milliseconds 250}while((Get-Date)-lt$limit)
  throw 'Host did not become idle.'
}
$records=@()
$caseNames=switch($TrialSet){'Axis'{@('scheme-axis-transfer','scheme-axis-adjust')}'WallAxis'{@('wall-axis-generated')}'Renumber'{@('axis-renumber-generated')}'DimPoints'{@('dim-points-generated')}'Dimension'{@('scheme-existing-dimension','scheme-added-dimension')}}
foreach($name in $caseNames){
  $display=Join-Path $run ('display-'+$name+'.dwg')
  if(!(Test-Path -LiteralPath $display)){[IO.File]::Copy((Join-Path $run ($name+'.dwg')),$display,$false)}
  $d=@($cad.Documents|Where-Object{$_.FullName -eq $display})|Select-Object -First 1
  if($d){$d.Activate()}else{$d=$cad.Documents.Open($display,$false)}
  Wait-Idle $d
  $l=$d.ActiveLayout
  if(!$l.ModelType){throw 'Expected model layout.'}
  $l.ConfigName='AutoCAD PDF (General Documentation).pc3';$l.RefreshPlotDeviceInfo()
  $media=@($l.GetCanonicalMediaNames()|Where-Object{$_ -eq 'ISO_A3_(420.00_x_297.00_MM)'})|Select-Object -First 1
  if(!$media){throw 'ISO A3 media absent.'}
  $oldBackground=$d.GetVariable('BACKGROUNDPLOT')
  try{
    $d.SetVariable('BACKGROUNDPLOT',0)
    $l.CanonicalMediaName=$media;$l.PaperUnits=1;$l.PlotRotation=0
    $l.SetWindowToPlot([double[]]@(11000,11000),[double[]]@(37000,39000))
    $l.PlotType=4;$l.CenterPlot=$true;$l.UseStandardScale=$false;$l.SetCustomScale(1.0,100.0)
    $l.PlotWithPlotStyles=$false;$l.PlotWithLineweights=$true
    $d.Regen(1);Wait-Idle $d
    if($cad.ActiveDocument.FullName -ne $d.FullName){throw 'Active document changed during plot setup.'}
    $pdf=Join-Path $run ($name+'.pdf')
    if(Test-Path -LiteralPath $pdf){throw 'PDF output already exists.'}
    $ok=$d.Plot.PlotToFile($pdf,$l.ConfigName);Wait-Idle $d
    if(!$ok){throw 'PlotToFile returned false.'}
    $d.Save()
    $records+=[pscustomobject]@{Case=$name;PDF=$pdf;Success=$ok;Device=$l.ConfigName;Media=$media;Scale='1:100';Styles=$false;Source='Scheme drawing working copy';ReferenceBuildingImported=$false}
  }finally{$d.SetVariable('BACKGROUNDPLOT',$oldBackground)}
  $receiptName=if($TrialSet -eq 'Axis'){'scheme-axis-plot-receipt.json'}elseif($TrialSet -eq 'Dimension'){'scheme-plot-receipt.json'}else{$TrialSet.ToLower()+'-plot-receipt.json'}
  $records|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $run $receiptName) -Encoding UTF8
}
$records|ConvertTo-Json -Depth 6
