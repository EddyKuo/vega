-- Vega i18n language database seed
-- This file is used at build time to generate language.db

CREATE TABLE IF NOT EXISTS languages (
    code    TEXT PRIMARY KEY,
    name    TEXT NOT NULL,
    native  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS translations (
    lang    TEXT NOT NULL,
    key     TEXT NOT NULL,
    value   TEXT NOT NULL,
    PRIMARY KEY (lang, key),
    FOREIGN KEY (lang) REFERENCES languages(code)
);

CREATE INDEX IF NOT EXISTS idx_translations_lang ON translations(lang);

-- Supported languages
INSERT INTO languages VALUES ('en',    'English',             'English');
INSERT INTO languages VALUES ('zh_tw', 'Traditional Chinese', '繁體中文');

-- ============================================================================
-- English (en)
-- ============================================================================

-- Menu
INSERT INTO translations VALUES ('en', 'menu.file',         'File');
INSERT INTO translations VALUES ('en', 'menu.open',         'Open RAW...');
INSERT INTO translations VALUES ('en', 'menu.save_recipe',  'Save Recipe');
INSERT INTO translations VALUES ('en', 'menu.export',       'Export...');
INSERT INTO translations VALUES ('en', 'menu.exit',         'Exit');
INSERT INTO translations VALUES ('en', 'menu.edit',         'Edit');
INSERT INTO translations VALUES ('en', 'menu.undo',         'Undo');
INSERT INTO translations VALUES ('en', 'menu.redo',         'Redo');
INSERT INTO translations VALUES ('en', 'menu.reset_all',    'Reset All');
INSERT INTO translations VALUES ('en', 'menu.view',         'View');
INSERT INTO translations VALUES ('en', 'menu.fit',          'Fit to Window');
INSERT INTO translations VALUES ('en', 'menu.before_after', 'Before/After');
INSERT INTO translations VALUES ('en', 'menu.language',     'Language');

-- Panels
INSERT INTO translations VALUES ('en', 'panel.develop',     'Develop');
INSERT INTO translations VALUES ('en', 'panel.viewport',    'Viewport');
INSERT INTO translations VALUES ('en', 'panel.histogram',   'Histogram');
INSERT INTO translations VALUES ('en', 'panel.metadata',    'Metadata');
INSERT INTO translations VALUES ('en', 'panel.folders',     'Folders');
INSERT INTO translations VALUES ('en', 'panel.grid',        'Library');

-- Folder
INSERT INTO translations VALUES ('en', 'folder.add',        '+ Add Folder');
INSERT INTO translations VALUES ('en', 'folder.remove',     'Remove Folder');
INSERT INTO translations VALUES ('en', 'folder.all_photos', 'All Photos');
INSERT INTO translations VALUES ('en', 'import.progress',   'Importing: %d / %d');

-- White Balance
INSERT INTO translations VALUES ('en', 'wb.header',         'White Balance');
INSERT INTO translations VALUES ('en', 'wb.temperature',    'Temperature');
INSERT INTO translations VALUES ('en', 'wb.tint',           'Tint');
INSERT INTO translations VALUES ('en', 'wb.eyedropper',     'WB Eyedropper');

-- Tone
INSERT INTO translations VALUES ('en', 'tone.header',       'Tone');
INSERT INTO translations VALUES ('en', 'tone.exposure',     'Exposure');
INSERT INTO translations VALUES ('en', 'tone.contrast',     'Contrast');
INSERT INTO translations VALUES ('en', 'tone.highlights',   'Highlights');
INSERT INTO translations VALUES ('en', 'tone.shadows',      'Shadows');
INSERT INTO translations VALUES ('en', 'tone.whites',       'Whites');
INSERT INTO translations VALUES ('en', 'tone.blacks',       'Blacks');
INSERT INTO translations VALUES ('en', 'tone.auto',         'Auto');

-- Crop & Straighten
INSERT INTO translations VALUES ('en', 'crop.header',   'Crop & Straighten');
INSERT INTO translations VALUES ('en', 'crop.free',     'Free');
INSERT INTO translations VALUES ('en', 'crop.reset',    'Reset Crop');

-- Presence
INSERT INTO translations VALUES ('en', 'presence.header',  'Presence');
INSERT INTO translations VALUES ('en', 'presence.clarity', 'Clarity');
INSERT INTO translations VALUES ('en', 'presence.texture', 'Texture');
INSERT INTO translations VALUES ('en', 'presence.dehaze',  'Dehaze');

-- Tone Curve
INSERT INTO translations VALUES ('en', 'curve.header',      'Tone Curve');

-- HSL
INSERT INTO translations VALUES ('en', 'hsl.header',        'HSL / Color');
INSERT INTO translations VALUES ('en', 'hsl.hue',           'Hue');
INSERT INTO translations VALUES ('en', 'hsl.saturation',    'Saturation');
INSERT INTO translations VALUES ('en', 'hsl.luminance',     'Luminance');
INSERT INTO translations VALUES ('en', 'hsl.red',           'Red');
INSERT INTO translations VALUES ('en', 'hsl.orange',        'Orange');
INSERT INTO translations VALUES ('en', 'hsl.yellow',        'Yellow');
INSERT INTO translations VALUES ('en', 'hsl.green',         'Green');
INSERT INTO translations VALUES ('en', 'hsl.aqua',          'Aqua');
INSERT INTO translations VALUES ('en', 'hsl.blue',          'Blue');
INSERT INTO translations VALUES ('en', 'hsl.purple',        'Purple');
INSERT INTO translations VALUES ('en', 'hsl.magenta',       'Magenta');
INSERT INTO translations VALUES ('en', 'hsl.color_mode',    'Color');
INSERT INTO translations VALUES ('en', 'hsl.bw_mode',       'B&&W');

-- Detail
INSERT INTO translations VALUES ('en', 'detail.header',     'Detail');
INSERT INTO translations VALUES ('en', 'detail.sharpen',    'Sharpening');
INSERT INTO translations VALUES ('en', 'detail.amount',     'Amount');
INSERT INTO translations VALUES ('en', 'detail.radius',     'Radius');
INSERT INTO translations VALUES ('en', 'detail.detail',     'Detail');
INSERT INTO translations VALUES ('en', 'detail.masking',    'Masking');
INSERT INTO translations VALUES ('en', 'detail.nr',         'Noise Reduction');
INSERT INTO translations VALUES ('en', 'detail.nr_luma',    'Luminance');
INSERT INTO translations VALUES ('en', 'detail.nr_color',   'Color');
INSERT INTO translations VALUES ('en', 'detail.nr_detail',  'Detail');

-- Color Grading
INSERT INTO translations VALUES ('en', 'cg.header',      'Color Grading');
INSERT INTO translations VALUES ('en', 'cg.shadows',     'Shadows');
INSERT INTO translations VALUES ('en', 'cg.midtones',    'Midtones');
INSERT INTO translations VALUES ('en', 'cg.highlights',  'Highlights');
INSERT INTO translations VALUES ('en', 'cg.shadow_hue',  'Hue##cg_s');
INSERT INTO translations VALUES ('en', 'cg.shadow_sat',  'Saturation##cg_s');
INSERT INTO translations VALUES ('en', 'cg.mid_hue',     'Hue##cg_m');
INSERT INTO translations VALUES ('en', 'cg.mid_sat',     'Saturation##cg_m');
INSERT INTO translations VALUES ('en', 'cg.high_hue',    'Hue##cg_h');
INSERT INTO translations VALUES ('en', 'cg.high_sat',    'Saturation##cg_h');
INSERT INTO translations VALUES ('en', 'cg.blending',    'Blending');
INSERT INTO translations VALUES ('en', 'cg.balance',     'Balance');

-- Effects
INSERT INTO translations VALUES ('en', 'fx.header',         'Effects');
INSERT INTO translations VALUES ('en', 'fx.vibrance',       'Vibrance');
INSERT INTO translations VALUES ('en', 'fx.saturation',     'Saturation');

-- Effects: Vignette
INSERT INTO translations VALUES ('en', 'fx.vignette_header', 'Vignette');
INSERT INTO translations VALUES ('en', 'fx.vig_amount',      'Amount##vig');
INSERT INTO translations VALUES ('en', 'fx.vig_midpoint',    'Midpoint##vig');
INSERT INTO translations VALUES ('en', 'fx.vig_roundness',   'Roundness##vig');
INSERT INTO translations VALUES ('en', 'fx.vig_feather',     'Feather##vig');

-- Effects: Grain
INSERT INTO translations VALUES ('en', 'fx.grain_header',    'Grain');
INSERT INTO translations VALUES ('en', 'fx.grain_amount',    'Amount##grain');
INSERT INTO translations VALUES ('en', 'fx.grain_size',      'Size##grain');
INSERT INTO translations VALUES ('en', 'fx.grain_roughness', 'Roughness##grain');

-- Status
INSERT INTO translations VALUES ('en', 'status.ready',      'Ready');
INSERT INTO translations VALUES ('en', 'status.open_hint',  'Ctrl+O or File > Open RAW to get started');
INSERT INTO translations VALUES ('en', 'status.zoom',       'Zoom');

-- Metadata
INSERT INTO translations VALUES ('en', 'meta.camera',       'Camera');
INSERT INTO translations VALUES ('en', 'meta.lens',         'Lens');

-- Toolbar buttons and tooltips
INSERT INTO translations VALUES ('en', 'tb.open',            'Open');
INSERT INTO translations VALUES ('en', 'tb.open.tip',        'Open RAW file (Ctrl+O)');
INSERT INTO translations VALUES ('en', 'tb.save',            'Save');
INSERT INTO translations VALUES ('en', 'tb.save.tip',        'Save recipe (Ctrl+S)');
INSERT INTO translations VALUES ('en', 'tb.export',          'Export');
INSERT INTO translations VALUES ('en', 'tb.export.tip',      'Export image (Ctrl+Shift+E)');
INSERT INTO translations VALUES ('en', 'tb.undo',            'Undo');
INSERT INTO translations VALUES ('en', 'tb.undo.tip',        'Undo (Ctrl+Z)');
INSERT INTO translations VALUES ('en', 'tb.redo',            'Redo');
INSERT INTO translations VALUES ('en', 'tb.redo.tip',        'Redo (Ctrl+Y)');
INSERT INTO translations VALUES ('en', 'tb.fit',             'Fit');
INSERT INTO translations VALUES ('en', 'tb.fit.tip',         'Fit to window (F)');
INSERT INTO translations VALUES ('en', 'tb.zoom100.tip',     'Zoom 100% (1)');
INSERT INTO translations VALUES ('en', 'tb.zoom200.tip',     'Zoom 200% (2)');
INSERT INTO translations VALUES ('en', 'tb.before_after',    'B/A');
INSERT INTO translations VALUES ('en', 'tb.before_after.tip','Before / After toggle');
INSERT INTO translations VALUES ('en', 'tb.develop',         'Dev');
INSERT INTO translations VALUES ('en', 'tb.develop.tip',     'Develop mode');
INSERT INTO translations VALUES ('en', 'tb.grid',            'Grid');
INSERT INTO translations VALUES ('en', 'tb.grid.tip',        'Grid / Library mode');

-- WB Presets
INSERT INTO translations VALUES ('en', 'wb.preset.as_shot',    'As Shot');
INSERT INTO translations VALUES ('en', 'wb.preset.daylight',   'Daylight');
INSERT INTO translations VALUES ('en', 'wb.preset.cloudy',     'Cloudy');
INSERT INTO translations VALUES ('en', 'wb.preset.shade',      'Shade');
INSERT INTO translations VALUES ('en', 'wb.preset.tungsten',   'Tungsten');
INSERT INTO translations VALUES ('en', 'wb.preset.fluorescent','Fluorescent');
INSERT INTO translations VALUES ('en', 'wb.preset.flash',      'Flash');

-- Slider labels (used by vegaSlider with ImGui ## IDs)
INSERT INTO translations VALUES ('en', 'Temperature',       'Temperature');
INSERT INTO translations VALUES ('en', 'Tint',              'Tint');
INSERT INTO translations VALUES ('en', 'Exposure',          'Exposure');
INSERT INTO translations VALUES ('en', 'Contrast',          'Contrast');
INSERT INTO translations VALUES ('en', 'Highlights',        'Highlights');
INSERT INTO translations VALUES ('en', 'Shadows',           'Shadows');
INSERT INTO translations VALUES ('en', 'Whites',            'Whites');
INSERT INTO translations VALUES ('en', 'Blacks',            'Blacks');
INSERT INTO translations VALUES ('en', 'Red',               'Red');
INSERT INTO translations VALUES ('en', 'Orange',            'Orange');
INSERT INTO translations VALUES ('en', 'Yellow',            'Yellow');
INSERT INTO translations VALUES ('en', 'Green',             'Green');
INSERT INTO translations VALUES ('en', 'Aqua',              'Aqua');
INSERT INTO translations VALUES ('en', 'Blue',              'Blue');
INSERT INTO translations VALUES ('en', 'Purple',            'Purple');
INSERT INTO translations VALUES ('en', 'Magenta',           'Magenta');
INSERT INTO translations VALUES ('en', 'Amount##sharp',     'Amount');
INSERT INTO translations VALUES ('en', 'Radius##sharp',     'Radius');
INSERT INTO translations VALUES ('en', 'Detail##sharp',     'Detail');
INSERT INTO translations VALUES ('en', 'Masking##sharp',    'Masking');
INSERT INTO translations VALUES ('en', 'Luminance##nr',     'Luminance');
INSERT INTO translations VALUES ('en', 'Color##nr',         'Color');
INSERT INTO translations VALUES ('en', 'Detail##nr',        'Detail');
INSERT INTO translations VALUES ('en', 'Vibrance',          'Vibrance');
INSERT INTO translations VALUES ('en', 'Saturation##effect','Saturation');
INSERT INTO translations VALUES ('en', 'Hue',               'Hue');
INSERT INTO translations VALUES ('en', 'Saturation',        'Saturation');
INSERT INTO translations VALUES ('en', 'Luminance',         'Luminance');
INSERT INTO translations VALUES ('en', 'Metadata',          'Metadata');
INSERT INTO translations VALUES ('en', 'Sharpening',        'Sharpening');
INSERT INTO translations VALUES ('en', 'Noise Reduction',   'Noise Reduction');
INSERT INTO translations VALUES ('en', 'Rotation##crop',    'Rotation');
INSERT INTO translations VALUES ('en', 'Left##crop',        'Left');
INSERT INTO translations VALUES ('en', 'Top##crop',         'Top');
INSERT INTO translations VALUES ('en', 'Right##crop',       'Right');
INSERT INTO translations VALUES ('en', 'Bottom##crop',      'Bottom');

-- ============================================================================
-- Traditional Chinese (zh_tw)
-- ============================================================================

-- Menu
INSERT INTO translations VALUES ('zh_tw', 'menu.file',         '檔案');
INSERT INTO translations VALUES ('zh_tw', 'menu.open',         '開啟 RAW...');
INSERT INTO translations VALUES ('zh_tw', 'menu.save_recipe',  '儲存設定');
INSERT INTO translations VALUES ('zh_tw', 'menu.export',       '匯出...');
INSERT INTO translations VALUES ('zh_tw', 'menu.exit',         '結束');
INSERT INTO translations VALUES ('zh_tw', 'menu.edit',         '編輯');
INSERT INTO translations VALUES ('zh_tw', 'menu.undo',         '復原');
INSERT INTO translations VALUES ('zh_tw', 'menu.redo',         '重做');
INSERT INTO translations VALUES ('zh_tw', 'menu.reset_all',    '重設全部');
INSERT INTO translations VALUES ('zh_tw', 'menu.view',         '檢視');
INSERT INTO translations VALUES ('zh_tw', 'menu.fit',          '適合視窗');
INSERT INTO translations VALUES ('zh_tw', 'menu.before_after', '前後對比');
INSERT INTO translations VALUES ('zh_tw', 'menu.language',     '語言');

-- Panels
INSERT INTO translations VALUES ('zh_tw', 'panel.develop',     '編輯');
INSERT INTO translations VALUES ('zh_tw', 'panel.viewport',    '預覽');
INSERT INTO translations VALUES ('zh_tw', 'panel.histogram',   '直方圖');
INSERT INTO translations VALUES ('zh_tw', 'panel.metadata',    '元資料');
INSERT INTO translations VALUES ('zh_tw', 'panel.folders',     '資料夾');
INSERT INTO translations VALUES ('zh_tw', 'panel.grid',        '圖庫');

-- Folder
INSERT INTO translations VALUES ('zh_tw', 'folder.add',        '+ 新增資料夾');
INSERT INTO translations VALUES ('zh_tw', 'folder.remove',     '移除資料夾');
INSERT INTO translations VALUES ('zh_tw', 'folder.all_photos', '所有照片');
INSERT INTO translations VALUES ('zh_tw', 'import.progress',   '匯入中: %d / %d');

-- White Balance
INSERT INTO translations VALUES ('zh_tw', 'wb.header',         '白平衡');
INSERT INTO translations VALUES ('zh_tw', 'wb.temperature',    '色溫');
INSERT INTO translations VALUES ('zh_tw', 'wb.tint',           '色調');
INSERT INTO translations VALUES ('zh_tw', 'wb.eyedropper',     '白平衡吸管');

-- Tone
INSERT INTO translations VALUES ('zh_tw', 'tone.header',       '色調');
INSERT INTO translations VALUES ('zh_tw', 'tone.exposure',     '曝光度');
INSERT INTO translations VALUES ('zh_tw', 'tone.contrast',     '對比');
INSERT INTO translations VALUES ('zh_tw', 'tone.highlights',   '亮部');
INSERT INTO translations VALUES ('zh_tw', 'tone.shadows',      '暗部');
INSERT INTO translations VALUES ('zh_tw', 'tone.whites',       '白色');
INSERT INTO translations VALUES ('zh_tw', 'tone.blacks',       '黑色');
INSERT INTO translations VALUES ('zh_tw', 'tone.auto',         '自動');

-- Presence
INSERT INTO translations VALUES ('zh_tw', 'presence.header',  '存在感');
INSERT INTO translations VALUES ('zh_tw', 'presence.clarity', '清晰度');
INSERT INTO translations VALUES ('zh_tw', 'presence.texture', '紋理');
INSERT INTO translations VALUES ('zh_tw', 'presence.dehaze',  '去霧');

-- Tone Curve
INSERT INTO translations VALUES ('zh_tw', 'curve.header',      '色調曲線');

-- HSL
INSERT INTO translations VALUES ('zh_tw', 'hsl.header',        'HSL / 色彩');
INSERT INTO translations VALUES ('zh_tw', 'hsl.hue',           '色相');
INSERT INTO translations VALUES ('zh_tw', 'hsl.saturation',    '飽和度');
INSERT INTO translations VALUES ('zh_tw', 'hsl.luminance',     '明度');
INSERT INTO translations VALUES ('zh_tw', 'hsl.red',           '紅色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.orange',        '橙色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.yellow',        '黃色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.green',         '綠色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.aqua',          '水色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.blue',          '藍色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.purple',        '紫色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.magenta',       '洋紅');
INSERT INTO translations VALUES ('zh_tw', 'hsl.color_mode',    '彩色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.bw_mode',       '黑白');

-- Detail
INSERT INTO translations VALUES ('zh_tw', 'detail.header',     '細節');
INSERT INTO translations VALUES ('zh_tw', 'detail.sharpen',    '銳利化');
INSERT INTO translations VALUES ('zh_tw', 'detail.amount',     '總量');
INSERT INTO translations VALUES ('zh_tw', 'detail.radius',     '半徑');
INSERT INTO translations VALUES ('zh_tw', 'detail.detail',     '細節');
INSERT INTO translations VALUES ('zh_tw', 'detail.masking',    '遮罩');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr',         '降噪');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr_luma',    '亮度');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr_color',   '色彩');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr_detail',  '細節');

