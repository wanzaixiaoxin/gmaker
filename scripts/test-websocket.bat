@echo off
REM ============================================
REM WebSocket Integration Test Script
REM ============================================
REM 验证 WebSocket 握手 + gmaker 握手 + 业务帧收发
REM 所有服务在独立窗口运行，与 start.bat 风格一致
REM ============================================

echo === WebSocket Integration Test ===
echo.

set BIN_DIR=.\bin
set BUILD_DIR=.\build\Release

REM 检查编译产物
if not exist "%BIN_DIR%\biz-go.exe" (
    echo [ERROR] biz-go.exe not found, please build first
    exit /b 1
)

if not exist "%BUILD_DIR%\gateway-cpp.exe" (
    echo [ERROR] gateway-cpp.exe not found, please build first
    exit /b 1
)

if not exist "%BIN_DIR%\testclient.exe" (
    echo [INFO] Building testclient...
    go build -o %BIN_DIR%\testclient.exe .\tools\testclient
)

REM 启动 Registry
echo [1/4] Starting Registry (new window) ...
start "Registry" bin\registry-go.exe -listen 127.0.0.1:2379 -store memory
timeout /t 1 /nobreak >nul

REM 启动 Biz
echo [2/4] Starting Biz (new window) ...
start "Biz" bin\biz-go.exe -config conf\biz.json
timeout /t 2 /nobreak >nul

REM 启动 Gateway
echo [3/4] Starting Gateway (new window) ...
start "Gateway" bin\gateway-cpp.exe --config conf\gateway.json
timeout /t 2 /nobreak >nul

REM 运行 WebSocket 测试
echo [4/4] Starting TestClient (new window) ...
start "TestClient-WS" bin\testclient.exe -addr 127.0.0.1:8083 -ws -bots 10 -scenario rawping -duration 10s

echo.
echo All processes started in separate windows.
echo Press any key to stop all services and close this window...

REM 等待用户中断
pause >nul

REM 清理
echo Stopping services...
taskkill /F /IM registry-go.exe >nul 2>&1
taskkill /F /IM biz-go.exe >nul 2>&1
taskkill /F /IM gateway-cpp.exe >nul 2>&1
taskkill /F /IM testclient.exe >nul 2>&1

echo All services stopped.
