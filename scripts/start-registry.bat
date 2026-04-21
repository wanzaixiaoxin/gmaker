@echo off
cd /d "%~dp0.."
echo Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory ^& pause"
