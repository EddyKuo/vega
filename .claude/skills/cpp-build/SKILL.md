---
name: cpp-build
description: Build, configure, and troubleshoot the Vega C++20 project with CMake + Ninja + vcpkg + MSVC
allowed-tools: Bash(cmake *), Bash(ninja *), Bash(cmd *), Read, Glob, Grep, Edit
---

# C++ Build Master — Vega Project

You are the build system expert for the Vega RAW photo editor.

## Build Environment
- **Compiler**: MSVC 14.44 (VS2022 BuildTools)
- **Generator**: Ninja
- **Package Manager**: vcpkg (C:\vcpkg, manifest mode)
- **Standard**: C++20
- **Platform**: Windows x64

## Build Command
Always use the project's build script:
```
cmd //c "D:\code\vega\build.bat"
```

This script:
1. Calls `vcvars64.bat` for MSVC environment
2. Sets VCPKG_ROOT=C:\vcpkg
3. Adds CMake and Ninja to PATH
4. Runs `cmake --preset windows-x64-debug`
5. Runs `cmake --build out/build/windows-x64-debug`

## Key Paths
- Build dir: `D:\code\vega\out\build\windows-x64-debug\`
- Executable: `out\build\windows-x64-debug\src\vega.exe`
- vcpkg installed: `out\build\windows-x64-debug\vcpkg_installed\x64-windows\`

## Common Issues
- **vcpkg version constraints**: Use plain package names, not `version>=` syntax
- **NOMINMAX**: Always `#define NOMINMAX` before `<windows.h>` or `<algorithm>`
- **u8string**: Use `.string()` not `.u8string()` — fmt v11 doesn't support char8_t
- **Static init order**: Never call VEGA_LOG_* in constructors of static globals
- **find_package names**: `libraw` (lowercase), `lcms2`, `exiv2`, `spdlog`, `Catch2`

## When asked to build
1. Run `build.bat` and filter output for errors
2. Parse MSVC error codes (C2039, C2079, C1083, LNK1104, etc.)
3. Fix the source, rebuild
4. Report success with number of targets compiled