-- Color Grading
INSERT INTO translations VALUES ('zh_tw', 'cg.header',      '色彩分級');
INSERT INTO translations VALUES ('zh_tw', 'cg.shadows',     '陰影');
INSERT INTO translations VALUES ('zh_tw', 'cg.midtones',    '中間調');
INSERT INTO translations VALUES ('zh_tw', 'cg.highlights',  '亮部');
INSERT INTO translations VALUES ('zh_tw', 'cg.shadow_hue',  '色相##cg_s');
INSERT INTO translations VALUES ('zh_tw', 'cg.shadow_sat',  '飽和度##cg_s');
INSERT INTO translations VALUES ('zh_tw', 'cg.mid_hue',     '色相##cg_m');
INSERT INTO translations VALUES ('zh_tw', 'cg.mid_sat',     '飽和度##cg_m');
INSERT INTO translations VALUES ('zh_tw', 'cg.high_hue',    '色相##cg_h');
INSERT INTO translations VALUES ('zh_tw', 'cg.high_sat',    '飽和度##cg_h');
INSERT INTO translations VALUES ('zh_tw', 'cg.blending',    '混合');
INSERT INTO translations VALUES ('zh_tw', 'cg.balance',     '平衡');

-- Effects
INSERT INTO translations VALUES ('zh_tw', 'fx.header',         '效果');
INSERT INTO translations VALUES ('zh_tw', 'fx.vibrance',       '自然飽和度');
INSERT INTO translations VALUES ('zh_tw', 'fx.saturation',     '飽和度');

