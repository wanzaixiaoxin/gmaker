@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo ==========================================
echo   Start Full Link (Phase 2)
echo ==========================================
echo.
echo This will start: Registry + DBProxy + Biz + Gateway
echo.
echo Prerequisites: MySQL on 3306, Redis on 6379
echo.
pause

echo [1/4] Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory -log-file logs\registry_%LOG_TS%.log -log-level info ^& pause"
timeout /t 1 /nobreak >nul

echo [2/4] Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -listen 127.0.0.1:3307 -redis 127.0.0.1:6379 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker -log-file logs\dbproxy_%LOG_TS%.log -log-level info ^& pause"
timeout /t 2 /nobreak >nul

echo [3/4] Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -dbproxy 127.0.0.1:3307 -metrics :9082 -log-file logs\biz_%LOG_TS%.log -log-level info ^& pause"
timeout /t 2 /nobreak >nul

echo [4/4] Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 --log-file logs\gateway_%LOG_TS%.log --log-level info ^& pause"
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
echo Logs     : logs\
echo.
echo Run tests: go run tests/phase2/main.go
echo.
pause
