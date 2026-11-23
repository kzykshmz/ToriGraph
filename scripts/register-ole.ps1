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
    if ($Candidate -and (Test-Path -LiteralPath $Candidate)) {
        return (Resolve-Path -LiteralPath $Candidate).Path
    }
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $defaults = @(
        (Join-Path $repoRoot "ToriGraph.exe"),
        (Join-Path $PSScriptRoot "ToriGraph.exe"),
        (Join-Path $repoRoot "build\scidavis\ToriGraph.exe"),
        (Join-Path $repoRoot "build\scidavis\Release\ToriGraph.exe"),
        (Join-Path $repoRoot "build\scidavis\scidavis.exe"),
        (Join-Path $repoRoot "build\scidavis\Release\scidavis.exe"),
        (Join-Path $repoRoot "dist\ToriGraph-win64\ToriGraph.exe"),
        (Join-Path $repoRoot "dist\ToriGraph.exe"),
        (Join-Path $repoRoot "dist\scidavis.exe")
    )
    foreach ($item in $defaults) {
        if (Test-Path -LiteralPath $item) {
            return (Resolve-Path -LiteralPath $item).Path
        }
    }
    if ($Mode -eq "Install") {
        throw "Could not locate ToriGraph.exe. Pass -ExePath explicitly or build the target first."
    }
    return $defaults[0]
}

function Set-DefaultValue {
    param([string]$RegistryPath, [string]$Value)
    if (-not (Test-Path -LiteralPath $RegistryPath)) {
        New-Item -Path $RegistryPath -Force | Out-Null
    }
    New-ItemProperty -LiteralPath $RegistryPath -Name "(default)" -Value $Value -PropertyType String -Force | Out-Null
}

function Set-NamedValue {
    param([string]$RegistryPath, [string]$Name, [string]$Value)
    if (-not (Test-Path -LiteralPath $RegistryPath)) {
        New-Item -Path $RegistryPath -Force | Out-Null
    }
    New-ItemProperty -LiteralPath $RegistryPath -Name $Name -Value $Value -PropertyType String -Force | Out-Null
}

# Path of the all-users (HKLM) OLE server, or $null when there is none.
function Get-HklmServerPath {
    $key = "HKLM:\Software\Classes\CLSID\$Clsid\LocalServer32"
    if (-not (Test-Path -LiteralPath $key)) { return $null }
    $item = Get-Item -LiteralPath $key
    $v = $item.GetValue("ServerExecutable")
    if (-not $v) {
        $cmd = [string]$item.GetValue("")
        if ($cmd -match '^\s*"([^"]+)"') { $v = $Matches[1] }
        elseif ($cmd -match '^(.+?\.exe)') { $v = $Matches[1] }
        else { $v = $cmd.Trim() }
    }
    if ($v) { return [string]$v }
    return $null
}

function Get-KeySummary {
    param([string]$RegistryPath)
    if (-not (Test-Path -LiteralPath $RegistryPath)) {
        return [pscustomobject]@{
            Path = $RegistryPath
            Exists = $false
            Default = $null
        }
    }
    $item = Get-Item -LiteralPath $RegistryPath
    return [pscustomobject]@{
        Path = $RegistryPath
        Exists = $true
        Default = $item.GetValue("")
    }
}

