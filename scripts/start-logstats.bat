@echo off
cd /d "%~dp0.."
echo Starting LogStats ...
start "LogStats" cmd /c "bin\logstats-go.exe -listen 127.0.0.1:8085 -http :8086 ^& pause"
