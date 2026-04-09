# Vega vs Adobe Lightroom Classic -- 功能對比

以 Adobe Lightroom Classic (v15.x, 2025) 為基準，列出 Vega 目前已實作與尚缺的功能。

符號說明：
- [x] Vega 已實作
- [ ] Vega 尚未實作
- [~] Vega 部分實作

---

## 1. 照片庫管理（Library）

### 1.1 匯入
- [x] 從磁碟資料夾匯入
- [ ] 從記憶卡 / USB 裝置匯入
- [ ] 連線拍攝（Tethered Capture）
- [ ] 監控資料夾（Watched Folder）自動匯入
- [ ] 匯入時選擇複製 / 移動 / 新增 / 轉 DNG
- [ ] 匯入時套用 Develop 預設
- [ ] 匯入時套用 Metadata 範本 / 關鍵字
- [ ] 匯入時檔案重新命名
- [ ] 匯入時選擇預覽品質（Minimal / Embedded / Standard / 1:1）

### 1.2 瀏覽模式
- [x] Grid View（縮圖網格）
- [~] Loupe View（單張放大）-- Develop 模式有，Library 模式無獨立 Loupe
- [ ] Compare View（雙圖比較：Select vs Candidate）
- [ ] Survey View（多圖並排評選）
- [ ] People View（人臉辨識）
- [~] Filmstrip（底部膠片條）-- 無

### 1.3 組織管理
- [x] Folders Panel（資料夾面板）
- [ ] Collections（虛擬相簿，不綁定磁碟位置）
- [ ] Collection Sets（相簿群組，階層式）
- [ ] Smart Collections（規則式動態相簿）
- [ ] Quick Collection（暫存相簿）
- [ ] Stacking（疊圖：依拍攝時間 / 視覺相似度自動堆疊）
- [ ] Virtual Copies（同一檔案多組編輯版本）
- [x] Color Labels（6 色標籤）
- [x] Star Ratings（0-5 星）
- [x] Flags（Pick / Reject / Unflagged）
- [ ] Assisted Culling（AI 自動篩選最佳照片）

### 1.4 Metadata 與關鍵字
- [x] EXIF 顯示（相機、鏡頭、參數、GPS、日期）
- [ ] IPTC 編輯（標題、版權、作者、聯絡資訊）
- [ ] 關鍵字標記（Keyword Tagging）
- [ ] 關鍵字階層（Parent / Child）
- [ ] 關鍵字建議 / 自動完成
- [ ] Metadata 範本 / 預設
- [ ] 關鍵字噴灑工具（Painter）

### 1.5 搜尋與篩選
- [x] 依評分 / 色彩標籤 / 旗標篩選
- [~] 文字搜尋 -- FTS5 支援（視 SQLite 編譯），有 LIKE fallback
- [ ] Library Filter Bar（多條件組合篩選）
- [ ] Metadata 瀏覽器（依相機/鏡頭/日期/ISO/焦距分欄瀏覽）
- [ ] Smart Collections 當作儲存的搜尋

### 1.6 人臉辨識
- [ ] AI 人臉偵測與索引
- [ ] 人臉分群辨識
- [ ] 命名管理
- [ ] 確認後自動加關鍵字

### 1.7 地圖模組
- [ ] 地圖顯示 GPS 照片（Google Maps）
- [ ] 拖放定位
- [ ] GPX 軌跡匯入自動標記
- [ ] 儲存地圖位置

---

## 2. 影像處理（Develop）

### 2.1 工具列
- [ ] 裁切與拉直（Crop & Straighten）：長寬比預設、自動拉直、裁切覆蓋引導線
- [ ] 修復工具（Healing / Clone / Content-Aware Remove）
- [ ] 紅眼校正
- [ ] 遮罩工具（Masking）-- 見 2.3

### 2.2 全域調整面板

**直方圖**
- [x] 即時直方圖（R/G/B/亮度）
- [x] 過曝/欠曝警告
- [ ] 互動式直方圖（拖拽區域調整曝光/亮暗部）
- [ ] 滑鼠下方 RGB 數值讀取

**基本面板**
- [x] 白平衡色溫 / 色調
- [x] WB 吸管取樣
- [ ] WB 預設（As Shot / Auto / Daylight / Cloudy / Shade / Tungsten / Fluorescent / Flash）
- [x] 曝光（-5 to +5）
- [x] 對比
- [x] 亮部 / 暗部 / 白色 / 黑色
- [ ] Texture（紋理）
- [ ] Clarity（清晰度 / 微對比）
- [ ] Dehaze（去霧）
- [x] Vibrance（自然飽和度）
- [x] Saturation（飽和度）
- [ ] Auto Tone 按鈕

