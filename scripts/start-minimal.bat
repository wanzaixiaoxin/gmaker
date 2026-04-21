@echo off
cd /d "%~dp0.."
echo ==========================================
echo   Start Minimal Link (Phase 1)
echo ==========================================
echo.
echo This will start: Registry + Biz + Gateway
echo.
pause

echo [1/3] Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory ^& pause"
timeout /t 1 /nobreak >nul

echo [2/3] Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -metrics :9082 ^& pause"
timeout /t 1 /nobreak >nul

echo [3/3] Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 ^& pause"
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
echo Run tests: go run tests/phase1/main.go
echo.
pause
