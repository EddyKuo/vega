# Vega RAW 照片編輯器 — 完整實作計畫

## 目錄

- Phase 2: 核心編輯工具
- Phase 3: 鏡頭校正與幾何變形
- Phase 4: 局部調整（遮罩系統）
- Phase 5: 照片庫增強
- Phase 6: 匯出增強
- Phase 7: 進階功能（HDR / 全景 / AI）
- Phase 8: 工作流程與打磨

---

## Phase 2：核心編輯工具

補齊攝影師最常使用的全域調整工具，使 Vega 能覆蓋 90% 的日常修圖需求。

---

### 2.1 裁切與拉直（Crop & Straighten）

在 Develop 模式中提供互動式裁切框，支援自由比例與固定長寬比（1:1, 4:3, 3:2, 16:9, 自訂），拖曳四角/四邊調整裁切範圍，框外區域以半透明遮罩顯示。旋轉滑桿（-45 到 +45 度）與自動拉直。裁切引導線（三分法、黃金比例、對角線）。

**技術方案**：

- `EditRecipe.h` 已有 `crop_left`, `crop_top`, `crop_right`, `crop_bottom`, `rotation` 欄位（L46-48），但 UI 和 GPU 管線端均未使用
- 新增 `src/ui/CropOverlay.h/.cpp` — 裁切覆蓋層 UI 元件
- 修改 `src/ui/ImageViewport.h/.cpp` — 新增 `CropMode` 狀態，啟用時攔截滑鼠事件繪製裁切框，在 `render()` 中疊加 ImDrawList 繪製裁切框、引導線、半透明遮罩
- 修改 `src/ui/DevelopPanel.h/.cpp` — 在 `render()` 頂端新增 `renderCropSection()`，含長寬比下拉選單、旋轉滑桿、自動拉直按鈕
- 新增 `shaders/crop_rotate.hlsl` — 在 gamma 輸出之後新增 pass，使用雙線性插值進行仿射變換（旋轉 + 裁切）
- 修改 `src/pipeline/GPUPipeline.h/.cpp` — 新增 `crop_rotate_shader_` 及 `CropRotateCB`，管線末尾插入 crop_rotate pass，輸出 texture 尺寸改為裁切後尺寸
- 新增 `src/pipeline/cpu/CropRotateNode.h/.cpp` — CPU fallback
- 修改 `src/export/ExportManager.cpp` — 匯出時套用裁切參數到最終尺寸
- 自動拉直：Canny 邊緣偵測 + Hough 直線檢測，找主要水平/垂直線的角度偏差

**依賴**：無（EditRecipe 欄位已存在）
**複雜度**：高

---

### 2.2 Clarity / Texture / Dehaze

三個 Presence 控制滑桿。Clarity 增強中頻細節（local contrast），Texture 增強高頻紋理但不影響皮膚，Dehaze 移除大氣霧氣。

**技術方案**：

- 修改 `EditRecipe.h` — 新增 `float clarity, texture, dehaze`（-100 to 100）
- 修改 `EditRecipe.cpp` — 序列化/反序列化
- 修改 `src/ui/DevelopPanel.h/.cpp` — 新增 `renderPresence()` 區段
- 新增 `shaders/presence.hlsl`（或分為 blur_h / blur_v / apply 三個 pass）：
  - Clarity：大半徑（~20-50px）高斯模糊 → 原圖減低頻 → 乘強度加回
  - Texture：小半徑（~5-10px），邊緣感知
  - Dehaze：暗通道先驗（Dark Channel Prior），計算區塊內 RGB 最小值的最小值估計大氣光
- 修改 `GPUPipeline.h/.cpp` — 新增 shader、CB，插入管線（WB+Exposure 之後、Tone Curve 之前）
- 需要額外的中間 texture 儲存模糊結果（從 TexturePool 分配）
- 新增 `src/pipeline/cpu/PresenceNode.h/.cpp` — CPU fallback，用 box blur 近似

**依賴**：無
**複雜度**：高

---

### 2.3 白平衡預設（WB Presets）

