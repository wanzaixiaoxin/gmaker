@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe --config gateway.json --log-file logs\gateway_%LOG_TS%.log --log-level info ^& pause"
