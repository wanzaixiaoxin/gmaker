@echo off
echo ========================================
echo  Redis Connectivity Test
echo  Target: 192.168.0.85:6379
echo ========================================
echo.

redis-cli -h 192.168.0.85 -p 6379 PING >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Redis connection successful!
    redis-cli -h 192.168.0.85 -p 6379 PING
) else (
    echo [FAIL] Redis connection failed.
    echo.
    echo Tried: redis-cli -h 192.168.0.85 -p 6379 PING
    echo.
    echo If redis-cli is not installed, you can test manually:
    echo   telnet 192.168.0.85 6379
    echo   Or use a Redis GUI client.
)
echo.
pause
