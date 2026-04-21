@echo off
cd /d "%~dp0.."
echo ==========================================
echo   Start Full Link (Phase 2)
echo ==========================================
echo.
echo This will start: Registry + DBProxy + Biz + Gateway + TestClient
echo.
echo Prerequisites: MySQL on 3306, Redis on 6379
echo.
pause

echo [1/5] Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory -log-level info ^& pause"
timeout /t 2 /nobreak >nul

echo [2/5] Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -listen 127.0.0.1:3307 -redis 127.0.0.1:6379 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker -log-level info ^& pause"
timeout /t 3 /nobreak >nul

echo [3/5] Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -dbproxy 127.0.0.1:3307 -metrics :9082 -log-level info ^& pause"
timeout /t 3 /nobreak >nul

echo [4/5] Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 --log-level info ^& pause"
timeout /t 2 /nobreak >nul

echo [5/5] Starting TestClient (heartbeat daemon) ...
start "TestClient" cmd /c "bin\testclient.exe -addr 127.0.0.1:8081 -bots 1 -scenario heartbeat -duration 0 -interval 5s -output text ^& pause"
timeout /t 1 /nobreak >nul

echo.
echo ==========================================
echo   Full link started!
echo ==========================================
echo.
echo Registry : 127.0.0.1:2379
echo DBProxy  : 127.0.0.1:3307
echo Biz      : 127.0.0.1:8082
echo Gateway  : 127.0.0.1:8081
echo.
echo Run scripts\stop-all.bat to stop all services.
echo.
pause
