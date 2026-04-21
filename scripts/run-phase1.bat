@echo off
cd /d "%~dp0.."
echo ==========================================
echo   Run Phase 1 Integration Test
echo ==========================================
echo.
echo Building services ...
call build.bat
if errorlevel 1 (
    echo Build failed, aborting test.
    pause
    exit /b 1
)

echo.
echo Running Phase 1 test ...
go run tests/phase1/main.go
