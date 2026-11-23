<#
.SYNOPSIS
    Unified developer & AI-agent workflow driver for ToriGraph on Windows.
.DESCRIPTION
    Automatically locates Visual Studio 2022 MSVC (vcvars64.bat with VSLANG=1033 to avoid
    CP932 /showIncludes garbling), VS bundled CMake/Ninja/Clang-Tidy/Clang-Format, and
    .tools/dev-env (sccache, cppcheck), then runs the requested subcommand.

.EXAMPLE
    .\scripts\dev.ps1 build
    .\scripts\dev.ps1 test -Suite 78
    .\scripts\dev.ps1 asan -Suite 78
    .\scripts\dev.ps1 py-test
    .\scripts\dev.ps1 lint
    .\scripts\dev.ps1 gate -Notify
#>
param(
    [Parameter(Position = 0)]
    [ValidateSet("configure", "build", "test", "asan", "py-test", "lint", "format", "cppcheck", "tidy", "sccache-stats", "gate")]
    [string]$Action = "build",

    [string]$BuildDir = "",
    [string]$Preset = "release",
    [string]$Suite = "",
    [string[]]$Files = @(),
    [switch]$Notify
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

# Prevent development test runs from overwriting the user's installed OLE registration
$env:TORIGRAPH_NO_OLE_REGISTER = "1"
$env:VSLANG = "1033"

function Initialize-VsBuildEnv {
    $vsCandidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
        "C:\Program Files\Microsoft Visual Studio\2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
    )
    $vsRoot = $vsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $vsRoot) {
        throw "Visual Studio 2022 installation not found."
    }

    $vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        $dump = cmd.exe /c "set VSLANG=1033 && call `"$vcvars`" >nul && set"
        foreach ($line in $dump) {
            if ($line -match "^([^=]+)=(.*)$") {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
            }
        }
    }

    $extraPaths = @(
        (Join-Path $repoRoot ".tools\dev-env\bin"),
        (Join-Path $repoRoot ".tools\dev-env\Library\bin"),
        (Join-Path $repoRoot ".tools\dev-env\Scripts"),
        (Join-Path $repoRoot ".tools\bin"),
        (Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"),
        (Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"),
        (Join-Path $vsRoot "VC\Tools\Llvm\x64\bin")
    ) | Where-Object { Test-Path $_ }

    foreach ($p in $extraPaths) {
        if (($env:PATH -split ";") -notcontains $p) {
            $env:PATH = "$p;$env:PATH"
        }
    }
}

function Set-QtRuntimeEnv {
    $env:QT_OPENGL = "software"
    $depsPlugins = Join-Path $repoRoot ".deps\Library\plugins\platforms"
    $depsBin = Join-Path $repoRoot ".deps\Library\bin"
    if (Test-Path $depsPlugins) {
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = $depsPlugins
    }
    if ((Test-Path $depsBin) -and (($env:PATH -split ";") -notcontains $depsBin)) {
        $env:PATH = "$depsBin;$env:PATH"
    }
}

function Invoke-Checked([string]$Label, [scriptblock]$Block) {
    Write-Host "==> $Label" -ForegroundColor Cyan
    & $Block
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

$resolvedBuildDir = if ($BuildDir) {
    $BuildDir
} elseif ($Preset -eq "asan" -or $Action -eq "asan") {
    Join-Path $repoRoot "build-asan"
} else {
    Join-Path $repoRoot "build"
}

switch ($Action) {
    "sccache-stats" {
        Initialize-VsBuildEnv
        sccache --show-stats
    }
    "configure" {
        Initialize-VsBuildEnv
        $prefix = Join-Path $repoRoot ".deps\Library"
        $isAsan = ($Preset -eq "asan")
        $bType = if ($isAsan) { "RelWithDebInfo" } else { "Release" }
        $asanFlag = if ($isAsan) { "ON" } else { "OFF" }
        Invoke-Checked "CMake Configure ($bType, ASan=$asanFlag -> $resolvedBuildDir)" {
            # Run CMake inside CP932 cmd.exe so Japanese cl.exe /showIncludes is decoded cleanly
            # even when the parent shell (e.g. Antigravity / UTF-8 PowerShell) uses code page 65001.
            $cmakeCmd = "chcp 932 >nul && cmake -B `"$resolvedBuildDir`" -G Ninja " +
                "-DCMAKE_BUILD_TYPE=$bType " +
                "-DCMAKE_PREFIX_PATH=`"$prefix`" " +
                "-DORIGIN_IMPORT=ON " +
                "-DBUILD_QWT=ON " +
                "-DBUILD_QWTPLOT3D=ON " +
                "-DBUILD_TESTS=OFF " +
                "-DTORIGRAPH_USE_CCACHE=ON " +
                "-DTORIGRAPH_ENABLE_PCH=ON " +
                "-DTORIGRAPH_SANITIZE_ADDRESS=$asanFlag"
            cmd.exe /c $cmakeCmd
        }
    }
    "build" {
        Initialize-VsBuildEnv
        if (-not (Test-Path (Join-Path $resolvedBuildDir "build.ninja"))) {
            & $PSCommandPath configure -Preset $Preset -BuildDir $resolvedBuildDir
        }
        Invoke-Checked "CMake Build ($resolvedBuildDir)" {
            cmd.exe /c "chcp 932 >nul && cmake --build `"$resolvedBuildDir`" --target scidavis scidavis_ole_test explin exp_saturation fitRational0 fitRational1 planck_wavelength --parallel"
        }
    }
    "test" {
        Initialize-VsBuildEnv
        Set-QtRuntimeEnv
        $testExe = Join-Path $resolvedBuildDir "scidavis\scidavis_ole_test.exe"
        if (-not (Test-Path $testExe)) {
            & $PSCommandPath build -Preset $Preset -BuildDir $resolvedBuildDir
        }
        if ($Suite) {
            Invoke-Checked "Run C++ Suite $Suite ($testExe)" {
                & $testExe "--suite=$Suite"
            }
        } else {
            Invoke-Checked "Run Full C++ Suite ($testExe)" {
                & $testExe
            }
        }
    }
    "asan" {
        Initialize-VsBuildEnv
        Set-QtRuntimeEnv
        $asanDir = if ($BuildDir) { $BuildDir } else { Join-Path $repoRoot "build-asan" }
        if (-not (Test-Path (Join-Path $asanDir "build.ninja"))) {
            & $PSCommandPath configure -Preset asan -BuildDir $asanDir
        }
        Invoke-Checked "Build ASan targets ($asanDir)" {
            cmd.exe /c "chcp 932 >nul && cmake --build `"$asanDir`" --target scidavis scidavis_ole_test --parallel"
        }
        $asanTestExe = Join-Path $asanDir "scidavis\scidavis_ole_test.exe"
        $env:ASAN_OPTIONS = "detect_leaks=0:halt_on_error=1:allocator_may_return_null=1"
        if ($Suite) {
            Invoke-Checked "Run ASan Suite $Suite" {
                & $asanTestExe "--suite=$Suite"
            }
        } else {
            Invoke-Checked "Run ASan Full Suite" {
                & $asanTestExe
            }
        }
    }
    "py-test" {
        Set-QtRuntimeEnv
        $exePath = Join-Path $resolvedBuildDir "scidavis\ToriGraph.exe"
        if (Test-Path $exePath) {
            $env:TORIGRAPH_EXE = (Resolve-Path $exePath).Path
        }
        Invoke-Checked "Run Python SDK Tests (pytest -n auto)" {
            python -m pytest python/tests -m "not slow" -n auto --dist loadfile
        }
    }
    "lint" {
        Invoke-Checked "Check Version Consistency" {
            python scripts/check_version.py
        }
        Invoke-Checked "Check Japanese Glossary" {
            python scripts/check_ja_glossary.py
        }
        Invoke-Checked "Run Ruff Linter on Python SDK & Scripts" {
            python -m ruff check --config python/pyproject.toml python scripts/check_version.py scripts/check_ja_glossary.py
        }
    }
    "format" {
        Initialize-VsBuildEnv
        $fileList = @($Files | ForEach-Object { $_ -split "," } | Where-Object { $_ })
        if ($fileList.Count -eq 0) {
            $fileList = @(git diff --name-only --diff-filter=ACMR HEAD | Where-Object { $_ -match "\.(cpp|h)$" })
        }
        if ($fileList.Count -gt 0) {
            Invoke-Checked "Format C++ files with clang-format" {
                & clang-format -i $fileList
            }
        } else {
            Write-Host "No modified C++ files to format."
        }
    }
    "cppcheck" {
        Initialize-VsBuildEnv
        $targets = @($Files | ForEach-Object { $_ -split "," } | Where-Object { $_ })
        if ($targets.Count -eq 0) {
            $targets = @("libscidavis/src/CrashHandler.cpp", "scidavis/src/main.cpp")
        }
        Invoke-Checked "Run cppcheck static analysis" {
            & cppcheck --enable=warning,performance,portability --std=c++17 --library=qt --suppress=missingIncludeSystem --inline-suppr --quiet $targets
        }
    }
    "tidy" {
        Initialize-VsBuildEnv
        $ccJson = Join-Path $resolvedBuildDir "compile_commands.json"
        if (-not (Test-Path $ccJson)) {
            & $PSCommandPath configure -Preset $Preset -BuildDir $resolvedBuildDir
        }
        # Strip MSVC PCH flags (/Yu, /Fp, /FI...cmake_pch.hxx) into a clean compile_commands copy for clang-tidy
        $tidyDir = Join-Path $resolvedBuildDir "clang-tidy-db"
        New-Item -ItemType Directory -Force -Path $tidyDir | Out-Null
        $rawCc = Get-Content -Raw -Path $ccJson -Encoding UTF8
        $cleanCc = $rawCc -replace '/Yu[^\s"]+', '' -replace '/Fp[^\s"]+', '' -replace '/FI[^\s"]*cmake_pch\.hxx', ''
        Set-Content -Path (Join-Path $tidyDir "compile_commands.json") -Value $cleanCc -Encoding UTF8
        $targets = @($Files | ForEach-Object { $_ -split "," } | Where-Object { $_ })
        if ($targets.Count -eq 0) {
            $targets = @("libscidavis/src/CrashHandler.cpp")
        }
        Invoke-Checked "Run clang-tidy" {
            & clang-tidy -p "$tidyDir" $targets
        }
    }
    "gate" {
        & $PSCommandPath lint
        & $PSCommandPath build -BuildDir $resolvedBuildDir
        if ($Suite) {
            & $PSCommandPath test -BuildDir $resolvedBuildDir -Suite $Suite
        } else {
            & $PSCommandPath test -BuildDir $resolvedBuildDir
        }
        & $PSCommandPath py-test -BuildDir $resolvedBuildDir
    }
}

if ($Notify) {
    $suffix = if ($Suite) { " (Suite $Suite)" } else { "" }
    & (Join-Path $PSScriptRoot "notify-phone.ps1") -Title "ToriGraph dev.ps1" -Message "Completed: $Action$suffix"
}
