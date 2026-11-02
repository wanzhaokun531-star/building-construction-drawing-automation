$ErrorActionPreference = 'Stop'
$cadApp = [Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
$rows = foreach ($doc in $cadApp.Documents) {
    [PSCustomObject]@{
        Name = $doc.Name
        FullName = $doc.FullName
        ReadOnly = $doc.ReadOnly
        INSUNITS = $doc.GetVariable('INSUNITS')
        MEASUREMENT = $doc.GetVariable('MEASUREMENT')
        LUNITS = $doc.GetVariable('LUNITS')
        DIMSCALE = $doc.GetVariable('DIMSCALE')
    }
}
$dbx = $cadApp.GetInterfaceObject('ObjectDBX.AxDbDocument.23')
$known = @()
foreach ($pair in @(
    @('方案', (Join-Path $PSScriptRoot '..\阶段0\输入快照\方案平面图.dwg'), 'CD84'),
    @('参考', (Join-Path $PSScriptRoot '..\阶段0\输入快照\施工图平面参考图.dwg'), '19A0')
)) {
    $dbx.Open([IO.Path]::GetFullPath($pair[1]))
    $obj = $dbx.HandleToObject($pair[2])
    $item = [ordered]@{Source=$pair[0];Handle=$pair[2];ObjectName=$obj.ObjectName}
    foreach ($name in @('Width','Height','StartPoint','EndPoint','WallWidth','Scale','Text','Measurement','Length')) {
        try { $item[$name] = $obj.$name } catch { }
    }
    $a = $null; $b = $null
    try { $obj.GetBoundingBox([ref]$a,[ref]$b); $item.BoundsMin=$a; $item.BoundsMax=$b } catch { }
    $known += [PSCustomObject]$item
}
$result = [PSCustomObject]@{Version=$cadApp.Version;Documents=$rows;KnownSamples=$known;Quiescent=$cadApp.GetAcadState().IsQuiescent;ReadOnlyQuery=$true}
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $PSScriptRoot '单位核对原始记录.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 10
