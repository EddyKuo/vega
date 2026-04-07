@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1

echo [1/4] Setting up MSVC environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if %errorlevel% neq 0 (
    echo ERROR: vcvars64.bat failed
    exit /b 1
)

set VCPKG_ROOT=C:\vcpkg
set "PATH=C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%LOCALAPPDATA%\Microsoft\WinGet\Packages\UPX.UPX_Microsoft.Winget.Source_8wekyb3d8bbwe\upx-5.1.1-win64;%PATH%"

echo [2/4] Verifying tools...
where cmake >nul 2>&1 || (echo ERROR: cmake not found & exit /b 1)
where ninja >nul 2>&1 || (echo ERROR: ninja not found & exit /b 1)
where cl >nul 2>&1 || (echo ERROR: cl.exe not found & exit /b 1)
echo cmake: OK, ninja: OK, cl: OK

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

if "%BUILD_TYPE%"=="release" (
    echo [3/4] CMake configure [RELEASE]...
    cd /d D:\code\vega
    cmake --preset windows-x64-release
) else (
    echo [3/4] CMake configure [DEBUG]...
    cd /d D:\code\vega
    cmake --preset windows-x64-debug
)
if %errorlevel% neq 0 (
    echo ERROR: CMake configure failed
    exit /b %errorlevel%
)

echo [4/4] Building...
if "%BUILD_TYPE%"=="release" (
    cmake --build out/build/windows-x64-release
) else (
    cmake --build out/build/windows-x64-debug
)
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    exit /b %errorlevel%
)

if "%BUILD_TYPE%"=="release" (
    where upx >nul 2>&1
    if !errorlevel! equ 0 (
        echo [5/5] Compressing with UPX...
        upx --best --lzma out\build\windows-x64-release\src\vega.exe 2>nul
    ) else (
        echo [5/5] UPX not found, skipping compression
    )
)

echo BUILD SUCCESSFUL
