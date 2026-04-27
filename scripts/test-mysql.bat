@echo off
echo ========================================
echo  MySQL Connectivity Test
echo  Target: 192.168.0.85:3306
echo  User:   root
echo  DB:     gmaker
echo ========================================
echo.

mysql -h 192.168.0.85 -P 3306 -u root -p123456 -e "SELECT 'MySQL connection OK' AS status;" >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] MySQL connection successful!
    mysql -h 192.168.0.85 -P 3306 -u root -p123456 -e "SELECT 'MySQL connection OK' AS status;"
) else (
    echo [FAIL] MySQL connection failed.
    echo.
    echo Tried: mysql -h 192.168.0.85 -P 3306 -u root -p123456
    echo.
    echo If mysql client is not installed, you can test manually:
    echo   telnet 192.168.0.85 3306
    echo   Or use a MySQL GUI client.
)
echo.
pause
