@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0.."

echo ==========================================
echo   gmaker Dependency Setup
echo ==========================================
echo.

:: ============ rapidjson ============
if exist "3rd\rapidjson\rapidjson.h" (
    echo [OK] rapidjson (local)
) else (
    echo [INFO] rapidjson will be fetched by CMake FetchContent automatically
)

:: ============ libuv ============
if exist "3rd\libuv\lib\win-x64\libuv.lib" (
    echo [OK] libuv (local prebuilt)
) else (
    echo [INFO] libuv will be fetched and built by CMake FetchContent automatically
)

:: ============ hiredis ============
if exist "3rd\hiredis\CMakeLists.txt" (
    echo [OK] hiredis (submodule)
) else (
    echo [WARN] hiredis submodule not initialized. Run: git submodule update --init
)

:: ============ protobuf ============
set "PROTOBUF_READY=0"
if exist "3rd\protobuf\protobuf-34.1\build\Release\protoc.exe" set "PROTOBUF_READY=1"
where protoc >nul 2>nul && set "PROTOBUF_READY=1"

if "%PROTOBUF_READY%"=="1" (
    echo [OK] protobuf
    goto :summary
)

echo [MISSING] protobuf C++ library not found.
echo.
echo Please install protobuf development files for your platform:
echo.
echo   Windows (vcpkg - recommended):
echo     vcpkg install protobuf
echo.
echo     IMPORTANT: When running build.bat, CMake must know where vcpkg is.
echo     Option 1: Set environment variable before build:
echo       set CMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
echo     Option 2: Pass directly to CMake:
echo       cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
echo.
echo   Ubuntu / Debian:
echo     sudo apt update
echo     sudo apt install libprotobuf-dev protobuf-compiler
echo.
echo   CentOS / RHEL / Fedora:
echo     sudo yum install protobuf-devel protobuf-compiler
echo     rem or: sudo dnf install protobuf-devel protobuf-compiler
echo.
echo   macOS:
echo     brew install protobuf
echo.
echo   Arch Linux:
echo     sudo pacman -S protobuf
echo.
echo   Or build locally (without package manager):
echo     1. Download: https://github.com/protocolbuffers/protobuf/releases/download/v34.1/protobuf-34.1.zip
echo     2. Extract to: 3rd\protobuf\
echo     3. cd 3rd\protobuf\protobuf-34.1
echo     4. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF
echo     5. cmake --build build --config Release
echo.
exit /b 1

:summary
echo.
echo ==========================================
echo   Dependency Setup Summary
echo ==========================================
if "%PROTOBUF_READY%"=="1" (echo [OK] protobuf) else (echo [MISSING] protobuf)
if exist "3rd\rapidjson\rapidjson.h" (echo [OK] rapidjson) else (echo [AUTO] rapidjson - CMake FetchContent)
if exist "3rd\libuv\lib\win-x64\libuv.lib" (echo [OK] libuv) else (echo [AUTO] libuv - CMake FetchContent)
if exist "3rd\hiredis\CMakeLists.txt" (echo [OK] hiredis) else (echo [MISSING] hiredis - git submodule update --init)
echo.
echo You can now run: build.bat
echo.
