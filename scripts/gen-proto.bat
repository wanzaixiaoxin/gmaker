@echo off
:: Generate protobuf code for Go and C++
:: Usage:
::   gen-proto          Compile all spec/proto/*.proto -> gen/go + gen/cpp
::   gen-proto --clean  Remove all generated Go/C++ protobuf files

cd /d "%~dp0.."

:: --- Clean mode ---

if "%~1"=="--clean" (
    echo Removing generated protobuf code...
    echo.
    set "CLEAN_COUNT=0"
    if exist "gen\go" (
        for /r gen\go %%f in (*.pb.go) do (
            echo   x %%f
            del /q "%%f"
            set /a CLEAN_COUNT+=1
        )
        for /r gen\go %%f in (*_grpc.pb.go) do (
            echo   x %%f
            del /q "%%f"
            set /a CLEAN_COUNT+=1
        )
        for /d %%d in (gen\go\*) do (
            dir /b "%%d" 2>nul | findstr "^" >nul || (
                rd "%%d"
                echo   x %%d\
                set /a CLEAN_COUNT+=1
            )
        )
    )
    if exist "gen\cpp" (
        for %%f in (gen\cpp\*.pb.cc gen\cpp\*.pb.h) do (
            echo   x %%f
            del /q "%%f"
            set /a CLEAN_COUNT+=1
        )
    )
    echo.
    echo Done.
    exit /b 0
)

:: --- Generate mode ---

echo ========================================
echo  Protobuf Code Generation
echo ========================================
echo.

echo [1/5] Checking tools...

:: Try system protoc first, then fall back to local prebuilt
set "PROTOC_CMD=protoc"
protoc --version 2>nul
if %errorlevel% neq 0 (
    if exist "3rd\protobuf\protobuf-34.1\build\Release\protoc.exe" (
        set "PROTOC_CMD=3rd\protobuf\protobuf-34.1\build\Release\protoc.exe"
    ) else (
        echo ERROR: protoc not found in PATH or in 3rd\protobuf\protobuf-34.1\build\Release\
        exit /b 1
    )
)

:: Ensure Go bin directory is in PATH for protoc plugins
for /f "tokens=*" %%a in ('go env GOPATH 2^>nul') do set "GOBIN=%%a\bin"
if exist "%GOBIN%" (
    set "PATH=%GOBIN%;%PATH%"
)

"%PROTOC_CMD%" --version 2>nul
if %errorlevel% neq 0 (
    echo ERROR: protoc failed to run
    exit /b 1
)

for /f "delims=" %%i in ('where protoc-gen-go 2^>nul') do echo   protoc-gen-go  : %%i
for /f "delims=" %%i in ('where protoc-gen-go-grpc 2^>nul') do echo   protoc-gen-go-grpc: %%i

echo.
echo [2/5] Configuration:
echo   Proto dir : spec\proto
echo   Go output : gen\go
echo   C++ output: gen\cpp
echo   Module    : github.com/gmaker/luffa

echo.
echo [3/5] Proto files to process:
for %%f in (spec\proto\*.proto) do (
    echo   - %%~nxf
)

echo.
echo [4/5] Running protoc...

set PROTO_DIR=spec\proto
set GEN_GO_DIR=gen\go
set GEN_CPP_DIR=gen\cpp

if not exist %GEN_GO_DIR% mkdir %GEN_GO_DIR%
if not exist %GEN_CPP_DIR% mkdir %GEN_CPP_DIR%

"%PROTOC_CMD%" --proto_path=%PROTO_DIR% --go_out=. --go_opt=module=github.com/gmaker/luffa --go-grpc_out=. --go-grpc_opt=module=github.com/gmaker/luffa --cpp_out=%GEN_CPP_DIR% %PROTO_DIR%\*.proto

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Protobuf generation failed, exit code %errorlevel%
    exit /b %errorlevel%
)

echo.
echo [5/5] Generated files:
echo   Go files:
for /r gen\go %%f in (*.pb.go) do (
    echo     %%f
)
echo   C++ files:
for %%f in (gen\cpp\*.pb.cc) do (
    echo     %%f
)

echo.
echo ========================================
echo  Protobuf generation done.
echo ========================================
