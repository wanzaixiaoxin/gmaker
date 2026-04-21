@echo off
cd /d "%~dp0.."
if not exist logs mkdir logs
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set LOG_TS=%%a
echo Starting LogStats ...
start "LogStats" cmd /c "bin\logstats-go.exe -listen 127.0.0.1:8085 -http :8086 -log-file logs\logstats_%LOG_TS%.log -log-level info ^& pause"
