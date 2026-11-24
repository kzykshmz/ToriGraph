param(
    [string]$Version = "",
    [string]$BuildDir = "",
    [string]$DepsDir = "",
    [string]$OutDir = "",
    [string]$IsccPath = "",
    [switch]$SkipPortableZip,
    [switch]$SkipInstaller
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot "build" }
if (-not $DepsDir) { $DepsDir = Join-Path $repoRoot ".deps" }
if (-not $OutDir) { $OutDir = Join-Path $repoRoot "dist" }

# The version comes from the top-level CMakeLists.txt project() VERSION, the
# single source of truth (also used for version.cpp and checked against the
# Python SDK by CMake / scripts/check_version.py).
$cmakeText = Get-Content -Raw -Path (Join-Path $repoRoot "CMakeLists.txt")
$cmakeMatch = [regex]::Match($cmakeText, 'project\s*\(\s*scidavis\b[^)]*?\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)', 'Singleline')
if (-not $cmakeMatch.Success) {
    throw "Could not read project(... VERSION x.y.z) from CMakeLists.txt"
}
$cmakeVersion = $cmakeMatch.Groups[1].Value
if (-not $Version) {
    $Version = $cmakeVersion
} elseif ($Version -ne $cmakeVersion) {
    throw "Requested installer version '$Version' differs from CMakeLists.txt VERSION '$cmakeVersion'. Bump the CMake VERSION instead of overriding it."
}

$exeSrc = Join-Path $BuildDir "scidavis\ToriGraph.exe"
if (-not (Test-Path $exeSrc)) {
    $exeSrc = Join-Path $BuildDir "scidavis\Release\ToriGraph.exe"
}
if (-not (Test-Path $exeSrc)) {
    throw "ToriGraph.exe not found under '$BuildDir\scidavis'. Build ToriGraph first."
}
$binSrcDir = Split-Path -Parent $exeSrc
$depsBinDir = Join-Path $DepsDir "Library\bin"
$depsPluginsDir = Join-Path $DepsDir "Library\plugins"

$stageDir = Join-Path $OutDir "ToriGraph-win64"
if (Test-Path $stageDir) {
    Remove-Item -Recurse -Force $stageDir
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Host "==> Staging ToriGraph $Version into $stageDir"

# 1. Main executable (+ legacy alias)
Copy-Item -Force $exeSrc (Join-Path $stageDir "ToriGraph.exe")
Copy-Item -Force $exeSrc (Join-Path $stageDir "scidavis.exe")

# 2. Translations
$transSrc = Join-Path $binSrcDir "translations"
if (Test-Path $transSrc) {
    $transDst = Join-Path $stageDir "translations"
    New-Item -ItemType Directory -Force -Path $transDst | Out-Null
    Copy-Item -Force (Join-Path $transSrc "*.qm") $transDst -ErrorAction SilentlyContinue
}

# 3. Fit plugins
$fitDst = Join-Path $stageDir "fitPlugins"
New-Item -ItemType Directory -Force -Path $fitDst | Out-Null
$fitRoot = Join-Path $BuildDir "fitPlugins"
if (Test-Path $fitRoot) {
    Get-ChildItem -Path $fitRoot -Recurse -Filter "*.dll" | ForEach-Object {
        Copy-Item -Force $_.FullName (Join-Path $fitDst $_.Name)
    }
}

# 4. Runtime DLLs (core + Qt5 + GSL/muParser/ICU/OpenSSL/MSVC; smart BLAS backend handling)
$dllPatterns = @(
    "Qt5Core*.dll", "Qt5Gui*.dll", "Qt5Widgets*.dll", "Qt5Svg*.dll",
    "Qt5Network*.dll", "Qt5PrintSupport*.dll", "Qt5Xml*.dll", "Qt5OpenGL*.dll", "Qt5Pdf*.dll",
    "gsl*.dll", "libcblas.dll", "libblas.dll", "openblas*.dll", "libopenblas*.dll",
    "muparser.dll", "zlib*.dll",
    "freetype.dll", "harfbuzz.dll", "graphite2.dll", "glib-2.0-0.dll", "intl-8.dll",
    "iconv.dll", "charset.dll", "pcre2-8.dll", "pcre2-16.dll",
    "icudt*.dll", "icuin*.dll", "icuuc*.dll",
    "liblzma.dll", "libzstd.dll", "zstd.dll", "libbz2.dll", "libexpat.dll",
    "sqlite3.dll", "turbojpeg.dll", "jpeg8.dll", "libpng16.dll",
    "libcrypto-3-x64.dll", "libssl-3-x64.dll",
    "libomp.dll", "tbb12.dll",
    "vcruntime140*.dll", "msvcp140*.dll", "vcomp140.dll", "concrt140.dll"
)

# Detect if libcblas.dll delegates to Intel MKL (mkl_rt)
$usesMkl = $false
$cblasCheck = @(
    (Join-Path $binSrcDir "libcblas.dll"),
    (Join-Path $depsBinDir "libcblas.dll"),
    (Join-Path $binSrcDir "libblas.dll"),
    (Join-Path $depsBinDir "libblas.dll"),
    (Join-Path $binSrcDir "liblapack.dll"),
    (Join-Path $depsBinDir "liblapack.dll")
) | Where-Object { Test-Path $_ }
foreach ($blasFile in $cblasCheck) {
    $rawBytes = [System.IO.File]::ReadAllBytes($blasFile)
    $ascii = [System.Text.Encoding]::ASCII.GetString($rawBytes)
    if ($ascii -match "mkl_rt") {
        $usesMkl = $true
        # Intel MKL must not be redistributed inside this GPL-3.0 package (license and ~100 MB);
        # stop instead of silently bundling it.
        throw ("$(Split-Path -Leaf $blasFile) ($blasFile) delegates to Intel MKL (mkl_rt). ToriGraph packages must not " +
               "redistribute MKL. Build against netlib or OpenBLAS (e.g. micromamba install " +
               "'libblas=*=*netlib' 'libcblas=*=*netlib') and run this script again.")
    }
}

$searchDirs = @($binSrcDir)
if (Test-Path $depsBinDir) { $searchDirs += $depsBinDir }

foreach ($pat in $dllPatterns) {
    foreach ($dir in $searchDirs) {
        Get-ChildItem -Path $dir -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
            $dst = Join-Path $stageDir $_.Name
            if (-not (Test-Path $dst)) {
                Copy-Item -Force $_.FullName $dst
            }
        }
    }
}