在白平衡區段新增下拉選單：As Shot / Auto / Daylight / Cloudy / Shade / Tungsten / Fluorescent / Flash。

**技術方案**：

- 修改 `src/ui/DevelopPanel.h/.cpp` — Temperature 滑桿前新增 `ImGui::Combo`
- 預設表：As Shot 用 `raw.wb_multipliers` 反算色溫、Daylight=5500K、Cloudy=6500K、Shade=7500K、Tungsten=2850K、Fluorescent=3800K/Tint+10、Flash=5500K
- 需要傳遞 RawImage metadata 到 DevelopPanel（用於 "As Shot"）
- 修改 `src/main.cpp` — 傳遞 raw image metadata

**依賴**：無
**複雜度**：低

---

### 2.4 Color Grading（色彩分級）

三輪色彩分級（Shadows / Midtones / Highlights + Global），每個色輪控制 Hue + Saturation，加上 Blending 和 Balance 滑桿。

**技術方案**：

- 修改 `EditRecipe.h` — 新增 `ColorGradingWheel` struct（hue, saturation, luminance）x4 + blending + balance
- 修改 `src/ui/DevelopPanel.h/.cpp` — 新增 `renderColorGrading()`，ImGui 自訂繪製色輪 UI
- 新增 `shaders/color_grading.hlsl` — 計算亮度分配 shadow/midtone/highlight 權重（smoothstep），在 HSL 空間加入色偏
- 修改 `GPUPipeline.h/.cpp` — 插入管線：HSL 之後、Denoise 之前
- 新增 `src/pipeline/cpu/ColorGradingNode.h/.cpp`

**依賴**：無
**複雜度**：中

---

### 2.5 Auto Tone（自動色調）

一鍵自動分析直方圖，設定 Exposure / Contrast / Highlights / Shadows / Whites / Blacks。

**技術方案**：

- 新增 `src/pipeline/AutoTone.h/.cpp` — 從 RGBA 資料計算亮度直方圖，找 P1/P5/P50/P95/P99 百分位數，反推最佳參數
- 修改 `src/ui/DevelopPanel.cpp` — Tone 區段新增 "Auto" 按鈕

**依賴**：無
**複雜度**：中

---

### 2.6 Post-Crop Vignetting 與 Grain

暗角效果在裁切後套用（Amount / Midpoint / Roundness / Feather），Grain 底片顆粒效果（Amount / Size / Roughness）。

**技術方案**：

- 修改 `EditRecipe.h` — 暗角 4 參數 + 顆粒 3 參數
- 新增 `shaders/effects.hlsl` — 暗角：橢圓距離漸變遮罩；顆粒：hash 偽隨機噪聲
- 修改 GPU 管線：gamma 之後（sRGB 空間）插入

**依賴**：2.1 裁切（暗角需知裁切後中心）
**複雜度**：中

---

### 2.7 B&W Mix（黑白混合）

轉為黑白模式，8 通道滑桿控制各色彩在灰階中的明度權重。

**技術方案**：

- 修改 `EditRecipe.h` — `bool bw_mode` + `float bw_mix[8]`
- 修改 `shaders/hsl_adjust.hlsl` — HSLCB 新增 bw_mode 和 bw_mix，若啟用則輸出加權灰度
- 修改 DevelopPanel — HSL 區段新增 Color/B&W 切換

**依賴**：無
**複雜度**：低

---

## Phase 3：鏡頭校正與幾何變形

消除鏡頭光學失真（畸變、暗角、色差），提供透視校正功能。

---

### 3.1 Lensfun 鏡頭 Profile 自動校正

根據 EXIF（相機 + 鏡頭 + 焦距 + 光圈）自動查找 lensfun 資料庫對應的 profile，一鍵校正畸變、暗角、色差。

**技術方案**：

