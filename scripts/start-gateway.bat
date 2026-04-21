@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 --log-file logs\gateway_%LOG_TS%.log --log-level info ^& pause"
