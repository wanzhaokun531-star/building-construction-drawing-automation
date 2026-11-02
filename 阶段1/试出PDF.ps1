param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference='Stop'
$run=[IO.Path]::GetFullPath($RunDirectory)
if(!$run.StartsWith((Join-Path $PSScriptRoot 'runs\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Run is outside stage1.'}
$plotDwg=Join-Path $run 'display-check.dwg'
if(!(Test-Path -LiteralPath $plotDwg)){[IO.File]::Copy((Join-Path $run 'clone-cohort.dwg'),$plotDwg,$false)}
$c=[Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
if(!$c.GetAcadState().IsQuiescent){throw 'CAD busy.'}
$d=@($c.Documents|Where-Object{$_.FullName -eq $plotDwg})|Select-Object -First 1
if($d){$d.Activate()}else{$d=$c.Documents.Open($plotDwg,$false)}
$deadline=(Get-Date).AddSeconds(30)
do{try{$idle=$c.GetAcadState().IsQuiescent -and $c.ActiveDocument.GetVariable('CMDNAMES') -eq ''}catch{$idle=$false};if(!$idle){Start-Sleep -Milliseconds 250}}while(!$idle -and (Get-Date)-lt$deadline)
if(!$idle){throw 'Display DWG not idle.'}
$members=foreach($h in @('6DD4','6DE9','DED7','DEDF','DF75')){
  $obj=$d.HandleToObject($h)
  [PSCustomObject]@{Handle=$h;Type=$obj.ObjectName;PublicMembers=@($obj|Get-Member|Select-Object Name,MemberType,Definition)}
}
$members|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $run '公开成员清单.json') -Encoding UTF8
$layout=$d.ActiveLayout
$layout.ConfigName='AutoCAD PDF (General Documentation).pc3'
$layout.RefreshPlotDeviceInfo()
$media=@($layout.GetCanonicalMediaNames())
$media|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'PDF设备纸张.json') -Encoding UTF8
$previousBackground=$d.GetVariable('BACKGROUNDPLOT')
$records=@()
$previousJobs=@()
$previousReceipt=Join-Path $run 'PDF试出记录.json'
if(Test-Path -LiteralPath $previousReceipt){$previousJobs=@((Get-Content -LiteralPath $previousReceipt -Raw|ConvertFrom-Json).Jobs)}
function Wait-PlotIdle {
  $end=(Get-Date).AddSeconds(20);$stable=0
  do{
    try{if($c.GetAcadState().IsQuiescent -and $d.GetVariable('CMDNAMES') -eq ''){$stable++}else{$stable=0}}catch{$stable=0}
    if($stable -ge 3){return}
    Start-Sleep -Milliseconds 250
  }while((Get-Date)-lt$end)
  throw 'Plot cleanup did not become idle.'
}
try{
  $d.SetVariable('BACKGROUNDPLOT',0)
  foreach($job in @(
    @{Name='reference-A3-reviewed';Size='A3';Min=@(34000.0,11000.0);Max=@(57000.0,37400.0)},
    @{Name='scheme-A2-reviewed';Size='A2';Min=@(113500.0,16500.0);Max=@(138500.0,35500.0)}
  )){
    $pdf=Join-Path $run ($job.Name+'.pdf')
    if(Test-Path -LiteralPath $pdf){
      $prior=@($previousJobs|Where-Object{$_.PDF -eq $pdf -and $_.Success -eq $true})|Select-Object -First 1
      if(!$prior){throw 'Existing plot has no successful prior receipt.'}
      $records+=$prior
      continue
    }
    Wait-PlotIdle
    $paper=@($media|Where-Object{$_ -like ('ISO_'+$job.Size+'_*')})|Select-Object -First 1
    if(!$paper){throw ('No normal ISO paper: '+$job.Size)}
    $layout.CanonicalMediaName=$paper
    $layout.PaperUnits=1
    $layout.PlotRotation=0
    $layout.SetWindowToPlot([double[]]$job.Min,[double[]]$job.Max)
    $layout.PlotType=4
    $layout.CenterPlot=$true
    $layout.UseStandardScale=$false
    $layout.SetCustomScale(1.0,100.0)
    $layout.PlotWithPlotStyles=$false
    $layout.PlotWithLineweights=$true
    $d.Regen(1)
    $ok=$d.Plot.PlotToFile($pdf,$layout.ConfigName)
    $records += [PSCustomObject]@{Name=$job.Name;Device=$layout.ConfigName;CanonicalMedia=$paper;Scale='1:100';PlotStyles=$false;Success=$ok;PDF=$pdf;WindowMin=$job.Min;WindowMax=$job.Max}
    if(!$ok){throw 'PlotToFile returned false.'}
    Wait-PlotIdle
  }
  $d.Save()
  $c.ZoomWindow([double[]]@(113500,16500,0),[double[]]@(138500,35500,0))
}finally{
  $d.SetVariable('BACKGROUNDPLOT',$previousBackground)
  [PSCustomObject]@{Jobs=$records;BackgroundPlotRestored=$previousBackground;CTB='Not supplied; this is a device/page test, not final printed-style acceptance';DisplayDWG=$plotDwg}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $run 'PDF试出记录.json') -Encoding UTF8
}
$records|ConvertTo-Json -Depth 6
