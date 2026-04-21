# gmaker 服务启动脚本
# 用法: .\scripts\start-services.ps1 [minimal|full|registry|biz|dbproxy|gateway|logstats]

param(
    [Parameter()]
    [ValidateSet("minimal", "full", "registry", "biz", "dbproxy", "gateway", "logstats", "all")]
    [string]$Target = "minimal",

    [Parameter()]
    [string]$LogLevel = "info"
)

$root = Split-Path -Parent $PSScriptRoot
$binDir = Join-Path $root "bin"
$logDir = Join-Path $root "logs"

if (!(Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}

$logTs = Get-Date -Format "yyyyMMdd_HHmmss"

function Start-ServiceProcess {
    param($Name, $ExePath, $Arguments)
    $proc = Get-Process | Where-Object { $_.ProcessName -eq (Split-Path $ExePath -Leaf).Replace('.exe','') } | Select-Object -First 1
    if ($proc) {
        Write-Host "  $Name is already running (PID: $($proc.Id))" -ForegroundColor Yellow
        return
    }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $ExePath
    $psi.Arguments = $Arguments
    $psi.WorkingDirectory = $root
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $false
    [System.Diagnostics.Process]::Start($psi) | Out-Null
    Write-Host "  $Name started" -ForegroundColor Green
}

function Start-Registry {
    $logFile = Join-Path $logDir "registry_${logTs}.log"
    Start-ServiceProcess "Registry" (Join-Path $binDir "registry-go.exe") "-listen 127.0.0.1:2379 -store memory -log-file `"$logFile`" -log-level $LogLevel"
}
function Start-DBProxy {
    $logFile = Join-Path $logDir "dbproxy_${logTs}.log"
    Start-ServiceProcess "DBProxy" (Join-Path $binDir "dbproxy-go.exe") "-listen 127.0.0.1:3307 -redis 127.0.0.1:6379 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker -log-file `"$logFile`" -log-level $LogLevel"
}
function Start-Biz {
    $logFile = Join-Path $logDir "biz_${logTs}.log"
    Start-ServiceProcess "Biz" (Join-Path $binDir "biz-go.exe") "-listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -metrics :9082 -log-file `"$logFile`" -log-level $LogLevel"
}
function Start-Gateway {
    $logFile = Join-Path $logDir "gateway_${logTs}.log"
    Start-ServiceProcess "Gateway" (Join-Path $binDir "gateway-cpp.exe") "8081 127.0.0.1:2379 127.0.0.1:8082 --log-file `"$logFile`" --log-level $LogLevel"
}
function Start-LogStats {
    $logFile = Join-Path $logDir "logstats_${logTs}.log"
    Start-ServiceProcess "LogStats" (Join-Path $binDir "logstats-go.exe") "-listen 127.0.0.1:8085 -http :8086 -log-file `"$logFile`" -log-level $LogLevel"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  gmaker Service Starter" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

switch ($Target) {
    "registry"  { Start-Registry }
    "dbproxy"   { Start-DBProxy }
    "biz"       { Start-Biz }
    "gateway"   { Start-Gateway }
    "logstats"  { Start-LogStats }
    "minimal"   {
        Write-Host "Starting minimal link (Registry -> Biz -> Gateway)..." -ForegroundColor Cyan
        Start-Registry; Start-Sleep -Milliseconds 800
        Start-Biz;      Start-Sleep -Milliseconds 800
        Start-Gateway;  Start-Sleep -Milliseconds 500
    }
    "full"      {
        Write-Host "Starting full link (Registry -> DBProxy -> Biz -> Gateway)..." -ForegroundColor Cyan
        Write-Host "Prerequisites: MySQL on 3306, Redis on 6379" -ForegroundColor Yellow
        Start-Registry; Start-Sleep -Milliseconds 800
        Start-DBProxy;  Start-Sleep -Milliseconds 1500
        Start-Biz;      Start-Sleep -Milliseconds 1500
        Start-Gateway;  Start-Sleep -Milliseconds 500
    }
    "all"       {
        Write-Host "Starting all services..." -ForegroundColor Cyan
        Start-Registry; Start-Sleep -Milliseconds 500
        Start-DBProxy;  Start-Sleep -Milliseconds 800
        Start-Biz;      Start-Sleep -Milliseconds 800
        Start-Gateway;  Start-Sleep -Milliseconds 500
        Start-LogStats; Start-Sleep -Milliseconds 500
    }
}

Write-Host ""
Write-Host "Done. Logs: $logDir" -ForegroundColor Green
Write-Host "Use .\scripts\stop-services.ps1 to stop." -ForegroundColor Green
