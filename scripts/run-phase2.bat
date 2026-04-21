@echo off
cd /d "%~dp0.."
echo ==========================================
echo   Run Phase 2 Integration Test
echo ==========================================
echo.
echo Prerequisites: MySQL on 3306, Redis on 6379
echo.
pause

echo Building services ...
call build.bat
if errorlevel 1 (
    echo Build failed, aborting test.
    pause
    exit /b 1
)

echo.
echo Running Phase 2 test ...
go run tests/phase2/main.go
