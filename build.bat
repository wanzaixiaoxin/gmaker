@echo off
setlocal EnableDelayedExpansion

echo ==========================================
echo   gmaker Build Script
echo ==========================================
echo.

cd /d "%~dp0"

where go >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Go not found. Install Go 1.22+ and add to PATH.
    exit /b 1
)

for /f "tokens=*" %%a in ('go env GOPROXY') do set "CURRENT_PROXY=%%a"
echo [INFO] Current GOPROXY: %CURRENT_PROXY%
echo [INFO] Setting GOPROXY to https://goproxy.cn,direct
go env -w GOPROXY=https://goproxy.cn,direct
go env -w GOSUMDB=sum.golang.google.cn

if not exist bin mkdir bin

set "BUILD_ERRORS=0"

echo.
echo [Phase 1/2] Building Go services ...
echo ------------------------------------------

set GO_SERVICES=registry-go dbproxy-go login-go biz-go chat-go logstats-go bot-go
set GO_TOOLS=testclient

for %%s in (%GO_SERVICES%) do (
    echo   [GO] Building services/%%s ...
    if exist "services\%%s\go.mod" (
        pushd "services\%%s"
        go mod tidy >nul 2>nul
        go build -o "..\..\bin\%%s.exe" . 2>nul
        popd
    ) else (
        go build -o "bin\%%s.exe" "./services/%%s" 2>nul
    )
    if errorlevel 1 (
        echo   [FAIL] services/%%s
        set "BUILD_ERRORS=1"
    ) else (
        echo   [OK]   bin\%%s.exe
    )
)

for %%t in (%GO_TOOLS%) do (
    echo   [GO] Building tools/%%t ...
    go build -o "bin\%%t.exe" "./tools/%%t" 2>nul
    if errorlevel 1 (
        echo   [FAIL] tools/%%t
        set "BUILD_ERRORS=1"
    ) else (
        echo   [OK]   bin\%%t.exe
    )
)

echo.
echo [Phase 2/2] Building C++ services ...
echo ------------------------------------------

where cmake >nul 2>nul
if errorlevel 1 (
    echo   [SKIP] CMake not found. C++ services skipped.
    echo          Install CMake to build gateway-cpp and realtime-cpp.
    goto summary
)

:: 检查 protobuf C++ 库（优先本地预编译，其次系统安装）
set "PROTOBUF_READY=0"
if exist "3rd\protobuf\protobuf-34.1\build\Release\protoc.exe" set "PROTOBUF_READY=1"
where protoc >nul 2>nul && set "PROTOBUF_READY=1"

if "%PROTOBUF_READY%"=="0" (
    echo   [SKIP] Protobuf C++ library not found.
    echo.
    echo          To build C++ services, install protobuf first:
    echo            vcpkg install protobuf                      ^(recommended on Windows^)
    echo            sudo apt install libprotobuf-dev protobuf-compiler   ^(Ubuntu/Debian^)
    echo            brew install protobuf                       ^(macOS^)
    echo.
    echo          Or place a local prebuilt copy in:
    echo            3rd\protobuf\protobuf-34.1\
    echo.
    goto summary
)

if not exist build mkdir build
pushd build

cmake .. -DCMAKE_BUILD_TYPE=Release >cmake.log 2>&1
if errorlevel 1 (
    echo   [FAIL] CMake config failed. See build\cmake.log
    type cmake.log
    popd
    set "BUILD_ERRORS=1"
    goto summary
)

cmake --build . --config Release >build.log 2>&1
if errorlevel 1 (
    echo   [FAIL] C++ build failed. See build\build.log
    type build.log
    popd
    set "BUILD_ERRORS=1"
    goto summary
)

popd

if exist "build\Release\gateway-cpp.exe" (
    copy /y "build\Release\gateway-cpp.exe" "bin\" >nul
    echo   [OK]   bin\gateway-cpp.exe
) else (
    echo   [WARN] gateway-cpp.exe not found
)

if exist "build\Release\realtime-cpp.exe" (
    copy /y "build\Release\realtime-cpp.exe" "bin\" >nul
    echo   [OK]   bin\realtime-cpp.exe
) else (
    echo   [WARN] realtime-cpp.exe not found
)

if exist "build\Release\test-crypto.exe" (
    copy /y "build\Release\test-crypto.exe" "bin\" >nul
    echo   [OK]   bin\test-crypto.exe
)

:summary
echo.
echo ==========================================
if "%BUILD_ERRORS%"=="0" (
    echo   Build SUCCEEDED
) else (
    echo   Build COMPLETED with ERRORS
)
echo ==========================================
echo   Output: %CD%\bin
echo.
echo   Go binaries:
for %%s in (%GO_SERVICES%) do (
    if exist "bin\%%s.exe" echo     [OK]   %%s.exe
)
for %%t in (%GO_TOOLS%) do (
    if exist "bin\%%t.exe" echo     [OK]   %%t.exe
)
echo.
echo   C++ binaries:
if exist "bin\gateway-cpp.exe"   echo     [OK]   gateway-cpp.exe
if exist "bin\realtime-cpp.exe"  echo     [OK]   realtime-cpp.exe
if not exist "bin\gateway-cpp.exe" (
    if not exist "bin\realtime-cpp.exe" (
        echo     (none - needs protobuf + CMake)
    )
)
echo.

endlocal
