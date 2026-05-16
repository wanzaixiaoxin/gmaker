# ============================================================
#  gmaker Build Script (PowerShell, Optimized)
#
#  Usage:
#    .\build.ps1                Build all (Go + C++)
#    .\build.ps1 -GoOnly        Go services only
#    .\build.ps1 -CppOnly       C++ services only
#    .\build.ps1 -Clean         Clean all build artifacts
#    .\build.ps1 -Tidy          Run go mod tidy before building
#    .\build.ps1 -Verbose       Show full build output
# ============================================================

param(
    [switch]$GoOnly,
    [switch]$CppOnly,
    [switch]$Clean,
    [switch]$Tidy,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"

# --- Clean mode ---
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path "bin")   { Write-Host "  x bin\";   Remove-Item -Recurse -Force "bin" }
    if (Test-Path "build") { Write-Host "  x build\"; Remove-Item -Recurse -Force "build" }
    Write-Host "Done."
    exit 0
}

$buildGo  = -not $CppOnly
$buildCpp = -not $GoOnly

# Fix Chinese encoding: sync PowerShell output encoding with system code page
# MSBuild/CMake output GBK (cp936) on Chinese Windows, but PowerShell defaults to UTF-8
$originalEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.Encoding]::GetEncoding("gb2312")
$originalCP = chcp | ForEach-Object { if ($_ -match ':\s*(\d+)') { $matches[1] } }

$services = @(
    "registry-go",
    "dbproxy-go",
    "login-go",
    "biz-go",
    "chat-go",
    "bot-go",
    "logstats-go",
    "config-go",
    "match-go"
)
$tools = @("testclient")

if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Name "bin" | Out-Null }

Write-Host "=========================================="
Write-Host "  gmaker Build Script (PowerShell)"
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Set proxy once (quiet)
go env -w GOPROXY=https://goproxy.cn,direct 2>$null | Out-Null
go env -w GOSUMDB=sum.golang.google.cn 2>$null | Out-Null

$buildErrors = 0
$jobs = [System.Environment]::ProcessorCount
if ($jobs -lt 2) { $jobs = 2 }
if ($jobs -gt 8) { $jobs = 8 }

$sw = [System.Diagnostics.Stopwatch]::StartNew()

# ============================================================
#  Phase 1: Go Services (parallel via Start-Job)
# ============================================================

if ($buildGo) {
    Write-Host "[Phase 1/2] Building Go services ..." -ForegroundColor Yellow
    Write-Host "------------------------------------------"
    Write-Host "  Parallel jobs: $jobs"

    # --- Tidy if requested ---
    if ($Tidy) {
        Write-Host "  Running go mod tidy ..."
        foreach ($svc in $services) {
            if (Test-Path "services/$svc/go.mod") {
                Push-Location "services/$svc"
                go mod tidy 2>&1 | ForEach-Object { if ($Verbose) { Write-Host $_ } }
                Pop-Location
            }
        }
        go mod tidy 2>$null | Out-Null
        Write-Host ""
    }

    # --- Launch parallel builds ---
    $allTargets = @()
    foreach ($svc in $services) {
        $allTargets += @{ Name = $svc; Type = "service" }
    }
    foreach ($t in $tools) {
        $allTargets += @{ Name = $t; Type = "tool" }
    }

    Write-Host "  Compiling $($allTargets.Count) targets in parallel..."

    $jobsList = @{}
    foreach ($target in $allTargets) {
        $name = $target.Name
        $type = $target.Type

        if ($type -eq "service" -and (Test-Path "services/$name/go.mod")) {
            $job = Start-Job -Name $name -ScriptBlock {
                param($svc, $root)
                Set-Location "$root/services/$svc"
                $output = go build -o "../../bin/$svc.exe" . 2>&1 | Out-String
                @{ Name = $svc; ExitCode = $LASTEXITCODE; Output = $output }
            } -ArgumentList $name, $PWD.Path
        } elseif ($type -eq "service") {
            $job = Start-Job -Name $name -ScriptBlock {
                param($svc, $root)
                Set-Location $root
                $output = go build -o "bin/$svc.exe" "./services/$svc" 2>&1 | Out-String
                @{ Name = $svc; ExitCode = $LASTEXITCODE; Output = $output }
            } -ArgumentList $name, $PWD.Path
        } else {
            $job = Start-Job -Name $name -ScriptBlock {
                param($t, $root)
                Set-Location $root
                $output = go build -o "bin/$t.exe" "./tools/$t" 2>&1 | Out-String
                @{ Name = $t; ExitCode = $LASTEXITCODE; Output = $output }
            } -ArgumentList $name, $PWD.Path
        }
        $jobsList[$name] = $job
    }

    # Wait for all jobs
    $results = @{}
    foreach ($entry in $jobsList.GetEnumerator()) {
        $result = Receive-Job -Job $entry.Value -Wait -AutoRemoveJob
        $results[$entry.Key] = $result
    }

    # Report results
    foreach ($target in $allTargets) {
        $name = $target.Name
        $r = $results[$name]
        if ($r.ExitCode -eq 0) {
            Write-Host "  [OK]   $name.exe" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] $name" -ForegroundColor Red
            $buildErrors++
            if ($Verbose -and $r.Output) {
                Write-Host $r.Output -ForegroundColor DarkGray
            }
        }
    }

    Write-Host ""
}

