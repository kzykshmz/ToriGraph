param(
    [string]$SdkDir = "",
    [string]$PythonExe = "",
    [switch]$Uninstall,
    [switch]$Silent
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Info {
    param([string]$Message)
    if (-not $Silent) {
        Write-Host "[ToriGraph Python SDK] $Message"
    }
}

function Resolve-PythonCommand {
    param([string]$Explicit)
    if ($Explicit -and (Test-Path $Explicit)) {
        return @{ Exe = (Resolve-Path $Explicit).Path; PrefixArgs = @() }
    }

    $candidates = @(
        @{ Name = "py"; PrefixArgs = @("-3") },
        @{ Name = "python"; PrefixArgs = @() },
        @{ Name = "python3"; PrefixArgs = @() }
    )
    foreach ($cand in $candidates) {
        $cmd = Get-Command $cand.Name -ErrorAction SilentlyContinue
        if (-not $cmd) { continue }
        # Skip Microsoft Store stub
        if ($cmd.Source -like "*WindowsApps*") { continue }
        try {
            $verArgs = @($cand.PrefixArgs + @("-c", "import sys; print(sys.version_info[0], sys.version_info[1])"))
            $out = & $cmd.Source @verArgs 2>$null
            if ($LASTEXITCODE -eq 0 -and $out) {
                $parts = ($out.ToString().Trim() -split "\s+")
                if ($parts.Count -ge 2 -and [int]$parts[0] -eq 3 -and [int]$parts[1] -ge 8) {
                    return @{ Exe = $cmd.Source; PrefixArgs = $cand.PrefixArgs }
                }
            }
        } catch {
            continue
        }
    }
    return $null
}

$py = Resolve-PythonCommand -Explicit $PythonExe
if (-not $py) {
    Write-Info "Python 3.8+ was not found on PATH. Skipping automatic torigraph SDK installation."
    Write-Info "You can install it later with: pip install `"$SdkDir`""
    exit 0
}

Write-Info "Using Python: $($py.Exe) $($py.PrefixArgs -join ' ')"

if ($Uninstall) {
    # pip writes "WARNING: Skipping torigraph as it is not installed." to stderr. With
    # $ErrorActionPreference = "Stop", Windows PowerShell 5.1 turns redirected native stderr into a
    # terminating error, which would abort the uninstaller. Run pip with "Continue" and never throw.
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $uArgs = @($py.PrefixArgs + @("-m", "pip", "uninstall", "-y", "torigraph"))
        $null = & $py.Exe @uArgs 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Info "Uninstalled torigraph Python SDK (if previously installed)."
        } else {
            Write-Info "pip uninstall exited with code $LASTEXITCODE (the SDK may not have been installed)."
        }
    } catch {
        Write-Info "Could not uninstall the torigraph Python SDK: $($_.Exception.Message)"
    } finally {
        $ErrorActionPreference = $prevEap
    }
    exit 0
}

if (-not $SdkDir) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $installRoot = Split-Path -Parent $scriptDir
    $SdkDir = Join-Path $installRoot "python"
}

if (-not (Test-Path $SdkDir)) {
    Write-Info "SDK directory not found: $SdkDir"
    exit 0
}

# Prefer bundled offline wheel if present
$wheelDir = Join-Path $SdkDir "wheels"
$wheelFile = $null
if (Test-Path $wheelDir) {
    $wheelFile = Get-ChildItem -Path $wheelDir -Filter "torigraph-*.whl" | Sort-Object Name -Descending | Select-Object -First 1
}

if ($wheelFile) {
    Write-Info "Installing bundled wheel: $($wheelFile.Name)"
    $pipArgs = @($py.PrefixArgs + @("-m", "pip", "install", "--upgrade", "--no-deps", "--force-reinstall", $wheelFile.FullName))
    & $py.Exe @pipArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Info "Retrying with --user..."
        $pipArgsUser = @($py.PrefixArgs + @("-m", "pip", "install", "--user", "--upgrade", "--no-deps", "--force-reinstall", $wheelFile.FullName))
        & $py.Exe @pipArgsUser
    }
} else {
    Write-Info "Installing from source directory: $SdkDir"
    $pipArgs = @($py.PrefixArgs + @("-m", "pip", "install", "--upgrade", "--no-build-isolation", "--no-deps", $SdkDir))
    & $py.Exe @pipArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Info "Retrying with --user..."
        $pipArgsUser = @($py.PrefixArgs + @("-m", "pip", "install", "--user", "--upgrade", "--no-build-isolation", "--no-deps", $SdkDir))
        & $py.Exe @pipArgsUser
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Info "torigraph Python SDK installed successfully."
} else {
    Write-Info "Warning: pip install exited with code $LASTEXITCODE. You can install manually via pip install `"$SdkDir`"."
}
exit 0
