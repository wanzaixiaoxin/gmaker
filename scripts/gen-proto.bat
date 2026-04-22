@echo off
:: Generate protobuf code for Go and C++

cd /d "%~dp0.."

echo ========================================
echo  Protobuf Code Generation
echo ========================================
echo.

echo [1/5] Checking tools...
protoc --version 2>nul
if %errorlevel% neq 0 (
    echo ERROR: protoc not found in PATH
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

protoc --proto_path=%PROTO_DIR% --go_out=. --go_opt=module=github.com/gmaker/luffa --go-grpc_out=. --go-grpc_opt=module=github.com/gmaker/luffa --cpp_out=%GEN_CPP_DIR% %PROTO_DIR%\*.proto

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
