# Vega — Windows C++ RAW Photo Editor

## Implementation Plan for Claude Code

> **目標**：使用 Visual Studio 2022 + C++20 + DirectX 11，在 Windows 平台上實作一個具備 Non-Destructive RAW Editing Pipeline 的專業照片編輯器。
>
> **專案代號**：Vega（天琴座 α 星 — 天文測光系統的零點參考星）
>
> **預計總工期**：24–30 週（6 個 Phase）

---

## Table of Contents

1. [Phase 0 — 專案基礎建設](#phase-0--專案基礎建設week-1-2)
2. [Phase 1 — RAW 解碼與基礎顯示](#phase-1--raw-解碼與基礎顯示week-3-6)
3. [Phase 2 — 核心影像處理 Pipeline](#phase-2--核心影像處理-pipelineweek-7-12)
4. [Phase 3 — GPU 加速與即時預覽](#phase-3--gpu-加速與即時預覽week-13-17)
5. [Phase 4 — 進階編輯功能](#phase-4--進階編輯功能week-18-22)
6. [Phase 5 — Catalog 與 DAM](#phase-5--catalog-與-damweek-23-27)
7. [Phase 6 — 打磨與發布準備](#phase-6--打磨與發布準備week-28-30)
8. [附錄](#附錄)

---

## Phase 0 — 專案基礎建設（Week 1-2）

### 目標

建立完整的 CMake + vcpkg 工具鏈，確保所有第三方依賴可自動安裝，Visual Studio 2022 可正確開啟並編譯。

### Step 0.1 — 建立專案目錄結構

```
vega/
├── CMakeLists.txt                    # 頂層 CMake
├── CMakePresets.json                 # VS 2022 preset 設定
├── vcpkg.json                        # vcpkg manifest
├── vcpkg-configuration.json          # vcpkg baseline 鎖版本
├── .clang-format                     # 程式碼風格
├── .gitignore
├── README.md
├── docs/
│   └── plan.md                       # 本文件
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                      # Win32 WinMain 入口
│   ├── core/                         # 核心工具（記憶體、日誌、型別）
│   │   ├── CMakeLists.txt
│   │   ├── Types.h                   # 全域型別定義
│   │   ├── Logger.h / Logger.cpp     # spdlog 封裝
│   │   ├── Arena.h                   # Arena allocator
│   │   ├── Result.h                  # Result<T, E> 型別
│   │   └── Timer.h                   # 高精度計時器（QueryPerformanceCounter）
│   ├── raw/                          # RAW 解碼層
│   │   ├── CMakeLists.txt
│   │   ├── RawDecoder.h / .cpp
│   │   ├── RawImage.h                # 解碼後的 RAW 資料結構
│   │   ├── CameraProfile.h / .cpp    # DCP / ICC profile 載入
│   │   └── ExifReader.h / .cpp       # EXIF/XMP metadata
│   ├── pipeline/                     # 影像處理管線
│   │   ├── CMakeLists.txt
│   │   ├── EditRecipe.h              # Non-destructive 參數結構
│   │   ├── IProcessNode.h            # 處理節點抽象介面
│   │   ├── Pipeline.h / .cpp         # Pipeline orchestrator
│   │   ├── TileScheduler.h / .cpp    # Tile 切割與排程
│   │   ├── cpu/                      # CPU 實作
│   │   │   ├── DemosaicNode.h / .cpp
│   │   │   ├── WhiteBalanceNode.h / .cpp
│   │   │   ├── ExposureNode.h / .cpp
│   │   │   ├── ToneCurveNode.h / .cpp
│   │   │   ├── HSLNode.h / .cpp
│   │   │   ├── SharpenNode.h / .cpp
│   │   │   ├── DenoiseNode.h / .cpp
│   │   │   ├── LensCorrectNode.h / .cpp
│   │   │   └── ColorSpaceNode.h / .cpp
│   │   └── gpu/                      # GPU (D3D11 Compute) 實作
│   │       ├── GPUDemosaicNode.h / .cpp
│   │       ├── GPUWhiteBalanceNode.h / .cpp
│   │       ├── GPUExposureNode.h / .cpp
│   │       ├── GPUToneCurveNode.h / .cpp
│   │       ├── GPUHSLNode.h / .cpp
│   │       ├── GPUSharpenNode.h / .cpp
│   │       ├── GPUDenoiseNode.h / .cpp
│   │       └── GPUColorSpaceNode.h / .cpp
│   ├── gpu/                          # D3D11 基礎設施
│   │   ├── CMakeLists.txt
│   │   ├── D3D11Context.h / .cpp     # Device, SwapChain, Debug Layer
│   │   ├── ComputeShader.h / .cpp    # HLSL 編譯 / 快取
│   │   ├── TexturePool.h / .cpp      # GPU Texture 記憶體池
│   │   ├── ConstantBuffer.h          # 泛型 Constant Buffer
│   │   └── GPUTimer.h / .cpp         # D3D11 timestamp query
│   ├── ui/                           # ImGui UI 層
│   │   ├── CMakeLists.txt
│   │   ├── UIManager.h / .cpp        # ImGui 初始化 + 主佈局
│   │   ├── ImageViewport.h / .cpp    # 圖片顯示、Pan/Zoom
│   │   ├── DevelopPanel.h / .cpp     # 右側參數面板
│   │   ├── CurveEditor.h / .cpp      # Bézier tone curve 控件
│   │   ├── HSLPanel.h / .cpp         # HSL 色彩調整面板
│   │   ├── HistogramView.h / .cpp    # 即時直方圖
│   │   ├── FileBrowser.h / .cpp      # 檔案瀏覽器
│   │   ├── BeforeAfter.h / .cpp      # 前後比較分割視圖
│   │   ├── StatusBar.h / .cpp        # 底部狀態列
│   │   └── Toolbar.h / .cpp          # 上方工具列
│   ├── catalog/                      # 照片管理
│   │   ├── CMakeLists.txt
│   │   ├── Database.h / .cpp         # SQLite 封裝
│   │   ├── ThumbnailCache.h / .cpp   # 多解析度縮圖快取
│   │   ├── Sidecar.h / .cpp          # XMP sidecar 讀寫
│   │   └── ImportManager.h / .cpp    # 匯入工作流程
│   └── export/                       # 輸出模組
│       ├── CMakeLists.txt
│       ├── JpegExporter.h / .cpp
│       ├── TiffExporter.h / .cpp
│       ├── PngExporter.h / .cpp
│       └── ExportManager.h / .cpp
├── shaders/                          # HLSL Compute Shaders
│   ├── common.hlsli                  # 共用結構 & 函式
│   ├── demosaic_bilinear.hlsl
│   ├── demosaic_amaze.hlsl
│   ├── white_balance.hlsl
│   ├── exposure.hlsl
│   ├── tone_curve.hlsl
│   ├── hsl_adjust.hlsl
│   ├── sharpen_usm.hlsl
│   ├── denoise_bilateral.hlsl
│   ├── denoise_nlm.hlsl
│   ├── lens_distortion.hlsl
│   ├── colorspace_transform.hlsl
│   └── histogram_compute.hlsl
├── resources/
│   ├── vega.ico                      # 應用程式圖示
│   ├── vega.rc                       # Windows resource file
│   └── profiles/                     # 內建 camera profiles
│       └── README.md
├── tests/
│   ├── CMakeLists.txt
│   ├── test_raw_decode.cpp
│   ├── test_pipeline.cpp
│   ├── test_color_science.cpp
│   ├── test_tile_scheduler.cpp
│   └── test_data/                    # 測試用 RAW 檔 (gitignore, 手動放置)
│       └── README.md
└── third_party/
    └── imgui/                        # ImGui 以 submodule 或 copy 方式引入
        ├── imgui.h / .cpp
        ├── imgui_impl_win32.h / .cpp
        ├── imgui_impl_dx11.h / .cpp
        └── ...
```

### Step 0.2 — vcpkg.json 依賴清單

```json
{
  "name": "vega",
  "version-string": "0.1.0",
  "builtin-baseline": "2025.01.13",
  "dependencies": [
    { "name": "libraw", "version>=": "0.21.2" },
    { "name": "lcms", "version>=": "2.16" },
    { "name": "lensfun", "version>=": "0.3.4" },
    { "name": "exiv2", "version>=": "0.28.3" },
    { "name": "libjpeg-turbo", "version>=": "3.0" },
    { "name": "libpng", "version>=": "1.6" },
    { "name": "tiff", "version>=": "4.6" },
    { "name": "sqlite3", "version>=": "3.46" },
    { "name": "nlohmann-json", "version>=": "3.11" },
    { "name": "spdlog", "version>=": "1.14" },
    { "name": "catch2", "version>=": "3.7" },
    { "name": "openimageio", "version>=": "2.5" },
    { "name": "onnxruntime", "platform": "windows" }
  ]
}
```

### Step 0.3 — CMakePresets.json（Visual Studio 2022 整合）

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 28, "patch": 0 },
  "configurePresets": [
    {
      "name": "windows-x64-debug",
      "displayName": "Windows x64 Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "installDir": "${sourceDir}/out/install/${presetName}",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_STANDARD": "20",
        "CMAKE_CXX_FLAGS": "/W4 /WX /EHsc /fsanitize=address",
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      },
      "architecture": { "value": "x64", "strategy": "external" }
    },
    {
      "name": "windows-x64-release",
      "displayName": "Windows x64 Release",
      "inherits": "windows-x64-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_FLAGS": "/W4 /EHsc /O2 /fp:fast /arch:AVX2 /DNDEBUG"
      }
    }
  ],
  "buildPresets": [
    { "name": "debug", "configurePreset": "windows-x64-debug" },
    { "name": "release", "configurePreset": "windows-x64-release" }
  ]
}
```

### Step 0.4 — 頂層 CMakeLists.txt

```cmake
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
```

### Step 0.5 — ImGui 引入

ImGui 不走 vcpkg（官方建議直接複製原始碼）：

```
操作步驟：
1. git clone https://github.com/ocornut/imgui.git third_party/imgui
2. 在 src/CMakeLists.txt 中加入 ImGui 原始碼為 STATIC library
3. 包含 backends: imgui_impl_win32.cpp, imgui_impl_dx11.cpp
```

在 `src/CMakeLists.txt` 中：

```cmake
add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC ${D3D11_LIB} ${DXGI_LIB})
```

### Step 0.6 — main.cpp 最小窗口驗證

```cpp
// src/main.cpp — Phase 0 驗證版本
#include <windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

static ID3D11Device*            g_device    = nullptr;
static ID3D11DeviceContext*     g_context   = nullptr;
static IDXGISwapChain*          g_swapchain = nullptr;
static ID3D11RenderTargetView*  g_rtv       = nullptr;

bool InitD3D11(HWND hwnd);
void CleanupD3D11();
void CreateRTV();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
        case WM_SIZE:
            if (g_device && wParam != SIZE_MINIMIZED) {
                if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
                g_swapchain->ResizeBuffers(0,
                    LOWORD(lParam), HIWORD(lParam),
                    DXGI_FORMAT_UNKNOWN, 0);
                CreateRTV();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0,
                      hInst, nullptr, nullptr, nullptr, nullptr,
                      L"VegaEditor", nullptr };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, L"Vega v0.1",
        WS_OVERLAPPEDWINDOW, 100, 100, 1920, 1080,
        nullptr, nullptr, hInst, nullptr);

    if (!InitD3D11(hwnd)) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        ImGui::Begin("Develop");
        ImGui::Text("Vega — Phase 0 OK");
        static float exposure = 0.0f;
        ImGui::SliderFloat("Exposure", &exposure, -5.0f, 5.0f);
        ImGui::End();

        ImGui::Render();
        const float clear[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapchain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D11();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}
```

### Step 0.7 — .clang-format

```yaml
BasedOnStyle: Microsoft
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: Inline
PointerAlignment: Left
```

### Step 0.8 — 驗收標準

- [ ] `cmake --preset windows-x64-debug` 成功
- [ ] Visual Studio 2022 開啟資料夾後可自動偵測 CMake
- [ ] F5 執行出現 1920x1080 視窗，ImGui 面板可拖曳 Exposure 滑桿
- [ ] Release build 編譯通過且無 warning（/W4 /WX）

---

## Phase 1 — RAW 解碼與基礎顯示（Week 3-6）

### 目標

用 LibRaw 解碼任意 RAW 檔（CR3, ARW, NEF, RAF, DNG），轉為 float linear buffer，用 D3D11 texture 顯示在 ImGui viewport 中。

### Step 1.1 — RawImage 資料結構

```cpp
// src/raw/RawImage.h
#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace vega {

struct RawImageMetadata {
    std::string camera_make;
    std::string camera_model;
    std::string lens_model;
    uint32_t    iso_speed;
    float       shutter_speed;     // 秒
    float       aperture;          // f-number
    float       focal_length_mm;
    int         orientation;       // EXIF orientation tag
    std::string datetime_original;
    uint32_t    bayer_pattern;     // RGGB, BGGR, GRBG, GBRG
};

struct RawImage {
    std::vector<float> bayer_data;    // single channel, W x H
    uint32_t width  = 0;
    uint32_t height = 0;
    float wb_multipliers[4] = {1, 1, 1, 1}; // R, G1, B, G2
    float color_matrix[9] = {};              // Camera → XYZ (3x3)
    RawImageMetadata metadata;
    uint16_t bits_per_sample = 14;
    float black_level = 0.0f;
    float white_level = 16383.0f;
};

} // namespace vega
```

### Step 1.2 — LibRaw 封裝 RawDecoder

```cpp
// src/raw/RawDecoder.h
#pragma once
#include "RawImage.h"
#include "core/Result.h"
#include <filesystem>

namespace vega {

enum class RawDecodeError {
    FileNotFound, UnsupportedFormat, CorruptFile, OutOfMemory, LibRawError
};

class RawDecoder {
public:
    static Result<RawImage, RawDecodeError>
        decode(const std::filesystem::path& filepath);

    static Result<RawImageMetadata, RawDecodeError>
        readMetadata(const std::filesystem::path& filepath);

    static Result<std::vector<uint8_t>, RawDecodeError>
        extractThumbnail(const std::filesystem::path& filepath);
};

} // namespace vega
```

**實作重點（decode 函式內部）：**

```
1. LibRaw::open_file(path)
2. LibRaw::unpack() — 取得原始 sensor data
3. 讀取 imgdata.idata (make/model)
4. 讀取 imgdata.color (cam_mul, rgb_cam matrix)
5. 讀取 imgdata.rawdata.raw_image (Bayer mosaic)
6. Black level subtraction:
   for each pixel: val = (raw_pixel - black_level) / (white_level - black_level)
   clamp to [0, 1]
7. 存入 RawImage::bayer_data as float
8. 填入 metadata, wb_multipliers, color_matrix
9. LibRaw::recycle()
```

### Step 1.3 — EXIF 讀取（exiv2）

```cpp
// src/raw/ExifReader.h
#pragma once
#include "RawImage.h"
#include <filesystem>
#include <optional>

namespace vega {

struct GpsCoord { double latitude, longitude, altitude; };

class ExifReader {
public:
    static void enrichMetadata(const std::filesystem::path& filepath,
                               RawImageMetadata& metadata);
    static std::optional<GpsCoord> readGps(const std::filesystem::path& filepath);
    static std::optional<std::string> readXmpSidecar(const std::filesystem::path& filepath);
};

} // namespace vega
```

### Step 1.4 — D3D11Context 初始化

```cpp
// src/gpu/D3D11Context.h
#pragma once
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace vega {

class D3D11Context {
public:
    bool initialize(HWND hwnd, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void present(bool vsync = true);
    void cleanup();

    ID3D11Device*           device()  const { return device_.Get(); }
    ID3D11DeviceContext*    context() const { return context_.Get(); }
    ID3D11RenderTargetView* rtv()    const { return rtv_.Get(); }

    ComPtr<ID3D11Texture2D> createTexture2D(
        uint32_t width, uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        bool uav = false, bool srv = true);
    ComPtr<ID3D11ShaderResourceView>  createSRV(ID3D11Texture2D* tex);
    ComPtr<ID3D11UnorderedAccessView> createUAV(ID3D11Texture2D* tex);

private:
    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain1>        swapchain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    void enableDebugLayer();
};

} // namespace vega
```

### Step 1.5 — CPU 端最小 Pipeline（Bilinear Demosaic → sRGB）

```
最小色彩轉換鏈：
  Bayer float → Bilinear Demosaic → RGB linear
  → White Balance (乘以 wb_multipliers, normalize by G)
  → Camera RGB → XYZ (用 color_matrix)
  → XYZ → linear sRGB (固定 3x3 矩陣)
  → sRGB gamma: x < 0.0031308 ? 12.92*x : 1.055*pow(x, 1/2.4) - 0.055
  → 8-bit clamp → D3D11 texture upload
```

### Step 1.6 — ImageViewport UI 控件

```cpp
// src/ui/ImageViewport.h
namespace vega {

class ImageViewport {
public:
    void render(ID3D11ShaderResourceView* image_srv,
                uint32_t img_width, uint32_t img_height);
private:
    float zoom_ = 1.0f;
    ImVec2 pan_ = {0, 0};
    bool dragging_ = false;
    // Zoom: 滾輪以滑鼠為中心, Pan: 中鍵/Space+左鍵
    // F=Fit, 1=100%, 2=200%, 雙擊=Fit
    void handleInput(ImVec2 viewport_size, uint32_t img_w, uint32_t img_h);
};

} // namespace vega
```

### Step 1.7 — 整合流程

```
1. FileBrowser 選擇 RAW 檔
2. RawDecoder::decode() → RawImage
3. CPU Pipeline → sRGB float buffer
4. Upload D3D11 Texture2D (RGBA8)
5. SRV → ImageViewport::render()
```

### Step 1.8 — 驗收標準

- [ ] 可開啟 Canon CR3, Sony ARW, Nikon NEF, Fuji RAF, Adobe DNG 各一張
- [ ] 色彩大致正確（與 LibRaw dcraw_process 結果肉眼比對）
- [ ] Pan / Zoom 60fps
- [ ] EXIF 正確顯示在 StatusBar
- [ ] 記憶體 < 2GB（60MP RAW）

---

## Phase 2 — 核心影像處理 Pipeline（Week 7-12）

### 目標

實作完整的 Non-Destructive CPU Pipeline，包含所有核心影像調整功能。

### Step 2.1 — EditRecipe 資料結構

```cpp
// src/pipeline/EditRecipe.h
namespace vega {

struct CurvePoint { float x, y; };

struct EditRecipe {
    // 基礎調整
    float exposure = 0, contrast = 0;
    float highlights = 0, shadows = 0, whites = 0, blacks = 0;

    // 白平衡
    float wb_temperature = 5500.0f, wb_tint = 0.0f;

    // 色調曲線
    std::vector<CurvePoint> tone_curve_rgb = {{0,0},{1,1}};
    std::vector<CurvePoint> tone_curve_r   = {{0,0},{1,1}};
    std::vector<CurvePoint> tone_curve_g   = {{0,0},{1,1}};
    std::vector<CurvePoint> tone_curve_b   = {{0,0},{1,1}};

    // HSL (8通道: R, O, Y, G, Aqua, B, Purple, Magenta)
    std::array<float, 8> hsl_hue = {}, hsl_saturation = {}, hsl_luminance = {};

    // Vibrance & Saturation
    float vibrance = 0, saturation = 0;

    // 銳化
    float sharpen_amount = 0, sharpen_radius = 1.0f;
    float sharpen_detail = 25, sharpen_masking = 0;

    // 降噪
    float denoise_luminance = 0, denoise_color = 0, denoise_detail = 50;

    // 鏡頭校正
    bool lens_correct_distortion = false, lens_correct_vignette = false, lens_correct_ca = false;
    float lens_manual_distortion = 0, lens_manual_vignette = 0;

    // 裁切
    float crop_left = 0, crop_top = 0, crop_right = 1, crop_bottom = 1;
    float rotation = 0;

    // 輸出
    enum class ColorSpace { sRGB, AdobeRGB, ProPhotoRGB, DisplayP3 };
    ColorSpace output_colorspace = ColorSpace::sRGB;

    nlohmann::json toJson() const;
    static EditRecipe fromJson(const nlohmann::json& j);
    bool operator==(const EditRecipe& o) const = default;
};

} // namespace vega
```

**Sidecar：** `.vgr` (Vega Recipe) — UTF-8 JSON，與 RAW 同目錄。

### Step 2.2 — Pipeline 節點介面

```cpp
// src/pipeline/IProcessNode.h
namespace vega {

enum class PipelineStage : uint32_t {
    Demosaic = 1<<0, WhiteBalance = 1<<1, Exposure = 1<<2,
    ToneCurve = 1<<3, HSL = 1<<4, Vibrance = 1<<5,
    Sharpen = 1<<6, Denoise = 1<<7, LensCorrection = 1<<8,
    ColorSpace = 1<<9, Crop = 1<<10, All = 0xFFFFFFFF
};

struct Tile {
    float* data; uint32_t x, y, width, height, overlap, stride, channels;
};

class IProcessNode {
public:
    virtual ~IProcessNode() = default;
    virtual std::string_view name() const = 0;
    virtual PipelineStage stage() const = 0;
    virtual void process(Tile& tile, const EditRecipe& recipe) = 0;
    virtual uint32_t requiredOverlap() const { return 0; }
};

} // namespace vega
```

### Step 2.3 — TileScheduler

```
策略：
  - tile_size = 256, overlap = max(所有 node requiredOverlap)
  - 邊界 mirror padding
  - std::jthread pool 平行處理 tiles
  - 輸出時裁掉 overlap
```

### Step 2.4 — CPU 各節點演算法

**White Balance：** Planckian locus → R,G,B multipliers → 每像素乘法

**Exposure：** `pixel *= pow(2, ev)` + luminance-based highlight/shadow masks

**Tone Curve：** Monotone cubic spline → 4096-entry LUT → per-channel lookup

**HSL：** RGB→HSL → 8通道 cosine blend → 調整 H/S/L → RGB

**AMaZE Demosaic：** 方向梯度估計 + 色差引導插值, overlap=5

**Sharpen：** RGB→Lab → Gaussian blur L → USM + Sobel edge mask

**Denoise：** RGB→YCbCr → Bilateral filter (Y: luminance, CbCr: color)

**Lens Correction：** lensfun DB lookup → distortion/vignetting/CA 反算

**Color Space：** lcms2 cmsCreateTransform → output gamma

### Step 2.5 — Pipeline Orchestrator

```
Stage Cache 策略：
  - 每 stage 完成後快取結果（LRU, 最多 3 個）
  - 參數改變時，從上一個有效 cache 開始重算
  - 例：改 HSL → 從 tone_curve cache 接續
```

### Step 2.6 — DevelopPanel UI

```
slider 規格：
  - 左鍵拖曳, 雙擊重置, 右鍵精確輸入
  - 各 section 可摺疊
  - 拖曳中 → scale=0.25 preview
  - 放開 200ms → scale=0.5
  - 停止 1s → full res
```

### Step 2.7 — Undo/Redo

```cpp
namespace vega {
struct EditCommand {
    std::string description;
    EditRecipe before, after;
    PipelineStage affected_stage;
};
class EditHistory {
    std::vector<EditCommand> stack_;
    int current_ = -1;
    static constexpr int MAX_HISTORY = 200;
public:
    void push(EditCommand cmd);
    bool canUndo() const; bool canRedo() const;
    EditRecipe undo(); EditRecipe redo();
};
} // namespace vega
```

### Step 2.8 — Histogram

```
- R/G/B/Luminance 4 通道, 256 bins
- Clipping warning (highlight/shadow)
- ImGui drawlist 自繪
```

### Step 2.9 — 驗收標準

- [ ] `.vgr` sidecar 序列化/反序列化正確
- [ ] 所有 slider 拖動即時更新
- [ ] Tone Curve 控制點 CRUD
- [ ] HSL 8 通道獨立可調
- [ ] Undo/Redo (Ctrl+Z/Y)
- [ ] Histogram 同步更新
- [ ] 色彩 PSNR > 35dB (vs darktable)
- [ ] 60MP full-res < 10s (CPU, i7-12700)

---

## Phase 3 — GPU 加速與即時預覽（Week 13-17）

### 目標

D3D11 Compute Shader 移植熱路徑，即時預覽。

### Step 3.1 — ComputeShader 基礎設施

```
Debug: 即時編譯 HLSL + hot reload (ReadDirectoryChangesW)
Release: CMake 預編譯 → .cso, 啟動載入
```

### Step 3.2 — TexturePool

```
GPU texture 記憶體池：acquire/release 模式
key = (width, height, format), LRU eviction
```

### Step 3.3 — HLSL Shaders

**common.hlsli：** LinearToSRGB, SRGBToLinear, RGBToHSL, HSLToRGB, Luminance

**demosaic_bilinear.hlsl：** Bayer → RGB, [numthreads(16,16,1)]

**white_balance_exposure.hlsl：** WB + Exposure + H/S recovery 合併

**tone_curve.hlsl：** 4x Texture1D LUT sampling

**hsl_adjust.hlsl：** 8-channel HSL + vibrance + global saturation

**histogram_compute.hlsl：** InterlockedAdd to 4x RWBuffer<uint>

### Step 3.4 — GPUPipeline Orchestrator

```cpp
namespace vega {
class GPUPipeline {
    D3D11Context& ctx_;
    TexturePool pool_;
    ComputeShader demosaic_, wb_exp_, tone_, hsl_, sharpen_, denoise_, colorspace_, histogram_;
    ComPtr<ID3D11Texture1D> curve_luts_[4];
public:
    ID3D11ShaderResourceView* process(const RawImage&, const EditRecipe&,
                                       uint32_t preview_w, uint32_t preview_h);
    void processFullRes(const RawImage&, const EditRecipe&, std::vector<float>& out);
};
} // namespace vega
```

### Step 3.5 — PreviewManager（漸進渲染）

```
T+0ms:    Fast (1/8 res, ~2ms GPU)
T+200ms:  Medium (1/4 res, ~10ms)
T+1000ms: Full (full res, ~50-200ms)
新輸入 → 中止 → 回 Fast → 重新 debounce
```

### Step 3.6 — Shader Hot Reload

```
Debug mode: 監控 shaders/ 目錄, 修改即重編譯
VS Graphics Diagnostics (Alt+F5) 可抓每個 dispatch
```

### Step 3.7 — 驗收標準

- [ ] GPU vs CPU PSNR > 40dB
- [ ] Fast preview < 5ms (GTX 1060+)
- [ ] Full preview < 200ms (60MP, RTX 3060)
- [ ] Slider 拖曳無感知延遲
- [ ] Histogram 即時
- [ ] Hot reload 正常
- [ ] GPU memory < 2GB (60MP)

---

## Phase 4 — 進階編輯功能（Week 18-22）

### Step 4.1 — Local Adjustments

```
Brush: size/feather/flow/density, GPU mask texture, O 鍵切換 overlay
Graduated Filter: 起終點 + feather, shader 即時計算 mask
Radial Filter: cx/cy/rx/ry + feather + invert
每個 LocalAdjustment 攜帶自己的 exposure/contrast/highlights/shadows/temp/tint/sat/sharp/clarity
```

### Step 4.2 — AI Denoise (ONNX Runtime)

```
NAFNet/Restormer → ONNX fp16
256x256 tiles, overlap 16
DirectML EP (GPU) → CPU EP fallback
目標: 60MP < 3s (RTX 3060)
```

### Step 4.3 — Before/After

```
SideBySide | SplitView (可拖分割線) | Toggle (按住切換)
```

### Step 4.4 — 匯出

```
格式: JPEG (Q1-100), TIFF 8/16, PNG 8/16
Color space embed, EXIF/XMP preserve, GPS strip option
Resize: Original / LongEdge / ShortEdge / Percentage
Batch export with progress callback
```

### Step 4.5 — 快捷鍵

```
Ctrl+O 開啟 | Ctrl+S 存 .vgr | Ctrl+Shift+E 匯出
Ctrl+Z/Y Undo/Redo | F Fit | 1 100% | 2 200%
Space+Drag Pan | [] 筆刷大小 | O mask overlay
\ Before/After | Y Side-by-side | R Crop | K Brush
M Grad filter | Shift+M Radial | Tab 面板 | G Grid | D Develop
```

### Step 4.6 — 驗收標準

- [ ] Brush 60fps (4K)
- [ ] Local + Global 可疊加
- [ ] AI denoise 可運行
- [ ] 匯出色彩正確 + ICC + EXIF
- [ ] Batch export 不阻塞 UI
- [ ] Before/After 三模式
- [ ] 所有快捷鍵可用

---

## Phase 5 — Catalog 與 DAM（Week 23-27）

### Step 5.1 — SQLite Schema (.vegacat)

```sql
CREATE TABLE photos (
    id INTEGER PRIMARY KEY, uuid TEXT UNIQUE, file_path TEXT, file_name TEXT,
    file_size INTEGER, file_hash TEXT,
    camera_make TEXT, camera_model TEXT, lens_model TEXT,
    iso INTEGER, shutter_speed REAL, aperture REAL, focal_length REAL,
    datetime_taken TEXT, gps_lat REAL, gps_lon REAL,
    width INTEGER, height INTEGER, orientation INTEGER,
    rating INTEGER DEFAULT 0, color_label INTEGER DEFAULT 0,
    flag INTEGER DEFAULT 0, caption TEXT,
    has_edits BOOLEAN DEFAULT 0, edit_recipe TEXT,
    imported_at TEXT DEFAULT (datetime('now')),
    modified_at TEXT DEFAULT (datetime('now')),
    deleted BOOLEAN DEFAULT 0
);
CREATE TABLE tags (id INTEGER PRIMARY KEY, name TEXT UNIQUE);
CREATE TABLE photo_tags (photo_id INTEGER, tag_id INTEGER, PRIMARY KEY(photo_id, tag_id));
CREATE TABLE collections (id INTEGER PRIMARY KEY, name TEXT, parent_id INTEGER, created_at TEXT);
CREATE TABLE collection_photos (collection_id INTEGER, photo_id INTEGER, sort_order INTEGER DEFAULT 0, PRIMARY KEY(collection_id, photo_id));
CREATE VIRTUAL TABLE photos_fts USING fts5(file_name, camera_model, lens_model, caption, content='photos', content_rowid='id');
```

**預設路徑：** `%USERPROFILE%/Pictures/Vega/catalog.vegacat`
**快取目錄：** `%USERPROFILE%/Pictures/Vega/cache/`

### Step 5.2 — ThumbnailCache

```
4 級: Micro 160px, Small 320px, Medium 1024px, Large 2048px
記憶體 LRU + 磁碟快取 ({cache_dir}/{uuid[0:2]}/{uuid}_{level}.jpg)
非同步產生 (jthread worker)
```

### Step 5.3 — Grid View

```
虛擬化: 只渲染可見行 ± 2 行 buffer
支援 10 萬張不卡頓
Rating/Color/Flag 即時更新
```

### Step 5.4 — Import Manager

```
掃描 RAW → SHA-256 dedup → 寫 DB + 產生 thumbnails
支援 recursive, copy/reference mode
Progress callback
```

### Step 5.5 — Filter & Search

```
Rating ≥N★, Color label, Flag, Camera, Lens,
Date range, ISO range, Focal length range,
Has edits, Tags, FTS5 full-text
```

### Step 5.6 — 驗收標準

- [ ] 匯入 1000 張 < 5 分鐘
- [ ] Grid 10 萬張 scroll 不卡
- [ ] Rating/Color/Flag 即時
- [ ] Tag CRUD
- [ ] FTS5 < 100ms
- [ ] 縮圖快取 1000 張 < 500MB

---

## Phase 6 — 打磨與發布準備（Week 28-30）

### Step 6.1 — 效能優化

```
CPU: AVX2 intrinsics per-pixel ops, VS Performance Profiler
GPU: 合併 shader, groupshared memory, fp16 中間 buffer
Memory: tile buffer 即時釋放, working set 監控
```

### Step 6.2 — 錯誤處理

```
SetUnhandledExceptionFilter → minidump
DXGI_ERROR_DEVICE_REMOVED → 重建 device
File IO → toast + skip
Memory → 釋放快取 + 降級 preview
```

### Step 6.3 — Settings

```
%APPDATA%/Vega/settings.json
Window geometry, UI scale, dark theme, GPU toggle,
preview quality, last catalog path, last export dir
```

### Step 6.4 — Windows 整合

```
檔案關聯 (.CR3/.ARW/.NEF/.RAF/.DNG → Vega)
IDropTarget 拖放, 右鍵選單 "Open with Vega"
Taskbar 進度列, Jump List
High DPI (PER_MONITOR_AWARE_V2), 多螢幕
```

### Step 6.5 — 安裝程式

```
WiX 4 或 Inno Setup:
  VC++ Redist, ONNX Runtime DLL, 檔案關聯, Start Menu, 解除安裝
```

### Step 6.6 — 最終驗收

- [ ] 冷啟動 < 2s
- [ ] 開 RAW → 可操作 < 1s
- [ ] 無 memory leak (VS Diagnostic Tools)
- [ ] 連續 1 小時不 crash
- [ ] 4K / High DPI 正常
- [ ] 安裝/解除安裝正確

---

## 附錄

### A. Vega 檔案格式

| 副檔名 | 用途 | 格式 |
|--------|------|------|
| `.vgr` | Vega Recipe (編輯參數 sidecar) | UTF-8 JSON |
| `.vegacat` | Vega Catalog (照片資料庫) | SQLite 3 |
| `.xmp` | 標準 XMP sidecar (相容性) | XML |

**.vgr 範例：**

```json
{
  "vega_version": "0.1.0",
  "recipe_version": 1,
  "created_at": "2026-04-01T12:00:00Z",
  "modified_at": "2026-04-01T12:30:00Z",
  "source_file_hash": "sha256:abcdef...",
  "recipe": {
    "exposure": 1.3, "contrast": 15,
    "highlights": -30, "shadows": 40,
    "wb_temperature": 5200, "wb_tint": 5,
    "tone_curve_rgb": [[0,0],[0.25,0.22],[0.75,0.80],[1,1]],
    "hsl_saturation": [0,10,-5,0,0,0,0,0],
    "sharpen_amount": 40, "sharpen_radius": 1.0,
    "output_colorspace": "sRGB"
  }
}
```

### B. 第三方授權

| 函式庫 | 授權 | 注意事項 |
|--------|------|----------|
| LibRaw | LGPL 2.1 / CDDL | 動態連結可閉源 |
| lcms2 | MIT | 無限制 |
| lensfun | LGPL 3 | 動態連結 |
| exiv2 | GPL 2 | **閉源需替代方案** |
| ImGui | MIT | 無限制 |
| SQLite | Public Domain | 無限制 |
| nlohmann-json | MIT | 無限制 |
| spdlog | MIT | 無限制 |
| ONNX Runtime | MIT | 無限制 |
| libjpeg-turbo | BSD | 無限制 |
| libpng | libpng License | 無限制 |
| OpenImageIO | Apache 2.0 | 無限制 |

### C. 效能基準

| 操作 | 目標 | 條件 |
|------|------|------|
| RAW 解碼 | < 500ms | 60MP CR3, NVMe |
| Demosaic GPU | < 20ms | 60MP, RTX 3060 |
| Full pipeline GPU | < 200ms | 60MP, RTX 3060 |
| Fast preview | < 5ms | 1/8 res |
| Slider response | < 16ms | 感知延遲 |
| Thumbnail gen | < 100ms/張 | |
| JPEG export | < 1s | 60MP→20MP Q92 |
| Grid scroll | 60fps | 10,000 張 |

### D. Claude Code 使用指南

```
每個 Step = 一個 Claude Code session

提示詞模板：
  "根據以下 header，實作 vega::{ClassName} 的 .cpp。
   C++20, MSVC, namespace vega。
   邊界條件：{列出}  效能：{列出}
   附 Catch2 測試。"

HLSL 模板：
  "實作 D3D11 CS (cs_5_0) for Vega。
   格式：{texture format}  演算法：{描述}
   numthreads(16,16,1), 處理非 16 整數倍邊界。
   附 C++ constant buffer struct (namespace vega)。"

Commit 粒度 = 每 Step 一個 commit
```

### E. 測試資料

```
raw-samples.github.io 免費樣本：
  Canon CR3, Sony ARW, Nikon NEF, Fuji RAF, Adobe DNG, Olympus ORF, Panasonic RW2
特殊: 過曝/欠曝/高ISO/超廣角/人像 各一張
效能: 100MP Phase One IIQ, 61MP Sony A7R V ARW
```

### F. 參考資源

```
色彩: brucelindbloom.com, DNG SDK, ICC v4.4
開源: darktable, RawTherapee, libvips
D3D11: MS DX Graphics Samples, Frank Luna DX11 book
ImGui: github.com/ocornut/imgui/wiki
```

---

> **Vega** — Named after α Lyrae, the zero-point reference star of photometric systems.
> The brightest calibration standard in the northern sky.
>
> Document Version 1.0 | 2026-04-01
