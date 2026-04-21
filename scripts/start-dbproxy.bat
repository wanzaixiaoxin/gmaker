@echo off
cd /d "%~dp0.."
echo Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -listen 127.0.0.1:3307 -redis 127.0.0.1:6379 -mysql root:123456@tcp(127.0.0.1:3306)/gmaker ^& pause"
