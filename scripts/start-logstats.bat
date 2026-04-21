@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting LogStats ...
start "LogStats" cmd /c "bin\logstats-go.exe -listen 127.0.0.1:8085 -http :8086 -log-file logs\logstats_%LOG_TS%.log -log-level info ^& pause"