# 5. Qt5 plugins + qt.conf
$qtPluginGroups = @("platforms", "styles", "imageformats", "iconengines", "printsupport")
$pluginsDstRoot = Join-Path $stageDir "plugins"
foreach ($grp in $qtPluginGroups) {
    $srcGrp = Join-Path $depsPluginsDir $grp
    if (Test-Path $srcGrp) {
        $dstGrp = Join-Path $pluginsDstRoot $grp
        New-Item -ItemType Directory -Force -Path $dstGrp | Out-Null
        Get-ChildItem -Path $srcGrp -Filter "*.dll" | Where-Object { $_.Name -notlike "*d.dll" -or $_.Name -eq "qdirect2d.dll" } | ForEach-Object {
            Copy-Item -Force $_.FullName (Join-Path $dstGrp $_.Name)
        }
    }
}

@"
[Paths]
Plugins = plugins
Translations = translations
"@ | Set-Content -Path (Join-Path $stageDir "qt.conf") -Encoding ASCII

# 6. Helper scripts (OLE registration & Python SDK installation)
$scriptsDst = Join-Path $stageDir "scripts"
New-Item -ItemType Directory -Force -Path $scriptsDst | Out-Null
Copy-Item -Force (Join-Path $repoRoot "scripts\register-ole.ps1") $scriptsDst
Copy-Item -Force (Join-Path $repoRoot "scripts\install-python-sdk.ps1") $scriptsDst

# 7. Python SDK (source + prebuilt offline wheel when available)
$pyDst = Join-Path $stageDir "python"
New-Item -ItemType Directory -Force -Path $pyDst | Out-Null
Copy-Item -Recurse -Force (Join-Path $repoRoot "python\torigraph") (Join-Path $pyDst "torigraph")
Get-ChildItem -Path (Join-Path $pyDst "torigraph") -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $repoRoot "python\pyproject.toml") $pyDst
Copy-Item -Force (Join-Path $repoRoot "python\README.md") $pyDst

