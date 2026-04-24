@echo off
cd /d "%~dp0.."
call scripts\_init-logs.bat
echo Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -config conf\biz.json ^& pause"