$resolvedExe = Resolve-DefaultExePath -Candidate $ExePath
$clsidBase = "HKCU:\Software\Classes\CLSID\$Clsid"
$progIdBase = "HKCU:\Software\Classes\$ProgId"
# The Inno Setup installer registers both the current and the legacy ProgID; keep them in step.
$progIds = @($ProgId, "SciDAVis.Graph.1") | Select-Object -Unique
$localServerCommand = "`"$resolvedExe`" -Embedding"

switch ($Mode) {
    "Inspect" {
        $summary = @(
            (Get-KeySummary -RegistryPath $clsidBase),
            (Get-KeySummary -RegistryPath "$clsidBase\LocalServer32"),
            (Get-KeySummary -RegistryPath "$clsidBase\ProgID"),
            (Get-KeySummary -RegistryPath "$clsidBase\Insertable"),
            (Get-KeySummary -RegistryPath $progIdBase),
            (Get-KeySummary -RegistryPath "$progIdBase\CLSID"),
            (Get-KeySummary -RegistryPath "HKCU:\Software\Classes\SciDAVis.Graph.1"),
            (Get-KeySummary -RegistryPath "HKLM:\Software\Classes\CLSID\$Clsid"),
            (Get-KeySummary -RegistryPath "HKLM:\Software\Classes\CLSID\$Clsid\LocalServer32")
        )
        $summary | Format-Table -AutoSize
        $hklmSrv = Get-HklmServerPath
        $hklmExists = [bool]($hklmSrv -and (Test-Path -LiteralPath $hklmSrv))
        $resolvedExists = [bool]($resolvedExe -and (Test-Path -LiteralPath $resolvedExe))
        Write-Host ""
        Write-Host "Planned server command: $localServerCommand (exe exists: $resolvedExists)"
        if ($hklmSrv) {
            Write-Host "All-users (HKLM) server: $hklmSrv (exe exists: $hklmExists)"
        }
    }
    "Install" {
        Set-DefaultValue -RegistryPath $clsidBase -Value $FriendlyName
        Set-DefaultValue -RegistryPath "$clsidBase\LocalServer32" -Value $localServerCommand
        Set-NamedValue -RegistryPath "$clsidBase\LocalServer32" -Name "ServerExecutable" -Value $resolvedExe
        Set-DefaultValue -RegistryPath "$clsidBase\ProgID" -Value $ProgId
        Set-DefaultValue -RegistryPath "$clsidBase\VersionIndependentProgID" -Value "ToriGraph.Graph"
        Set-DefaultValue -RegistryPath "$clsidBase\InprocHandler32" -Value "ole32.dll"
        Set-DefaultValue -RegistryPath "$clsidBase\DefaultIcon" -Value "`"$resolvedExe`",0"
        Set-DefaultValue -RegistryPath "$clsidBase\Verb\0" -Value "編集(&E),0,2"
        Set-DefaultValue -RegistryPath "$clsidBase\Verb\1" -Value "開く(&O),0,2"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\0" -Value "Embed Source,1,8,1"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\1" -Value "Embedded Object,1,8,1"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\2" -Value "Object Descriptor,1,1,1"
        Set-DefaultValue -RegistryPath "$clsidBase\DataFormats\GetSet\3" -Value "14,1,64,1"
        Set-DefaultValue -RegistryPath "$clsidBase\MiscStatus" -Value "0"
        Set-DefaultValue -RegistryPath "$clsidBase\MiscStatus\1" -Value "1"
        Set-DefaultValue -RegistryPath "$clsidBase\AuxUserType\2" -Value "ToriGraph"
        Set-DefaultValue -RegistryPath "$clsidBase\AuxUserType\3" -Value "ToriGraph"
        Set-DefaultValue -RegistryPath "$clsidBase\Insertable" -Value ""

        foreach ($pid_ in $progIds) {
            $base = "HKCU:\Software\Classes\$pid_"
            Set-DefaultValue -RegistryPath $base -Value $FriendlyName
            Set-DefaultValue -RegistryPath "$base\CLSID" -Value $Clsid
            Set-DefaultValue -RegistryPath "$base\Insertable" -Value ""
            Set-DefaultValue -RegistryPath "$base\shell\open\command" -Value "`"$resolvedExe`" `"%1`""
        }

        # A machine-wide (all users) registration is still visible; this per-user one overrides it
        # for the current user only. Say so when the two disagree.
        $hklmServer = Get-HklmServerPath
        if ($hklmServer -and ($hklmServer -ine $resolvedExe)) {
            Write-Warning "An all-users OLE registration exists and points to '$hklmServer'. This per-user registration (-> '$resolvedExe') takes precedence for the current user only."
        }

        Write-Host "Registered HKCU OLE keys for $($progIds -join ', ') -> $resolvedExe"
    }
    "Uninstall" {
        $paths = @($clsidBase)
        foreach ($pid_ in $progIds) { $paths += "HKCU:\Software\Classes\$pid_" }
        foreach ($path in $paths) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
        Write-Host "Removed HKCU OLE keys for $($progIds -join ', ') and $Clsid"
        if (Get-HklmServerPath) {
            Write-Warning "An all-users (HKLM) OLE registration remains; remove it by uninstalling ToriGraph from Windows Settings (needs administrator rights)."
        }
    }
}