-- Effects: Vignette
INSERT INTO translations VALUES ('zh_tw', 'fx.vignette_header', '暗角');
INSERT INTO translations VALUES ('zh_tw', 'fx.vig_amount',      '強度##vig');
INSERT INTO translations VALUES ('zh_tw', 'fx.vig_midpoint',    '中心點##vig');
INSERT INTO translations VALUES ('zh_tw', 'fx.vig_roundness',   '圓度##vig');
INSERT INTO translations VALUES ('zh_tw', 'fx.vig_feather',     '羽化##vig');

-- Effects: Grain
INSERT INTO translations VALUES ('zh_tw', 'fx.grain_header',    '顆粒');
INSERT INTO translations VALUES ('zh_tw', 'fx.grain_amount',    '強度##grain');
INSERT INTO translations VALUES ('zh_tw', 'fx.grain_size',      '大小##grain');
INSERT INTO translations VALUES ('zh_tw', 'fx.grain_roughness', '粗糙度##grain');

-- Status
INSERT INTO translations VALUES ('zh_tw', 'status.ready',      '就緒');
INSERT INTO translations VALUES ('zh_tw', 'status.open_hint',  'Ctrl+O 或 檔案 > 開啟 RAW');
INSERT INTO translations VALUES ('zh_tw', 'status.zoom',       '縮放');

