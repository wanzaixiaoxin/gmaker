@echo off
cd /d "%~dp0.."
echo ==========================================
echo   Start Minimal Link (Phase 1)
echo ==========================================
echo.
echo This will start: Registry + Biz + Gateway + TestClient
echo.
pause

echo [1/4] Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory -log-level info ^& pause"
timeout /t 2 /nobreak >nul

echo [2/4] Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -config biz.json -listen 127.0.0.1:8082 -metrics :9082 -log-level info ^& pause"
timeout /t 2 /nobreak >nul

echo [3/4] Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe --config gateway.json ^& pause"
timeout /t 2 /nobreak >nul

echo [4/4] Starting TestClient (heartbeat daemon) ...
start "TestClient" cmd /c "bin\testclient.exe -addr 127.0.0.1:8081 -bots 1 -scenario heartbeat -duration 0 -interval 5s -output text ^& pause"
timeout /t 1 /nobreak >nul

echo.
echo ==========================================
echo   Minimal link started!
echo ==========================================
echo.
echo Registry : 127.0.0.1:2379
echo Biz      : 127.0.0.1:8082
echo Gateway  : 127.0.0.1:8081
echo.
echo Run scripts\stop-all.bat to stop all services.
echo.
pause
