@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0.."

call scripts\_init-logs.bat

set "CMD=%~1"
if "%CMD%"=="" goto :help

if /I "%CMD%"=="registry" goto :start_registry
if /I "%CMD%"=="dbproxy" goto :start_dbproxy
if /I "%CMD%"=="biz" goto :start_biz
if /I "%CMD%"=="gateway" goto :start_gateway
if /I "%CMD%"=="logstats" goto :start_logstats
if /I "%CMD%"=="testclient" goto :start_testclient
if /I "%CMD%"=="minimal" goto :start_minimal
if /I "%CMD%"=="full" goto :start_full
if /I "%CMD%"=="all" goto :start_all

echo Unknown command: %CMD%
goto :help

:help
echo ==========================================
echo   gmaker Service Starter
echo ==========================================
echo.
echo Usage: scripts\start.bat ^<command^>
echo.
echo Commands:
echo   registry    Start Registry (memory mode)
echo   dbproxy     Start DBProxy (requires MySQL)
echo   biz         Start Biz
echo   gateway     Start Gateway
echo   logstats    Start LogStats
echo   testclient  Start TestClient (heartbeat daemon)
echo   minimal     Start minimal link: Registry + Biz + Gateway
echo   full        Start full link: Registry + DBProxy + Biz + Gateway
echo   all         Start all services
echo.
echo Examples:
echo   scripts\start.bat registry
echo   scripts\start.bat full
echo.
pause
goto :eof

:start_registry
echo Starting Registry (memory mode) ...
start "Registry" cmd /c "bin\registry-go.exe -listen 127.0.0.1:2379 -store memory -log-file logs\registry_%LOG_TS%.log -log-level info ^& pause"
goto :eof

:start_dbproxy
echo Starting DBProxy ...
start "DBProxy" cmd /c "bin\dbproxy-go.exe -config conf\dbproxy.json -mysql root:123456@tcp(127.0.0.1:3306)/gmaker ^& pause"
goto :eof

:start_biz
echo Starting Biz ...
start "Biz" cmd /c "bin\biz-go.exe -config conf\biz.json ^& pause"
goto :eof

:start_gateway
echo Starting Gateway ...
start "Gateway" cmd /c "bin\gateway-cpp.exe --config conf\gateway.json --log-file logs\gateway_%LOG_TS%.log --log-level info ^& pause"
goto :eof

:start_logstats
echo Starting LogStats ...
start "LogStats" cmd /c "bin\logstats-go.exe -listen 127.0.0.1:8085 -http :8086 -log-file logs\logstats_%LOG_TS%.log -log-level info ^& pause"
goto :eof

:start_testclient
echo Starting TestClient (heartbeat daemon) ...
start "TestClient" cmd /c "bin\testclient.exe -addr 127.0.0.1:8081 -bots 1 -scenario heartbeat -duration 0 -rate 0 -interval 5s -log-file logs\testclient_%LOG_TS%.log ^& pause"
goto :eof

:start_minimal
echo ==========================================
echo   Start Minimal Link (Phase 1)
echo ==========================================
echo.
call :start_registry
timeout /t 2 /nobreak >nul
call :start_biz
timeout /t 2 /nobreak >nul
call :start_gateway
timeout /t 2 /nobreak >nul
echo.
echo Minimal link started: Registry + Biz + Gateway
echo.
goto :eof

:start_full
echo ==========================================
echo   Start Full Link (Phase 2)
echo ==========================================
echo.
echo Prerequisites: MySQL on 3306, Redis on 6379
echo.
call :start_registry
timeout /t 2 /nobreak >nul
call :start_dbproxy
timeout /t 3 /nobreak >nul
call :start_biz
timeout /t 3 /nobreak >nul
call :start_gateway
timeout /t 2 /nobreak >nul
echo.
echo Full link started: Registry + DBProxy + Biz + Gateway
echo.
goto :eof

:start_all
echo ==========================================
echo   Start All Services
echo ==========================================
echo.
echo Prerequisites: MySQL on 3306, Redis on 6379
echo.
call :start_registry
timeout /t 2 /nobreak >nul
call :start_dbproxy
timeout /t 3 /nobreak >nul
call :start_biz
timeout /t 3 /nobreak >nul
call :start_gateway
timeout /t 2 /nobreak >nul
call :start_logstats
timeout /t 2 /nobreak >nul
echo.
echo All services started.
echo.
goto :eof
