param(
    [ValidateSet("Inspect", "Install", "Uninstall")]
    [string]$Mode = "Inspect",
    [string]$ExePath = "",
    [string]$Clsid = "{8E6B9541-3C74-4D28-A915-7D84E5A12001}",
    [string]$ProgId = "ToriGraph.Graph.1",
    [string]$FriendlyName = "ToriGraph Graph"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-DefaultExePath {
    param([string]$Candidate)
    if ($Candidate -and (Test-Path $Candidate)) {
        return (Resolve-Path $Candidate).Path
    }
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $defaults = @(
        (Join-Path $repoRoot "ToriGraph.exe"),
        (Join-Path $PSScriptRoot "ToriGraph.exe"),
        (Join-Path $repoRoot "build\scidavis\ToriGraph.exe"),
        (Join-Path $repoRoot "build\scidavis\scidavis.exe"),
        (Join-Path $repoRoot "dist\ToriGraph.exe"),
        (Join-Path $repoRoot "dist\scidavis.exe")
    )
    foreach ($item in $defaults) {
        if (Test-Path $item) {
            return (Resolve-Path $item).Path
        }
    }
    if ($Mode -eq "Install") {
        throw "Could not locate ToriGraph.exe. Pass -ExePath explicitly or build the target first."
    }
    return $defaults[0]
}

function Set-DefaultValue {
    param([string]$RegistryPath, [string]$Value)
    if (-not (Test-Path $RegistryPath)) {
        New-Item -Path $RegistryPath -Force | Out-Null
    }
    New-ItemProperty -Path $RegistryPath -Name "(default)" -Value $Value -PropertyType String -Force | Out-Null
}

function Set-NamedValue {
    param([string]$RegistryPath, [string]$Name, [string]$Value)
    if (-not (Test-Path $RegistryPath)) {
        New-Item -Path $RegistryPath -Force | Out-Null
    }
    New-ItemProperty -Path $RegistryPath -Name $Name -Value $Value -PropertyType String -Force | Out-Null
}

function Get-KeySummary {
    param([string]$RegistryPath)
    if (-not (Test-Path $RegistryPath)) {
        return [pscustomobject]@{
            Path = $RegistryPath
            Exists = $false
            Default = $null
        }
    }
    $item = Get-Item -Path $RegistryPath
    return [pscustomobject]@{
        Path = $RegistryPath
        Exists = $true
        Default = $item.GetValue("")
    }
}

$resolvedExe = Resolve-DefaultExePath -Candidate $ExePath
$clsidBase = "HKCU:\Software\Classes\CLSID\$Clsid"
$progIdBase = "HKCU:\Software\Classes\$ProgId"
$localServerCommand = "`"$resolvedExe`" -Embedding"

switch ($Mode) {
    "Inspect" {
        $summary = @(
            (Get-KeySummary -RegistryPath $clsidBase),
            (Get-KeySummary -RegistryPath "$clsidBase\LocalServer32"),
            (Get-KeySummary -RegistryPath "$clsidBase\ProgID"),
            (Get-KeySummary -RegistryPath "$clsidBase\Insertable"),
            (Get-KeySummary -RegistryPath $progIdBase),
            (Get-KeySummary -RegistryPath "$progIdBase\CLSID")
        )
        $summary | Format-Table -AutoSize
        Write-Host ""
        Write-Host "Planned server command: $localServerCommand"
    }
    "Install" {
        Set-DefaultValue -RegistryPath $clsidBase -Value $FriendlyName
        Set-DefaultValue -RegistryPath "$clsidBase\LocalServer32" -Value $localServerCommand
        Set-NamedValue -RegistryPath "$clsidBase\LocalServer32" -Name "ServerExecutable" -Value $resolvedExe
        Set-DefaultValue -RegistryPath "$clsidBase\ProgID" -Value $ProgId
        Set-DefaultValue -RegistryPath "$clsidBase\VersionIndependentProgID" -Value "ToriGraph.Graph"
        Set-DefaultValue -RegistryPath "$clsidBase\InprocHandler32" -Value "ole32.dll"
        Set-DefaultValue -RegistryPath "$clsidBase\DefaultIcon" -Value "$resolvedExe,0"
        Set-DefaultValue -RegistryPath "$clsidBase\Verb\0" -Value "編集(&E),0,2"
        Set-DefaultValue -RegistryPath "$clsidBase\Verb\1" -Value "開く(&O),0,2"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\0" -Value "Embed Source,1,8,1"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\1" -Value "Embedded Object,1,8,1"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\2" -Value "Object Descriptor,1,1,1"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\3" -Value "14,1,64,1"
        Set-DefaultValue -RegistryPath "$clsidBase\MiscStatus" -Value "0"
        Set-DefaultValue -RegistryPath "$clsidBase\MiscStatus\1" -Value "1"
        Set-DefaultValue -RegistryPath "$clsidBase\AuxUserType\2" -Value $FriendlyName
        Set-DefaultValue -RegistryPath "$clsidBase\Insertable" -Value ""

        Set-DefaultValue -RegistryPath $progIdBase -Value $FriendlyName
        Set-DefaultValue -RegistryPath "$progIdBase\CLSID" -Value $Clsid
        Set-DefaultValue -RegistryPath "$progIdBase\Insertable" -Value ""
        Set-DefaultValue -RegistryPath "$progIdBase\shell\open\command" -Value "`"$resolvedExe`" `"%1`""

        Write-Host "Registered HKCU OLE keys for $ProgId -> $resolvedExe"
    }
    "Uninstall" {
        foreach ($path in @($clsidBase, $progIdBase)) {
            if (Test-Path $path) {
                Remove-Item -Path $path -Recurse -Force
            }
        }
        Write-Host "Removed HKCU OLE keys for $ProgId and $Clsid"
    }
}