- 修改 `vcpkg.json` — 加入 `"lensfun"`
- 修改 `CMakeLists.txt` — `find_package(lensfun)`
- 新增 `src/pipeline/LensCorrection.h/.cpp` — 封裝 lensfun API，生成畸變反映射 UV map、暗角校正表、色差位移表，上傳為 GPU texture
- 修改 `EditRecipe.h` — `lens_profile_enabled`, `lens_distortion`, `lens_vignetting`, `lens_ca`, 手動畸變/暗角滑桿, defringe 參數
- 新增 `shaders/lens_correction.hlsl` — 根據 UV 映射表取樣（R/G/B 三通道各自 offset 處理色差），乘逆暗角表
- 管線位置：Demosaic -> **LensCorrection** -> WB+Exposure
- 修改 DevelopPanel — 新增 `renderLensCorrection()` 區段

**依賴**：需要 EXIF metadata（已有）
**複雜度**：高

---

### 3.2 去色差（Defringe）

手動去除紫邊/綠邊，不依賴鏡頭 profile。

**技術方案**：

- 整合到 `lens_correction.hlsl` — 偵測邊緣區域，對指定色相範圍降低飽和度或中值濾波

**依賴**：3.1（共用基礎設施）
**複雜度**：中

---

### 3.3 Transform / Upright

手動透視校正（Vertical / Horizontal / Rotate / Aspect / Scale / Offset），Upright 自動校正（Auto / Level / Vertical / Full / Guided）。

**技術方案**：

- 修改 `EditRecipe.h` — 7 個 transform 參數 + UprightMode enum
- 新增 `shaders/transform.hlsl` — 3x3 單應性矩陣（homography）反投影取樣
- 新增 `src/pipeline/Upright.h/.cpp` — Canny + Hough 偵測直線，分類水平/垂直，計算校正矩陣
- 管線位置：LensCorrection 之後、WB 之前

**依賴**：2.1 裁切（transform 需配合 crop 自動調整）
**複雜度**：高

---

## Phase 4：局部調整（遮罩系統）

建立完整的遮罩基礎架構，支援筆刷、漸層、範圍遮罩及布林運算。

---

### 4.1 遮罩基礎架構

遮罩系統核心引擎。管理多個遮罩層，每個遮罩有獨立的調整滑桿，遮罩以單通道 float texture 表示。

**技術方案**：

- 新增 `src/pipeline/MaskSystem.h/.cpp` — MaskLayer struct（type, parameters, local EditRecipe, mask texture SRV/UAV）+ MaskSystem class
- 修改 `EditRecipe.h` — 新增 `struct LocalAdjustment { mask_type, mask_params JSON, adjustments JSON }` 陣列
- 新增 `shaders/apply_mask.hlsl` — `output = lerp(original, adjusted, mask * opacity)`
- 修改 GPU 管線 — 全域調整完成後，對每個 mask layer：生成 mask -> 局部調整 -> apply_mask 合成
- 新增 `src/ui/MaskPanel.h/.cpp` — 遮罩列表 UI，局部調整滑桿

**依賴**：無
**複雜度**：非常高（架構性變更）

---

### 4.2 筆刷遮罩

可調 Size / Feather / Flow / Density 的筆刷，支援 Auto Mask（邊緣感知），橡皮擦模式。

**技術方案**：

- 修改 `ImageViewport.h/.cpp` — BrushMode 狀態，追蹤描邊點列表
- 新增 `shaders/generate_mask_brush.hlsl` — 描邊點上傳為 StructuredBuffer，計算最近距離生成 mask
- Auto Mask：取樣中心色，色差大的像素降低 mask 值

**依賴**：4.1
**複雜度**：高

---

### 4.3 線性漸層遮罩

拖曳畫出漸變帶，可旋轉、移動、調整寬度。

**技術方案**：

- 新增 `shaders/generate_mask_gradient.hlsl` — 投影距離 smoothstep 生成 0-1 漸變
- UI：三條線（起始/中/結束）可拖曳

**依賴**：4.1
**複雜度**：中

---

### 4.4 徑向漸層遮罩

橢圓形漸層，可調大小、位置、旋轉、羽化，可反轉。

**技術方案**：

- 共用 `generate_mask_gradient.hlsl`，使用橢圓距離函數

**依賴**：4.1
**複雜度**：中

---

### 4.5 亮度範圍遮罩

