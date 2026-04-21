@echo off
cd /d "%~dp0.."
if not exist logs mkdir logs
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set LOG_TS=%%a
echo Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory -log-file logs\registry_%LOG_TS%.log -log-level info ^& pause"
