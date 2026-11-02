<#
.SYNOPSIS
Read-only inspection of AutoCAD 2020 native project declarations and existing evidence.
.DESCRIPTION
Outputs one JSON object to stdout. Does not evaluate MSBuild imports, execute build
targets, load DLLs, launch CAD, change settings, install software, or write files.
Use explicit paths discovered in the project. Relative paths are based on Workspace.
Missing inputs and unknown conditions remain unknown. Exit 0 means inspection returned,
not that the project builds or that its tests passed. Invalid Workspace terminates.
.EXAMPLE
& ./Inspect-Arx2020.ps1 -Workspace C:/work/project -ProjectFile EnvCheck.vcxproj
.PARAMETER BinaryPath
Current native plugin used for PE architecture and comparison with evidence hashes.
.PARAMETER EvidencePaths
Explicit JSON paths. Records are preserved; native hashes are compared separately.
.PARAMETER Configuration
Used only for simple literal MSBuild conditions; defaults to Release.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Workspace,
    [string]$ProjectFile,
    [string]$SdkRoot,
    [string]$BuildToolsRoot,
    [string]$AutoCadRoot,
    [string]$BinaryPath,
    [string[]]$EvidencePaths = @(),
    [string]$Configuration = 'Release',
    [string]$Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$workspaceItem = Get-Item -LiteralPath $Workspace
if (-not $workspaceItem.PSIsContainer -or $workspaceItem.PSProvider.Name -ne 'FileSystem') {
    throw 'Workspace must be an existing filesystem directory.'
}
$workspaceFull = $workspaceItem.FullName
$findings = New-Object 'System.Collections.Generic.List[object]'

