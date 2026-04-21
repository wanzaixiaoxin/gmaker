@echo off
cd /d "%~dp0.."
if not exist logs mkdir logs
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set LOG_TS=%%a
echo Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -listen 127.0.0.1:3307 -redis 127.0.0.1:6379 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker -log-file logs\dbproxy_%LOG_TS%.log -log-level info ^& pause"
