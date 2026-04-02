# Vega -- Work Progress

## Phase 0 -- Project Foundation [DONE]
**Date**: 2026-04-01 | **Commit**: `0c404b5`

- CMake 3.28 + vcpkg + Ninja + MSVC 14.44 (VS2022 BuildTools)
- ImGui docking branch + D3D11 swap chain
- Core: Logger (spdlog), Result<T,E>, Timer (QPC), Arena allocator

---

## Phase 1 -- RAW Decode + Display [DONE]
**Date**: 2026-04-02 | **Commit**: `4403c65`

- RawDecoder (LibRaw), ExifReader (exiv2), SimplePipeline (Bayer -> sRGB)
- D3D11Context abstraction, ImageViewport (pan/zoom)
- Known fix: NOMINMAX, toLong -> toInt64, static init order

---

## Phase 2 -- Non-Destructive Pipeline [DONE]
**Date**: 2026-04-02 | **Commit**: `cbb4383`

- EditRecipe + .vgr JSON sidecar
- CPU nodes: WhiteBalance, Exposure, ToneCurve (cubic spline LUT), HSL (8-ch cosine blend), ColorSpace
- Pipeline orchestrator with stage cache + dirty detection
- DevelopPanel (sliders, tone curve editor, HSL tabs)
- EditHistory (undo/redo, max 200), HistogramView (R/G/B/L, log scale, clipping)

---

## Phase 3 -- GPU Acceleration [DONE]
**Date**: 2026-04-02

- ComputeShader helper (compile from HLSL / load .cso)
- ConstantBuffer<T> template (dynamic, 16-byte aligned)
- TexturePool (acquire/release, LRU)
- HLSL shaders: common.hlsli, white_balance_exposure, tone_curve (1D LUT), hsl_adjust (8-ch), histogram_compute (groupshared)
- GPUPipeline orchestrator (3-pass ping-pong compute)
- PreviewManager (Fast 1/8 -> Medium 1/4 -> Full, debounced)

---

## Phase 4 -- Advanced Editing [DONE]
**Date**: 2026-04-02

- BeforeAfter view (SideBySide / SplitView with draggable divider / Toggle)
- ExportManager (JPEG via libjpeg-turbo, PNG via libpng, TIFF via libtiff, bilinear resize)
- ExportDialog (format/quality/resize/metadata UI, async export with progress)
- Keyboard shortcuts: B (before/after), M (cycle mode), Ctrl+Shift+E (export), Ctrl+Shift+R (reset)

---

## Phase 5 -- Catalog + DAM [DONE]
**Date**: 2026-04-02

- Database (SQLite, .vegacat, FTS5 search, tags, rating/flag/color)
- ThumbnailCache (4-level: 160/320/1024/2048, memory LRU + disk, WIC decode)
- GridView (virtualized rendering, rating stars, color labels, flag icons)
- ImportManager (recursive scan, SHA-256 dedup via BCrypt, UUID v4, progress)

---

## Phase 6 -- Polish + Release Prep [DONE]
**Date**: 2026-04-02

- CrashHandler (SetUnhandledExceptionFilter, MiniDumpWriteDump)
- Settings (%APPDATA%/Vega/settings.json, window geometry, preferences)
- WindowsIntegration (High DPI, dark title bar, drag-drop, taskbar progress)
- Toolbar (Open/Save/Export/Undo/Redo/Zoom/Mode buttons)
- StatusBar (file info, timing, zoom, undo position, GPU indicator)

---

## Build Status
- Executable: 3.7MB (Debug), runs on RTX 3090
- All phases compile successfully with MSVC 14.44
- 78 source files, 5 HLSL shaders