**色調曲線**
- [x] Point Curve（自由控制點，RGB + 個別通道）
- [ ] Parametric Curve（Highlights / Lights / Darks / Shadows 滑桿 + 可調範圍）
- [ ] Target Adjustment Tool（點圖調曲線）
- [ ] 曲線預設（Linear / Medium / Strong Contrast）

**HSL**
- [x] HSL 8 通道（色相/飽和度/明度）
- [ ] Target Adjustment Tool（點圖調 HSL）
- [ ] B&W Mix（黑白混合模式）

**Color Grading**
- [ ] 三輪色彩分級（陰影/中間調/亮部 色輪）
- [ ] Global 色輪
- [ ] Blending / Balance 滑桿

**細節面板**
- [x] 銳利化（Amount / Radius / Detail / Masking）
- [x] 降噪 -- 亮度 / 色彩 / 細節
- [ ] AI Denoise（一鍵 AI 降噪，產生 DNG）
- [ ] Raw Details / Enhance（AI 細節增強）
- [ ] Super Resolution（2x AI 超解析度）

**鏡頭校正**
- [ ] 鏡頭 Profile 自動校正（畸變/暗角/色差）
- [ ] 手動畸變校正
- [ ] 去色差（Defringe）
- [ ] 手動暗角校正

**變形 / 幾何**
- [ ] Upright 自動校正（Auto / Level / Vertical / Full / Guided）
- [ ] 手動變形（垂直/水平/旋轉/長寬比/縮放/偏移）

**效果面板**
- [ ] Post-Crop Vignetting（裁切後暗角）
- [ ] Grain（底片顆粒）

**校準面板**
- [ ] Camera Profile 選擇器
- [ ] 陰影色調 / RGB Primary 色相飽和度微調

### 2.3 遮罩（局部調整）

**AI 遮罩**
- [ ] Select Subject（AI 主體偵測）
- [ ] Select Sky（AI 天空偵測）
- [ ] Select Background（AI 背景偵測）
- [ ] Select People（AI 人物偵測 + 細部：臉部皮膚/眉毛/眼睛/嘴唇/牙齒/頭髮/衣物）
- [ ] Select Objects（框選物件 AI 遮罩）
- [ ] Select Landscape（AI 7 種地景元素）

**手動遮罩**
- [ ] Brush（筆刷 + Auto Mask + 橡皮擦）
- [ ] Linear Gradient（線性漸層）
- [ ] Radial Gradient（徑向漸層）

**遮罩範圍精修**
- [ ] Color Range（色彩範圍遮罩）
- [ ] Luminance Range（亮度範圍遮罩）
- [ ] Depth Range（景深範圍遮罩）

**遮罩操作**
- [ ] Add / Subtract / Intersect 組合遮罩
- [ ] Invert / Duplicate / Rename
- [ ] 遮罩覆蓋顯示（多種模式）

**局部調整滑桿**
- [ ] 遮罩內：基本面板所有參數 + Sharpness / Noise / Moire / Defringe / Color

### 2.4 生成式 AI
- [ ] Generative Remove（AI 內容感知移除）
- [ ] People Removal（一鍵移除人物）
- [ ] Reflection Removal（反射移除）

### 2.5 合併功能
- [ ] HDR Merge（曝光包圍合成 HDR DNG）
- [ ] Panorama Merge（全景接圖：球面/圓柱/透視）
- [ ] HDR Panorama Merge

### 2.6 預設與 Profile
- [ ] Camera Matching Profiles（製造商風格模擬）
- [ ] Adobe 內建 Profiles（Color / Landscape / Portrait / Vivid / Monochrome）
- [ ] Adaptive Profiles（AI 自適應 Profile）
- [ ] Develop Presets（含分類、匯入匯出、hover 預覽）
- [ ] Adaptive Presets（AI 遮罩 + 調整組合）
- [ ] ISO Adaptive Presets（依 ISO 自動套用）
- [ ] Preset 強度滑桿

### 2.7 歷史與快照
- [x] Undo / Redo（200 步）
- [ ] History Panel（完整歷史列表，可點選任一步）
- [ ] Snapshots（命名書籤，可切換）
- [x] Before / After 比較（三種模式）
- [ ] Reference View（釘選參考圖）
- [ ] Copy / Paste Settings（跨照片複製設定）
- [ ] Sync Settings（批次同步設定）
- [ ] Auto Sync 模式

---

## 3. 匯出

- [x] JPEG（可調品質）
- [x] PNG 8-bit
- [x] TIFF 8/16-bit
- [ ] PSD 8/16-bit
- [ ] DNG
- [x] 輸出尺寸調整（長邊/短邊/百分比）
- [ ] 色彩空間選擇（sRGB / Adobe RGB / ProPhoto RGB / Display P3）
- [ ] 輸出銳化（Screen / Matte / Glossy，三段強度）
- [x] 檔名範本
- [x] EXIF 保留
- [x] GPS 移除選項
- [ ] 浮水印（文字 / 圖片，位置/透明度/大小）
- [ ] 匯出後動作（開啟資料夾 / 開啟應用程式）
- [ ] 匯出預設（儲存/載入匯出組態）
- [ ] Publish Services（持續同步到線上平台）
- [ ] 影片匯出

