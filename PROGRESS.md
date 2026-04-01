# Vega — 工作進度紀錄

## Phase 0 — 專案基礎建設 ✅
**完成日期**: 2026-04-01

| Step | 描述 | 狀態 |
|------|------|------|
| 0.1 | 專案目錄結構 | ✅ |
| 0.2 | vcpkg.json 依賴清單 | ✅ |
| 0.3 | CMakePresets.json (VS2022) | ✅ |
| 0.4 | 頂層 CMakeLists.txt | ✅ |
| 0.5 | ImGui 引入 (docking branch) | ✅ |
| 0.6 | main.cpp 最小窗口驗證 | ✅ |
| 0.7 | .clang-format + core modules | ✅ |
| 0.8 | Git init + commit | ✅ |

**Commit**: `0c404b5` Phase 0: project foundation

**備註**:
- 安裝 CMake 4.3.1, Ninja 1.13.2, vcpkg, VS2022 BuildTools (MSVC 14.44)
- VCPKG_ROOT=C:\vcpkg

---

## Phase 1 — RAW 解碼與基礎顯示 ✅
**完成日期**: 2026-04-02

| Step | 描述 | 狀態 |
|------|------|------|
| 1.1 | RawImage 資料結構 | ✅ |
| 1.2 | RawDecoder (LibRaw 封裝) | ✅ |
| 1.3 | ExifReader (exiv2) | ✅ |
| 1.4 | D3D11Context 抽象層 | ✅ |
| 1.5 | CPU 最小 Pipeline (Bilinear Demosaic → sRGB) | ✅ |
| 1.6 | ImageViewport (Pan/Zoom) | ✅ |
| 1.7 | 整合 + CMake + Build | ✅ |

**Commit**: `4403c65` Phase 1: RAW decode, CPU pipeline, and image display

**備註**:
- 移除 lensfun（glib 在 Windows 編譯失敗），延後至 Phase 2+
- exiv2 API: `toLong()` → `toInt64()` (v0.28 breaking change)
- LibRaw: `raw_count` 欄位不存在於 v0.21，改用固定 14-bit
- ImageViewport: 需加 `#define NOMINMAX` 避免 Windows min/max macro 衝突
- Build: 45/45 targets, vega.exe 2.8MB (Debug)

**已知 UI 問題**:
- Layout 需要調整（使用者回報）

---

## Phase 2 — 核心影像處理 Pipeline 🔄
**開始日期**: 2026-04-02

| Step | 描述 | 狀態 |
|------|------|------|
| 2.1 | EditRecipe 資料結構 + JSON 序列化 (.vgr) | ✅ |
| 2.2 | IProcessNode 介面 + PipelineStage | ✅ |
| 2.3 | TileScheduler (多執行緒 tile 處理) | ⏳ 延後 (目前 full-image pass) |
| 2.4 | CPU 節點: WB, Exposure, ToneCurve, HSL, ColorSpace | ✅ |
| 2.5 | Pipeline Orchestrator (stage cache) | ✅ |
| 2.6 | DevelopPanel UI (slider + sections + tone curve editor) | ✅ |
| 2.7 | Undo/Redo (EditHistory, Ctrl+Z/Y) | ✅ |
| 2.8 | Histogram (R/G/B/L, clipping warning, log scale) | ✅ |
| 2.9 | 驗收 | 🔄 |

**Commit**: (pending)

**備註**:
- TileScheduler 延後至 Phase 3 GPU 加速時一併實作
- Sharpen/Denoise 節點延後（需 overlap tile 支援）
- EditRecipe.cpp: `path.u8string()` → `path.string()` (fmt v11 不支援 char8_t)
- DevelopPanel: 包含 tone curve 編輯器、HSL 8 通道、Vibrance/Saturation
- Pipeline: stage cache + dirty stage detection 已實作
- 快捷鍵: Ctrl+O 開啟、Ctrl+S 存 .vgr、Ctrl+Z/Y Undo/Redo