-- Metadata
INSERT INTO translations VALUES ('zh_tw', 'meta.camera',       '相機');
INSERT INTO translations VALUES ('zh_tw', 'meta.lens',         '鏡頭');

-- Slider labels
INSERT INTO translations VALUES ('zh_tw', 'Temperature',       '色溫');
INSERT INTO translations VALUES ('zh_tw', 'Tint',              '色調');
INSERT INTO translations VALUES ('zh_tw', 'Exposure',          '曝光度');
INSERT INTO translations VALUES ('zh_tw', 'Contrast',          '對比');
INSERT INTO translations VALUES ('zh_tw', 'Highlights',        '亮部');
INSERT INTO translations VALUES ('zh_tw', 'Shadows',           '暗部');
INSERT INTO translations VALUES ('zh_tw', 'Whites',            '白色');
INSERT INTO translations VALUES ('zh_tw', 'Blacks',            '黑色');
INSERT INTO translations VALUES ('zh_tw', 'Red',               '紅色');
INSERT INTO translations VALUES ('zh_tw', 'Orange',            '橙色');
INSERT INTO translations VALUES ('zh_tw', 'Yellow',            '黃色');
INSERT INTO translations VALUES ('zh_tw', 'Green',             '綠色');
INSERT INTO translations VALUES ('zh_tw', 'Aqua',              '水色');
INSERT INTO translations VALUES ('zh_tw', 'Blue',              '藍色');
INSERT INTO translations VALUES ('zh_tw', 'Purple',            '紫色');
INSERT INTO translations VALUES ('zh_tw', 'Magenta',           '洋紅');
INSERT INTO translations VALUES ('zh_tw', 'Amount##sharp',     '總量');
INSERT INTO translations VALUES ('zh_tw', 'Radius##sharp',     '半徑');
INSERT INTO translations VALUES ('zh_tw', 'Detail##sharp',     '細節');
INSERT INTO translations VALUES ('zh_tw', 'Masking##sharp',    '遮罩');
INSERT INTO translations VALUES ('zh_tw', 'Luminance##nr',     '亮度');
INSERT INTO translations VALUES ('zh_tw', 'Color##nr',         '色彩');
INSERT INTO translations VALUES ('zh_tw', 'Detail##nr',        '細節');
INSERT INTO translations VALUES ('zh_tw', 'Vibrance',          '自然飽和度');
INSERT INTO translations VALUES ('zh_tw', 'Saturation##effect','飽和度');
INSERT INTO translations VALUES ('zh_tw', 'Hue',               '色相');
INSERT INTO translations VALUES ('zh_tw', 'Saturation',        '飽和度');
INSERT INTO translations VALUES ('zh_tw', 'Luminance',         '明度');
INSERT INTO translations VALUES ('zh_tw', 'Metadata',          '元資料');
INSERT INTO translations VALUES ('zh_tw', 'Sharpening',        '銳利化');
INSERT INTO translations VALUES ('zh_tw', 'Noise Reduction',   '降噪');