# Build a pure-Python offline wheel so the installer works offline without setuptools
$pyCmd = Get-Command python -ErrorAction SilentlyContinue
if ($pyCmd -and ($pyCmd.Source -notlike "*WindowsApps*")) {
    $wheelDst = Join-Path $pyDst "wheels"
    New-Item -ItemType Directory -Force -Path $wheelDst | Out-Null
    Write-Host "==> Building offline wheel for torigraph Python SDK..."
    $wheelOk = $false
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $pyCmd.Source -m pip wheel --no-deps --no-build-isolation --wheel-dir $wheelDst (Join-Path $repoRoot "python") 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0 -and (Get-ChildItem -Path $wheelDst -Filter "torigraph-*.whl" -ErrorAction SilentlyContinue)) {
            $wheelOk = $true
        }
    } finally {
        $ErrorActionPreference = $prevEap
    }
    if (-not $wheelOk) {
        Write-Host "    Building pure-Python PEP 427 wheel using stdlib zipfile..."
        $pyBuildWheel = @"
import base64, hashlib, sys, zipfile
from pathlib import Path

src_root = Path(sys.argv[1])
out_dir = Path(sys.argv[2])
ver = sys.argv[3]
pkg_dir = src_root / "torigraph"
dist_info = f"torigraph-{ver}.dist-info"
whl_path = out_dir / f"torigraph-{ver}-py3-none-any.whl"

metadata = (
    f"Metadata-Version: 2.1\n"
    f"Name: torigraph\n"
    f"Version: {ver}\n"
    f"Summary: Pure-Python JSON-RPC 2.0 Automation SDK for ToriGraph scientific plotting & data analysis\n"
    f"License: GPL-2.0-or-later\n"
    f"Requires-Python: >=3.8\n"
).encode("utf-8")

wheel_meta = (
    "Wheel-Version: 1.0\n"
    "Generator: torigraph-packager\n"
    "Root-Is-Purelib: true\n"
    "Tag: py3-none-any\n"
).encode("utf-8")

records = []
with zipfile.ZipFile(whl_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for p in sorted(
        p for p in pkg_dir.rglob("*")
        if p.is_file() and (p.suffix == ".py" or p.name == "py.typed") and "__pycache__" not in p.parts
    ):
        rel = p.relative_to(src_root).as_posix()
        data = p.read_bytes()
        zf.writestr(rel, data)
        h = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode("ascii")
        records.append(f"{rel},sha256={h},{len(data)}")
    for name, data in ((f"{dist_info}/METADATA", metadata), (f"{dist_info}/WHEEL", wheel_meta)):
        zf.writestr(name, data)
        h = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode("ascii")
        records.append(f"{name},sha256={h},{len(data)}")
    rec_path = f"{dist_info}/RECORD"
    records.append(f"{rec_path},,")
    zf.writestr(rec_path, ("\n".join(records) + "\n").encode("utf-8"))
print(f"    Created wheel: {whl_path.name}")
"@
        $pyBuildWheel | & $pyCmd.Source - (Join-Path $repoRoot "python") $wheelDst $Version
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Could not prebuild wheel; installer will fall back to source install."
            Remove-Item -Recurse -Force $wheelDst -ErrorAction SilentlyContinue
        }
    }
}

# 8. Docs & License (GPL-2.0 text for the sources, GPL-3.0 text for the binaries,
#    and the third-party notices; all are required)
foreach ($doc in @("LICENSE", "LICENSE-GPL-3.0.txt", "THIRD_PARTY_LICENSES.md", "README.md")) {
    $srcDoc = Join-Path $repoRoot $doc
    if (-not (Test-Path $srcDoc)) { throw "Required license/document file missing: $doc" }
    Copy-Item -Force $srcDoc $stageDir
}
# Built-in Japanese quick guide opened by F1 (ApplicationWindow::showHelp looks for manual\index.html)
$manualSrc = Join-Path $repoRoot "manual"
$manualDst = Join-Path $stageDir "manual"
New-Item -ItemType Directory -Force -Path $manualDst | Out-Null
Copy-Item -Recurse -Force (Join-Path $manualSrc "*") $manualDst

