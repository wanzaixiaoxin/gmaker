# ============================================
# Chat Service Integration Test Script (PowerShell)
# ============================================
# 使用 tools/testclient 进行聊天服务测试
# ============================================

param(
    [string]$RedisAddr = "192.168.0.85:6379",
    [int]$Bots = 3,
    [int]$DurationSeconds = 30
)

Write-Host "=== Chat Service Integration Test ===" -ForegroundColor Cyan
Write-Host "Test client: tools/testclient"
Write-Host "Redis: $RedisAddr"
Write-Host "MySQL: Not required (using memory fallback)"
Write-Host "Test config: $Bots bots, ${DurationSeconds}s duration"
Write-Host ""

$BinDir = ".\bin"
$BuildDir = ".\build\Release"

# 检查编译产物
if (-not (Test-Path "$BinDir\chat-go.exe")) {
    Write-Host "[ERROR] chat-go.exe not found, please build first" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$BuildDir\gateway-cpp.exe")) {
    Write-Host "[ERROR] gateway-cpp.exe not found, please build first" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$BinDir\testclient.exe")) {
    Write-Host "[INFO] Building testclient..." -ForegroundColor Yellow
    go build -o "$BinDir\testclient.exe" .\tools\testclient
}

# 清理旧进程
Write-Host "[INFO] Cleaning up old processes..." -ForegroundColor Yellow
Get-Process -Name "registry-go" -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name "chat-go" -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name "gateway-cpp" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

# 启动 Registry
Write-Host "[1/3] Starting Registry..." -ForegroundColor Green
$registry = Start-Process -FilePath "$BinDir\registry-go.exe" -ArgumentList "-listen", "127.0.0.1:2379", "-store", "memory" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 1

# 启动 Chat
Write-Host "[2/3] Starting Chat Service..." -ForegroundColor Green
$chat = Start-Process -FilePath "$BinDir\chat-go.exe" -ArgumentList "-listen", ":8086", "-registry", "127.0.0.1:2379", "-redis", $RedisAddr, "-node-id", "1" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

# 启动 Gateway
Write-Host "[3/3] Starting Gateway..." -ForegroundColor Green
$gateway = Start-Process -FilePath "$BuildDir\gateway-cpp.exe" -ArgumentList "8081", "127.0.0.1:2379", "127.0.0.1:8082" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== Running Chat Test ===" -ForegroundColor Cyan
Write-Host ""

# 运行测试
& "$BinDir\testclient.exe" -addr "127.0.0.1:8081" -bots $Bots -scenario chat -duration "${DurationSeconds}s"

Write-Host ""
Write-Host "=== Test Complete ===" -ForegroundColor Cyan
Write-Host "Press any key to stop all services..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# 清理
Write-Host "Stopping services..." -ForegroundColor Yellow
Get-Process -Name "registry-go" -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name "chat-go" -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name "gateway-cpp" -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host "All services stopped." -ForegroundColor Green
