@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting TestClient (heartbeat daemon) ...
start "TestClient" cmd /c "bin\testclient.exe -addr 127.0.0.1:8081 -bots 1 -scenario heartbeat -duration 0 -rate 0 -interval 5s -log-file logs\testclient_%LOG_TS%.log ^& pause"
