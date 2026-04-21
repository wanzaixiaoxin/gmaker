@echo off
cd /d "%~dp0.."
if not exist logs mkdir logs
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set LOG_TS=%%a
echo Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 --log-file logs\gateway_%LOG_TS%.log --log-level info ^& pause"
