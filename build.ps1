# gmaker Build Script (PowerShell)
$ErrorActionPreference = "Continue"

$services = @(
    "registry-go",
    "dbproxy-go",
    "login-go",
    "biz-go",
    "chat-go",
    "bot-go",
    "logstats-go"
)

$tools = @("testclient")

if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Name "bin" | Out-Null }

Write-Host "=========================================="
Write-Host "  gmaker Build Script (PowerShell)"
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

$buildErrors = 0

# Build Go services
Write-Host "[Phase 1/2] Building Go services ..." -ForegroundColor Yellow
Write-Host "------------------------------------------"
foreach ($svc in $services) {
    Write-Host -NoNewline "  [GO] Building services/$svc ... "
    Push-Location "services/$svc"
    $err = $null
    go mod tidy 2>$null
    go build -o "../../bin/$svc.exe" . 2>$err
    Pop-Location
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL]" -ForegroundColor Red
        $buildErrors++
    } else {
        Write-Host "[OK]" -ForegroundColor Green
    }
}

# Build Go tools
foreach ($t in $tools) {
    Write-Host -NoNewline "  [GO] Building tools/$t ... "
    go build -o "bin/$t.exe" "./tools/$t" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL]" -ForegroundColor Red
        $buildErrors++
    } else {
        Write-Host "[OK]" -ForegroundColor Green
    }
}

Write-Host ""

# Build C++ services
Write-Host "[Phase 2/2] Building C++ services ..." -ForegroundColor Yellow
Write-Host "------------------------------------------"

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "  [SKIP] CMake not found." -ForegroundColor Yellow
} else {
    if (-not (Test-Path "build")) { New-Item -ItemType Directory -Name "build" | Out-Null }
    Push-Location "build"
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [FAIL] CMake config failed" -ForegroundColor Red
        $buildErrors++
    } else {
        cmake --build . --config Release 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  [FAIL] C++ build failed" -ForegroundColor Red
            $buildErrors++
        }
    }
    Pop-Location

    if (Test-Path "build/Release/gateway-cpp.exe") {
        Copy-Item "build/Release/gateway-cpp.exe" "bin/" -Force
        Write-Host "  [OK]   bin/gateway-cpp.exe" -ForegroundColor Green
    }
    if (Test-Path "build/Release/realtime-cpp.exe") {
        Copy-Item "build/Release/realtime-cpp.exe" "bin/" -Force
        Write-Host "  [OK]   bin/realtime-cpp.exe" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "=========================================="
if ($buildErrors -eq 0) {
    Write-Host "  Build SUCCEEDED" -ForegroundColor Green
} else {
    Write-Host "  Build COMPLETED with $buildErrors ERROR(S)" -ForegroundColor Red
}
Write-Host "=========================================="
Write-Host "  Output: $(Resolve-Path bin)"
Write-Host ""
