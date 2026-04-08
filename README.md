# Vega - RAW 照片編輯器

Vega 是一款 Windows 原生的 RAW 照片編輯器，以 C++20 和 DirectX 11 打造，提供非破壞性的影像處理管線（Non-Destructive Editing Pipeline），支援即時預覽與 GPU 加速。設計靈感來自 Adobe Lightroom，具備照片庫管理與單張沖洗兩種工作模式。

專案代號取自天琴座 a 星（Vega）— 天文測光系統的零點參考星。

## 功能特色

### 照片庫管理（Library Mode）
- 資料夾匯入：選擇本機資料夾，背景遞迴掃描所有 RAW 檔案並匯入
- 縮圖網格瀏覽：從 RAW 內嵌 JPEG 自動產生縮圖，支援 EXIF 方向自動旋轉，虛擬捲動（上千張不卡頓）
- 照片管理：星級評分（1-5 星）、色彩標籤（6 色）、旗標（Pick / Reject）
- 篩選與搜尋：依評分、色彩標籤、旗標、相機型號、日期範圍過濾
- 雙擊縮圖直接進入 Develop 模式編輯
- SQLite 資料庫持久化（含縮圖 BLOB），重啟後自動載入已匯入的照片庫
- 多資料夾支援，各資料夾顯示 RAW 檔案數量，點選資料夾僅顯示該資料夾照片
- 記憶上次選取的資料夾，重啟後自動恢復
- 路徑去重（同一檔案不會重複匯入）

### 影像處理（Develop Mode）
- 支援主流 RAW 格式：Canon CR2/CR3、Sony ARW、Nikon NEF、Fuji RAF、Adobe DNG、Olympus ORF、Panasonic RW2、Pentax PEF 等
- 白平衡調整（色溫 2000-12000K / 色調 / 吸管取樣）
- 曝光度、對比、亮部、暗部、白色、黑色 六軸色調控制
- 色調曲線編輯器（RGB 主通道 + 個別 R/G/B 通道，可拖拽控制點，單調三次樣條）
- HSL 8 通道獨立調整（紅/橙/黃/綠/水/藍/紫/洋紅 x 色相/飽和度/明度）
- 自然飽和度 / 飽和度
- 銳利化（Unsharp Mask，可調總量/半徑/細節/遮罩）
- 降噪（YCbCr 空間分離處理，亮度/色彩/細節獨立控制，邊緣保留）
- sRGB gamma 輸出

### GPU 加速
- 7 個 HLSL Compute Shader（cs_5_0），Build time 預編譯為 CSO bytecode
- 6-pass GPU 處理管線：WB+曝光 -> 色調曲線 -> HSL -> 降噪 -> 銳化 -> Gamma
- RTX 3090 上全解析度處理 < 50ms
- 自動 CPU fallback（GPU 不可用時無縫切換）
- Release 發佈不含 HLSL 原始碼，僅帶預編譯 .cso

### 使用介面
- ImGui 1.92 (Docking) 建構的專業暗色介面
- Library / Develop 雙模式切換（按 G / D 或工具列按鈕）
- 中文 / 英文雙語即時切換
- 即時直方圖（R/G/B/亮度，對數刻度，過曝/欠曝警告）
- Pan / Zoom 影像瀏覽（滑鼠左鍵拖拉、滾輪縮放、F 適配、1/2 倍率）
- 前後對比（並排 / 分割 / 切換三種模式，按 B 開關）
- Undo / Redo（最多 200 步，全操作可回溯）
- .vgr 設定檔（JSON 格式，自動儲存/載入）
- 漸進式預覽（拖拽時 1/8 解析度即時回應，放開後背景 thread 補全解析度）
- 可調整縮圖大小（80px - 400px）

### 匯出
- JPEG（可調品質 1-100）、PNG 8-bit、TIFF 8/16-bit
- 可調整輸出尺寸（原始大小/長邊/短邊/百分比）
- ICC Profile 嵌入
- EXIF 保留（可選擇移除 GPS）
- 輸出檔名模板
- 非同步匯出（不阻塞 UI）

### 系統整合
- 視窗位置 / 大小 / 最大化狀態記憶（%APPDATA%/Vega/settings.json）
- 照片庫資料夾清單持久化（重啟後自動恢復）
- Windows 暗色標題列
- 檔案拖放開啟 RAW 檔
- 高 DPI 支援（Per-Monitor DPI Aware V2）
- Crash handler（MiniDump 自動產生）
- 日誌記錄（vega.log）

## 工作流程

1. 啟動 Vega，預設進入 **Library 模式**
2. 點選左側 **「+ 新增資料夾」** 選擇存放 RAW 檔的資料夾
3. 背景自動掃描並匯入所有 RAW 檔案，產生縮圖
4. 在網格中瀏覽照片，可設定星級、色彩標籤、旗標進行篩選
5. **雙擊**縮圖進入 **Develop 模式** 開始編輯
6. 調整白平衡、曝光、色調曲線、HSL 等參數，即時預覽效果
7. 編輯參數自動儲存為 `.vgr` sidecar 檔案（非破壞性）
8. 按 **Ctrl+Shift+E** 匯出為 JPEG / PNG / TIFF
9. 按 **G** 回到 Library 模式繼續瀏覽其他照片

