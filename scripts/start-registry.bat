@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory -log-file logs\registry_%LOG_TS%.log -log-level info ^& pause"