# ============================================================
#  Phase 2: C++ Services (parallel cmake --build)
# ============================================================

if ($buildCpp) {
    Write-Host "[Phase 2/2] Building C++ services ..." -ForegroundColor Yellow
    Write-Host "------------------------------------------"

    # Switch to UTF-8 code page so MSBuild outputs UTF-8 instead of GBK
    chcp 65001 > $null 2>&1
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Host "  [SKIP] CMake not found." -ForegroundColor Yellow
    } else {
        $protoReady = $false
        if (Test-Path "3rd/protobuf/protobuf-34.1/build/Release/protoc.exe") {
            $protoReady = $true
        }
        if (Get-Command protoc -ErrorAction SilentlyContinue) {
            $protoReady = $true
        }

        if (-not $protoReady) {
            Write-Host "  [SKIP] Protobuf C++ library not found." -ForegroundColor Yellow
        } else {
            if (-not (Test-Path "build")) { New-Item -ItemType Directory -Name "build" | Out-Null }
            Push-Location "build"

            # Incremental: only re-run cmake configure if no cache
            $cmakeOk = $true
            if (-not (Test-Path "CMakeCache.txt")) {
                Write-Host "  Running CMake configure ..."
                $cmakeOutput = cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-String
                if ($LASTEXITCODE -ne 0) {
                    Write-Host "  [FAIL] CMake configure failed" -ForegroundColor Red
                    Write-Host $cmakeOutput
                    $buildErrors++
                    $cmakeOk = $false
                    Pop-Location
                }
            }

            if ($cmakeOk) {
                # Parallel C++ build
                Write-Host "  Building with -j$jobs ..."
                $buildOutput = cmake --build . --config Release -j $jobs 2>&1 | Out-String
                if ($LASTEXITCODE -ne 0) {
                    Write-Host "  [FAIL] C++ build failed" -ForegroundColor Red
                    Write-Host $buildOutput
                    $buildErrors++
                    Pop-Location
                } else {
                    Pop-Location

                    if (Test-Path "build/Release/gateway-cpp.exe") {
                        Copy-Item "build/Release/gateway-cpp.exe" "bin/" -Force
                        Write-Host "  [OK]   gateway-cpp.exe" -ForegroundColor Green
                    } else {
                        Write-Host "  [WARN] gateway-cpp.exe not found" -ForegroundColor Yellow
                    }
                    if (Test-Path "build/Release/realtime-cpp.exe") {
                        Copy-Item "build/Release/realtime-cpp.exe" "bin/" -Force
                        Write-Host "  [OK]   realtime-cpp.exe" -ForegroundColor Green
                    } else {
                        Write-Host "  [WARN] realtime-cpp.exe not found" -ForegroundColor Yellow
                    }
                    if (Test-Path "build/Release/test-crypto.exe") {
                        Copy-Item "build/Release/test-crypto.exe" "bin/" -Force
                        Write-Host "  [OK]   test-crypto.exe" -ForegroundColor Green
                    }
                }
            }
        }
    }
    Write-Host ""
}

# ============================================================
#  Summary
# ============================================================

$sw.Stop()

# Restore original code page
if ($originalCP -and $originalCP -ne "65001") {
    chcp $originalCP > $null 2>&1
    [Console]::OutputEncoding = $originalEncoding
}

Write-Host "=========================================="
if ($buildErrors -eq 0) {
    Write-Host "  Build SUCCEEDED" -ForegroundColor Green
} else {
    Write-Host "  Build COMPLETED with $buildErrors ERROR(S)" -ForegroundColor Red
}
Write-Host "  Elapsed: $($sw.Elapsed.ToString('mm\:ss'))"
Write-Host "=========================================="
Write-Host "  Output: $(Resolve-Path bin)"
Write-Host ""

if ($buildGo) {
    Write-Host "  Go binaries:"
    foreach ($svc in $services) {
        if (Test-Path "bin/$svc.exe") { Write-Host "    [OK]   $svc.exe" -ForegroundColor Green }
    }
    foreach ($t in $tools) {
        if (Test-Path "bin/$t.exe") { Write-Host "    [OK]   $t.exe" -ForegroundColor Green }
    }
    Write-Host ""
}

if ($buildCpp) {
    Write-Host "  C++ binaries:"
    if (Test-Path "bin/gateway-cpp.exe")  { Write-Host "    [OK]   gateway-cpp.exe" -ForegroundColor Green }
    if (Test-Path "bin/realtime-cpp.exe") { Write-Host "    [OK]   realtime-cpp.exe" -ForegroundColor Green }
    if (-not (Test-Path "bin/gateway-cpp.exe") -and -not (Test-Path "bin/realtime-cpp.exe")) {
        Write-Host "    (none - needs protobuf + CMake)"
    }
    Write-Host ""
}
