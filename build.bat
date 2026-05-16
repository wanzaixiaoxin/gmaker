@echo off
setlocal EnableDelayedExpansion

:: ============================================================
::  gmaker Build Script (Optimized)
::
::  Usage:
::    build              Build all (Go + C++)
::    build --go         Go services only
::    build --cpp        C++ services only
::    build --clean      Clean all build artifacts
::    build --tidy       Run go mod tidy before building
::    build --verbose    Show full build output
:: ============================================================

cd /d "%~dp0"

:: --- Parse arguments ---
set "BUILD_GO=1"
set "BUILD_CPP=1"
set "RUN_TIDY=0"
set "VERBOSE=0"
set "CLEAN=0"

:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="--go"    set "BUILD_CPP=0" & shift & goto :parse_args
if /i "%~1"=="--cpp"   set "BUILD_GO=0"  & shift & goto :parse_args
if /i "%~1"=="--tidy"  set "RUN_TIDY=1"  & shift & goto :parse_args
if /i "%~1"=="--verbose"  set "VERBOSE=1"  & shift & goto :parse_args
if /i "%~1"=="--clean" (
    set "CLEAN=1"
    shift
    goto :parse_args
)
shift
goto :parse_args
:done_args

:: --- Clean mode ---
if "%CLEAN%"=="1" (
    echo Cleaning build artifacts...
    if exist bin   (echo   x bin\   & rmdir /s /q bin)
    if exist build (echo   x build\ & rmdir /s /q build)
    echo Done.
    exit /b 0
)

echo ==========================================
echo   gmaker Build Script
echo ==========================================
echo.

where go >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Go not found. Install Go 1.22+ and add to PATH.
    exit /b 1
)

:: Set proxy once (quiet)
go env -w GOPROXY=https://goproxy.cn,direct >nul 2>nul
go env -w GOSUMDB=sum.golang.google.cn >nul 2>nul

if not exist bin mkdir bin

set "BUILD_ERRORS=0"
set "START_TIME=%time%"

:: ============================================================
::  Phase 1: Go Services (parallel)
:: ============================================================

if "%BUILD_GO%"=="0" goto :cpp_phase

echo [Phase 1/2] Building Go services ...
echo ------------------------------------------

set GO_SERVICES=registry-go dbproxy-go login-go biz-go chat-go logstats-go bot-go config-go match-go
set GO_TOOLS=testclient

:: Detect CPU count for parallel builds
set /a PARALLEL_JOBS=0
for /f "tokens=*" %%c in ('wmic cpu get NumberOfLogicalProcessors /value 2^>nul ^| findstr "="') do set /a PARALLEL_JOBS=%%c
if %PARALLEL_JOBS% lss 1 set "PARALLEL_JOBS=2"
if %PARALLEL_JOBS% gtr 8 set "PARALLEL_JOBS=8"
echo   Parallel jobs: %PARALLEL_JOBS%

:: --- Tidy if requested ---
if "%RUN_TIDY%"=="1" (
    echo   Running go mod tidy ...
    for %%s in (%GO_SERVICES%) do (
        if exist "services\%%s\go.mod" (
            pushd "services\%%s"
            go mod tidy
            popd
        )
    )
    go mod tidy 2>nul
    echo.
)

:: --- Build Go services in parallel via background processes ---
set "GO_BUILD_COUNT=0"
set "GO_FAIL_COUNT=0"

:: Create temp dir for build status files
set "BUILD_TMP=%TEMP%\gmaker-build-%RANDOM%"
mkdir "%BUILD_TMP%" 2>nul

:: Launch all Go builds as background processes
for %%s in (%GO_SERVICES%) do (
    set /a GO_BUILD_COUNT+=1
    if exist "services\%%s\go.mod" (
        start /b "" cmd /c "pushd services\%%s && go build -o ..\..\bin\%%s.exe . > "%BUILD_TMP%\%%s.log" 2>&1 && echo OK > "%BUILD_TMP%\%%s.ok" || echo FAIL > "%BUILD_TMP%\%%s.ok" & popd"
    ) else (
        start /b "" cmd /c "go build -o bin\%%s.exe ./services/%%s > "%BUILD_TMP%\%%s.log" 2>&1 && echo OK > "%BUILD_TMP%\%%s.ok" || echo FAIL > "%BUILD_TMP%\%%s.ok""
    )
)
for %%t in (%GO_TOOLS%) do (
    set /a GO_BUILD_COUNT+=1
    start /b "" cmd /c "go build -o bin\%%t.exe ./tools/%%t > "%BUILD_TMP%\%%t.log" 2>&1 && echo OK > "%BUILD_TMP%\%%t.ok" || echo FAIL > "%BUILD_TMP%\%%t.ok""
)

:: Wait for all background builds to complete
echo   Compiling %GO_BUILD_COUNT% targets in parallel...
timeout /t 1 /nobreak >nul

