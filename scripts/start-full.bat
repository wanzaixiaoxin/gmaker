@echo off
cd /d "%~dp0.."
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
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory ^& pause"
timeout /t 1 /nobreak >nul

echo [2/4] Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -listen 127.0.0.1:3307 -redis 127.0.0.1:6379 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker ^& pause"
timeout /t 2 /nobreak >nul

echo [3/4] Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -dbproxy 127.0.0.1:3307 -metrics :9082 ^& pause"
timeout /t 2 /nobreak >nul

echo [4/4] Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 ^& pause"
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
echo Run tests: go run tests/phase2/main.go
echo.
pause
