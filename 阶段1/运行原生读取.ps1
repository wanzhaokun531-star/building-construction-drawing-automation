$ErrorActionPreference = 'Stop'
$stageRoot = $PSScriptRoot
$runRoot = Join-Path $stageRoot ('runs\probe-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$scheme = [IO.Path]::GetFullPath((Join-Path $stageRoot '..\阶段0\输入快照\方案平面图.dwg'))
$reference = [IO.Path]::GetFullPath((Join-Path $stageRoot '..\阶段0\输入快照\施工图平面参考图.dwg'))
$arx = Join-Path $stageRoot 'bin\Stage1Probe.arx'
$ini = "[probe]`r`nscheme=$scheme`r`nreference=$reference`r`noutput=$runRoot`r`n"
[IO.File]::WriteAllText((Join-Path $stageRoot 'bin\probe.ini'), $ini, [Text.Encoding]::Unicode)
$cad = [Runtime.InteropServices.Marshal]::GetActiveObject('AutoCAD.Application.23.1')
function Wait-Idle {
    $deadline = (Get-Date).AddSeconds(30)
    do {
        try { if ($cad.GetAcadState().IsQuiescent -and $cad.ActiveDocument.GetVariable('CMDNAMES') -eq '') { return } } catch { }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)
    throw 'CAD did not become idle within 30 seconds; user command was not cancelled.'
}
Wait-Idle
$originalDocument = $cad.ActiveDocument
$originalPath = $originalDocument.FullName
$control = $cad.Documents.Add()
Wait-Idle
$control.SaveAs((Join-Path $runRoot 'stage1-control.dwg'))
Wait-Idle
$secureLoad = $control.GetVariable('SECURELOAD')
$receipt = [ordered]@{
    Run=$runRoot; Host=$cad.FullName; HostVersion=$cad.Version
    OriginalActiveDocument=$originalPath; ControlDocument=$control.FullName
    Arx=$arx; ArxSHA256=(Get-FileHash -LiteralPath $arx -Algorithm SHA256).Hash
    SECURELOAD=$secureLoad; SecuritySettingsChanged=$false
    UnitDecision='User confirmed both drawings: 1 drawing unit = 1 millimeter, 2026-10-28'
    Command='S1READ'; Load='not executed'; Read='not executed'; Unload='not executed'
}
try {
    $cad.LoadARX($arx)
    $receipt.Load='returned successfully'
    $control.SendCommand("S1READ`n")
    $deadline = (Get-Date).AddSeconds(40)
    while (!(Test-Path -LiteralPath (Join-Path $runRoot 'native-read.json')) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Wait-Idle
    $receipt.Read = if (Test-Path -LiteralPath (Join-Path $runRoot 'native-read.json')) {'result file created; assertions pending'} else {'result missing'}
    $cad.UnloadARX($arx)
    $receipt.Unload='returned successfully'
} catch {
    $receipt.Error=$_.Exception.Message
} finally {
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runRoot 'host-receipt.json') -Encoding UTF8
    $runRoot | Set-Content -LiteralPath (Join-Path $stageRoot '最新运行目录.txt') -Encoding UTF8
}
$receipt | ConvertTo-Json -Depth 8