精修工具，基於像素亮度篩選遮罩範圍（min-max + smoothness）。

**技術方案**：

- 新增 `shaders/range_mask.hlsl` — 亮度 smoothstep 生成遮罩，可作為其他遮罩的 Intersect

**依賴**：4.1
**複雜度**：低

---

### 4.6 色彩範圍遮罩

點選取樣色 + 容許度，基於 deltaE 色差生成遮罩。

**技術方案**：

- 擴充 `range_mask.hlsl` — Lab 空間色差計算

**依賴**：4.1
**複雜度**：中

---

### 4.7 遮罩布林運算（Add / Subtract / Intersect）

組合多個遮罩來源。

**技術方案**：

- 新增 `shaders/mask_combine.hlsl` — Add=max, Subtract=clamp(a-b), Intersect=min

**依賴**：4.1
**複雜度**：低

---

## Phase 5：照片庫增強

提升大量照片的組織管理效率。

---

### 5.1 Collections（虛擬相簿）

不綁定磁碟位置的虛擬相簿，照片可同時存在多個 Collection。支援 Collection Sets（巢狀群組）。

**技術方案**：

- 修改 `Database.h/.cpp` — 新增 `collections` 和 `collection_photos` table + CRUD API
- 新增 `src/ui/CollectionPanel.h/.cpp` — 樹狀結構顯示，右鍵選單，拖放照片
- 修改 `GridView.h/.cpp` — 支援 collection 篩選

**依賴**：無
**複雜度**：中

---

### 5.2 Smart Collections

規則式動態相簿（評分 >= X、色標 = X、相機型號、日期範圍、焦距/ISO 範圍）。

**技術方案**：

- 修改 `Database.h/.cpp` — `smart_rules` JSON 欄位 + `evaluateSmartCollection()` 動態 SQL
- Smart Collection 建立對話框：多條件組合 UI

**依賴**：5.1
**複雜度**：中

---

### 5.3 Keyword Tagging

階層式關鍵字標記，自動完成，批次套用。

**技術方案**：

- 修改 `Database.h/.cpp` — `keywords` + `photo_keywords` table（支援 parent_id 階層），納入 FTS5 索引
- 新增 `src/ui/KeywordPanel.h/.cpp` — 樹狀結構 + 文字輸入 + 自動完成

**依賴**：無
**複雜度**：中

---

### 5.4 Compare View（雙圖比較）

並排 Select vs Candidate，同步縮放平移，上/下鍵切換 Candidate。

**技術方案**：

- 新增 `src/ui/CompareView.h/.cpp` — 左右分割，同步 pan/zoom
- 修改 `main.cpp` — 新增 ViewMode enum

**依賴**：無
**複雜度**：中

---

### 5.5 Survey View（多圖並排）

同時顯示 2-12 張大預覽，按 X 移除不喜歡的。

**技術方案**：

- 新增 `src/ui/SurveyView.h/.cpp` — 自動排版演算法

**依賴**：5.4
**複雜度**：中

---

### 5.6 Filmstrip

Develop 模式底部水平捲動縮圖條，可快速切換照片。

**技術方案**：

- 新增 `src/ui/Filmstrip.h/.cpp` — 水平虛擬捲動，固定高度（~80-100px）
- 修改 `main.cpp` — dockspace 底部新增 Filmstrip 視窗

**依賴**：無
**複雜度**：低

---

### 5.7 批次操作（Copy / Paste / Sync Settings）

複製/貼上/同步 Develop 設定到多張照片，可選擇群組。

**技術方案**：

- 新增 `src/pipeline/SettingsClipboard.h/.cpp` — EditRecipe 副本 + 選擇性貼上 bitmask
- 修改 `GridView.h/.cpp` — 多選（Ctrl/Shift+Click），右鍵選單
- 新增 `SettingsSyncDialog` — 勾選同步群組
- Ctrl+C / Ctrl+V 快捷鍵

**依賴**：無
**複雜度**：中

---

### 5.8 Metadata 瀏覽器

多欄顯示 Camera / Lens / Date / ISO / Focal Length 分佈，點選篩選。

