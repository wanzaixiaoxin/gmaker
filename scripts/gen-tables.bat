@echo off
chcp 65001 >nul 2>nul

cd /d "%~dp0.."

echo ========================================
echo  gen-tables: Excel -^> SQL + Proto
echo ========================================
echo.

:: --- Handle special modes ---

if "%~1"=="--init" (
    if "%~2"=="" (
        echo Usage: gen-tables --init ^<table_name^>
        exit /b 1
    )
    echo Creating template for: %~2
    pushd tools\xlsx2all
    go run . --init "%~2" --dir ..\..\tables
    popd
    exit /b 0
)

if "%~1"=="--demo" (
    echo Creating demo Excel files...
    pushd tools\xlsx2all
    go run . --demo --dir ..\..\tables
    popd
    exit /b 0
)

if "%~1"=="--clean" (
    echo Removing generated SQL and Proto files...
    pushd tools\xlsx2all
    go run . --clean --dir ..\..\tables --sql-out ..\..\sql --proto-out ..\..\spec\proto
    popd
    exit /b 0
)

:: --- Step 1: Build xlsx2all ---

echo [1/2] Building xlsx2all tool...

if not exist "tools\xlsx2all\xlsx2all.exe" (
    pushd tools\xlsx2all
    go build -o xlsx2all.exe .
    if errorlevel 1 (
        echo ERROR: Failed to build xlsx2all
        popd
        exit /b 1
    )
    popd
    echo   Built: tools\xlsx2all\xlsx2all.exe
) else (
    echo   Already built: tools\xlsx2all\xlsx2all.exe
)

:: --- Step 2: Excel -> SQL + Proto ---

echo.
echo [2/2] Generating SQL and Proto from Excel...

tools\xlsx2all\xlsx2all.exe --dir tables --sql-out sql --proto-out spec/proto --module github.com/gmaker/luffa --db gmaker
if errorlevel 1 (
    echo ERROR: xlsx2all failed
    exit /b 1
)

echo.
echo ========================================
echo  Done!
echo    SQL:   sql\*.sql
echo    Proto: spec\proto\*.proto
echo.
echo  Next: run gen-proto to compile Proto to Go/C++
echo ========================================
