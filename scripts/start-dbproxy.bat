@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -config dbproxy.json -listen 127.0.0.1:3307 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker -log-file logs\dbproxy_%LOG_TS%.log -log-level info ^& pause"