## 系統需求

- Windows 10/11 (x64)
- DirectX 11 相容顯示卡
- 建議：NVIDIA GTX 1060 以上（GPU 加速）

## 建置方式

### 前置需求

- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) — C++ Desktop Development 工作負載
- [CMake 3.28+](https://cmake.org/download/)
- [Ninja](https://ninja-build.org/)
- [vcpkg](https://github.com/microsoft/vcpkg)

### 安裝工具鏈

```bash
# 安裝 CMake 和 Ninja（如果尚未安裝）
winget install Kitware.CMake
winget install Ninja-build.Ninja

# 安裝 vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# 設定環境變數
setx VCPKG_ROOT C:\vcpkg
```

### 編譯

```bash
# Debug 版本
build.bat

# Release 版本（含 AVX2 優化）
build.bat release
```

### 執行

```bash
# Debug
out\build\windows-x64-debug\src\vega.exe

# Release
out\build\windows-x64-release\src\vega.exe
```

### 測試

```bash
# 執行全部測試（需要 samples/_R4C7773.CR2 測試圖檔）
out\build\windows-x64-release\tests\vega_tests.exe
```

## 技術架構

### 目錄結構

```
vega/
├── src/
│   ├── main.cpp                    # 應用程式入口 + ImGui 主迴圈
│   ├── core/                       # 核心工具
│   │   ├── Logger.h/.cpp           # spdlog 封裝
│   │   ├── Timer.h/.cpp            # QPC 高精度計時器
│   │   ├── Settings.h/.cpp         # 應用程式設定 (JSON)
│   │   ├── CrashHandler.h/.cpp     # MiniDump crash handler
│   │   ├── WindowsIntegration.h/.cpp # 暗色標題列、拖放、DPI
│   │   ├── i18n.h/.cpp             # 中英文國際化
│   │   ├── Result.h                # Result<T, E> 型別
│   │   ├── Arena.h                 # Arena allocator
│   │   └── Types.h                 # 全域型別定義
│   ├── raw/                        # RAW 解碼
│   │   ├── RawDecoder.h/.cpp       # LibRaw 封裝
│   │   ├── ExifReader.h/.cpp       # exiv2 EXIF/GPS 讀取
│   │   └── RawImage.h              # RAW 資料結構
│   ├── pipeline/                   # 影像處理管線
│   │   ├── Pipeline.h/.cpp         # CPU 管線 orchestrator
│   │   ├── GPUPipeline.h/.cpp      # GPU compute shader 管線
│   │   ├── EditRecipe.h/.cpp       # 非破壞性編輯參數 + .vgr 序列化
│   │   ├── EditHistory.h/.cpp      # Undo/Redo 堆疊
│   │   ├── IProcessNode.h          # 處理節點抽象介面
│   │   ├── PreviewManager.h/.cpp   # 漸進式預覽管理
│   │   ├── SimplePipeline.h/.cpp   # 簡易管線（Phase 1 遺留）
│   │   └── cpu/                    # CPU 處理節點
│   │       ├── WhiteBalanceNode     # Planckian locus 白平衡
│   │       ├── ExposureNode         # 曝光 + sigmoid 對比 + 亮暗部
│   │       ├── ToneCurveNode        # 單調三次樣條 → 4096 LUT
│   │       ├── HSLNode              # 8 通道 cosine blend + vibrance
│   │       ├── DenoiseNode          # YCbCr box blur + 邊緣保留
│   │       ├── SharpenNode          # Unsharp Mask + 邊緣遮罩
│   │       ├── ColorSpaceNode       # sRGB gamma
│   │       └── FastMath.h           # fast_exp, gamma LUT, forceinline
│   ├── gpu/                        # D3D11 基礎設施
│   │   ├── D3D11Context.h/.cpp     # Device, SwapChain, RTV
│   │   ├── ComputeShader.h/.cpp    # CSO 載入 / HLSL 即時編譯（Debug）
│   │   ├── TexturePool.h/.cpp      # GPU texture 記憶體池
│   │   ├── ConstantBuffer.h        # 泛型 Constant Buffer
│   │   └── GPUTimer.h/.cpp         # D3D11 timestamp query
│   ├── ui/                         # ImGui UI 層
│   │   ├── ImageViewport.h/.cpp    # Pan/Zoom 影像顯示 + WB 吸管
│   │   ├── DevelopPanel.h/.cpp     # 編輯參數面板
│   │   ├── HistogramView.h/.cpp    # 即時直方圖
│   │   ├── BeforeAfter.h/.cpp      # 前後對比分割視圖
│   │   ├── ExportDialog.h/.cpp     # 匯出設定對話框
│   │   ├── GridView.h/.cpp         # 照片縮圖網格（虛擬捲動）
│   │   ├── FolderPanel.h/.cpp      # 資料夾管理面板
│   │   ├── Toolbar.h/.cpp          # 上方工具列
│   │   └── StatusBar.h/.cpp        # 底部狀態列
│   ├── catalog/                    # 照片管理（DAM）
│   │   ├── Database.h/.cpp         # SQLite 資料庫（FTS5 + 縮圖 BLOB）
│   │   ├── ThumbnailCache.h/.cpp   # DB-backed 縮圖快取（WIC 解碼 + EXIF 旋轉）
│   │   └── ImportManager.h/.cpp    # 背景匯入工作流程
│   └── export/                     # 輸出模組
│       └── ExportManager.h/.cpp    # JPEG/PNG/TIFF 匯出
├── shaders/                        # HLSL Compute Shaders（Build time 編譯為 .cso）
│   ├── common.hlsli                # 共用函式（LinearToSRGB, RGBToHSL...）
│   ├── white_balance_exposure.hlsl # WB + 曝光 + 對比 + 亮暗部
│   ├── tone_curve.hlsl             # 1D LUT 色調曲線
│   ├── hsl_adjust.hlsl             # HSL 8 通道 + vibrance
│   ├── denoise.hlsl                # bilateral 降噪
│   ├── sharpen_usm.hlsl            # Unsharp Mask 銳化
│   ├── gamma_output.hlsl           # sRGB gamma 輸出
│   └── histogram_compute.hlsl      # GPU histogram
├── tests/                          # Catch2 測試
│   ├── test_pipeline.cpp           # CPU 管線測試 (16 cases)
│   └── test_gpu_pipeline.cpp       # GPU 管線測試 (5 cases)
├── third_party/imgui/              # ImGui (docking branch)
├── resources/                      # Windows 資源檔
├── CMakeLists.txt                  # 頂層 CMake
├── CMakePresets.json               # VS2022 preset
├── vcpkg.json                      # vcpkg 依賴清單
└── build.bat                       # 建置腳本
```

### 第三方函式庫

| 函式庫 | 用途 | 授權 |
|--------|------|------|
| LibRaw | RAW 解碼 | LGPL 2.1 / CDDL |
| exiv2 | EXIF/XMP 讀取 | GPL 2 |
| lcms2 | 色彩管理 | MIT |
| ImGui | 圖形介面 | MIT |
| spdlog | 日誌系統 | MIT |
| nlohmann-json | JSON 序列化 | MIT |
| SQLite | 資料庫 | Public Domain |
| libjpeg-turbo | JPEG 匯出 | BSD |
| libpng | PNG 匯出 | libpng License |
| libtiff | TIFF 匯出 | BSD |
| Catch2 | 測試框架 | BSL 1.0 |

### 效能數據（Canon 5D Mark III, 5920x3950, Release build）

| 操作 | 時間 |
|------|------|
| RAW 解碼 | ~230ms |
| CPU 管線（全部節點） | ~400ms |
| CPU Preview (1/8 res) | ~30ms |
| GPU 管線（6-pass） | < 50ms |
| Slider 互動回應 | ~30ms（preview） |

## 快捷鍵

### 全域

| 快捷鍵 | 功能 |
|--------|------|
| Ctrl+O | 開啟 RAW 檔 |
| Ctrl+Shift+I | 新增資料夾（匯入） |
| G | 切換至 Library 模式 |
| D | 切換至 Develop 模式 |

### Develop 模式

| 快捷鍵 | 功能 |
|--------|------|
| Ctrl+S | 儲存 .vgr 設定 |
| Ctrl+Shift+E | 匯出 |
| Ctrl+Z / Ctrl+Y | 復原 / 重做 |
| F | 適配視窗 |
| 1 / 2 | 100% / 200% 縮放 |
| W | 白平衡吸管 |
| B | 前後對比開關 |
| M | 切換對比模式 |
| 左鍵拖拉 | 平移照片 |
| 滾輪 | 縮放（以滑鼠為中心） |

### Library 模式

| 快捷鍵 | 功能 |
|--------|------|
| 雙擊縮圖 | 開啟該照片進入 Develop 模式 |
| 右鍵資料夾 | 移除資料夾 |

## 資料儲存

| 路徑 | 用途 | 格式 |
|------|------|------|
| `%APPDATA%/Vega/settings.json` | 應用程式設定、資料夾清單、上次選取資料夾 | JSON |
| `<exe同目錄>/catalog.db` | 照片資料庫（元資料、評分、標籤、縮圖 BLOB） | SQLite 3 |
| `<RAW檔同目錄>/<檔名>.vgr` | 編輯參數（sidecar） | UTF-8 JSON |

## 授權

本專案採用 MIT License 授權，詳見 [LICENSE](LICENSE) 檔案。