**技術方案**：

- 修改 `Database.h/.cpp` — aggregation 查詢 API
- 新增 `src/ui/MetadataBrowser.h/.cpp` — 多欄可捲動列表

**依賴**：無
**複雜度**：中

---

## Phase 6：匯出增強

---

### 6.1 色彩空間選擇

sRGB / Adobe RGB / ProPhoto RGB / Display P3。

**技術方案**：

- 修改 `shaders/gamma_output.hlsl` — CB 新增色空間 3x3 矩陣和 gamma 值
- 修改 ExportDialog — 下拉選單
- 修改 ExportManager — lcms2 嵌入 ICC profile

**依賴**：無
**複雜度**：中

---

### 6.2 輸出銳化

Screen / Matte Paper / Glossy Paper x Low / Standard / High。

**技術方案**：

- 修改 ExportManager — resize 後套用 Unsharp Mask（CPU）
- 修改 ExportDialog — 銳化選項

**依賴**：無
**複雜度**：低

---

### 6.3 浮水印

文字或圖片浮水印，可調位置（9 宮格）、透明度、大小。

**技術方案**：

- 修改 ExportManager — 文字用 DirectWrite 渲染，圖片載入 + alpha blend
- 修改 ExportDialog — 浮水印設定 + 預覽

**依賴**：無
**複雜度**：中

---

### 6.4 匯出預設

儲存/載入完整匯出設定組態。

**技術方案**：

- 新增 `src/export/ExportPresets.h/.cpp` — 儲存於 UIStateDB
- 修改 ExportDialog — 預設下拉選單

**依賴**：無
**複雜度**：低

---

### 6.5 匯出後動作

開啟資料夾 / 開啟應用程式 / 不做。

**技術方案**：

- 修改 ExportManager — `ShellExecute()` 開啟資料夾或應用程式

**依賴**：無
**複雜度**：低

---

### 6.6 DNG 匯出

匯出為 DNG 格式（含 XMP metadata）。

**技術方案**：

- 需要 DNG SDK 或 LibRaw DNG 寫入功能
- 建議推遲到 HDR Merge 需要時一併實作

**依賴**：Phase 7 HDR Merge
**複雜度**：高

---

## Phase 7：進階功能

---

### 7.1 Presets / Profiles 系統

Profiles：色彩風格（Camera Matching / Creative / Monochrome）。Presets：儲存的 Develop 設定組合，hover 預覽，分類管理。

**技術方案**：

- 新增 `src/pipeline/PresetManager.h/.cpp` — `.vgp` JSON 檔案管理
- 新增 `src/pipeline/CameraProfile.h/.cpp` — 相機色彩 profile（DNG Color Matrix 或 ICC）
- 新增 `src/ui/PresetPanel.h/.cpp` — hover 預覽（低解析度即時套用），強度滑桿

**依賴**：無
**複雜度**：中

---

### 7.2 HDR Merge

多張曝光包圍合成 HDR DNG。

**技術方案**：

- 新增 `src/pipeline/HDRMerge.h/.cpp` — 解碼全部 RAW → MTB 對齊 → Debevec 輻射度映射或 Mertens fusion → 鬼影偵測 → 輸出 32-bit float
- GridView 多選 + 右鍵 "Photo Merge > HDR"

**依賴**：5.7 多選
**複雜度**：非常高

---

### 7.3 Panorama Merge

多張重疊照片拼接全景（球面 / 圓柱 / 透視投影）。

**技術方案**：

- 新增 `src/pipeline/PanoramaMerge.h/.cpp` — 特徵偵測（FAST/ORB）→ 匹配 → Bundle Adjustment → 投影 → Laplacian Blending
- 考慮加入 `opencv4[stitching]` vcpkg 依賴

**依賴**：5.7 多選
**複雜度**：非常高

---

### 7.4 AI 遮罩（Select Subject / Sky / Background）

一鍵 AI 偵測主體/天空/背景，自動生成遮罩。

**技術方案**：

