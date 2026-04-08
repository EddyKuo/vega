@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1

:: Usage: build.bat [debug|release|clean|package] [upx]
::   build.bat              -> debug build
::   build.bat release      -> release build (no compression)
::   build.bat release upx  -> release build + UPX compression
::   build.bat clean        -> delete all build output
::   build.bat package      -> create dist/ folder with runtime files only

if "%1"=="clean" (
    echo Cleaning build output...
    rd /s /q out\build\windows-x64-debug 2>nul
    rd /s /q out\build\windows-x64-release 2>nul
    rd /s /q dist 2>nul
    echo Cleaning user data...
    del /q "%APPDATA%\Vega\settings.json" 2>nul
    rd /s /q "%APPDATA%\Vega" 2>nul
    echo Clean done. All build output, databases, and settings removed.
    exit /b 0
)

if "%1"=="package" (
    echo Packaging release build...
    if not exist out\build\windows-x64-release\src\vega.exe (
        echo ERROR: Release build not found. Run 'build.bat release' first.
        exit /b 1
    )
    rd /s /q dist 2>nul
    mkdir dist\vega
    copy out\build\windows-x64-release\src\vega.exe dist\vega\
    copy out\build\windows-x64-release\src\*.dll dist\vega\
    xcopy /s /i out\build\windows-x64-release\src\shaders dist\vega\shaders
    copy LICENSE dist\vega\
    copy README.md dist\vega\
    echo.
    echo Package created in dist\vega\
    dir dist\vega\ /s | findstr "File(s)"
    exit /b 0
)

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
if "%BUILD_TYPE%"=="upx" set BUILD_TYPE=debug

set DO_UPX=0
if "%2"=="upx" set DO_UPX=1
if "%1"=="upx" set DO_UPX=1

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

if "%DO_UPX%"=="1" (
    if "%BUILD_TYPE%"=="release" (
        where upx >nul 2>&1
        if !errorlevel! equ 0 (
            echo [5/5] Compressing with UPX...
            for %%f in (out\build\windows-x64-release\src\vega.exe out\build\windows-x64-release\src\*.dll) do (
                upx -t "%%f" >nul 2>&1
                if !errorlevel! neq 0 (
                    upx --best --lzma "%%f" 2>nul
                ) else (
                    echo     %%~nxf already packed, skipping
                )
            )
        ) else (
            echo [5/5] UPX not found, skipping compression
        )
    ) else (
        echo UPX compression only available for release builds
    )
)

echo BUILD SUCCESSFUL