:wait_go
set "ALL_DONE=1"
for %%s in (%GO_SERVICES%) do (
    if not exist "%BUILD_TMP%\%%s.ok" set "ALL_DONE=0"
)
for %%t in (%GO_TOOLS%) do (
    if not exist "%BUILD_TMP%\%%t.ok" set "ALL_DONE=0"
)
if "%ALL_DONE%"=="0" (
    timeout /t 1 /nobreak >nul
    goto :wait_go
)

:: Collect results
for %%s in (%GO_SERVICES%) do (
    set /p STATUS=<"%BUILD_TMP%\%%s.ok" 2>nul
    if "!STATUS!"=="OK" (
        echo   [OK]   %%s.exe
    ) else (
        echo   [FAIL] %%s
        set "BUILD_ERRORS=1"
        set /a GO_FAIL_COUNT+=1
        type "%BUILD_TMP%\%%s.log"
    )
)
for %%t in (%GO_TOOLS%) do (
    set /p STATUS=<"%BUILD_TMP%\%%t.ok" 2>nul
    if "!STATUS!"=="OK" (
        echo   [OK]   %%t.exe
    ) else (
        echo   [FAIL] %%t
        set "BUILD_ERRORS=1"
        set /a GO_FAIL_COUNT+=1
        type "%BUILD_TMP%\%%t.log"
    )
)

:: Cleanup temp
rmdir /s /q "%BUILD_TMP%" 2>nul

echo.

:: ============================================================
::  Phase 2: C++ Services (parallel cmake --build)
:: ============================================================

:cpp_phase
if "%BUILD_CPP%"=="0" goto :summary

echo [Phase 2/2] Building C++ services ...
echo ------------------------------------------

where cmake >nul 2>nul
if errorlevel 1 (
    echo   [SKIP] CMake not found. C++ services skipped.
    goto :summary
)

set "PROTOBUF_READY=0"
if exist "3rd\protobuf\protobuf-34.1\build\Release\protoc.exe" set "PROTOBUF_READY=1"
where protoc >nul 2>nul && set "PROTOBUF_READY=1"

if "%PROTOBUF_READY%"=="0" (
    echo   [SKIP] Protobuf C++ library not found.
    goto :summary
)

if not exist build mkdir build
pushd build

:: Only re-run cmake configure if CMakeCache.txt doesn't exist (incremental)
if not exist CMakeCache.txt (
    echo   Running CMake configure ...
    cmake .. -DCMAKE_BUILD_TYPE=Release >cmake.log 2>&1
    if errorlevel 1 (
        echo   [FAIL] CMake configure failed. See build\cmake.log
        type cmake.log
        popd
        set "BUILD_ERRORS=1"
        goto :summary
    )
)

:: Parallel C++ build with -j
echo   Building with -j%PARALLEL_JOBS% ...
cmake --build . --config Release -j %PARALLEL_JOBS% >build.log 2>&1
if errorlevel 1 (
    echo   [FAIL] C++ build failed. See build\build.log
    type build.log
    popd
    set "BUILD_ERRORS=1"
    goto :summary
)

popd

if exist "build\Release\gateway-cpp.exe" (
    copy /y "build\Release\gateway-cpp.exe" "bin\" >nul
    echo   [OK]   gateway-cpp.exe
) else (
    echo   [WARN] gateway-cpp.exe not found
)

if exist "build\Release\realtime-cpp.exe" (
    copy /y "build\Release\realtime-cpp.exe" "bin\" >nul
    echo   [OK]   realtime-cpp.exe
) else (
    echo   [WARN] realtime-cpp.exe not found
)

if exist "build\Release\test-crypto.exe" (
    copy /y "build\Release\test-crypto.exe" "bin\" >nul
    echo   [OK]   test-crypto.exe
)

echo.

:: ============================================================
::  Summary
:: ============================================================

:summary
:: Calculate elapsed time
set "END_TIME=%time%"

echo ==========================================
if "%BUILD_ERRORS%"=="0" (
    echo   Build SUCCEEDED
) else (
    echo   Build COMPLETED with ERRORS
)
echo   Started:  %START_TIME%
echo   Finished: %END_TIME%
echo ==========================================
echo   Output: %CD%\bin
echo.

if "%BUILD_GO%"=="1" (
    echo   Go binaries:
    for %%s in (%GO_SERVICES%) do (
        if exist "bin\%%s.exe" echo     [OK]   %%s.exe
    )
    for %%t in (%GO_TOOLS%) do (
        if exist "bin\%%t.exe" echo     [OK]   %%t.exe
    )
    echo.
)

if "%BUILD_CPP%"=="1" (
    echo   C++ binaries:
    if exist "bin\gateway-cpp.exe"   echo     [OK]   gateway-cpp.exe
    if exist "bin\realtime-cpp.exe"  echo     [OK]   realtime-cpp.exe
    if not exist "bin\gateway-cpp.exe" (
        if not exist "bin\realtime-cpp.exe" (
            echo     (none - needs protobuf + CMake^)
        )
    )
    echo.
)

endlocal