- 新增 vcpkg 依賴 `onnxruntime` 或使用 DirectML
- 新增 `src/ai/ModelInference.h/.cpp` — ONNX Runtime 初始化 + 載入模型 + 推理
- 新增 `src/ai/SegmentationModels.h/.cpp` — U2-Net / IS-Net 語義分割
- 模型需隨 app 發佈（~40-100MB .onnx）

**依賴**：Phase 4 遮罩基礎架構
**複雜度**：非常高

---

### 7.5 AI Denoise

一鍵 AI 降噪。

**技術方案**：

- 新增 `src/ai/AIDenoise.h/.cpp` — NAFNet / Restormer ONNX 推理，tile-based 分塊處理
- 修改 DevelopPanel — Detail 區段新增 "AI Denoise" 按鈕 + 強度
- GPU 推理使用 DirectML backend

**依賴**：7.4（共用 ONNX Runtime）
**複雜度**：高

---

## Phase 8：工作流程與打磨

---

### 8.1 History Panel

完整編輯歷史列表，可點擊跳轉任一步驟。

**技術方案**：

- 修改 `EditHistory.h/.cpp` — 新增 `getEntries()` 和 `jumpTo(index)`
- 新增 `src/ui/HistoryPanel.h/.cpp` — 捲動列表，高亮目前位置

**依賴**：無
**複雜度**：低

---

### 8.2 Snapshots

命名快照書籤，可一鍵恢復。

**技術方案**：

- 修改 `EditHistory.h` — `Snapshot { name, recipe, timestamp }`
- 序列化到 .vgr sidecar 的 `"snapshots"` 欄位
- 新增 `src/ui/SnapshotPanel.h/.cpp` — 列表 + hover 預覽

**依賴**：無
**複雜度**：低

---

### 8.3 XMP Sidecar 相容

讀寫標準 XMP，使 Vega 編輯可被 Lightroom/ACR 讀取。

**技術方案**：

- 新增 `src/pipeline/XMPBridge.h/.cpp` — 使用 exiv2 XMP 功能
- EditRecipe <-> Camera Raw namespace (`crs:`) 映射表
- `loadRecipe()` fallback：無 .vgr 時嘗試讀 .xmp

**依賴**：exiv2 XMP（已有依賴）
**複雜度**：中

---

### 8.4 Virtual Copies

同一 RAW 多組獨立編輯版本。

**技術方案**：

- 修改 `Database.h/.cpp` — `virtual_copy_id` + `virtual_copy_of` 欄位
- GridView 以堆疊方式顯示
- 獨立 .vgr 檔案（`photo.vc1.vgr`）

**依賴**：無
**複雜度**：中

---

### 8.5 Tethered / Watched Folder

連接相機或監控資料夾自動匯入。

**技術方案**：

- 新增 `src/catalog/FolderWatcher.h/.cpp` — `ReadDirectoryChangesW()` Win32 API
- 偵測新 RAW 自動匯入
- 完整 Tethered Capture 需要相機 SDK（Canon EDSDK / Nikon SDK）

**依賴**：無
**複雜度**：中（Watched Folder）/ 非常高（完整 Tethered）

---

### 8.6 Reference View

編輯時釘選參考圖並排顯示，方便統一風格。

**技術方案**：

- 修改 `BeforeAfter.h/.cpp` — 擴充 Reference 模式

**依賴**：無
**複雜度**：低

---

### 8.7 批次重新命名

多選照片按模板批次重命名。

**技術方案**：

- 新增 `src/ui/BatchRenameDialog.h/.cpp` — 模板系統 + 預覽
- 修改 `Database.cpp` — `updateFilePath()` 方法

**依賴**：5.7 多選
**複雜度**：中

---

## 實作優先順序

