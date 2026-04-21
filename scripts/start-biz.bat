@echo off
cd /d "%~dp0.."
if not exist logs mkdir logs
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set LOG_TS=%%a
echo Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -metrics :9082 -log-file logs\biz_%LOG_TS%.log -log-level info ^& pause"
