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
    [ValidateSet("configure", "build", "test", "asan", "py-test", "lint", "format", "cppcheck", "tidy", "sccache-stats", "gate", "gate-parallel")]
    [string]$Action = "build",

    [string]$BuildDir = "",
    [string]$Preset = "release",
    [string]$Suite = "",
    [string[]]$Files = @(),
    [switch]$Parallel,
    [int]$Jobs = 8,
    [switch]$TestOnly,
    [switch]$NoBuild,
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
        if ($TestOnly) {
            Invoke-Checked "CMake Build Test Target Only ($resolvedBuildDir)" {
                cmd.exe /c "chcp 932 >nul && cmake --build `"$resolvedBuildDir`" --target scidavis_ole_test --parallel"
            }
        } else {
            Invoke-Checked "CMake Build ($resolvedBuildDir)" {
                cmd.exe /c "chcp 932 >nul && cmake --build `"$resolvedBuildDir`" --target scidavis scidavis_ole_test explin exp_saturation fitRational0 fitRational1 planck_wavelength --parallel"
            }
        }
    }
    "test" {
        Initialize-VsBuildEnv
        Set-QtRuntimeEnv
        if (-not $env:QT_FONT_DPI) {
            $env:QT_FONT_DPI = "96"
        }
        $testExe = Join-Path $resolvedBuildDir "scidavis\scidavis_ole_test.exe"
        if (-not (Test-Path $testExe)) {
            & $PSCommandPath build -Preset $Preset -BuildDir $resolvedBuildDir
        }
        if ($Suite) {
            Invoke-Checked "Run C++ Suite $Suite ($testExe)" {
                & $testExe "--suite=$Suite"
            }
        } elseif ($Parallel) {
            $swAll = [System.Diagnostics.Stopwatch]::StartNew()
            Write-Host "==> Discovering C++ Suites for Parallel Run ($testExe)" -ForegroundColor Cyan
            $listLines = & $testExe "--list-suites"
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to enumerate suites with --list-suites (exit code $LASTEXITCODE)"
            }
            $allModularIds = @()
            foreach ($ln in $listLines) {
                if ($ln -match "^Suite\s+(\d+):") {
                    $allModularIds += [int]$matches[1]
                }
            }
            $serialClipboardSet = @(47, 48, 51, 56, 59, 60, 61, 68, 71, 73, 75)
            $bucket0Suites = @($allModularIds | Where-Object { $serialClipboardSet -contains $_ })
            $parallelIds = @($allModularIds | Where-Object { $serialClipboardSet -notcontains $_ })

            $numParallelBuckets = [Math]::Max(1, [Math]::Min([Math]::Max(1, $Jobs - 1), [Math]::Max(1, $parallelIds.Count)))
            $bucketLists = @()
            for ($b = 0; $b -lt $numParallelBuckets; $b++) {
                $bucketLists += ,(@())
            }
            for ($i = 0; $i -lt $parallelIds.Count; $i++) {
                $idx = $i % $numParallelBuckets
                $bucketLists[$idx] += $parallelIds[$i]
            }

            $runId = [System.Diagnostics.Process]::GetCurrentProcess().Id
            $logDir = Join-Path $env:TEMP ("torigraph_ctest_par_" + $runId)
            New-Item -ItemType Directory -Force -Path $logDir | Out-Null
            $jobsMeta = @()
            try {
                # Bucket 0 runs legacy inline suites (1..35) + all clipboard-using modular suites serially
                $b0Out = Join-Path $logDir "bucket0.out.log"
                $b0Err = Join-Path $logDir "bucket0.err.log"
                $b0Cmd = "& `"$testExe`" --legacy-only"
                if ($bucket0Suites.Count -gt 0) {
                    $b0Csv = ($bucket0Suites -join ",")
                    $b0Cmd += "; if (`$LASTEXITCODE -eq 0) { & `"$testExe`" --suite=$b0Csv }"
                }
                $b0Cmd += "; exit `$LASTEXITCODE"
                $b0Proc = Start-Process -FilePath "powershell.exe" -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", $b0Cmd) -NoNewWindow -PassThru -RedirectStandardOutput $b0Out -RedirectStandardError $b0Err
                $null = $b0Proc.Handle
                $jobsMeta += [PSCustomObject]@{
                    Name = "bucket-0 (legacy+clipboard: $($bucket0Suites -join ','))"
                    Proc = $b0Proc
                    OutLog = $b0Out
                    ErrLog = $b0Err
                    Timer = [System.Diagnostics.Stopwatch]::StartNew()
                }

                for ($b = 0; $b -lt $numParallelBuckets; $b++) {
                    $ids = @($bucketLists[$b])
                    if ($ids.Count -eq 0) { continue }
                    $csv = ($ids -join ",")
                    $outLog = Join-Path $logDir ("bucket" + ($b + 1) + ".out.log")
                    $errLog = Join-Path $logDir ("bucket" + ($b + 1) + ".err.log")
                    $p = Start-Process -FilePath $testExe -ArgumentList @("--suite=$csv") -NoNewWindow -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
                    $null = $p.Handle
                    $jobsMeta += [PSCustomObject]@{
                        Name = "bucket-$($b + 1) ($csv)"
                        Proc = $p
                        OutLog = $outLog
                        ErrLog = $errLog
                        Timer = [System.Diagnostics.Stopwatch]::StartNew()
                    }
                }

                $failed = $false
                foreach ($jm in $jobsMeta) {
                    $jm.Proc.WaitForExit()
                    $jm.Timer.Stop()
                    $rc = $jm.Proc.ExitCode
                    $sec = [Math]::Round($jm.Timer.Elapsed.TotalSeconds, 1)
                    if ($rc -ne 0) {
                        $failed = $true
                        Write-Host "[FAIL] $($jm.Name) rc=$rc (${sec}s)" -ForegroundColor Red
                        if (Test-Path $jm.OutLog) { Get-Content -Path $jm.OutLog -Encoding UTF8 | Write-Host }
                        if (Test-Path $jm.ErrLog) { Get-Content -Path $jm.ErrLog -Encoding UTF8 | Write-Host }
                    } else {
                        Write-Host "[PASS] $($jm.Name) (${sec}s)" -ForegroundColor Green
                    }
                }
                $swAll.Stop()
                Write-Host ("==> Parallel C++ Suites Finished in {0:F1}s" -f $swAll.Elapsed.TotalSeconds) -ForegroundColor Cyan
                if ($failed) {
                    throw "One or more parallel C++ test buckets failed."
                }
            } finally {
                Remove-Item -Path $logDir -Recurse -Force -ErrorAction SilentlyContinue
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
        if (-not $env:QT_FONT_DPI) {
            $env:QT_FONT_DPI = "96"
        }
        $asanDir = if ($BuildDir) { $BuildDir } else { Join-Path $repoRoot "build-asan" }
        if (-not (Test-Path (Join-Path $asanDir "build.ninja"))) {
            & $PSCommandPath configure -Preset asan -BuildDir $asanDir
        }
        $asanTestExe = Join-Path $asanDir "scidavis\scidavis_ole_test.exe"
        if (-not $NoBuild -or -not (Test-Path $asanTestExe)) {
            Invoke-Checked "Build ASan targets ($asanDir)" {
                cmd.exe /c "chcp 932 >nul && cmake --build `"$asanDir`" --target scidavis scidavis_ole_test --parallel"
            }
        }
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
        Invoke-Checked "Check Static Hygiene Rules" {
            python scripts/check_static_hygiene.py
        }
        Invoke-Checked "Run Ruff Linter on Python SDK & Scripts" {
            python -m ruff check --config python/pyproject.toml python scripts/check_version.py scripts/check_ja_glossary.py scripts/check_static_hygiene.py
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
    "gate-parallel" {
        $swGate = [System.Diagnostics.Stopwatch]::StartNew()
        & $PSCommandPath lint

        Initialize-VsBuildEnv
        $asanDir = Join-Path $repoRoot "build-asan"
        if (-not (Test-Path (Join-Path $resolvedBuildDir "build.ninja"))) {
            & $PSCommandPath configure -Preset release -BuildDir $resolvedBuildDir
        }
        if (-not (Test-Path (Join-Path $asanDir "build.ninja"))) {
            & $PSCommandPath configure -Preset asan -BuildDir $asanDir
        }

        $runId = [System.Diagnostics.Process]::GetCurrentProcess().Id
        $logDir = Join-Path $env:TEMP ("torigraph_gate_par_" + $runId)
        New-Item -ItemType Directory -Force -Path $logDir | Out-Null
        try {
            Write-Host "==> Parallel Build: Release ($resolvedBuildDir) + ASan ($asanDir)" -ForegroundColor Cyan
            $swBuild = [System.Diagnostics.Stopwatch]::StartNew()
            $relOut = Join-Path $logDir "build_rel.out.log"
            $relErr = Join-Path $logDir "build_rel.err.log"
            $asanOut = Join-Path $logDir "build_asan.out.log"
            $asanErr = Join-Path $logDir "build_asan.err.log"
            $pRel = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", "chcp 932 >nul && cmake --build `"$resolvedBuildDir`" --target scidavis scidavis_ole_test explin exp_saturation fitRational0 fitRational1 planck_wavelength --parallel") -NoNewWindow -PassThru -RedirectStandardOutput $relOut -RedirectStandardError $relErr
            $null = $pRel.Handle
            $pAsan = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", "chcp 932 >nul && cmake --build `"$asanDir`" --target scidavis scidavis_ole_test --parallel") -NoNewWindow -PassThru -RedirectStandardOutput $asanOut -RedirectStandardError $asanErr
            $null = $pAsan.Handle
            $pRel.WaitForExit()
            $pAsan.WaitForExit()
            $swBuild.Stop()
            if ($pRel.ExitCode -ne 0) {
                Get-Content -Path $relOut -Encoding UTF8 | Write-Host
                Get-Content -Path $relErr -Encoding UTF8 | Write-Host
                throw "Release build failed with exit code $($pRel.ExitCode)"
            }
            if ($pAsan.ExitCode -ne 0) {
                Get-Content -Path $asanOut -Encoding UTF8 | Write-Host
                Get-Content -Path $asanErr -Encoding UTF8 | Write-Host
                throw "ASan build failed with exit code $($pAsan.ExitCode)"
            }
            Write-Host ("[PASS] Parallel Release + ASan build ({0:F1}s)" -f $swBuild.Elapsed.TotalSeconds) -ForegroundColor Green

            $testExe = Join-Path $resolvedBuildDir "scidavis\scidavis_ole_test.exe"
            $asanSuites = $Suite
            if (-not $asanSuites) {
                $listOut = & $testExe "--list-suites"
                if ($listOut -match "^Suite\s+80:") {
                    $asanSuites = "53,59,66,77,78,79,80"
                } else {
                    $asanSuites = "53,59,66,77,78,79"
                }
            }

            Write-Host "==> Parallel Test Wave: C++ Parallel Test + pytest (then ASan overlap)" -ForegroundColor Cyan
            $swTests = [System.Diagnostics.Stopwatch]::StartNew()
            $cppOut = Join-Path $logDir "cpp_par.out.log"
            $cppErr = Join-Path $logDir "cpp_par.err.log"
            $pyOut = Join-Path $logDir "py_test.out.log"
            $pyErr = Join-Path $logDir "py_test.err.log"
            $pCpp = Start-Process -FilePath "powershell.exe" -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "test", "-Parallel", "-Jobs", "$Jobs", "-BuildDir", $resolvedBuildDir) -NoNewWindow -PassThru -RedirectStandardOutput $cppOut -RedirectStandardError $cppErr
            $null = $pCpp.Handle
            $pPy = Start-Process -FilePath "powershell.exe" -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "py-test", "-BuildDir", $resolvedBuildDir) -NoNewWindow -PassThru -RedirectStandardOutput $pyOut -RedirectStandardError $pyErr
            $null = $pPy.Handle

            # Wait for C++ test wave first so clipboard is completely free before ASan Suite 59 runs
            $pCpp.WaitForExit()
            if ($pCpp.ExitCode -ne 0) {
                Get-Content -Path $cppOut -Encoding UTF8 | Write-Host
                Get-Content -Path $cppErr -Encoding UTF8 | Write-Host
                throw "Parallel C++ test wave failed with exit code $($pCpp.ExitCode)"
            }
            Write-Host "[PASS] Parallel C++ test wave" -ForegroundColor Green

            # Now run ASan suites while py-test continues in parallel
            $asanRunOut = Join-Path $logDir "asan_run.out.log"
            $asanRunErr = Join-Path $logDir "asan_run.err.log"
            $pAsanRun = Start-Process -FilePath "powershell.exe" -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "asan", "-NoBuild", "-BuildDir", $asanDir, "-Suite", $asanSuites) -NoNewWindow -PassThru -RedirectStandardOutput $asanRunOut -RedirectStandardError $asanRunErr
            $null = $pAsanRun.Handle

            $pAsanRun.WaitForExit()
            $pPy.WaitForExit()
            $swTests.Stop()
            if ($pAsanRun.ExitCode -ne 0) {
                Get-Content -Path $asanRunOut -Encoding UTF8 | Write-Host
                Get-Content -Path $asanRunErr -Encoding UTF8 | Write-Host
                throw "ASan verification ($asanSuites) failed with exit code $($pAsanRun.ExitCode)"
            }
            if ($pPy.ExitCode -ne 0) {
                Get-Content -Path $pyOut -Encoding UTF8 | Write-Host
                Get-Content -Path $pyErr -Encoding UTF8 | Write-Host
                throw "py-test failed with exit code $($pPy.ExitCode)"
            }
            Write-Host ("[PASS] Parallel Test+ASan+Pytest finished in {0:F1}s" -f $swTests.Elapsed.TotalSeconds) -ForegroundColor Green
        } finally {
            Remove-Item -Path $logDir -Recurse -Force -ErrorAction SilentlyContinue
        }
        $swGate.Stop()
        Write-Host ("==> gate-parallel ALL PASSED in {0:F1}s" -f $swGate.Elapsed.TotalSeconds) -ForegroundColor Green
    }
}

if ($Notify) {
    $suffix = if ($Suite) { " (Suite $Suite)" } else { "" }
    & (Join-Path $PSScriptRoot "notify-phone.ps1") -Title "ToriGraph dev.ps1" -Message "Completed: $Action$suffix"
}