| 序 | 功能 | Phase | 複雜度 |
|----|------|-------|--------|
| 1 | Crop & Straighten | 2.1 | 高 |
| 2 | Clarity / Texture / Dehaze | 2.2 | 高 |
| 3 | WB Presets | 2.3 | 低 |
| 4 | Color Grading | 2.4 | 中 |
| 5 | Auto Tone | 2.5 | 中 |
| 6 | Vignette & Grain | 2.6 | 中 |
| 7 | B&W Mix | 2.7 | 低 |
| 8 | Lensfun 鏡頭校正 | 3.1 | 高 |
| 9 | Defringe | 3.2 | 中 |
| 10 | Transform / Upright | 3.3 | 高 |
| 11 | Copy/Paste/Sync | 5.7 | 中 |
| 12 | Collections | 5.1 | 中 |
| 13 | Keywords | 5.3 | 中 |
| 14 | Filmstrip | 5.6 | 低 |
| 15 | History Panel | 8.1 | 低 |
| 16 | Snapshots | 8.2 | 低 |
| 17 | Compare View | 5.4 | 中 |
| 18 | Survey View | 5.5 | 中 |
| 19 | 色彩空間選擇 | 6.1 | 中 |
| 20 | Output Sharpening | 6.2 | 低 |
| 21 | Watermark | 6.3 | 中 |
| 22 | Export Presets | 6.4 | 低 |
| 23 | Mask Infrastructure | 4.1 | 非常高 |
| 24 | Brush Mask | 4.2 | 高 |
| 25 | Linear Gradient | 4.3 | 中 |
| 26 | Radial Gradient | 4.4 | 中 |
| 27 | Luminance Range | 4.5 | 低 |
| 28 | Color Range | 4.6 | 中 |
| 29 | Mask Boolean Ops | 4.7 | 低 |
| 30 | Presets/Profiles | 7.1 | 中 |
| 31 | XMP 相容 | 8.3 | 中 |
| 32 | Virtual Copies | 8.4 | 中 |
| 33 | Smart Collections | 5.2 | 中 |
| 34 | Metadata Browser | 5.8 | 中 |
| 35 | Watched Folder | 8.5 | 中 |
| 36 | Reference View | 8.6 | 低 |
| 37 | 批次重命名 | 8.7 | 中 |
| 38 | HDR Merge | 7.2 | 非常高 |
| 39 | Panorama Merge | 7.3 | 非常高 |
| 40 | AI Masking | 7.4 | 非常高 |
| 41 | AI Denoise | 7.5 | 高 |
| 42 | 匯出後動作 | 6.5 | 低 |
| 43 | DNG 匯出 | 6.6 | 高 |

---

## 新增檔案清單

### 原始碼

| 檔案 | Phase | 用途 |
|------|-------|------|
| `src/ui/CropOverlay.h/.cpp` | 2.1 | 裁切覆蓋層 UI |
| `src/pipeline/cpu/CropRotateNode.h/.cpp` | 2.1 | CPU 裁切旋轉 |
| `src/pipeline/cpu/PresenceNode.h/.cpp` | 2.2 | CPU Clarity/Texture/Dehaze |
| `src/pipeline/cpu/ColorGradingNode.h/.cpp` | 2.4 | CPU 色彩分級 |
| `src/pipeline/AutoTone.h/.cpp` | 2.5 | 自動色調 |
| `src/pipeline/LensCorrection.h/.cpp` | 3.1 | Lensfun 封裝 |
| `src/pipeline/cpu/LensCorrectionNode.h/.cpp` | 3.1 | CPU 鏡頭校正 |
| `src/pipeline/Upright.h/.cpp` | 3.3 | 自動透視校正 |
| `src/pipeline/MaskSystem.h/.cpp` | 4.1 | 遮罩引擎 |
| `src/ui/MaskPanel.h/.cpp` | 4.1 | 遮罩管理 UI |
| `src/ui/CollectionPanel.h/.cpp` | 5.1 | 虛擬相簿面板 |
| `src/ui/KeywordPanel.h/.cpp` | 5.3 | 關鍵字面板 |
| `src/ui/CompareView.h/.cpp` | 5.4 | 雙圖比較 |
| `src/ui/SurveyView.h/.cpp` | 5.5 | 多圖評選 |
| `src/ui/Filmstrip.h/.cpp` | 5.6 | 底部膠片條 |
| `src/pipeline/SettingsClipboard.h/.cpp` | 5.7 | 設定複製貼上 |
| `src/ui/MetadataBrowser.h/.cpp` | 5.8 | Metadata 瀏覽器 |
| `src/export/ExportPresets.h/.cpp` | 6.4 | 匯出預設 |
| `src/pipeline/PresetManager.h/.cpp` | 7.1 | Develop 預設 |
| `src/pipeline/CameraProfile.h/.cpp` | 7.1 | 相機 Profile |
| `src/ui/PresetPanel.h/.cpp` | 7.1 | 預設面板 |
| `src/pipeline/HDRMerge.h/.cpp` | 7.2 | HDR 合併 |
| `src/pipeline/PanoramaMerge.h/.cpp` | 7.3 | 全景接圖 |
| `src/ai/ModelInference.h/.cpp` | 7.4 | ONNX Runtime |
| `src/ai/SegmentationModels.h/.cpp` | 7.4 | AI 分割 |
| `src/ai/AIDenoise.h/.cpp` | 7.5 | AI 降噪 |
| `src/ui/HistoryPanel.h/.cpp` | 8.1 | 歷史面板 |
| `src/ui/SnapshotPanel.h/.cpp` | 8.2 | 快照面板 |
| `src/pipeline/XMPBridge.h/.cpp` | 8.3 | XMP 讀寫 |
| `src/catalog/FolderWatcher.h/.cpp` | 8.5 | 資料夾監控 |
| `src/ui/BatchRenameDialog.h/.cpp` | 8.7 | 批次重命名 |

