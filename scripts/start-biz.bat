@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -metrics :9082 -log-file logs\biz_%LOG_TS%.log -log-level info ^& pause"