# 9. Quick standalone smoke check (verify staged files & no immediate DLL load crash)
Write-Host "==> Running staged payload verification..."
$requiredRel = @(
    "ToriGraph.exe",
    "qt.conf",
    "plugins\platforms\qwindows.dll",
    "scripts\register-ole.ps1",
    "scripts\install-python-sdk.ps1",
    "python\torigraph\__init__.py",
    "LICENSE",
    "LICENSE-GPL-3.0.txt",
    "THIRD_PARTY_LICENSES.md",
    "manual\index.html",
    "manual\index.en.html"
)
foreach ($rel in $requiredRel) {
    $full = Join-Path $stageDir $rel
    if (-not (Test-Path $full)) {
        throw "Required staged file missing: $rel"
    }
}
$prevQpa = $env:QT_QPA_PLATFORM
$env:QT_QPA_PLATFORM = "offscreen"
try {
    $stagedExe = Join-Path $stageDir "ToriGraph.exe"
    # --version returns before any plugin is loaded; "--headless --help" additionally creates the
    # QApplication (offscreen), so missing Qt DLLs / platform plugins show up here.
    foreach ($smokeArgs in @("--version", "--headless --help")) {
        $proc = Start-Process -FilePath $stagedExe -ArgumentList $smokeArgs -NoNewWindow -PassThru -ErrorAction SilentlyContinue
        if ($proc) {
            $null = $proc.Handle
            if ($proc.WaitForExit(15000)) {
                $rc = $proc.ExitCode
                Write-Host "    Staged ToriGraph.exe $smokeArgs exited with code $rc"
                if ($rc -eq -1073741515 -or $rc -eq 3221225781) {
                    throw "Staged ToriGraph.exe failed with STATUS_DLL_NOT_FOUND (0xC0000135)!"
                }
                if ($rc -ne 0) {
                    throw "Staged ToriGraph.exe $smokeArgs exited with non-zero code $rc"
                }
            } else {
                try { $proc.Kill() } catch {}
                throw "Staged ToriGraph.exe $smokeArgs timed out after 15000 ms."
            }
        } else {
            throw "Failed to launch staged ToriGraph.exe $smokeArgs"
        }
    }
} finally {
    if ($null -eq $prevQpa) { Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue } else { $env:QT_QPA_PLATFORM = $prevQpa }
}

$artifacts = @()

# 10. Portable ZIP archive
if (-not $SkipPortableZip) {
    $zipPath = Join-Path $OutDir "ToriGraph-$Version-win64-portable.zip"
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Write-Host "==> Creating portable archive: $zipPath"
    Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
    $artifacts += $zipPath
}

# 11. Inno Setup 6 integrated installer (.exe)
if (-not $SkipInstaller) {
    $resolvedIscc = $IsccPath
    if (-not $resolvedIscc) {
        $candidates = @(
            "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
            "C:\Program Files\Inno Setup 6\ISCC.exe",
            (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe")
        )
        foreach ($c in $candidates) {
            if (Test-Path $c) { $resolvedIscc = $c; break }
        }
        if (-not $resolvedIscc) {
            $cmd = Get-Command iscc -ErrorAction SilentlyContinue
            if ($cmd) { $resolvedIscc = $cmd.Source }
        }
    }

    if ($resolvedIscc -and (Test-Path $resolvedIscc)) {
        $issFile = Join-Path $repoRoot "installer\ToriGraph.iss"
        Write-Host "==> Compiling integrated Windows installer with Inno Setup: $resolvedIscc"
        & $resolvedIscc "/DMyAppVersion=$Version" "/DStageDir=$stageDir" "/DOutDir=$OutDir" $issFile
        if ($LASTEXITCODE -ne 0) {
            throw "ISCC.exe failed with exit code $LASTEXITCODE"
        }
        $setupExe = Join-Path $OutDir "ToriGraph-$Version-win64-setup.exe"
        if (Test-Path $setupExe) {
            $artifacts += $setupExe
        }
    } else {
        Write-Warning "Inno Setup 6 (ISCC.exe) not found. Staged files and portable ZIP are ready."
        Write-Warning "Install Inno Setup 6 (e.g. 'winget install JRSoftware.InnoSetup') and re-run this script to build the .exe installer."
    }
}

# 12. Generate SHA256SUMS.txt
if ($artifacts.Count -gt 0) {
    $sumFile = Join-Path $OutDir "SHA256SUMS.txt"
    $lines = foreach ($a in $artifacts) {
        $h = (Get-FileHash -Algorithm SHA256 -Path $a).Hash.ToLowerInvariant()
        "$h  $(Split-Path -Leaf $a)"
    }
    $lines | Set-Content -Path $sumFile -Encoding ASCII
    Write-Host "==> Build artifacts ready in $OutDir :"
    foreach ($a in $artifacts) {
        $szMB = [math]::Round((Get-Item $a).Length / 1MB, 2)
        Write-Host "    - $(Split-Path -Leaf $a) ($szMB MB)"
    }
    Write-Host "    - SHA256SUMS.txt"
}
