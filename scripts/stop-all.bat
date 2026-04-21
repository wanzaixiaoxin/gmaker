@echo off
echo ==========================================
echo   gmaker Stop All Services
echo ==========================================
echo.

echo Stopping Go services ...
taskkill /F /IM registry-go.exe 2>nul
taskkill /F /IM biz-go.exe 2>nul
taskkill /F /IM dbproxy-go.exe 2>nul
taskkill /F /IM logstats-go.exe 2>nul

echo Stopping C++ services ...
taskkill /F /IM gateway-cpp.exe 2>nul
taskkill /F /IM realtime-cpp.exe 2>nul

echo Stopping test clients ...
taskkill /F /IM testclient.exe 2>nul

echo.
echo All services stopped.
pause