### Shader

| 檔案 | Phase | 用途 |
|------|-------|------|
| `shaders/crop_rotate.hlsl` | 2.1 | 裁切旋轉 |
| `shaders/presence.hlsl` | 2.2 | Clarity/Texture/Dehaze |
| `shaders/color_grading.hlsl` | 2.4 | 色彩分級 |
| `shaders/effects.hlsl` | 2.6 | 暗角 + 顆粒 |
| `shaders/lens_correction.hlsl` | 3.1 | 鏡頭校正 |
| `shaders/transform.hlsl` | 3.3 | 幾何變形 |
| `shaders/apply_mask.hlsl` | 4.1 | 遮罩合成 |
| `shaders/generate_mask_brush.hlsl` | 4.2 | 筆刷遮罩 |
| `shaders/generate_mask_gradient.hlsl` | 4.3/4.4 | 漸層遮罩 |
| `shaders/range_mask.hlsl` | 4.5/4.6 | 範圍遮罩 |
| `shaders/mask_combine.hlsl` | 4.7 | 遮罩布林 |

---

## 關鍵架構決策

### 1. GPU 管線重構

目前為固定 7-pass 序列。建議重構為可配置的 pass 陣列：

```cpp
struct PipelinePass {
    ComputeShader* shader;
    std::function<void(ID3D11DeviceContext*)> bindCB;
    bool enabled;
};
std::vector<PipelinePass> passes_;
```

完整管線序列（Phase 2-4 完成後）：

```
Demosaic -> LensCorrection -> Transform -> WB+Exposure -> Presence ->
ToneCurve -> HSL/B&W -> ColorGrading -> Denoise -> Sharpen ->
CropRotate -> Gamma -> Effects -> [per-mask: partial pipeline + apply_mask]
```

### 2. EditRecipe 版本遷移

.vgr 已有 `recipe_version` 欄位（目前值 1）。每次新增欄位時遞增版本號，`fromJson()` 對舊版本提供 default fallback。

### 3. DB Schema Migration

`createTables()` 新增 migration 機制：

```cpp
void Database::migrate() {
    int version = getUserVersion(); // PRAGMA user_version
    if (version < 2) {
        exec("ALTER TABLE photos ADD COLUMN virtual_copy_id INTEGER DEFAULT 0");
        exec("CREATE TABLE IF NOT EXISTS collections (...)");
        // ...
        setUserVersion(2);
    }
}
```

### 4. i18n 擴充

每個新 UI 區段需在 `core/i18n.h` 新增 string key，在 `core/i18n.cpp` 新增英文和中文翻譯。
