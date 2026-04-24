@echo off
REM ============================================
REM Chat Service Integration Test Script
REM ============================================
REM 使用 tools/testclient 进行聊天服务测试
REM ============================================

echo === Chat Service Integration Test ===
echo Test client: tools/testclient
echo Redis: 192.168.0.85:6379
echo MySQL: Not required (using memory fallback)
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
echo [1/3] Starting Registry...
start /B "%BIN_DIR%\registry-go.exe" -listen 127.0.0.1:2379 -store memory
timeout /t 1 /nobreak >nul

REM 启动 Chat (不使用 dbproxy，使用 Redis 和内存回退)
echo [2/3] Starting Chat Service...
start /B "%BIN_DIR%\chat-go.exe" -config chat.json -listen :8086 -redis %REDIS_ADDR% -node-id 1
timeout /t 2 /nobreak >nul

REM 启动 Gateway
echo [3/3] Starting Gateway...
start /B "%BUILD_DIR%\gateway-cpp.exe" 8081 127.0.0.1:2379 127.0.0.1:8082
timeout /t 2 /nobreak >nul

REM 运行测试
echo.
echo === Running Chat Test ===
echo Test config: 3 bots, chat scenario, 30s duration
echo.
"%BIN_DIR%\testclient.exe" -addr 127.0.0.1:8081 -bots 3 -scenario chat -duration 30s

echo.
echo === Test Complete ===
echo Press any key to stop all services...

REM 等待用户中断
pause >nul

REM 清理
echo Stopping services...
taskkill /F /IM registry-go.exe >nul 2>&1
taskkill /F /IM chat-go.exe >nul 2>&1
taskkill /F /IM gateway-cpp.exe >nul 2>&1

echo All services stopped.