function Add-Finding([string]$Code, [string]$Detail) {
    $findings.Add([pscustomobject]@{Code=$Code; Detail=$Detail})
}
function Get-InputPath([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return $null }
    if ([IO.Path]::IsPathRooted($Value)) { return [IO.Path]::GetFullPath($Value) }
    return [IO.Path]::GetFullPath((Join-Path $workspaceFull $Value))
}
function Get-FileFact([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject]@{Path=$Path; Exists=$false; ProductVersion=$null}
    }
    $item = Get-Item -LiteralPath $Path
    return [pscustomobject]@{Path=$item.FullName; Exists=$true; ProductVersion=$item.VersionInfo.ProductVersion}
}
function Test-LiteralCondition([string]$Condition) {
    if ([string]::IsNullOrWhiteSpace($Condition)) { return $true }
    $expanded = $Condition.Replace('$(Configuration)', $Configuration).Replace('$(Platform)', $Platform)
    if ($expanded.Contains('$(') -or $expanded.Contains('@(') -or $expanded.Contains('%(')) { return $null }
    if ($expanded -match "^\s*'([^']*)'\s*(==|!=)\s*'([^']*)'\s*$") {
        if ($Matches[2] -eq '==') { return $Matches[1] -eq $Matches[3] }
        return $Matches[1] -ne $Matches[3]
    }
    return $null
}
function Read-Project([string]$Path) {
    $result = [ordered]@{Path=$Path; Status='Unspecified'; Evaluated=$false; Declarations=@(); Imports=@()}
    if (-not $Path) { return [pscustomobject]$result }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        $result.Status='Missing'; Add-Finding 'ProjectMissing' $Path; return [pscustomobject]$result
    }
    $reader = $null
    try {
        $settings = New-Object Xml.XmlReaderSettings
        $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
        $settings.XmlResolver = $null
        $reader = [Xml.XmlReader]::Create($Path, $settings)
        $xml = New-Object Xml.XmlDocument
        $xml.XmlResolver = $null
        $xml.Load($reader)
        $result.Status='Read'
        $result.Imports = @($xml.SelectNodes("//*[local-name()='Import']") | ForEach-Object { $_.GetAttribute('Project') })
        $names = @('PlatformToolset','LanguageStandard','RuntimeLibrary','CharacterSet','TargetExt',
                   'WindowsTargetPlatformVersion','AdditionalIncludeDirectories','AdditionalLibraryDirectories',
                   'AdditionalDependencies','ModuleDefinitionFile','ObjectArxSdk','CLRSupport','ConfigurationType')
        $query = '//*[' + (($names | ForEach-Object { "local-name()='$_'" }) -join ' or ') + ']'
        $declarations = New-Object 'System.Collections.Generic.List[object]'
        foreach ($node in $xml.SelectNodes($query)) {
            $conditions = New-Object 'System.Collections.Generic.List[string]'
            $applies = $true
            $parent = $node
            while ($parent -is [Xml.XmlElement]) {
                if ($parent.HasAttribute('Condition')) {
                    $condition = $parent.GetAttribute('Condition')
                    $conditions.Add($condition)
                    $conditionResult = Test-LiteralCondition $condition
                    if ($conditionResult -eq $false) { $applies=$false }
                    elseif ($null -eq $conditionResult -and $applies -ne $false) { $applies=$null }
                }
                if ($parent.LocalName -in @('Choose','When','Otherwise')) { $applies=$null }
                $parent=$parent.ParentNode
            }
            $value=$node.InnerText.Trim()
            $declarations.Add([pscustomobject]@{Name=$node.LocalName;Value=$value;Conditions=@($conditions.ToArray());Applies=$applies})
            if ($applies -ne $true) { continue }
            if ($node.LocalName -eq 'PlatformToolset' -and $value -notmatch '\$\(' -and $value -ne 'v141') {
                Add-Finding 'ToolsetMismatch' ('Active literal declaration: ' + $value + '; expected the VS2017/v141 family.')
            }
            if ($node.LocalName -eq 'LanguageStandard' -and $value -match 'stdcpp(20|23|latest)') {
                Add-Finding 'LanguageBeyondV141' ('Active language declaration: ' + $value)
            }
            if ($node.LocalName -eq 'RuntimeLibrary' -and $value -match 'Debug') {
                Add-Finding 'DebugCRT' ('Active CRT declaration: ' + $value)
            } elseif ($node.LocalName -eq 'RuntimeLibrary' -and $value -eq 'MultiThreaded') {
                Add-Finding 'StaticCRT' 'Active /MT declaration requires correction for the intended host ABI.'
            }
            if ($node.LocalName -eq 'CLRSupport' -and $value -notin @('false','None','')) {
                Add-Finding 'ManagedConfiguration' ('CLRSupport=' + $value + '; this is not a pure native configuration.')
            }
        }
        $result.Declarations=@($declarations.ToArray())
        if ($result.Imports.Count -gt 0 -or @($result.Declarations | Where-Object { $null -eq $_.Applies -or $_.Value -match '\$\(' }).Count -gt 0) {
            Add-Finding 'UnresolvedProject' 'Imports, macros, and non-literal conditions were not evaluated. These are declarations, not effective MSBuild settings.'
        }
    } catch {
        $result.Status='Unreadable'; $result.Error=$_.Exception.Message
        Add-Finding 'ProjectUnreadable' $Path
    } finally { if ($null -ne $reader) { $reader.Dispose() } }
    return [pscustomobject]$result
}
function Read-Binary([string]$Path) {
    $result=[ordered]@{Path=$Path;Status='Unspecified';SHA256=$null;Machine=$null;ProductVersion=$null}
    if (-not $Path) { return [pscustomobject]$result }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { $result.Status='Missing'; return [pscustomobject]$result }
    $stream=$null; $reader=$null
    try {
        $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        $reader=New-Object IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Not a DOS/PE image.' }
        $stream.Position=0x3C; $peOffset=$reader.ReadUInt32()
        if ($peOffset -gt $stream.Length - 24) { throw 'Invalid PE header offset.' }
        $stream.Position=$peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw 'Invalid PE signature.' }
        $machine=$reader.ReadUInt16()
        $result.Machine=switch ($machine) { 0x8664 {'x64'} 0x14C {'x86'} 0xAA64 {'arm64'} default { '0x{0:X4}' -f $machine } }
        $stream.Position=0
        $hasher=[Security.Cryptography.SHA256]::Create()
        try { $result.SHA256=[BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','') }
        finally { $hasher.Dispose() }
        $reader.Dispose(); $reader=$null; $stream=$null
        $result.ProductVersion=(Get-Item -LiteralPath $Path).VersionInfo.ProductVersion
        $result.Status='Read'
    } catch { $result.Status='Unreadable'; $result.Error=$_.Exception.Message }
    finally {
        if ($null -ne $reader) { $reader.Dispose() }
        elseif ($null -ne $stream) { $stream.Dispose() }
    }
    return [pscustomobject]$result
}
function Read-Sdk([string]$Path) {
    $result=[ordered]@{Path=$Path;Status='Unspecified';Release=$null;Files=@()}
    if (-not $Path) { return [pscustomobject]$result }
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { $result.Status='Missing'; return [pscustomobject]$result }
    $required=@('inc/id.h','inc/dbmain.h','inc/dbtrans.h','inc/dbobjptr.h','lib-x64/acdb23.lib','lib-x64/rxapi.lib')
    $result.Files=@($required | ForEach-Object { Get-FileFact (Join-Path $Path $_) })
    $result.Status='Read'
    if (@($result.Files | Where-Object { -not $_.Exists }).Count -gt 0) { Add-Finding 'SdkFilesMissing' $Path }
    $header=Join-Path $Path 'inc/id.h'
    if (Test-Path -LiteralPath $header -PathType Leaf) {
        $content=Get-Content -LiteralPath $header -Raw
        $major=[regex]::Match($content,'(?m)^\s*#define\s+ACADV_RELMAJOR\s+(\d+)')
        $minor=[regex]::Match($content,'(?m)^\s*#define\s+ACADV_RELMINOR\s+(\d+)')
        if ($major.Success -and $minor.Success) {
            $result.Release=$major.Groups[1].Value+'.'+$minor.Groups[1].Value
            if ($result.Release -ne '23.1') { Add-Finding 'SdkReleaseMismatch' ('SDK id.h declares '+$result.Release+'; this skill targets 23.1 (2020).') }
        }
    }
    if (-not $result.Release) { Add-Finding 'SdkReleaseUnknown' 'Could not read a release from inc/id.h; folder names are not version evidence.' }
    return [pscustomobject]$result
}
function Collect-EvidenceHashes($Node, [string]$Location, $Output) {
    if ($null -eq $Node -or $Node -is [string] -or $Node -is [ValueType]) { return }
    if ($Node -is [Array]) {
        for ($i=0;$i -lt $Node.Count;$i++) { Collect-EvidenceHashes $Node[$i] ($Location+'['+$i+']') $Output }
        return
    }
    foreach ($property in $Node.PSObject.Properties) {
        $childLocation=$Location+'.'+$property.Name
        $isNativeHash=$property.Name -in @('NativeSHA256','ArxSHA256','PluginSHA256','PluginHash')
        if ($property.Name -eq 'CopiedPluginSHA256' -and $Location -match '\.Native(?:\.|\[|$)') { $isNativeHash=$true }
        if ($Location -match '\.Oracle(?:\.|\[|$)') { $isNativeHash=$false }
        if ($isNativeHash -and $property.Value -is [string]) {
            $match=$null
            if ($property.Value -match '^[0-9a-fA-F]{64}$' -and $binary.SHA256) { $match=$property.Value -ieq $binary.SHA256 }
            $Output.Add([pscustomobject]@{Field=$childLocation;Recorded=$property.Value;Match=$match})
        } else { Collect-EvidenceHashes $property.Value $childLocation $Output }
    }
}

$project=Read-Project (Get-InputPath $ProjectFile)
$sdk=Read-Sdk (Get-InputPath $SdkRoot)
$binary=Read-Binary (Get-InputPath $BinaryPath)
if ($Platform -ne 'x64') { Add-Finding 'TargetNotX64' ('Requested platform: '+$Platform) }
if ($binary.Machine -and $binary.Machine -ne 'x64') { Add-Finding 'BinaryNotX64' $binary.Machine }
$buildRoot=Get-InputPath $BuildToolsRoot
$toolchain=[ordered]@{Path=$buildRoot;Status='Unspecified';MSBuild=$null;Compilers=@()}
if ($buildRoot) {
    $toolchain.Status=if (Test-Path -LiteralPath $buildRoot -PathType Container) {'Read'} else {'Missing'}
    $toolchain.MSBuild=Get-FileFact (Join-Path $buildRoot 'MSBuild/15.0/Bin/MSBuild.exe')
    $toolFolder=Join-Path $buildRoot 'VC/Tools/MSVC'
    if (Test-Path -LiteralPath $toolFolder -PathType Container) {
        $toolchain.Compilers=@(Get-ChildItem -LiteralPath $toolFolder -Directory | ForEach-Object {
            $fact=Get-FileFact (Join-Path $_.FullName 'bin/Hostx64/x64/cl.exe')
            [pscustomobject]@{ToolDirectory=$_.Name;File=$fact}
        })
    }
}
$acadRoot=Get-InputPath $AutoCadRoot
$hostInfo=[ordered]@{Path=$acadRoot;Status='Unspecified';Files=@()}
if ($acadRoot) {
    $hostInfo.Status=if (Test-Path -LiteralPath $acadRoot -PathType Container) {'Read'} else {'Missing'}
    $hostInfo.Files=@('acad.exe','accoreconsole.exe' | ForEach-Object { Get-FileFact (Join-Path $acadRoot $_) })
    foreach ($hostFile in $hostInfo.Files) {
        if ($hostFile.ProductVersion -and $hostFile.ProductVersion -notmatch '^R?23\.1(?:\.|\s|$)') { Add-Finding 'HostVersionMismatch' ($hostFile.Path+': '+$hostFile.ProductVersion) }
    }
}
$evidence=New-Object 'System.Collections.Generic.List[object]'
foreach ($evidencePath in $EvidencePaths) {
    $path=Get-InputPath $evidencePath
    $item=[ordered]@{Path=$path;Status='Missing';BinaryHashes=@();Record=$null}
    if ($path -and (Test-Path -LiteralPath $path -PathType Leaf)) {
        try {
            $item.Record=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
            $hashes=New-Object 'System.Collections.Generic.List[object]'
            Collect-EvidenceHashes $item.Record '$' $hashes
            $item.BinaryHashes=@($hashes.ToArray())
            $item.Status='Read'
            if (@($item.BinaryHashes | Where-Object { $_.Match -eq $false }).Count -gt 0) { Add-Finding 'EvidenceHashMismatch' $path }
            if ($item.BinaryHashes.Count -eq 0) { Add-Finding 'EvidenceHashUnknown' $path }
        } catch { $item.Status='Unreadable'; $item.Error=$_.Exception.Message }
    }
    $evidence.Add([pscustomobject]$item)
}
[pscustomobject][ordered]@{
    SchemaVersion=1; InspectedAt=(Get-Date).ToString('o'); Workspace=$workspaceFull
    Assessment='InspectionOnly'; CadExecuted=$false
    SelectedConfiguration=$Configuration; SelectedPlatform=$Platform
    Project=$project; Sdk=$sdk; Toolchain=[pscustomobject]$toolchain
    Host=[pscustomobject]$hostInfo; Binary=$binary; Evidence=@($evidence.ToArray())
    Findings=@($findings.ToArray())
    Limits=@('No build or CAD execution.','Project declarations only: imports and complex MSBuild conditions are not evaluated.',
             'Hash equality identifies bytes, not source provenance or present runtime success.',
             'SDK marker and required-file presence do not authenticate the complete SDK.',
             'PE machine is inspected; imports, CLR header and API/ABI compatibility are not verified.')
} | ConvertTo-Json -Depth 50
