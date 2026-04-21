@echo off
cd /d "%~dp0.."
echo Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379 -metrics :9082 ^& pause"
