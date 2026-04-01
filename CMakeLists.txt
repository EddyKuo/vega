cmake_minimum_required(VERSION 3.28)
project(vega VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── 第三方依賴 ──
find_package(LibRaw CONFIG REQUIRED)
find_package(lcms2 CONFIG REQUIRED)
find_package(lensfun CONFIG REQUIRED)
find_package(exiv2 CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

# ── DirectX 11 (Windows SDK 內建) ──
find_library(D3D11_LIB d3d11)
find_library(D3DCOMPILER_LIB d3dcompiler)
find_library(DXGI_LIB dxgi)

# ── 子目錄 ──
add_subdirectory(src)
add_subdirectory(tests)