-- Toolbar
INSERT INTO translations VALUES ('zh_tw', 'tb.open',            '開啟');
INSERT INTO translations VALUES ('zh_tw', 'tb.open.tip',        '開啟 RAW 檔 (Ctrl+O)');
INSERT INTO translations VALUES ('zh_tw', 'tb.save',            '儲存');
INSERT INTO translations VALUES ('zh_tw', 'tb.save.tip',        '儲存設定 (Ctrl+S)');
INSERT INTO translations VALUES ('zh_tw', 'tb.export',          '匯出');
INSERT INTO translations VALUES ('zh_tw', 'tb.export.tip',      '匯出影像 (Ctrl+Shift+E)');
INSERT INTO translations VALUES ('zh_tw', 'tb.undo',            '復原');
INSERT INTO translations VALUES ('zh_tw', 'tb.undo.tip',        '復原 (Ctrl+Z)');
INSERT INTO translations VALUES ('zh_tw', 'tb.redo',            '重做');
INSERT INTO translations VALUES ('zh_tw', 'tb.redo.tip',        '重做 (Ctrl+Y)');
INSERT INTO translations VALUES ('zh_tw', 'tb.fit',             '適配');
INSERT INTO translations VALUES ('zh_tw', 'tb.fit.tip',         '適合視窗 (F)');
INSERT INTO translations VALUES ('zh_tw', 'tb.zoom100.tip',     '縮放 100% (1)');
INSERT INTO translations VALUES ('zh_tw', 'tb.zoom200.tip',     '縮放 200% (2)');
INSERT INTO translations VALUES ('zh_tw', 'tb.before_after',    '前/後');
INSERT INTO translations VALUES ('zh_tw', 'tb.before_after.tip','前後對比切換');
INSERT INTO translations VALUES ('zh_tw', 'tb.develop',         '編輯');
INSERT INTO translations VALUES ('zh_tw', 'tb.develop.tip',     '編輯模式');
INSERT INTO translations VALUES ('zh_tw', 'tb.grid',            '圖庫');
INSERT INTO translations VALUES ('zh_tw', 'tb.grid.tip',        '圖庫 / Library 模式');

-- WB Presets
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.as_shot',    '拍攝設定');
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.daylight',   '日光');
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.cloudy',     '多雲');
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.shade',      '陰影');
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.tungsten',   '鎢絲燈');
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.fluorescent','螢光燈');
INSERT INTO translations VALUES ('zh_tw', 'wb.preset.flash',      '閃光燈');

-- Crop & Straighten
INSERT INTO translations VALUES ('zh_tw', 'crop.header',   '裁切與拉直');
INSERT INTO translations VALUES ('zh_tw', 'crop.free',     '自由');
INSERT INTO translations VALUES ('zh_tw', 'crop.reset',    '重設裁切');
INSERT INTO translations VALUES ('zh_tw', 'Rotation##crop','旋轉');
INSERT INTO translations VALUES ('zh_tw', 'Left##crop',    '左');
INSERT INTO translations VALUES ('zh_tw', 'Top##crop',     '上');
INSERT INTO translations VALUES ('zh_tw', 'Right##crop',   '右');
INSERT INTO translations VALUES ('zh_tw', 'Bottom##crop',  '下');