---

## 4. 列印模組
- [ ] 版面配置（Single / Contact Sheet / Picture Package）
- [ ] 邊界 / 間距 / 行列數
- [ ] 列印銳化
- [ ] 色彩管理（ICC Profile / 算繪意圖）
- [ ] 列印到 JPEG
- [ ] 版面範本

## 5. Book 模組
- [ ] 整合 Blurb 相簿
- [ ] 自動排版
- [ ] 頁面範本
- [ ] 文字 / 背景設定
- [ ] 匯出 PDF / JPEG

## 6. Slideshow 模組
- [ ] 幻燈片播放
- [ ] 轉場效果
- [ ] 背景音樂
- [ ] 匯出 MP4

## 7. Web 模組
- [ ] HTML Gallery 產生
- [ ] FTP 上傳

---

## 8. 效能

- [x] GPU 加速影像處理（DX11 Compute Shader 7-pass）
- [x] GPU Bayer Demosaic（~5ms 取代 CPU ~500ms）
- [x] 漸進式預覽（1/8 即時 + 背景全解析度）
- [x] 多執行緒縮圖解碼（worker pool, CPU cores 數量）
- [ ] Smart Previews（Lossy DNG 代理，~2MB，離線編輯）
- [ ] GPU 加速預覽產生
- [ ] GPU 加速匯出
- [ ] Catalog 最佳化 / 完整性檢查
- [ ] Camera Raw Cache 大小設定

---

## 9. 整合功能

- [ ] Lightroom Cloud 同步（行動版 / Web 版雙向同步）
- [ ] Edit in Photoshop（round-trip 往返編輯）
- [ ] Open as Smart Object
- [ ] 外部編輯器支援（設定第三方應用程式）
- [ ] Plugin 架構（SDK 擴充）
- [ ] Publish Services（Flickr / SmugMug / Adobe Stock）
- [ ] Adobe Firefly AI 整合

---

## 10. 工作流程與工具

- [x] 非破壞性編輯（.vgr sidecar）
- [ ] XMP sidecar 支援（Lightroom / ACR 相容）
- [ ] 批次 Develop 設定（Sync / Auto Sync / Copy-Paste）
- [ ] 批次重新命名
- [ ] 批次 Metadata 套用
- [ ] 多 Catalog 支援
- [ ] Catalog 匯入 / 匯出
- [ ] 找遺失照片 / 同步資料夾
- [ ] Catalog 備份排程
- [x] 中文 / 英文雙語
- [ ] 完整多國語系

---

## 統計摘要

| 類別 | Lightroom 功能數 | Vega 已有 | 覆蓋率 |
|------|-----------------|-----------|--------|
| Library 管理 | ~40 | 10 | ~25% |
| Develop 全域調整 | ~50 | 18 | ~36% |
| Masking 局部調整 | ~25 | 0 | 0% |
| AI / 生成式 | ~10 | 0 | 0% |
| 預設 / Profile | ~10 | 0 | 0% |
| 匯出 | ~15 | 7 | ~47% |
| 列印/Book/Slideshow/Web | ~20 | 0 | 0% |
| 效能 | ~10 | 4 | ~40% |
| 整合 | ~10 | 0 | 0% |
| 工作流程 | ~15 | 3 | ~20% |
| **總計** | **~205** | **~42** | **~20%** |

---

## 建議優先順序

以下是建議的實作優先順序，依使用者影響力排列：

### 高優先（核心攝影工作流程缺失）
1. **Crop & Straighten** -- 裁切是最基本的編輯操作
2. **Clarity / Texture / Dehaze** -- 三個常用的 Presence 控制
3. **Lens Profile 校正** -- 幾乎所有攝影師都需要
4. **WB 預設** -- As Shot / Daylight / Cloudy 等快速選擇
5. **Copy / Paste / Sync Settings** -- 批次處理效率關鍵
6. **Color Grading** -- 色彩分級是進階調色標配

### 中優先（提升管理效率）
7. **Collections / Smart Collections** -- 虛擬相簿管理
8. **Keyword Tagging** -- 大量照片管理必備
9. **Compare / Survey View** -- 篩選照片時很實用
10. **History Panel** -- 視覺化完整歷史
11. **Export Presets / Watermark** -- 匯出效率
12. **Local Adjustments (Brush / Gradient)** -- 局部調整

### 低優先（進階 / 利基功能）
13. **AI Masking** (Subject / Sky / People)
14. **HDR / Panorama Merge**
15. **AI Denoise / Super Resolution**
16. **Tethered Capture**
17. **Print / Book / Slideshow / Web modules**
18. **Cloud Sync / Photoshop Integration**
