@echo off
cd /d "%~dp0.."
echo Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe 8081 127.0.0.1:2379 127.0.0.1:8082 ^& pause"
