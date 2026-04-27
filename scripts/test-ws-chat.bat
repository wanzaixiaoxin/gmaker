@echo off
REM ============================================
REM WebSocket Chat Service Integration Test
REM ============================================
REM 使用 WebSocket 模式运行 chat 场景压测
REM 所有服务在独立窗口运行，与 start.bat 风格一致
REM ============================================

echo === WebSocket Chat Integration Test ===
echo Redis: 192.168.0.85:6379
echo.

REM 设置路径
set BIN_DIR=.\bin
set BUILD_DIR=.\build\Release
set REDIS_ADDR=192.168.0.85:6379

REM 检查编译产物
if not exist "%BIN_DIR%\chat-go.exe" (
    echo [ERROR] chat-go.exe not found, please build first
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

REM 启动 Chat
echo [2/4] Starting Chat Service (new window) ...
start "Chat" bin\chat-go.exe -config conf\chat.json -listen :8086 -redis %REDIS_ADDR% -node-id 1
timeout /t 2 /nobreak >nul

REM 启动 Gateway
echo [3/4] Starting Gateway (new window) ...
start "Gateway" bin\gateway-cpp.exe --config conf\gateway.json
timeout /t 2 /nobreak >nul

REM 运行测试
echo [4/4] Starting TestClient (new window) ...
start "TestClient-WS" bin\testclient.exe -addr 127.0.0.1:8083 -ws -bots 3 -scenario chat -duration 30s

echo.
echo All processes started in separate windows.
echo Press any key to stop all services and close this window...

REM 等待用户中断
pause >nul

REM 清理
echo Stopping services...
taskkill /F /IM registry-go.exe >nul 2>&1
taskkill /F /IM chat-go.exe >nul 2>&1
taskkill /F /IM gateway-cpp.exe >nul 2>&1
taskkill /F /IM testclient.exe >nul 2>&1

echo All services stopped.
